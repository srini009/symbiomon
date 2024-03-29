/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <assert.h>
#include <math.h>
#include<time.h>
#include "symbiomon/symbiomon-server.h"
#include "symbiomon/symbiomon-common.h"
#include "symbiomon/symbiomon-backend.h"
#include "provider.h"
#include "types.h"
#ifdef USE_AGGREGATOR
#include <sdskv-client.h>
#endif
#ifdef USE_REDUCER
#include <reducer/reducer-client.h>
#include <reducer/reducer-provider-handle.h>
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
    //p->abtio = a.abtio;

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
    p->use_reducer = 0;

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
        int i = 0;
        fscanf(fp_agg, "%d\n", &(p->num_aggregators));
        sdskv_client_init(mid, &p->aggcl);
        sdskv_provider_handle_t *aggphs = (sdskv_provider_handle_t *)malloc(sizeof(sdskv_provider_handle_t)*p->num_aggregators);
        sdskv_database_id_t *aggdbids = (sdskv_database_id_t *)malloc(sizeof(sdskv_database_id_t)*p->num_aggregators);
        while(fscanf(fp_agg, "%s %u %s\n", svr_addr_str, &p_id, db_name) != EOF) {
          hg_addr_t svr_addr; 
          int hret = margo_addr_lookup(mid, svr_addr_str, &svr_addr);
          assert(hret == HG_SUCCESS);
	  hret = sdskv_provider_handle_create(p->aggcl, svr_addr, p_id, &(aggphs[i]));
	  assert(hret == SDSKV_SUCCESS);
	  hret = sdskv_open(aggphs[i], db_name, &aggdbids[i]); 
	  assert(hret == SDSKV_SUCCESS);
          i++;
        }
        p->use_aggregator = 1;
        p->aggphs = aggphs;
        p->aggdbids = aggdbids;
	fprintf(stderr, "Aggregator successfully set.\n");
    } else {
        fprintf(stderr, "AGGREGATOR_ADDRESS_FILE is not set. Continuing on without aggregator support");
    }
