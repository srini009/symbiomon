/*
 * (C) 2020 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#include <stdarg.h>
#include "types.h"
#include "client.h"
#include "symbiomon/symbiomon-client.h"
#include "symbiomon/symbiomon-common.h"

symbiomon_return_t symbiomon_client_init(margo_instance_id mid, symbiomon_client_t* client)
{
    symbiomon_client_t c = (symbiomon_client_t)calloc(1, sizeof(*c));
    if(!c) return SYMBIOMON_ERR_ALLOCATION;

    c->mid = mid;

    hg_bool_t flag;
    hg_id_t id;
    margo_registered_name(mid, "symbiomon_remote_metric_fetch", &id, &flag);

    if(flag == HG_TRUE) {
        margo_registered_name(mid, "symbiomon_remote_metric_fetch", &c->metric_fetch_id, &flag);
        margo_registered_name(mid, "symbiomon_remote_list_metrics", &c->list_metrics_id, &flag);
    } else {
        c->metric_fetch_id = MARGO_REGISTER(mid, "symbiomon_remote_metric_fetch", metric_fetch_in_t, metric_fetch_out_t, NULL);
        c->list_metrics_id = MARGO_REGISTER(mid, "symbiomon_remote_list_metrics", list_metrics_in_t, list_metrics_out_t, NULL);
    }

    c->num_metric_handles = 0;
    *client = c;
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_client_finalize(symbiomon_client_t client)
{
    if(client->num_metric_handles != 0) {
        fprintf(stderr,  
                "Warning: %ld metric handles not released when symbiomon_client_finalize was called\n",
                client->num_metric_handles);
    }
    free(client);
    return SYMBIOMON_SUCCESS;
}

/* APIs for microservice clients */
symbiomon_return_t symbiomon_taglist_create(symbiomon_taglist_t *taglist, int num_tags, ...) 
{
    *taglist = (symbiomon_taglist_t)malloc(sizeof(symbiomon_taglist));
    va_list valist;
    va_start(valist, num_tags);

    (*taglist)->taglist = (char **)malloc(num_tags*sizeof(char*));
    (*taglist)->num_tags = num_tags;
    int i = 0;

    for(i = 0; i < num_tags; i++) {
        (*taglist)->taglist[i] = (char*)malloc(36*sizeof(char*));
	strcpy((*taglist)->taglist[i], va_arg(valist, char*));
    }

    va_end(valist);
    return SYMBIOMON_SUCCESS;

}

symbiomon_return_t symbiomon_taglist_destroy(symbiomon_taglist_t taglist)
{
    int i;
    for(i = 0; i < taglist->num_tags; i++) {
        free(taglist->taglist[i]);
    }

    free(taglist->taglist);
    free(taglist);

}

symbiomon_return_t symbiomon_metric_create(char *ns, char *name, symbiomon_metric_type_t t, char *desc, symbiomon_taglist_t taglist, symbiomon_metric_t* m, symbiomon_provider_t p)
{
    return symbiomon_provider_metric_create(ns, name, t, desc, taglist, m, p);
}

symbiomon_return_t symbiomon_metric_destroy(symbiomon_metric_t m, symbiomon_provider_t p)
{
    return symbiomon_provider_metric_destroy(m, p);
}

symbiomon_return_t symbiomon_metric_destroy_all(symbiomon_provider_t p)
{
    return symbiomon_provider_destroy_all_metrics(p);
}

symbiomon_return_t symbiomon_metric_update(symbiomon_metric_t m, double val)
{
    m->buffer[m->buffer_index].val = val;
    m->buffer[m->buffer_index].time = ABT_get_wtime();
    m->buffer_index++;

    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_metric_register_retrieval_callback(char *ns, func f)
{
    return SYMBIOMON_SUCCESS;
}

/* APIs for remote monitoring clients */
symbiomon_return_t symbiomon_remote_metric_get_id(char *ns, char *name, symbiomon_taglist_t taglist, symbiomon_metric_id_t* metric_id)
{
    if(!ns || !name)
        return SYMBIOMON_ERR_INVALID_NAME;

    symbiomon_id_from_string_identifiers(ns, name, taglist->taglist, taglist->num_tags, metric_id);

    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_remote_metric_fetch(symbiomon_metric_handle_t handle, int64_t *num_samples_requested, symbiomon_metric_buffer *buf)
{
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_remote_metric_handle_create(
        symbiomon_client_t client,
        hg_addr_t addr,
        uint16_t provider_id,
        symbiomon_metric_id_t metric_id,
        symbiomon_metric_handle_t* handle)
{
    if(client == SYMBIOMON_CLIENT_NULL)
        return SYMBIOMON_ERR_INVALID_ARGS;

    symbiomon_metric_handle_t rh =
        (symbiomon_metric_handle_t)calloc(1, sizeof(*rh));

    if(!rh) return SYMBIOMON_ERR_ALLOCATION;

    hg_return_t ret = margo_addr_dup(client->mid, addr, &(rh->addr));
    if(ret != HG_SUCCESS) {
        free(rh);
        return SYMBIOMON_ERR_FROM_MERCURY;
    }

    rh->client      = client;
    rh->provider_id = provider_id;
    rh->metric_id   = metric_id;
    rh->refcount    = 1;

    client->num_metric_handles += 1;

    *handle = rh;
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_remote_metric_handle_ref_incr(
        symbiomon_metric_handle_t handle)
{
    if(handle == SYMBIOMON_METRIC_HANDLE_NULL)
        return SYMBIOMON_ERR_INVALID_ARGS;
    handle->refcount += 1;
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_remote_metric_handle_release(symbiomon_metric_handle_t handle)
{
    if(handle == SYMBIOMON_METRIC_HANDLE_NULL)
        return SYMBIOMON_ERR_INVALID_ARGS;
    handle->refcount -= 1;
    if(handle->refcount == 0) {
        margo_addr_free(handle->client->mid, handle->addr);
        handle->client->num_metric_handles -= 1;
        free(handle);
    }
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_remote_list_metrics(symbiomon_client_t client, hg_addr_t addr, uint16_t provider_id, symbiomon_metric_id_t* ids, size_t* count)
{
    hg_handle_t h;
    list_metrics_in_t  in;
    list_metrics_out_t out;
    symbiomon_return_t ret;
    hg_return_t hret;

    in.max_ids = *count;

    hret = margo_create(client->mid, addr, client->list_metrics_id, &h);
    if(hret != HG_SUCCESS)
        return SYMBIOMON_ERR_FROM_MERCURY;

    hret = margo_provider_forward(provider_id, h, &in);
    if(hret != HG_SUCCESS) {
        margo_destroy(h);
        return SYMBIOMON_ERR_FROM_MERCURY;
    }

    hret = margo_get_output(h, &out);
    if(hret != HG_SUCCESS) {
        margo_destroy(h);
        return SYMBIOMON_ERR_FROM_MERCURY;
    }

    ret = out.ret;
    if(ret == SYMBIOMON_SUCCESS) {
        *count = out.count;
	ids = (symbiomon_metric_id_t*)malloc(out.count*sizeof(symbiomon_metric_id_t));
        memcpy(ids, out.ids, out.count*sizeof(*ids));
    }
    
    margo_free_output(h, &out);
    margo_destroy(h);
    return ret;
}
