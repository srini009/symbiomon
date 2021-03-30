/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <assert.h>
#include "symbiomon/symbiomon-server.h"
#include "symbiomon/symbiomon-common.h"
#include "symbiomon/symbiomon-backend.h"
#include "provider.h"
#include "types.h"
#ifdef USE_AGGREGATOR
#include <sdskv-client.h>
#endif

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
        const symbiomon_metric_id_t* id);

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
    p->use_aggregator = 0;

    /* add other RPC registration here */
    /* ... */

    /* add backends available at compiler time (e.g. default/dummy backends) */

#ifdef USE_AGGREGATOR
    #define MAXCHAR 100
    FILE *fp_agg = NULL;
    char * aggregator_addr_file = getenv("AGGREGATOR_ADDRESS_FILE");
    char db_name[128];
    if(aggregator_addr_file) {
        char svr_addr_str[MAXCHAR];
        uint16_t p_id;
        fp_agg = fopen(aggregator_addr_file, "r");
        int32_t num_aggregators;
        sdskv_database_id_t db_id;
        int i = 0;
        fscanf(fp_agg, "%d\n", &num_aggregators);
        sdskv_client_init(mid, &p->aggcl);
        sdskv_provider_handle_t *aggphs = (sdskv_provider_handle_t *)malloc(sizeof(sdskv_provider_handle_t)*num_aggregators);
        sdskv_database_id_t *aggdbids = (sdskv_database_id_t *)malloc(sizeof(sdskv_database_id_t)*num_aggregators);
        while(fscanf(fp_agg, "%s %u %s\n", svr_addr_str, &p_id, db_name) != EOF) {
          hg_addr_t svr_addr; 
          int hret = margo_addr_lookup(mid, svr_addr_str, &svr_addr);
          assert(hret == HG_SUCCESS);
	  sdskv_provider_handle_create(p->aggcl, svr_addr, p_id, &(aggphs[i]));
	  sdskv_open(aggphs[i], db_name, &aggdbids[db_id]); 
          i++;
        }
	p->num_aggregators = num_aggregators;
        p->use_aggregator = 1;
        p->aggphs = aggphs;
        p->aggdbids = aggdbids;
	fprintf(stderr, "Successfully setup aggregator support.\n");
    } else {
        fprintf(stderr, "AGGREGATOR_ADDRESS_FILE is not set. Continuing on without aggregator support");
    }
#endif

    if(a.push_finalize_callback)
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

symbiomon_return_t symbiomon_provider_metric_create(const char *ns, const char *name, symbiomon_metric_type_t t, const char *desc, symbiomon_taglist_t tl, symbiomon_metric_t* m, symbiomon_provider_t provider, symbiomon_metric_agg_op_t agg)
{
    if(!ns || !name)
        return SYMBIOMON_ERR_INVALID_NAME;

    /* create an id for the new metric */
    symbiomon_metric_id_t id;
    symbiomon_id_from_string_identifiers(ns, name, tl->taglist, tl->num_tags, &id);

    symbiomon_metric* existing = find_metric(provider, &(id));
    if(existing) {
        return SYMBIOMON_ERR_METRIC_EXISTS;
    }

    /* allocate a metric, set it up, and add it to the provider */
    symbiomon_metric* metric = (symbiomon_metric*)calloc(1, sizeof(*metric));
    ABT_mutex_create(&metric->metric_mutex);
    metric->id  = id;
    strcpy(metric->name, name);
    strcpy(metric->ns, ns);
    strcpy(metric->desc, desc);
    metric->type = t;
    metric->taglist = tl;
    metric->buffer_index = 0;
    metric->agg_op = agg;
    metric->buffer = (symbiomon_metric_buffer)calloc(METRIC_BUFFER_SIZE, sizeof(symbiomon_metric_sample));
    add_metric(provider, metric);

    fprintf(stderr, "Created metric with name and ns: %s and %s\n", name, ns);
    *m = metric;

    return SYMBIOMON_SUCCESS;
}

