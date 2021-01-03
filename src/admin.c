/*
 * (C) 2020 The University of Chicago
 * 
 * See COPYRIGHT in top-level directory.
 */
#include "types.h"
#include "admin.h"
#include "symbiomon/symbiomon-admin.h"

symbiomon_return_t symbiomon_admin_init(margo_instance_id mid, symbiomon_admin_t* admin)
{
    symbiomon_admin_t a = (symbiomon_admin_t)calloc(1, sizeof(*a));
    if(!a) return SYMBIOMON_ERR_ALLOCATION;

    a->mid = mid;

    *admin = a;
    return SYMBIOMON_SUCCESS;
}

symbiomon_return_t symbiomon_admin_finalize(symbiomon_admin_t admin)
{
    free(admin);
    return SYMBIOMON_SUCCESS;
}
