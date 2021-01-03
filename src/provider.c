/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include "symbiomon/symbiomon-server.h"
#include "provider.h"
#include "types.h"

// backends that we want to add at compile time
#include "dummy/dummy-backend.h"

static void symbiomon_finalize_provider(void* p);

/* Functions to manipulate the hash of metrics */
static inline symbiomon_metric* find_metric(
        symbiomon_provider_t provider,
        const symbiomon_metric_id_t* id);

static inline symbiomon_return_t add_metric(
        symbiomon_provider_t provider,
        symbiomon_metric* metric);

static inline symbiomon_return_t remove_metric(
        symbiomon_provider_t provider,
        const symbiomon_metric_id_t* id,
        int close_metric);

static inline void remove_all_metrics(
        symbiomon_provider_t provider);

/* Functions to manipulate the list of backend types */
static inline symbiomon_backend_impl* find_backend_impl(
        symbiomon_provider_t provider,
        const char* name);

static inline symbiomon_return_t add_backend_impl(
        symbiomon_provider_t provider,
        symbiomon_backend_impl* backend);

/* Function to check the validity of the token sent by an admin
 * (returns 0 is the token is incorrect) */
static inline int check_token(
        symbiomon_provider_t provider,
        const char* token);

/* Admin RPCs */
static DECLARE_MARGO_RPC_HANDLER(symbiomon_create_metric_ult)
static void symbiomon_create_metric_ult(hg_handle_t h);
static DECLARE_MARGO_RPC_HANDLER(symbiomon_open_metric_ult)
static void symbiomon_open_metric_ult(hg_handle_t h);
static DECLARE_MARGO_RPC_HANDLER(symbiomon_close_metric_ult)
static void symbiomon_close_metric_ult(hg_handle_t h);
static DECLARE_MARGO_RPC_HANDLER(symbiomon_destroy_metric_ult)
static void symbiomon_destroy_metric_ult(hg_handle_t h);
static DECLARE_MARGO_RPC_HANDLER(symbiomon_list_metrics_ult)
static void symbiomon_list_metrics_ult(hg_handle_t h);

/* Client RPCs */
static DECLARE_MARGO_RPC_HANDLER(symbiomon_hello_ult)
static void symbiomon_hello_ult(hg_handle_t h);
static DECLARE_MARGO_RPC_HANDLER(symbiomon_sum_ult)
static void symbiomon_sum_ult(hg_handle_t h);

/* add other RPC declarations here */