static void symbiomon_metric_fetch_ult(hg_handle_t h)
{
    hg_return_t hret;
    metric_fetch_in_t  in;
    metric_fetch_out_t out;
    hg_bulk_t local_bulk;

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

    /* create a bulk region */
    symbiomon_metric_buffer b = calloc(in.count, sizeof(symbiomon_metric_sample));
    hg_size_t buf_size = in.count * sizeof(symbiomon_metric_sample);
    hret = margo_bulk_create(mid, 1, (void**)&b, &buf_size, HG_BULK_READ_ONLY, &local_bulk);

    if(hret != HG_SUCCESS) {
        margo_info(provider->mid, "Could not create bulk_handle (mercury error %d)", hret);
        out.ret = SYMBIOMON_ERR_FROM_MERCURY;
        goto finish;
    }

    symbiomon_metric_id_t requested_id = in.metric_id;
    symbiomon_metric* metric = find_metric(provider, &(requested_id));
    if(!metric) {
        out.ret = SYMBIOMON_ERR_INVALID_METRIC;
	goto finish;
    }


    out.name = (char*)malloc(36*sizeof(char));
    out.ns = (char*)malloc(36*sizeof(char));
    strcpy(out.name, metric->name);
    strcpy(out.ns, metric->ns);

    /* copyout metric buffer of requested size */
    if(metric->buffer_index < in.count) {
        out.actual_count = metric->buffer_index;
        memcpy(b, metric->buffer, out.actual_count*sizeof(symbiomon_metric_sample));
    } else {
	out.actual_count = in.count;
        memcpy(b, metric->buffer + (metric->buffer_index - out.actual_count), out.actual_count*sizeof(symbiomon_metric_sample));
    }

    /* do the bulk transfer */
    hret = margo_bulk_transfer(mid, HG_BULK_PUSH, info->addr, in.bulk, 0, local_bulk, 0, buf_size);
    if(hret != HG_SUCCESS) {
        margo_info(provider->mid, "Could not create bulk_handle (mercury error %d)", hret);
        out.ret = SYMBIOMON_ERR_FROM_MERCURY;
        goto finish;
    }

    /* set the response */
    out.ret = SYMBIOMON_SUCCESS;

finish:
    free(b);
    hret = margo_respond(h, &out);
    hret = margo_free_input(h, &in);
    margo_destroy(h);
    margo_bulk_free(local_bulk);
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
    remove_metric(provider, &metric->id);

    ABT_mutex_free(&metric->metric_mutex);
    free(metric->buffer);

    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_provider_destroy_all_metrics(symbiomon_provider_t provider)
{

    remove_all_metrics(provider);

    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_provider_metric_aggregate(symbiomon_metric_t m, symbiomon_provider_t provider)
{

#ifdef USE_AGGREGATOR
    /* find the metric */
    symbiomon_metric* metric = find_metric(provider, &m->id);
    if(!metric) {
        return SYMBIOMON_ERR_INVALID_METRIC;
    }

    unsigned int current_index = m->buffer_index;
    if (current_index == 0) return SYMBIOMON_SUCCESS;

    uint32_t agg_id = (uint32_t)(m->id)%(provider->num_aggregators);
    int ret;

    double min=9999999999.0;
    double max=-9999999999.0;
    double avg=0.0;
    double sum=0.0;
    switch(metric->agg_op) {
        case SYMBIOMON_AGG_OP_NULL: {
            break;
        }
	case SYMBIOMON_AGG_OP_SUM: {
	    int i=0;
	    for(i=0; i < current_index; i++) {
                sum += m->buffer[current_index].val; 
            }
	    break;
        }
	case SYMBIOMON_AGG_OP_AVG: {
	    int i=0;
	    for(i=0; i < current_index; i++) {
                sum += m->buffer[current_index].val; 
            }
	    avg = sum/(double)current_index;
	    char *key = (char *)malloc(128*sizeof(char));
	    strcat(key, m->ns);
	    strcat(key, "_");
	    strcat(key, m->name);
	    strcat(key, "_");
	    strcat(key, "_AVG");
	    ret = sdskv_put(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key), &avg, sizeof(double));
	    if(ret = SDSKV_SUCCESS) { fprintf(stderr, "Key-Value pair successfully written\n"); }
	    break;
        }
	case SYMBIOMON_AGG_OP_MIN: {
	    int i=0;
	    for(i=0; i < current_index; i++) {
                min = (m->buffer[current_index].val < min ? m->buffer[current_index].val:min);
            }
	    break;
        }
	case SYMBIOMON_AGG_OP_MAX: {
	    int i=0;
	    for(i=0; i < current_index; i++) {
                max = (m->buffer[current_index].val > max ? m->buffer[current_index].val:max);
            }
	    break;
        }
	case SYMBIOMON_AGG_OP_STORE: {
	    symbiomon_metric_buffer buf = (symbiomon_metric_buffer)malloc(current_index*sizeof(symbiomon_metric_sample));
	    memcpy(buf, m->buffer, current_index*sizeof(symbiomon_metric_sample));
	    break;
        }

    }
#endif
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_provider_aggregate_all_metrics(symbiomon_provider_t provider)
{
    symbiomon_metric *r, *tmp;
    symbiomon_return_t ret;
    HASH_ITER(hh, provider->metrics, r, tmp) {
	ret = symbiomon_provider_metric_aggregate(r, provider);
        if(ret != SYMBIOMON_SUCCESS) { return ret;}
    }

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

symbiomon_return_t symbiomon_provider_register_backend()
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
        const symbiomon_metric_id_t* id)
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