#endif
#ifdef USE_REDUCER
    FILE *fp_red = NULL;
    #define MAXCHAR 100
    char * reducer_addr_file = getenv("REDUCER_ADDRESS_FILE");
    if(reducer_addr_file) {
        char svr_addr_str[MAXCHAR];
        uint16_t p_id;
        fp_red = fopen(reducer_addr_file, "r");
        reducer_client_init(mid, &p->redcl);
        fscanf(fp_red, "%s %u\n", svr_addr_str, &p_id);
        hg_addr_t svr_addr;
        int hret = margo_addr_lookup(mid, svr_addr_str, &svr_addr);
        assert(hret == HG_SUCCESS);
        hret = reducer_metric_handle_create(p->redcl, svr_addr, p_id, &p->redphl);
        assert(hret == REDUCER_SUCCESS);
        p->use_reducer = 1;
    } else {
        fprintf(stderr, "REDUCER_ADDRESS_FILE is not set. Continuing on without reducer support");
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

symbiomon_return_t symbiomon_provider_metric_create_with_reduction(const char *ns, const char *name, symbiomon_metric_type_t t, const char *desc, symbiomon_taglist_t tl, symbiomon_metric_t* m, symbiomon_provider_t provider, symbiomon_metric_reduction_op_t op)
{
    int i;
    symbiomon_return_t ret = symbiomon_provider_metric_create(ns, name, t, desc, tl, m, provider);
    if(ret != SYMBIOMON_SUCCESS) return ret;

#ifdef USE_AGGREGATOR
    strcat((*m)->stringify, ns);
    strcat((*m)->stringify, "_");
    strcat((*m)->stringify, name);
    (*m)->aggregator_id = symbiomon_hash((*m)->stringify);
    (*m)->reduction_op = op;

    for(i = 0; i < tl->num_tags; i++) {
        strcat((*m)->stringify, "_");
        strcat((*m)->stringify, tl->taglist[i]);
    }
#endif

    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_provider_metric_create(const char *ns, const char *name, symbiomon_metric_type_t t, const char *desc, symbiomon_taglist_t tl, symbiomon_metric_t* m, symbiomon_provider_t provider)
{
    if(!ns || !name)
        return SYMBIOMON_ERR_INVALID_NAME;

    /* create an id for the new metric */
    symbiomon_metric_id_t id;
    symbiomon_id_from_string_identifiers(ns, name, tl->taglist, tl->num_tags, &id);
    int i;

    symbiomon_metric* existing = find_metric(provider, &(id));
    if(existing) {
        *m = existing;
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
    metric->buffer = (symbiomon_metric_buffer)calloc(METRIC_BUFFER_SIZE, sizeof(symbiomon_metric_sample));
    add_metric(provider, metric);

    *m = metric;
    //fprintf(stderr, "Created metric with id: %lu and name: %s\n", metric->id, name);

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

symbiomon_return_t symbiomon_provider_metric_list_all(symbiomon_provider_t provider, const char *filename)
{
    FILE *fp = fopen(filename, "w");

    symbiomon_metric *r, *tmp;
    symbiomon_return_t ret;
#ifdef USE_AGGREGATOR
    HASH_ITER(hh, provider->metrics, r, tmp) {
        fprintf(fp, "%s %s %s\n", r->ns, r->name, r->stringify);
    }
#else
    HASH_ITER(hh, provider->metrics, r, tmp) {
        fprintf(fp, "%s %s\n", r->ns, r->name);
    }
#endif

    fclose(fp);
    return SYMBIOMON_SUCCESS;
}

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

symbiomon_return_t symbiomon_provider_metric_reduce(symbiomon_metric_t m, symbiomon_provider_t provider)
{

#ifdef USE_AGGREGATOR
    /* find the metric */
    symbiomon_metric* metric = find_metric(provider, &m->id);
    if(!metric) {
        return SYMBIOMON_ERR_INVALID_METRIC;
    }

    unsigned int current_index = m->buffer_index;
    if (current_index == 0) return SYMBIOMON_SUCCESS;

    uint32_t agg_id = (uint32_t)(m->aggregator_id)%(provider->num_aggregators);
    int ret;

    switch(metric->reduction_op) {
        case SYMBIOMON_REDUCTION_OP_NULL: {
            break;
        }
	case SYMBIOMON_REDUCTION_OP_SUM: {
	    int i=0;
	    double sum = 0;
	    for(i=0; i < current_index; i++) {
                sum += m->buffer[i].val; 
            }
	    char *key = (char *)malloc(256*sizeof(char));
	    strcpy(key, m->stringify);
	    strcat(key, "_SUM");
	    ret = sdskv_erase(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key));
	    assert(ret == SDSKV_SUCCESS);
	    ret = sdskv_put(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key), &sum, sizeof(double));
	    assert(ret == SDSKV_SUCCESS);
	    fprintf(stderr, "SYMBIOMON: Writing reduced SUM value: %lf with key: %s\n", sum, key);
            free(key);
	    break;
        }
	case SYMBIOMON_REDUCTION_OP_AVG: {
	    int i=0;
            double sum = 0, avg = 0;
	    for(i=0; i < current_index; i++) {
                sum += m->buffer[i].val; 
            }
	    avg = sum/(double)current_index;
	    char *key = (char *)malloc(256*sizeof(char));
	    strcpy(key, m->stringify);
	    strcat(key, "_AVG");
	    ret = sdskv_erase(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key));
	    assert(ret == SDSKV_SUCCESS);
	    ret = sdskv_put(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key), &avg, sizeof(double));
	    assert(ret == SDSKV_SUCCESS);
            free(key);
	    break;
        }
	case SYMBIOMON_REDUCTION_OP_MIN: {
	    int i=0; double min;
            if(m->type == SYMBIOMON_TYPE_GAUGE) {
              min = 9999999999.0;
  	      for(i=0; i < current_index; i++) {
                  min = (m->buffer[i].val < min ? m->buffer[i].val:min);
              }
            } else {
              if(current_index) {
                min = m->buffer[current_index-1].val;
              } else {
                min = 0.0;
              }
            }
	    char *key = (char *)malloc(256*sizeof(char));
	    strcpy(key, m->stringify);
	    strcat(key, "_MIN");
	    ret = sdskv_erase(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key));
	    assert(ret == SDSKV_SUCCESS);
	    ret = sdskv_put(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key), &min, sizeof(min));
	    assert(ret == SDSKV_SUCCESS);
            free(key);
	    break;
        }
	case SYMBIOMON_REDUCTION_OP_MAX: {
	    int i=0;
            double max;
            if(m->type == SYMBIOMON_TYPE_GAUGE) {
              max = -9999999999.0;
	      for(i=0; i < current_index; i++) {
                  max = (m->buffer[i].val > max ? m->buffer[i].val:max);
              }
            } else {
              if(current_index) {
                max = m->buffer[current_index-1].val;
              } else {
                max = 0.0;
              }
            }
	    char *key = (char *)malloc(256*sizeof(char));
	    strcpy(key, m->stringify);
	    strcat(key, "_MAX");
	    ret = sdskv_erase(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key));
	    assert(ret == SDSKV_SUCCESS);
	    ret = sdskv_put(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key), &max, sizeof(max));
	    assert(ret == SDSKV_SUCCESS);
	    fprintf(stderr, "SYMBIOMON: Writing reduced MAX value: %lf with key: %s\n", max, key);
            free(key);
	    break;
        }
	case SYMBIOMON_REDUCTION_OP_STORE: {
	    char *key = (char *)malloc(256*sizeof(char));
	    strcpy(key, m->stringify);
	    symbiomon_metric_buffer buf = (symbiomon_metric_buffer)malloc(current_index*sizeof(symbiomon_metric_sample));
	    memcpy(buf, m->buffer, current_index*sizeof(symbiomon_metric_sample));
	    ret = sdskv_put(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key), (const void *)buf, sizeof(current_index*sizeof(symbiomon_metric_sample)));
	    assert(ret == SDSKV_SUCCESS);
            free(key);
	    break;
        }

	case SYMBIOMON_REDUCTION_OP_ANOMALY: {
	    int i = 0; int num_outliers = 0;
            double sum = 0, avg = 0, sd = 0;
	    for(i=0; i < current_index; i++) {
                sum += m->buffer[i].val; 
            }

	    avg = sum/(double)current_index;
            for(i=0; i < current_index; i++)
                sd += pow(m->buffer[i].val - avg, 2);

            double * outlier_list = (double*)malloc(sizeof(double)*current_index);
	    for(i=0; i < current_index; i++) {
                if ((m->buffer[i].val < avg-3*sd) || (m->buffer[i].val > avg+3*sd)) {
                    outlier_list[num_outliers] = m->buffer[i].val;
                    num_outliers++;
                }
            }
                
	    char *key = (char *)malloc(256*sizeof(char));
	    strcpy(key, m->stringify);
	    strcat(key, "_ANOMALY");
	    ret = sdskv_erase(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key));
	    assert(ret == SDSKV_SUCCESS);

	    ret = sdskv_put(provider->aggphs[agg_id], provider->aggdbids[agg_id], (const void *)key, strlen(key), (const void *)outlier_list, sizeof(num_outliers*sizeof(double)));
	    assert(ret == SDSKV_SUCCESS);

            free(outlier_list);
            free(key);
	    break;
        }
    }
