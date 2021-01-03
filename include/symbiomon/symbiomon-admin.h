/*
 * (C) 2020 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef __SYMBIOMON_ADMIN_H
#define __SYMBIOMON_ADMIN_H

#include <margo.h>
#include <symbiomon/symbiomon-common.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct symbiomon_admin* symbiomon_admin_t;
#define SYMBIOMON_ADMIN_NULL ((symbiomon_admin_t)NULL)

#define SYMBIOMON_METRIC_ID_IGNORE ((symbiomon_metric_id_t*)NULL)

/**
 * @brief Creates a SYMBIOMON admin.
 *
 * @param[in] mid Margo instance
 * @param[out] admin SYMBIOMON admin
 *
 * @return SYMBIOMON_SUCCESS or error code defined in symbiomon-common.h
 */
symbiomon_return_t symbiomon_admin_init(margo_instance_id mid, symbiomon_admin_t* admin);

/**
 * @brief Finalizes a SYMBIOMON admin.
 *
 * @param[in] admin SYMBIOMON admin to finalize
 *
 * @return SYMBIOMON_SUCCESS or error code defined in symbiomon-common.h
 */
symbiomon_return_t symbiomon_admin_finalize(symbiomon_admin_t admin);

#endif
