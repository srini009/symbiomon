/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __SYMBIOMON_COMMON_H
#define __SYMBIOMON_COMMON_H

#include <uuid/uuid.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Error codes that can be returned by SYMBIOMON functions.
 */
typedef enum symbiomon_return_t {
    SYMBIOMON_SUCCESS,
    SYMBIOMON_ERR_INVALID_NAME,      /* Metric creation error - name or ns missing */
    SYMBIOMON_ERR_ALLOCATION,        /* Allocation error */
    SYMBIOMON_ERR_INVALID_ARGS,      /* Invalid argument */
    SYMBIOMON_ERR_INVALID_PROVIDER,  /* Invalid provider id */
    SYMBIOMON_ERR_INVALID_METRIC,    /* Invalid metric id */
    SYMBIOMON_ERR_INVALID_BACKEND,   /* Invalid backend type */
    SYMBIOMON_ERR_INVALID_CONFIG,    /* Invalid configuration */
    SYMBIOMON_ERR_INVALID_TOKEN,     /* Invalid token */
    SYMBIOMON_ERR_FROM_MERCURY,      /* Mercury error */
    SYMBIOMON_ERR_FROM_ARGOBOTS,     /* Argobots error */
    SYMBIOMON_ERR_OP_UNSUPPORTED,    /* Unsupported operation */
    SYMBIOMON_ERR_OP_FORBIDDEN,      /* Forbidden operation */
    /* ... TODO add more error codes here if needed */
    SYMBIOMON_ERR_OTHER              /* Other error */
} symbiomon_return_t;

/**
 * @brief Identifier for a metric.
 */
typedef struct symbiomon_metric_id_t {
    unsigned long uuid;
} symbiomon_metric_id_t;

typedef enum symbiomon_metric_type {
   SYMBIOMON_TYPE_COUNTER,
   SYMBIOMON_TYPE_TIMER,
   SYMBIOMON_TYPE_GAUGE    
} symbiomon_metric_type_t;

#ifdef __cplusplus
}
#endif

#endif
