/*
 * (C) 2020 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef _PARAMS_H
#define _PARAMS_H

#include <mercury.h>
#include <abt.h>
#include <mercury_macros.h>
#include <mercury_proc.h>
#include <mercury_proc_string.h>
#include "symbiomon/symbiomon-common.h"
#include "uthash.h"

static inline hg_return_t hg_proc_symbiomon_metric_id_t(hg_proc_t proc, symbiomon_metric_id_t *id);

/* Admin RPC types */

/* Client RPC types */

MERCURY_GEN_PROC(list_metrics_in_t,
        ((hg_size_t)(max_ids)))

typedef struct list_metrics_out_t {
    int32_t ret;
    hg_size_t count;
    symbiomon_metric_id_t* ids;
} list_metrics_out_t;

static inline hg_return_t hg_proc_list_metrics_out_t(hg_proc_t proc, void *data)
{
    list_metrics_out_t* out = (list_metrics_out_t*)data;
    hg_return_t ret;

    ret = hg_proc_hg_int32_t(proc, &(out->ret));
    if(ret != HG_SUCCESS) return ret;

    ret = hg_proc_hg_size_t(proc, &(out->count));
    if(ret != HG_SUCCESS) return ret;

    switch(hg_proc_get_op(proc)) {
    case HG_DECODE:
        out->ids = (symbiomon_metric_id_t*)calloc(out->count, sizeof(*(out->ids)));
        /* fall through */
    case HG_ENCODE:
        if(out->ids)
            ret = hg_proc_memcpy(proc, out->ids, sizeof(*(out->ids))*out->count);
        break;
    case HG_FREE:
        free(out->ids);
        break;
    }
    return ret;
}

MERCURY_GEN_PROC(metric_fetch_in_t,
        ((symbiomon_metric_id_t)(metric_id))\
	((int64_t)(count))\
	((hg_bulk_t)(bulk)))

MERCURY_GEN_PROC(metric_fetch_out_t,
	((int64_t)(actual_count))\
	((hg_string_t)(name))\
	((hg_string_t)(ns))\
        ((int32_t)(ret)))

/* Extra hand-coded serialization functions */

static inline hg_return_t hg_proc_symbiomon_metric_id_t(
        hg_proc_t proc, symbiomon_metric_id_t *id)
{
    return hg_proc_memcpy(proc, id, sizeof(*id));
}

typedef struct symbiomon_metric {
    symbiomon_metric_type_t type;
    symbiomon_metric_agg_op_t agg_op;
    symbiomon_metric_buffer buffer;
    unsigned int buffer_index;
    char desc[200];
    char name[128];
    char ns[128];
    symbiomon_taglist_t taglist;
    symbiomon_metric_id_t id;
    UT_hash_handle      hh;
    ABT_mutex metric_mutex; /* Needed because metric can be updated simulateneously by many ULTs */
} symbiomon_metric;

typedef symbiomon_metric* symbiomon_metric_t;

#endif
