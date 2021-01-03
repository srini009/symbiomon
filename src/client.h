/*
 * (C) 2020 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef _CLIENT_H
#define _CLIENT_H

#include "types.h"
#include "symbiomon/symbiomon-client.h"
#include "symbiomon/symbiomon-metric.h"

typedef struct symbiomon_client {
   margo_instance_id mid;
   hg_id_t           metric_fetch_id;
   uint64_t          num_metric_handles;
} symbiomon_client;

typedef struct symbiomon_metric_handle {
    symbiomon_client_t      client;
    hg_addr_t           addr;
    uint16_t            provider_id;
    uint64_t            refcount;
    symbiomon_metric_id_t metric_id;
} symbiomon_metric_handle;

#endif