#endif
    return SYMBIOMON_SUCCESS;
}

/* Experimental support */
typedef struct {
    symbiomon_provider_t provider;
    symbiomon_metric_t metric;
} thread_arg_t;

/* Experimental support */
static void symbiomon_provider_reduce_metric_ult(void *arg) {
   symbiomon_provider_t provider = ((thread_arg_t *)arg)->provider; 
   symbiomon_metric *m = ((thread_arg_t *)arg)->metric;
   symbiomon_provider_metric_reduce(m, provider);
}

symbiomon_return_t symbiomon_provider_reduce_all_metrics(symbiomon_provider_t provider)
{
    if(provider->use_aggregator == 0) return SYMBIOMON_SUCCESS;

    symbiomon_metric *r, *tmp;
    symbiomon_return_t ret;

    int i = 0;
    HASH_ITER(hh, provider->metrics, r, tmp) {
	ret = symbiomon_provider_metric_reduce(r, provider);
        if(ret != SYMBIOMON_SUCCESS) { return ret;}
    }

    return SYMBIOMON_SUCCESS;
}

/* Purely experimental support! Not very performant at the moment */
symbiomon_return_t symbiomon_provider_reduce_all_metrics_batched(symbiomon_provider_t provider)
{
    if(provider->use_aggregator == 0) return SYMBIOMON_SUCCESS;

    #ifdef USE_AGGREGATOR
    symbiomon_metric *m, *tmp;
    srand(time(NULL));
    uint32_t agg_id = (uint32_t)(rand())%(provider->num_aggregators);

    char **keys = (char**)malloc(sizeof(char*)*provider->num_metrics);
    double **vals = (double**)malloc(provider->num_metrics*sizeof(double*));
    size_t *key_sizes = (hg_size_t*)calloc(provider->num_metrics, sizeof(hg_size_t));
    size_t *val_sizes = (hg_size_t*)calloc(provider->num_metrics, sizeof(hg_size_t));
    int ret;

    int metric_index = 0;
    HASH_ITER(hh, provider->metrics, m, tmp) {
        /* find the metric */
        symbiomon_metric* metric = find_metric(provider, &m->id);
        if(!metric) {
            return SYMBIOMON_ERR_INVALID_METRIC;
        }

        unsigned int current_index = m->buffer_index;
        if (current_index == 0) return SYMBIOMON_SUCCESS;

        switch(metric->reduction_op) {
   	    case SYMBIOMON_REDUCTION_OP_MAX: {
	        int i=0;
                double max = -9999999999.0;
    	        for(i=0; i < current_index; i++) {
                    max = (m->buffer[i].val > max ? m->buffer[i].val:max);
                }
    	        keys[metric_index] = (char *)malloc(256*sizeof(char));
    	        vals[metric_index] = (double *)calloc(1, sizeof(double));
	        strcpy(keys[metric_index], m->stringify);
	        strcat(keys[metric_index], "_MAX");
                key_sizes[metric_index] = strlen(keys[metric_index]);
                val_sizes[metric_index] = sizeof(double);
                vals[metric_index][0] = max;
	        break;
            }
            default:
	        break;
        }
        metric_index += 1;
    }

    ret = sdskv_erase_multi(provider->aggphs[agg_id], provider->aggdbids[agg_id], provider->num_metrics, (const void* const*)keys, (const hg_size_t *)key_sizes);
    assert(ret == SDSKV_SUCCESS);
    ret = sdskv_put_multi(provider->aggphs[agg_id], provider->aggdbids[agg_id], provider->num_metrics, (const void * const*)keys, (const hg_size_t *)key_sizes, (const void * const*)vals, (const hg_size_t *)val_sizes);
    assert(ret == SDSKV_SUCCESS);
    free(keys);
    free(vals);
    #endif
    return SYMBIOMON_SUCCESS;
}