int symbiomon_provider_register(
        margo_instance_id mid,
        uint16_t provider_id,
        const struct symbiomon_provider_args* args,
        symbiomon_provider_t* provider)
{
    struct symbiomon_provider_args a = SYMBIOMON_PROVIDER_ARGS_INIT;
    if(args) a = *args;
    symbiomon_provider_t p;
    hg_id_t id;
    hg_bool_t flag;

    margo_info(mid, "Registering SYMBIOMON provider with provider id %u", provider_id);

    flag = margo_is_listening(mid);
    if(flag == HG_FALSE) {
        margo_error(mid, "Margo instance is not a server");
        return SYMBIOMON_ERR_INVALID_ARGS;
    }

    margo_provider_registered_name(mid, "symbiomon_sum", provider_id, &id, &flag);
    if(flag == HG_TRUE) {
        margo_error(mid, "Provider with the same provider id (%u) already register", provider_id);
        return SYMBIOMON_ERR_INVALID_PROVIDER;
    }

    p = (symbiomon_provider_t)calloc(1, sizeof(*p));
    if(p == NULL) {
        margo_error(mid, "Could not allocate memory for provider");
        return SYMBIOMON_ERR_ALLOCATION;
    }

    p->mid = mid;
    p->provider_id = provider_id;
    p->pool = a.pool;
    p->abtio = a.abtio;
    p->token = (a.token && strlen(a.token)) ? strdup(a.token) : NULL;

    /* Admin RPCs */
    id = MARGO_REGISTER_PROVIDER(mid, "symbiomon_create_metric",
            create_metric_in_t, create_metric_out_t,
            symbiomon_create_metric_ult, provider_id, p->pool);
    margo_register_data(mid, id, (void*)p, NULL);
    p->create_metric_id = id;

    id = MARGO_REGISTER_PROVIDER(mid, "symbiomon_open_metric",
            open_metric_in_t, open_metric_out_t,
            symbiomon_open_metric_ult, provider_id, p->pool);
    margo_register_data(mid, id, (void*)p, NULL);
    p->open_metric_id = id;

    id = MARGO_REGISTER_PROVIDER(mid, "symbiomon_close_metric",
            close_metric_in_t, close_metric_out_t,
            symbiomon_close_metric_ult, provider_id, p->pool);
    margo_register_data(mid, id, (void*)p, NULL);
    p->close_metric_id = id;

    id = MARGO_REGISTER_PROVIDER(mid, "symbiomon_destroy_metric",
            destroy_metric_in_t, destroy_metric_out_t,
            symbiomon_destroy_metric_ult, provider_id, p->pool);
    margo_register_data(mid, id, (void*)p, NULL);
    p->destroy_metric_id = id;

    id = MARGO_REGISTER_PROVIDER(mid, "symbiomon_list_metrics",
            list_metrics_in_t, list_metrics_out_t,
            symbiomon_list_metrics_ult, provider_id, p->pool);
    margo_register_data(mid, id, (void*)p, NULL);
    p->list_metrics_id = id;

    /* Client RPCs */

    id = MARGO_REGISTER_PROVIDER(mid, "symbiomon_hello",
            hello_in_t, void,
            symbiomon_hello_ult, provider_id, p->pool);
    margo_register_data(mid, id, (void*)p, NULL);
    p->hello_id = id;
    margo_registered_disable_response(mid, id, HG_TRUE);

    id = MARGO_REGISTER_PROVIDER(mid, "symbiomon_sum",
            sum_in_t, sum_out_t,
            symbiomon_sum_ult, provider_id, p->pool);
    margo_register_data(mid, id, (void*)p, NULL);
    p->sum_id = id;

    /* add other RPC registration here */
    /* ... */

    /* add backends available at compiler time (e.g. default/dummy backends) */
    symbiomon_provider_register_dummy_backend(p); // function from "dummy/dummy-backend.h"

    margo_provider_push_finalize_callback(mid, p, &symbiomon_finalize_provider, p);

    if(provider)
        *provider = p;
    margo_info(mid, "SYMBIOMON provider registration done");
    return SYMBIOMON_SUCCESS;
}

static void symbiomon_finalize_provider(void* p)
{
    symbiomon_provider_t provider = (symbiomon_provider_t)p;
    margo_info(provider->mid, "Finalizing SYMBIOMON provider");
    margo_deregister(provider->mid, provider->create_metric_id);
    margo_deregister(provider->mid, provider->open_metric_id);
    margo_deregister(provider->mid, provider->close_metric_id);
    margo_deregister(provider->mid, provider->destroy_metric_id);
    margo_deregister(provider->mid, provider->list_metrics_id);
    margo_deregister(provider->mid, provider->hello_id);
    margo_deregister(provider->mid, provider->sum_id);
    /* deregister other RPC ids ... */
    remove_all_metrics(provider);
    free(provider->backend_types);
    free(provider->token);
    free(provider);
    margo_info(provider->mid, "SYMBIOMON provider successfuly finalized");
}

