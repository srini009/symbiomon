/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <string.h>
#include "symbiomon/symbiomon-backend.h"
#include "../provider.h"
#include "dummy-backend.h"

typedef struct dummy_context {
    /* ... */
} dummy_context;

static symbiomon_return_t dummy_create_metric(
        symbiomon_provider_t provider,
        const char* config_str,
        void** context)
{
    (void)provider;

    dummy_context* ctx = (dummy_context*)calloc(1, sizeof(*ctx));
    *context = (void*)ctx;
    return SYMBIOMON_SUCCESS;
}

static symbiomon_return_t dummy_open_metric(
        symbiomon_provider_t provider,
        const char* config_str,
        void** context)
{
    (void)provider;

    dummy_context* ctx = (dummy_context*)calloc(1, sizeof(*ctx));
    *context = (void*)ctx;
    return SYMBIOMON_SUCCESS;
}

static symbiomon_return_t dummy_close_metric(void* ctx)
{
    dummy_context* context = (dummy_context*)ctx;
    free(context);
    return SYMBIOMON_SUCCESS;
}

static symbiomon_return_t dummy_destroy_metric(void* ctx)
{
    dummy_context* context = (dummy_context*)ctx;
    free(context);
    return SYMBIOMON_SUCCESS;
}

static void dummy_say_hello(void* ctx)
{
    dummy_context* context = (dummy_context*)ctx;
    (void)context;
    printf("Hello World from Dummy metric\n");
}

static int32_t dummy_compute_sum(void* ctx, int32_t x, int32_t y)
{
    (void)ctx;
    return x+y;
}

static symbiomon_backend_impl dummy_backend = {
    .name             = "dummy",

    .create_metric  = dummy_create_metric,
    .open_metric    = dummy_open_metric,
    .close_metric   = dummy_close_metric,
    .destroy_metric = dummy_destroy_metric,

    .hello            = dummy_say_hello,
    .sum              = dummy_compute_sum
};

symbiomon_return_t symbiomon_provider_register_dummy_backend(symbiomon_provider_t provider)
{
    return symbiomon_provider_register_backend(provider, &dummy_backend);
}
