/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <margo.h>
#include <assert.h>
#include <symbiomon/symbiomon-client.h>
#include <symbiomon/symbiomon-metric.h>

#define FATAL(...) \
    do { \
        margo_critical(__VA_ARGS__); \
        exit(-1); \
    } while(0)

int main(int argc, char** argv)
{
    if(argc != 4) {
        fprintf(stderr,"Usage: %s <server address> <provider id> <metric id>\n", argv[0]);
        exit(-1);
    }

    symbiomon_return_t ret;
    hg_return_t hret;
    const char* svr_addr_str = argv[1];
    uint16_t    provider_id  = atoi(argv[2]);
    const char* id_str       = argv[3];
    if(strlen(id_str) != 3) {
        FATAL(MARGO_INSTANCE_NULL,"id should be 3 character long");
    }

    margo_instance_id mid = margo_init("na+sm://", MARGO_CLIENT_MODE, 0, 0);
    assert(mid);

    hg_addr_t svr_addr;
    hret = margo_addr_lookup(mid, svr_addr_str, &svr_addr);
    if(hret != HG_SUCCESS) {
        FATAL(mid,"margo_addr_lookup failed for address %s", svr_addr_str);
    }

    symbiomon_client_t symbiomon_clt;
    symbiomon_metric_handle_t symbiomon_rh;

    margo_info(mid, "Creating SYMBIOMON client");
    ret = symbiomon_client_init(mid, &symbiomon_clt);
    if(ret != SYMBIOMON_SUCCESS) {
        FATAL(mid,"symbiomon_client_init failed (ret = %d)", ret);
    }

    margo_info(mid, "Releasing metric handle");
    ret = symbiomon_remote_metric_handle_release(symbiomon_rh);
    if(ret != SYMBIOMON_SUCCESS) {
        FATAL(mid,"symbiomon_metric_handle_release failed (ret = %d)", ret);
    }

    margo_info(mid, "Finalizing client");
    ret = symbiomon_client_finalize(symbiomon_clt);
    if(ret != SYMBIOMON_SUCCESS) {
        FATAL(mid,"symbiomon_client_finalize failed (ret = %d)", ret);
    }

    hret = margo_addr_free(mid, svr_addr);
    if(hret != HG_SUCCESS) {
        FATAL(mid,"Could not free address (margo_addr_free returned %d)", hret);
    }

    margo_finalize(mid);

    return 0;
}