static symbiomon_return_t symbiomon_provider_global_metric_reduce(symbiomon_metric_t m, symbiomon_provider_t provider, size_t cohort_size)
{
#ifdef USE_REDUCER
    /* find the metric */
    symbiomon_metric* metric = find_metric(provider, &m->id);
    if(!metric) {
        return SYMBIOMON_ERR_INVALID_METRIC;
    }

    uint32_t agg_id = (uint32_t)(m->aggregator_id)%(provider->num_aggregators);
    int ret;

    switch(metric->reduction_op) {
        case SYMBIOMON_REDUCTION_OP_NULL: {
            break;
        }
	case SYMBIOMON_REDUCTION_OP_SUM: {
            reducer_metric_reduce(m->ns, m->name, m->stringify, agg_id, REDUCER_REDUCTION_OP_SUM, provider->redphl, cohort_size);
	    break;
        }
	case SYMBIOMON_REDUCTION_OP_AVG: {
            reducer_metric_reduce(m->ns, m->name, m->stringify, agg_id, REDUCER_REDUCTION_OP_AVG, provider->redphl, cohort_size);
	    break;
        }
	case SYMBIOMON_REDUCTION_OP_MIN: {
            reducer_metric_reduce(m->ns, m->name, m->stringify, agg_id, REDUCER_REDUCTION_OP_MIN, provider->redphl, cohort_size);
	    break;
        }
	case SYMBIOMON_REDUCTION_OP_MAX: {
            reducer_metric_reduce(m->ns, m->name, m->stringify, agg_id, REDUCER_REDUCTION_OP_MAX, provider->redphl, cohort_size);
	    break;
        }
	case SYMBIOMON_REDUCTION_OP_ANOMALY: {
            reducer_metric_reduce(m->ns, m->name, m->stringify, agg_id, REDUCER_REDUCTION_OP_ANOMALY, provider->redphl, cohort_size);
	    break;
        }
    }
#endif
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_provider_global_reduce_all_metrics(symbiomon_provider_t provider, size_t cohort_size)
{
    if(provider->use_reducer == 0) return SYMBIOMON_SUCCESS;
    symbiomon_metric *r, *tmp;
    symbiomon_return_t ret;
    HASH_ITER(hh, provider->metrics, r, tmp) {
	ret = symbiomon_provider_global_metric_reduce(r, provider, cohort_size);
        if(ret != SYMBIOMON_SUCCESS) { return ret; }
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
