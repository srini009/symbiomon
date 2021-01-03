/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __SYMBIOMON_CLIENT_H
#define __SYMBIOMON_CLIENT_H

#include <margo.h>
#include <symbiomon/symbiomon-common.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct symbiomon_client* symbiomon_client_t;
#define SYMBIOMON_CLIENT_NULL ((symbiomon_client_t)NULL)

/**
 * @brief Creates a SYMBIOMON client.
 *
 * @param[in] mid Margo instance
 * @param[out] client SYMBIOMON client
 *
 * @return SYMBIOMON_SUCCESS or error code defined in symbiomon-common.h
 */
symbiomon_return_t symbiomon_client_init(margo_instance_id mid, symbiomon_client_t* client);

/**
 * @brief Finalizes a SYMBIOMON client.
 *
 * @param[in] client SYMBIOMON client to finalize
 *
 * @return SYMBIOMON_SUCCESS or error code defined in symbiomon-common.h
 */
symbiomon_return_t symbiomon_client_finalize(symbiomon_client_t client);

#ifdef __cplusplus
}
#endif

#endif