int symbiomon_provider_destroy(
        symbiomon_provider_t provider)
{
    margo_instance_id mid = provider->mid;
    margo_info(mid, "Destroying SYMBIOMON provider");
    /* pop the finalize callback */
    margo_provider_pop_finalize_callback(provider->mid, provider);
    /* call the callback */
    symbiomon_finalize_provider(provider);
    margo_info(mid, "SYMBIOMON provider successfuly destroyed");
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_provider_register_backend(
        symbiomon_provider_t provider,
        symbiomon_backend_impl* backend_impl)
{
    margo_info(provider->mid, "Adding backend implementation \"%s\" to SYMBIOMON provider",
             backend_impl->name);
    return add_backend_impl(provider, backend_impl);
}

static void symbiomon_create_metric_ult(hg_handle_t h)
{
    hg_return_t hret;
    symbiomon_return_t ret;
    create_metric_in_t  in;
    create_metric_out_t out;

    /* find the margo instance */
    margo_instance_id mid = margo_hg_handle_get_instance(h);

    /* find the provider */
    const struct hg_info* info = margo_get_info(h);
    symbiomon_provider_t provider = (symbiomon_provider_t)margo_registered_data(mid, info->id);

    /* deserialize the input */
    hret = margo_get_input(h, &in);
    if(hret != HG_SUCCESS) {
        margo_info(provider->mid, "Could not deserialize output (mercury error %d)", hret);
        out.ret = SYMBIOMON_ERR_FROM_MERCURY;
        goto finish;
    }

    /* check the token sent by the admin */
    if(!check_token(provider, in.token)) {
        margo_error(provider->mid, "Invalid token");
        out.ret = SYMBIOMON_ERR_INVALID_TOKEN;
        goto finish;
    }

    /* find the backend implementation for the requested type */
    symbiomon_backend_impl* backend = find_backend_impl(provider, in.type);
    if(!backend) {
        margo_error(provider->mid, "Could not find backend of type \"%s\"", in.type);
        out.ret = SYMBIOMON_ERR_INVALID_BACKEND;
        goto finish;
    }

    /* create a uuid for the new metric */
    symbiomon_metric_id_t id;
    uuid_generate(id.uuid);

    /* create the new metric's context */
    void* context = NULL;
    ret = backend->create_metric(provider, in.config, &context);
    if(ret != SYMBIOMON_SUCCESS) {
        out.ret = ret;
        margo_error(provider->mid, "Could not create metric, backend returned %d", ret);
        goto finish;
    }

    /* allocate a metric, set it up, and add it to the provider */
    symbiomon_metric* metric = (symbiomon_metric*)calloc(1, sizeof(*metric));
    metric->fn  = backend;
    metric->ctx = context;
    metric->id  = id;
    add_metric(provider, metric);

    /* set the response */
    out.ret = SYMBIOMON_SUCCESS;
    out.id = id;

    char id_str[37];
    symbiomon_metric_id_to_string(id, id_str);
    margo_debug(provider->mid, "Created metric %s of type \"%s\"", id_str, in.type);

finish:
    ret = margo_respond(h, &out);
    ret = margo_free_input(h, &in);
    margo_destroy(h);
}
static DEFINE_MARGO_RPC_HANDLER(symbiomon_create_metric_ult)

static void symbiomon_open_metric_ult(hg_handle_t h)
{
    hg_return_t hret;
    symbiomon_return_t ret;
    open_metric_in_t  in;
    open_metric_out_t out;

    /* find the margo instance */
    margo_instance_id mid = margo_hg_handle_get_instance(h);

    /* find the provider */
    const struct hg_info* info = margo_get_info(h);
    symbiomon_provider_t provider = (symbiomon_provider_t)margo_registered_data(mid, info->id);

    /* deserialize the input */
    hret = margo_get_input(h, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid, "Could not deserialize output (mercury error %d)", hret);
        out.ret = SYMBIOMON_ERR_FROM_MERCURY;
        goto finish;
    }

    /* check the token sent by the admin */
    if(!check_token(provider, in.token)) {
        margo_error(mid, "Invalid token");
        out.ret = SYMBIOMON_ERR_INVALID_TOKEN;
        goto finish;
    }

    /* find the backend implementation for the requested type */
    symbiomon_backend_impl* backend = find_backend_impl(provider, in.type);
    if(!backend) {
        margo_error(mid, "Could not find backend of type \"%s\"", in.type);
        out.ret = SYMBIOMON_ERR_INVALID_BACKEND;
        goto finish;
    }

    /* create a uuid for the new metric */
    symbiomon_metric_id_t id;
    uuid_generate(id.uuid);

    /* create the new metric's context */
    void* context = NULL;
    ret = backend->open_metric(provider, in.config, &context);
    if(ret != SYMBIOMON_SUCCESS) {
        margo_error(mid, "Backend failed to open metric");
        out.ret = ret;
        goto finish;
    }

    /* allocate a metric, set it up, and add it to the provider */
    symbiomon_metric* metric = (symbiomon_metric*)calloc(1, sizeof(*metric));
    metric->fn  = backend;
    metric->ctx = context;
    metric->id  = id;
    add_metric(provider, metric);

    /* set the response */
    out.ret = SYMBIOMON_SUCCESS;
    out.id = id;

    char id_str[37];
    symbiomon_metric_id_to_string(id, id_str);
    margo_debug(mid, "Created metric %s of type \"%s\"", id_str, in.type);

finish:
    hret = margo_respond(h, &out);
    hret = margo_free_input(h, &in);
    margo_destroy(h);
}
static DEFINE_MARGO_RPC_HANDLER(symbiomon_open_metric_ult)

