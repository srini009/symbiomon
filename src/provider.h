/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __PROVIDER_H
#define __PROVIDER_H

#include <margo.h>
#include <abt-io.h>
#include "uthash.h"
#include "types.h"
#ifdef USE_AGGREGATOR
#include <sdskv-client.h>
#endif
#ifdef USE_REDUCER
#include <reducer/reducer-client.h>
#endif

typedef struct symbiomon_provider {
    /* Margo/Argobots/Mercury environment */
    margo_instance_id  mid;                 // Margo instance
    uint16_t           provider_id;         // Provider id
    ABT_pool           pool;                // Pool on which to post RPC requests
    abt_io_instance_id abtio;               // ABT-IO instance
    /* Resources and backend types */
    size_t               num_metrics;     // number of metrics
    symbiomon_metric*      metrics;         // hash of metrics by id
    /* RPC identifiers for clients */
    hg_id_t list_metrics_id;
    hg_id_t metric_fetch_id;
    /* ... add other RPC identifiers here ... */
    uint8_t use_aggregator;
    uint8_t use_reducer;
    uint32_t num_aggregators;
#ifdef USE_AGGREGATOR
    sdskv_client_t aggcl;
    sdskv_provider_handle_t * aggphs;
    sdskv_database_id_t * aggdbids;
#endif
#ifdef USE_REDUCER
    reducer_client_t redcl;
    reducer_provider_handle_t redphl;
#endif
} symbiomon_provider;

symbiomon_return_t symbiomon_provider_metric_create_with_reduction(const char *ns, const char *name, symbiomon_metric_type_t t, const char *desc, symbiomon_taglist_t tl, symbiomon_metric_t* m, symbiomon_provider_t provider, symbiomon_metric_reduction_op_t op);

symbiomon_return_t symbiomon_provider_metric_create(const char *ns, const char *name, symbiomon_metric_type_t t, const char *desc, symbiomon_taglist_t tl, symbiomon_metric_t* m, symbiomon_provider_t provider);

symbiomon_return_t symbiomon_provider_metric_destroy(symbiomon_metric_t m, symbiomon_provider_t provider);

symbiomon_return_t symbiomon_provider_destroy_all_metrics(symbiomon_provider_t provider);

symbiomon_return_t symbiomon_provider_metric_reduce(symbiomon_metric_t m, symbiomon_provider_t provider);

symbiomon_return_t symbiomon_provider_reduce_all_metrics(symbiomon_provider_t provider);

symbiomon_return_t symbiomon_provider_global_reduce_all_metrics(symbiomon_provider_t provider);

#endif
