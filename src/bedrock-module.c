/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <bedrock/module.h>
#include "symbiomon/symbiomon-server.h"
#include "symbiomon/symbiomon-client.h"
#include "symbiomon/symbiomon-admin.h"
#include "symbiomon/symbiomon-provider-handle.h"
#include "client.h"
#include <string.h>

static int symbiomon_register_provider(
        bedrock_args_t args,
        bedrock_module_provider_t* provider)
{
    margo_instance_id mid = bedrock_args_get_margo_instance(args);
    uint16_t provider_id  = bedrock_args_get_provider_id(args);

    struct symbiomon_provider_args symbiomon_args = { 0 };
    symbiomon_args.config = bedrock_args_get_config(args);
    symbiomon_args.pool   = bedrock_args_get_pool(args);

    symbiomon_args.abtio = (abt_io_instance_id)
        bedrock_args_get_dependency(args, "abt_io", 0);

    return symbiomon_provider_register(mid, provider_id, &symbiomon_args,
                                   (symbiomon_provider_t*)provider);
}

static int symbiomon_deregister_provider(
        bedrock_module_provider_t provider)
{
    return symbiomon_provider_destroy((symbiomon_provider_t)provider);
}

static char* symbiomon_get_provider_config(
        bedrock_module_provider_t provider) {
    (void)provider;
    // TODO
    return strdup("{}");
}

static int symbiomon_init_client(
        margo_instance_id mid,
        bedrock_module_client_t* client)
{
    return symbiomon_client_init(mid, (symbiomon_client_t*)client);
}

static int symbiomon_finalize_client(
        bedrock_module_client_t client)
{
    return symbiomon_client_finalize((symbiomon_client_t)client);
}

static int symbiomon_create_provider_handle(
        bedrock_module_client_t client,
        hg_addr_t address,
        uint16_t provider_id,
        bedrock_module_provider_handle_t* ph)
{
    symbiomon_client_t c = (symbiomon_client_t)client;
    symbiomon_provider_handle_t tmp = calloc(1, sizeof(*tmp));
    margo_addr_dup(c->mid, address, &(tmp->addr));
    tmp->provider_id = provider_id;
    *ph = (bedrock_module_provider_handle_t)tmp;
    return BEDROCK_SUCCESS;
}

static int symbiomon_destroy_provider_handle(
        bedrock_module_provider_handle_t ph)
{
    symbiomon_provider_handle_t tmp = (symbiomon_provider_handle_t)ph;
    margo_addr_free(tmp->mid, tmp->addr);
    free(tmp);
    return BEDROCK_SUCCESS;
}

static struct bedrock_module symbiomon = {
    .register_provider       = symbiomon_register_provider,
    .deregister_provider     = symbiomon_deregister_provider,
    .get_provider_config     = symbiomon_get_provider_config,
    .init_client             = symbiomon_init_client,
    .finalize_client         = symbiomon_finalize_client,
    .create_provider_handle  = symbiomon_create_provider_handle,
    .destroy_provider_handle = symbiomon_destroy_provider_handle,
    .provider_dependencies   = NULL,
    .client_dependencies     = NULL
};

BEDROCK_REGISTER_MODULE(symbiomon, symbiomon)