static void symbiomon_close_metric_ult(hg_handle_t h)
{
    hg_return_t hret;
    symbiomon_return_t ret;
    close_metric_in_t  in;
    close_metric_out_t out;

    /* find the margo instance */
    margo_instance_id mid = margo_hg_handle_get_instance(h);

    /* find the provider */
    const struct hg_info* info = margo_get_info(h);
    symbiomon_provider_t provider = (symbiomon_provider_t)margo_registered_data(mid, info->id);

    /* deserialize the input */
    hret = margo_get_input(h, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid, "Could not deserialize output (mercury error %d)", hret);
        out.ret = SYMBIOMON_ERR_FROM_MERCURY;
        goto finish;
    }

    /* check the token sent by the admin */
    if(!check_token(provider, in.token)) {
        margo_error(mid, "Invalid token");
        out.ret = SYMBIOMON_ERR_INVALID_TOKEN;
        goto finish;
    }

    /* remove the metric from the provider 
     * (its close function will be called) */
    ret = remove_metric(provider, &in.id, 1);
    out.ret = ret;

    char id_str[37];
    symbiomon_metric_id_to_string(in.id, id_str);
    margo_debug(mid, "Removed metric with id %s", id_str);

finish:
    hret = margo_respond(h, &out);
    hret = margo_free_input(h, &in);
    margo_destroy(h);
}
static DEFINE_MARGO_RPC_HANDLER(symbiomon_close_metric_ult)

static void symbiomon_destroy_metric_ult(hg_handle_t h)
{
    hg_return_t hret;
    destroy_metric_in_t  in;
    destroy_metric_out_t out;

    /* find the margo instance */
    margo_instance_id mid = margo_hg_handle_get_instance(h);

    /* find the provider */
    const struct hg_info* info = margo_get_info(h);
    symbiomon_provider_t provider = (symbiomon_provider_t)margo_registered_data(mid, info->id);

    /* deserialize the input */
    hret = margo_get_input(h, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid, "Could not deserialize output (mercury error %d)", hret);
        out.ret = SYMBIOMON_ERR_FROM_MERCURY;
        goto finish;
    }

    /* check the token sent by the admin */
    if(!check_token(provider, in.token)) {
        margo_error(mid, "Invalid token");
        out.ret = SYMBIOMON_ERR_INVALID_TOKEN;
        goto finish;
    }

    /* find the metric */
    symbiomon_metric* metric = find_metric(provider, &in.id);
    if(!metric) {
        margo_error(mid, "Could not find metric");
        out.ret = SYMBIOMON_ERR_INVALID_METRIC;
        goto finish;
    }

    /* destroy the metric's context */
    metric->fn->destroy_metric(metric->ctx);

    /* remove the metric from the provider 
     * (its close function will NOT be called) */
    out.ret = remove_metric(provider, &in.id, 0);

    if(out.ret == SYMBIOMON_SUCCESS) {
        char id_str[37];
        symbiomon_metric_id_to_string(in.id, id_str);
        margo_debug(mid, "Destroyed metric with id %s", id_str);
    } else {
        margo_error(mid, "Could not destroy metric, metric may be left in an invalid state");
    }


