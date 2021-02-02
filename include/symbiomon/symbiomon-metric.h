/*
 * (C) 2020 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#ifndef __SYMBIOMON_METRIC_H
#define __SYMBIOMON_METRIC_H

#include <margo.h>
#include <symbiomon/symbiomon-common.h>
#include <symbiomon/symbiomon-client.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct symbiomon_metric_handle *symbiomon_metric_handle_t;
typedef struct symbiomon_metric *symbiomon_metric_t;
typedef enum symbiomon_metric_type symbiomon_metric_type_t;
typedef struct symbiomon_provider* symbiomon_provider_t;
typedef struct symbiomon_taglist* symbiomon_taglist_t;
typedef struct symbiomon_metric_sample* symbiomon_metric_buffer;
typedef struct symbiomon_metric_sample symbiomon_metric_sample;
typedef void (*func)();
#define SYMBIOMON_METRIC_HANDLE_NULL ((symbiomon_metric_handle_t)NULL)

/* APIs for providers to record performance data */
symbiomon_return_t symbiomon_taglist_create(symbiomon_taglist_t *taglist, int num_tags, ...);
symbiomon_return_t symbiomon_taglist_destroy(symbiomon_taglist_t taglist);
symbiomon_return_t symbiomon_metric_create(const char *ns, const char *name, symbiomon_metric_type_t t, const char *desc, symbiomon_taglist_t taglist, symbiomon_metric_t* metric_handle, symbiomon_provider_t provider);
symbiomon_return_t symbiomon_metric_destroy(symbiomon_metric_t m, symbiomon_provider_t provider);
symbiomon_return_t symbiomon_metric_destroy_all(symbiomon_provider_t provider);
symbiomon_return_t symbiomon_metric_update(symbiomon_metric_t m, double val);
symbiomon_return_t symbiomon_metric_dump_histogram(symbiomon_metric_t m, const char *filename, size_t num_buckets);
symbiomon_return_t symbiomon_metric_class_register_retrieval_callback(char *ns, func f);

/* APIs for remote clients to request for performance data */
symbiomon_return_t symbiomon_remote_metric_get_id(char *ns, char *name, symbiomon_taglist_t taglist, symbiomon_metric_id_t* metric_id);
symbiomon_return_t symbiomon_remote_metric_handle_create(symbiomon_client_t client, hg_addr_t addr, uint16_t provider_id, symbiomon_metric_id_t metric_id, symbiomon_metric_handle_t* handle);
symbiomon_return_t symbiomon_remote_metric_handle_ref_incr(symbiomon_metric_handle_t handle);
symbiomon_return_t symbiomon_remote_metric_handle_release(symbiomon_metric_handle_t handle);
symbiomon_return_t symbiomon_remote_metric_fetch(symbiomon_metric_handle_t handle, int64_t *num_samples_requested, symbiomon_metric_buffer *buf, char **name, char **ns);
symbiomon_return_t symbiomon_remote_list_metrics(symbiomon_client_t client, hg_addr_t addr, uint16_t provider_id, symbiomon_metric_id_t** ids, size_t* count);

#ifdef __cplusplus
}
#endif

#endif
