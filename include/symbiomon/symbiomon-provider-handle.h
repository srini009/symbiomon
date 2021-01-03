/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __SYMBIOMON_PROVIDER_HANDLE_H
#define __SYMBIOMON_PROVIDER_HANDLE_H

#include <margo.h>
#include <symbiomon/symbiomon-common.h>

#ifdef __cplusplus
extern "C" {
#endif

struct symbiomon_provider_handle {
    margo_instance_id mid;
    hg_addr_t         addr;
    uint16_t          provider_id;
};

typedef struct symbiomon_provider_handle* symbiomon_provider_handle_t;
#define SYMBIOMON_PROVIDER_HANDLE_NULL ((symbiomon_provider_handle_t)NULL)

#ifdef __cplusplus
}
#endif

#endif
