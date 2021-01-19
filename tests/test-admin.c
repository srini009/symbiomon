/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <stdio.h>
#include <margo.h>
#include <symbiomon/symbiomon-server.h>
#include <symbiomon/symbiomon-admin.h>
#include "munit/munit.h"

struct test_context {
    margo_instance_id mid;
    hg_addr_t         addr;
};

static const char* valid_token = "ABCDEFGH";
static const uint16_t provider_id = 42;

static void* test_context_setup(const MunitParameter params[], void* user_data)
{
    (void) params;
    (void) user_data;
    margo_instance_id mid;
    hg_addr_t         addr;
    // create margo instance
    mid = margo_init("na+sm", MARGO_SERVER_MODE, 0, 0);
    munit_assert_not_null(mid);
    // get address of current process
    hg_return_t hret = margo_addr_self(mid, &addr);
    munit_assert_int(hret, ==, HG_SUCCESS);
    // register symbiomon provider
    struct symbiomon_provider_args args = SYMBIOMON_PROVIDER_ARGS_INIT;
    args.token = valid_token;
    symbiomon_return_t ret = symbiomon_provider_register(
            mid, provider_id, &args,
            SYMBIOMON_PROVIDER_IGNORE);
    munit_assert_int(ret, ==, SYMBIOMON_SUCCESS);
    // create test context
    struct test_context* context = (struct test_context*)calloc(1, sizeof(*context));
    munit_assert_not_null(context);
    context->mid  = mid;
    context->addr = addr;
    return context;
}

static void test_context_tear_down(void* fixture)
{
    struct test_context* context = (struct test_context*)fixture;
    // free address
    margo_addr_free(context->mid, context->addr);
    // we are not checking the return value of the above function with
    // munit because we need margo_finalize to be called no matter what.
    margo_finalize(context->mid);
}

static MunitResult test_admin(const MunitParameter params[], void* data)
{
    (void)params;
    (void)data;
    struct test_context* context = (struct test_context*)data;
    symbiomon_admin_t admin;
    symbiomon_return_t ret;
    // test that we can create an admin object
    ret = symbiomon_admin_init(context->mid, &admin);
    munit_assert_int(ret, ==, SYMBIOMON_SUCCESS);
    // test that we can free the admin object
    ret = symbiomon_admin_finalize(admin);
    munit_assert_int(ret, ==, SYMBIOMON_SUCCESS);

    return MUNIT_OK;
}

static MunitResult test_metric(const MunitParameter params[], void* data)
{
    (void)params;
    (void)data;
    struct test_context* context = (struct test_context*)data;
    symbiomon_admin_t admin;
    symbiomon_return_t ret;
    // test that we can create an admin object
    ret = symbiomon_admin_init(context->mid, &admin);
    munit_assert_int(ret, ==, SYMBIOMON_SUCCESS);
    
    // test that we can free the admin object
    ret = symbiomon_admin_finalize(admin);
    munit_assert_int(ret, ==, SYMBIOMON_SUCCESS);

    return MUNIT_OK;
}

static MunitResult test_invalid(const MunitParameter params[], void* data)
{
    (void)params;
    (void)data;
    struct test_context* context = (struct test_context*)data;
    symbiomon_admin_t admin;
    symbiomon_return_t ret;
    // test that we can create an admin object
    ret = symbiomon_admin_init(context->mid, &admin);
    munit_assert_int(ret, ==, SYMBIOMON_SUCCESS);

    // test that we can free the admin object
    ret = symbiomon_admin_finalize(admin);
    munit_assert_int(ret, ==, SYMBIOMON_SUCCESS);

    return MUNIT_OK;
}

static MunitTest test_suite_tests[] = {
    { (char*) "/admin",    test_admin,    test_context_setup, test_context_tear_down, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "/metric", test_metric, test_context_setup, test_context_tear_down, MUNIT_TEST_OPTION_NONE, NULL },
    { (char*) "/invalid",  test_invalid,  test_context_setup, test_context_tear_down, MUNIT_TEST_OPTION_NONE, NULL },
    { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }
};

static const MunitSuite test_suite = { 
    (char*) "/symbiomon/admin", test_suite_tests, NULL, 1, MUNIT_SUITE_OPTION_NONE
};

int main(int argc, char* argv[MUNIT_ARRAY_PARAM(argc + 1)]) {
    return munit_suite_main(&test_suite, (void*) "symbiomon", argc, argv);
}
