/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <assert.h>
#include "symbiomon/symbiomon-server.h"
#include "symbiomon/symbiomon-backend.h"
#include "provider.h"
#include "types.h"

#define METRIC_BUFFER_SIZE 100000

static void symbiomon_finalize_provider(void* p);

/* Functions to manipulate the hash of metrics */
static inline void prepare_string_for_uuid(char *input, char *output);

static inline void uuid_xor(symbiomon_metric_id_t first, symbiomon_metric_id_t second, symbiomon_metric_id_t * result);

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

/* Admin RPCs */

/* Client RPCs */
static DECLARE_MARGO_RPC_HANDLER(symbiomon_metric_fetch_ult)
static void symbiomon_metric_fetch_ult(hg_handle_t h);
static DECLARE_MARGO_RPC_HANDLER(symbiomon_list_metrics_ult)
static void symbiomon_list_metrics_ult(hg_handle_t h);

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

    margo_provider_registered_name(mid, "symbiomon_remote_metric_fetch", provider_id, &id, &flag);
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

    /* Admin RPCs */

    /* Client RPCs */
    id = MARGO_REGISTER_PROVIDER(mid, "symbiomon_remote_metric_fetch",
            metric_fetch_in_t, metric_fetch_out_t,
            symbiomon_metric_fetch_ult, provider_id, p->pool);
    margo_register_data(mid, id, (void*)p, NULL);
    p->metric_fetch_id = id;

    id = MARGO_REGISTER_PROVIDER(mid, "symbiomon_remote_list_metrics",
            list_metrics_in_t, list_metrics_out_t,
            symbiomon_list_metrics_ult, provider_id, p->pool);
    margo_register_data(mid, id, (void*)p, NULL);
    p->list_metrics_id = id;

    /* add other RPC registration here */
    /* ... */

    /* add backends available at compiler time (e.g. default/dummy backends) */

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
    margo_deregister(provider->mid, provider->metric_fetch_id);
    margo_deregister(provider->mid, provider->list_metrics_id);
    /* deregister other RPC ids ... */
    remove_all_metrics(provider);
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

symbiomon_return_t symbiomon_provider_metric_create(char *ns, char *name, symbiomon_metric_type_t t, char *desc, char **taglist, int num_tags, symbiomon_metric_t* m, symbiomon_provider_t provider)
{
    if(!ns || !name)
        return SYMBIOMON_ERR_INVALID_NAME;

    /* create a uuid for the new metric */
    symbiomon_metric_id_t id, temp_id;
    char buffered_str[37];

    prepare_string_for_uuid(ns, buffered_str); 
    symbiomon_metric_id_from_string(buffered_str, &id);

    prepare_string_for_uuid(name, buffered_str); 
    symbiomon_metric_id_from_string(buffered_str, &temp_id);

    uuid_xor(id, temp_id, &id);

    /* XOR all the tag ids, so that any ordering of tags returns the same final metric id */
    int i;
    for(i = 0; i < num_tags; i++) {
        prepare_string_for_uuid(taglist[i], buffered_str);
        symbiomon_metric_id_from_string(buffered_str, &temp_id);
        uuid_xor(id, temp_id, &id);
    }

    /* allocate a metric, set it up, and add it to the provider */
    symbiomon_metric* metric = (symbiomon_metric*)calloc(1, sizeof(*metric));
    metric->id  = id;
    strcpy(metric->name, name);
    strcpy(metric->ns, ns);
    strcpy(metric->desc, desc);
    metric->type = t;
    metric->buffer_index = 0;
    metric->buffer = (symbiomon_metric_buffer)calloc(METRIC_BUFFER_SIZE, sizeof(symbiomon_metric_sample));
    add_metric(provider, metric);

    char id_str[37];
    symbiomon_metric_id_to_string(id, id_str);
    margo_debug(provider->mid, "Created metric %s of type \"%s\"", id_str, metric->type);

    *m = metric;

    return SYMBIOMON_SUCCESS;
}

static void symbiomon_metric_fetch_ult(hg_handle_t h)
{
    hg_return_t hret;
    symbiomon_return_t ret;
    metric_fetch_in_t  in;
    metric_fetch_out_t out;

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

    /* set the response */
    out.ret = SYMBIOMON_SUCCESS;

finish:
    ret = margo_respond(h, &out);
    ret = margo_free_input(h, &in);
    margo_destroy(h);
}
static DEFINE_MARGO_RPC_HANDLER(symbiomon_metric_fetch_ult)

symbiomon_return_t symbiomon_provider_metric_destroy(symbiomon_metric_t m, symbiomon_provider_t provider)
{

    /* find the metric */
    symbiomon_metric* metric = find_metric(provider, &m->id);
    if(!metric) {
        return SYMBIOMON_ERR_INVALID_METRIC;
    }

    /* remove the metric from the provider */
    remove_metric(provider, &metric->id, 1);

    char id_str[37];
    symbiomon_metric_id_to_string(metric->id, id_str);
    
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_provider_destroy_all_metrics(symbiomon_provider_t provider)
{

    remove_all_metrics(provider);

    return SYMBIOMON_SUCCESS;
}

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

symbiomon_return_t symbiomon_provider_register_backend(
        symbiomon_provider_t provider,
        symbiomon_backend_impl* backend_impl)
{
    return SYMBIOMON_SUCCESS;
}


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
        free(r);
    }
    provider->num_metrics = 0;
}

static inline void prepare_string_for_uuid(char *input, char *output)
{
    int input_strlen = strlen(input);
    assert(input_strlen <= 36);
    strcpy(output, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\0");
    int i;
    for(i = 0; i < input_strlen; i++)
        output[i] = input[i];
}

static inline void uuid_xor(symbiomon_metric_id_t first, symbiomon_metric_id_t second, symbiomon_metric_id_t * result)
{
  char first_str[37], second_str[37], xored_str[37];
  symbiomon_metric_id_to_string(first, first_str); 
  symbiomon_metric_id_to_string(second, second_str); 

  int i = 0;
  for(i = 0; i < 36; i++)
      xored_str[i] = first_str[i] ^ second_str[i];
  xored_str[36] = '\0';

  symbiomon_metric_id_from_string(xored_str, result);
}
