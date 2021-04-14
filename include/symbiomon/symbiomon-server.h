/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __SYMBIOMON_SERVER_H
#define __SYMBIOMON_SERVER_H

#include <symbiomon/symbiomon-common.h>
#include <margo.h>
#include <abt-io.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SYMBIOMON_ABT_POOL_DEFAULT ABT_POOL_NULL

typedef struct symbiomon_provider* symbiomon_provider_t;
#define SYMBIOMON_PROVIDER_NULL ((symbiomon_provider_t)NULL)
#define SYMBIOMON_PROVIDER_IGNORE ((symbiomon_provider_t*)NULL)

struct symbiomon_provider_args {
    uint8_t            push_finalize_callback;
    const char*        token;  // Security token
    const char*        config; // JSON configuration
    ABT_pool           pool;   // Pool used to run RPCs
    abt_io_instance_id abtio;  // ABT-IO instance
    // ...
};

#define SYMBIOMON_PROVIDER_ARGS_INIT { \
    .push_finalize_callback = 1,\
    .token = NULL, \
    .config = NULL, \
    .pool = ABT_POOL_NULL, \
    .abtio = ABT_IO_INSTANCE_NULL \
}

/**
 * @brief Creates a new SYMBIOMON provider. If SYMBIOMON_PROVIDER_IGNORE
 * is passed as last argument, the provider will be automatically
 * destroyed when calling margo_finalize.
 *
 * @param[in] mid Margo instance
 * @param[in] provider_id provider id
 * @param[in] args argument structure
 * @param[out] provider provider
 *
 * @return SYMBIOMON_SUCCESS or error code defined in symbiomon-common.h
 */
int symbiomon_provider_register(
        margo_instance_id mid,
        uint16_t provider_id,
        const struct symbiomon_provider_args* args,
        symbiomon_provider_t* provider);

/**
 * @brief Destroys the Alpha provider and deregisters its RPC.
 *
 * @param[in] provider Alpha provider
 *
 * @return SYMBIOMON_SUCCESS or error code defined in symbiomon-common.h
 */
int symbiomon_provider_destroy(
        symbiomon_provider_t provider);

#ifdef __cplusplus
}
#endif

#endif
