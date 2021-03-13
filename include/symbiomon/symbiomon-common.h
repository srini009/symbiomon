/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#ifndef __SYMBIOMON_COMMON_H
#define __SYMBIOMON_COMMON_H

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
    SYMBIOMON_ERR_INVALID_VALUE,     /* Invalid metric update value */
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

#define METRIC_BUFFER_SIZE 160000

/**
 * @brief Identifier for a metric.
 */
typedef uint32_t symbiomon_metric_id_t;

typedef enum symbiomon_metric_type {
   SYMBIOMON_TYPE_COUNTER,
   SYMBIOMON_TYPE_TIMER,
   SYMBIOMON_TYPE_GAUGE    
} symbiomon_metric_type_t;

typedef struct symbiomon_metric_sample {
   double time;
   double val;
   uint64_t sample_id;
} symbiomon_metric_sample;

typedef symbiomon_metric_sample* symbiomon_metric_buffer;

typedef struct symbiomon_taglist {
    char **taglist;
    int num_tags;
} symbiomon_taglist;

typedef symbiomon_taglist* symbiomon_taglist_t;

inline uint32_t symbiomon_hash(char *str);

inline void symbiomon_id_from_string_identifiers(char *ns, char *name, char **taglist, int num_tags, symbiomon_metric_id_t *id_);

/* djb2 hash from Dan Bernstein */
inline uint32_t
symbiomon_hash(char *str)
{
    uint32_t hash = 5381;
    uint32_t c;

    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}


inline void symbiomon_id_from_string_identifiers(char *ns, char *name, char **taglist, int num_tags, symbiomon_metric_id_t *id_)
{
    symbiomon_metric_id_t id, temp_id;

    id = symbiomon_hash(ns);
    temp_id = symbiomon_hash(name);
    id = id^temp_id;

    /* XOR all the tag ids, so that any ordering of tags returns the same final metric id */
    int i;
    for(i = 0; i < num_tags; i++) {
	temp_id = symbiomon_hash(taglist[i]);
	id = id^temp_id;
    }
   
    *id_ = id;
}

#ifdef __cplusplus
}
#endif

#endif