finish:
    hret = margo_respond(h, &out);
    hret = margo_free_input(h, &in);
    margo_destroy(h);
}
static DEFINE_MARGO_RPC_HANDLER(symbiomon_destroy_metric_ult)

static void symbiomon_list_metrics_ult(hg_handle_t h)
{
    hg_return_t hret;
    list_metrics_in_t  in;
    list_metrics_out_t out;
    out.ids = NULL;

    /* find margo instance */
    margo_instance_id mid = margo_hg_handle_get_instance(h);

    /* find provider */
    const struct hg_info* info = margo_get_info(h);
    symbiomon_provider_t provider = (symbiomon_provider_t)margo_registered_data(mid, info->id);

    /* deserialize the input */
    hret = margo_get_input(h, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid, "Could not deserialize output (mercury error %d)", hret);
        out.ret = SYMBIOMON_ERR_FROM_MERCURY;
        goto finish;
    }

    /* check the token sent by the admin */
    if(!check_token(provider, in.token)) {
        margo_error(mid, "Invalid token");
        out.ret = SYMBIOMON_ERR_INVALID_TOKEN;
        goto finish;
    }

    /* allocate array of metric ids */
    out.ret   = SYMBIOMON_SUCCESS;
    out.count = provider->num_metrics < in.max_ids ? provider->num_metrics : in.max_ids;
    out.ids   = (symbiomon_metric_id_t*)calloc(provider->num_metrics, sizeof(*out.ids));

    /* iterate over the hash of metrics to fill the array of metric ids */
    unsigned i = 0;
    symbiomon_metric *r, *tmp;
    HASH_ITER(hh, provider->metrics, r, tmp) {
        out.ids[i++] = r->id;
    }

    margo_debug(mid, "Listed metrics");

finish:
    hret = margo_respond(h, &out);
    hret = margo_free_input(h, &in);
    free(out.ids);
    margo_destroy(h);
}
static DEFINE_MARGO_RPC_HANDLER(symbiomon_list_metrics_ult)

static void symbiomon_hello_ult(hg_handle_t h)
{
    hg_return_t hret;
    hello_in_t in;

    /* find margo instance */
    margo_instance_id mid = margo_hg_handle_get_instance(h);

    /* find provider */
    const struct hg_info* info = margo_get_info(h);
    symbiomon_provider_t provider = (symbiomon_provider_t)margo_registered_data(mid, info->id);

    /* deserialize the input */
    hret = margo_get_input(h, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid, "Could not deserialize output (mercury error %d)", hret);
        goto finish;
    }

    /* find the metric */
    symbiomon_metric* metric = find_metric(provider, &in.metric_id);
    if(!metric) {
        margo_error(mid, "Could not find requested metric");
        goto finish;
    }

    /* call hello on the metric's context */
    metric->fn->hello(metric->ctx);

    margo_debug(mid, "Called hello RPC");

finish:
    margo_destroy(h);
}
static DEFINE_MARGO_RPC_HANDLER(symbiomon_hello_ult)

