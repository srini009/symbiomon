/*
 * (C) 2020 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef __SYMBIOMON_BACKEND_H
#define __SYMBIOMON_BACKEND_H

#include <symbiomon/symbiomon-server.h>
#include <symbiomon/symbiomon-common.h>

typedef symbiomon_return_t (*symbiomon_backend_create_fn)(symbiomon_provider_t, void**);
typedef symbiomon_return_t (*symbiomon_backend_open_fn)(symbiomon_provider_t, void**);
typedef symbiomon_return_t (*symbiomon_backend_close_fn)(void*);
typedef symbiomon_return_t (*symbiomon_backend_destroy_fn)(void*);

/**
 * @brief Implementation of an SYMBIOMON backend.
 */
typedef struct symbiomon_backend_impl {
    // backend name
    const char* name;
    // backend management functions
    symbiomon_backend_create_fn   create_metric;
    symbiomon_backend_open_fn     open_metric;
    symbiomon_backend_close_fn    close_metric;
    symbiomon_backend_destroy_fn  destroy_metric;
    // RPC functions
    void (*hello)(void*);
    int32_t (*sum)(void*, int32_t, int32_t);
    // ... add other functions here
} symbiomon_backend_impl;

/**
 * @brief Registers a backend implementation to be used by the
 * specified provider.
 *
 * Note: the backend implementation will not be copied; it is
 * therefore important that it stays valid in memory until the
 * provider is destroyed.
 *
 * @param provider provider.
 * @param backend_impl backend implementation.
 *
 * @return SYMBIOMON_SUCCESS or error code defined in symbiomon-common.h 
 */
symbiomon_return_t symbiomon_provider_register_backend();

#endif
