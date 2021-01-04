/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __PROVIDER_H
#define __PROVIDER_H

#include <margo.h>
#include <abt-io.h>
#include <uuid/uuid.h>
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
    symbiomon_metric*      metrics;         // hash of metrics by uuid
    /* RPC identifiers for clients */
    hg_id_t list_metrics_id;
    hg_id_t metric_fetch_id;
    /* ... add other RPC identifiers here ... */
} symbiomon_provider;

#endif