static void symbiomon_sum_ult(hg_handle_t h)
{
    hg_return_t hret;
    sum_in_t     in;
    sum_out_t   out;

    /* find the margo instance */
    margo_instance_id mid = margo_hg_handle_get_instance(h);

    /* find the provider */
    const struct hg_info* info = margo_get_info(h);
    symbiomon_provider_t provider = (symbiomon_provider_t)margo_registered_data(mid, info->id);

    /* deserialize the input */
    hret = margo_get_input(h, &in);
    if(hret != HG_SUCCESS) {
        margo_error(mid, "Could not deserialize output (mercury error %d)", hret);
        out.ret = SYMBIOMON_ERR_FROM_MERCURY;
        goto finish;
    }

    /* find the metric */
    symbiomon_metric* metric = find_metric(provider, &in.metric_id);
    if(!metric) {
        margo_error(mid, "Could not find requested metric");
        out.ret = SYMBIOMON_ERR_INVALID_METRIC;
        goto finish;
    }

    /* call hello on the metric's context */
    out.result = metric->fn->sum(metric->ctx, in.x, in.y);
    out.ret = SYMBIOMON_SUCCESS;

    margo_debug(mid, "Called sum RPC");

finish:
    hret = margo_respond(h, &out);
    hret = margo_free_input(h, &in);
    margo_destroy(h);
}
static DEFINE_MARGO_RPC_HANDLER(symbiomon_sum_ult)

static inline symbiomon_metric* find_metric(
        symbiomon_provider_t provider,
        const symbiomon_metric_id_t* id)
{
    symbiomon_metric* metric = NULL;
    HASH_FIND(hh, provider->metrics, id, sizeof(symbiomon_metric_id_t), metric);
    return metric;
}

static inline symbiomon_return_t add_metric(
        symbiomon_provider_t provider,
        symbiomon_metric* metric)
{
    symbiomon_metric* existing = find_metric(provider, &(metric->id));
    if(existing) {
        return SYMBIOMON_ERR_INVALID_METRIC;
    }
    HASH_ADD(hh, provider->metrics, id, sizeof(symbiomon_metric_id_t), metric);
    provider->num_metrics += 1;
    return SYMBIOMON_SUCCESS;
}

static inline symbiomon_return_t remove_metric(
        symbiomon_provider_t provider,
        const symbiomon_metric_id_t* id,
        int close_metric)
{
    symbiomon_metric* metric = find_metric(provider, id);
    if(!metric) {
        return SYMBIOMON_ERR_INVALID_METRIC;
    }
    symbiomon_return_t ret = SYMBIOMON_SUCCESS;
    if(close_metric) {
        ret = metric->fn->close_metric(metric->ctx);
    }
    HASH_DEL(provider->metrics, metric);
    free(metric);
    provider->num_metrics -= 1;
    return ret;
}

static inline void remove_all_metrics(
        symbiomon_provider_t provider)
{
    symbiomon_metric *r, *tmp;
    HASH_ITER(hh, provider->metrics, r, tmp) {
        HASH_DEL(provider->metrics, r);
        r->fn->close_metric(r->ctx);
        free(r);
    }
    provider->num_metrics = 0;
}

static inline symbiomon_backend_impl* find_backend_impl(
        symbiomon_provider_t provider,
        const char* name)
{
    size_t i;
    for(i = 0; i < provider->num_backend_types; i++) {
        symbiomon_backend_impl* impl = provider->backend_types[i];
        if(strcmp(name, impl->name) == 0)
            return impl;
    }
    return NULL;
}

static inline symbiomon_return_t add_backend_impl(
        symbiomon_provider_t provider,
        symbiomon_backend_impl* backend)
{
    provider->num_backend_types += 1;
    provider->backend_types = realloc(provider->backend_types,
                                      provider->num_backend_types);
    provider->backend_types[provider->num_backend_types-1] = backend;
    return SYMBIOMON_SUCCESS;
}

static inline int check_token(
        symbiomon_provider_t provider,
        const char* token)
{
    if(!provider->token) return 1;
    return !strcmp(provider->token, token);
}
