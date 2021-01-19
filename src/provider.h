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
} symbiomon_provider;

symbiomon_return_t symbiomon_provider_metric_create(char *ns, char *name, symbiomon_metric_type_t t, char *desc, symbiomon_taglist_t tl, symbiomon_metric_t* m, symbiomon_provider_t provider);

symbiomon_return_t symbiomon_provider_metric_destroy(symbiomon_metric_t m, symbiomon_provider_t provider);

symbiomon_return_t symbiomon_provider_destroy_all_metrics(symbiomon_provider_t provider);
#endif
