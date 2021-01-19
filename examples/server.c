/*
 * (C) 2020 The University of Chicago
 *
 * See COPYRIGHT in top-level directory.
 */
#include <assert.h>
#include <stdio.h>
#include <margo.h>
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#include <symbiomon/symbiomon-server.h>
#include <symbiomon/symbiomon-metric.h>
#include <symbiomon/symbiomon-common.h>

symbiomon_metric_t m, m2, m3;

void metric_update(int signum)
{  
    static int i = 0;
    symbiomon_metric_update(m2, 9+i*1.1);
    signal(signum, metric_update);
    alarm(1);
    i++;
}

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    margo_instance_id mid = margo_init("na+sm://", MARGO_SERVER_MODE, 0, 0);
    assert(mid);

    hg_addr_t my_address;
    margo_addr_self(mid, &my_address);
    char addr_str[128];
    size_t addr_str_size = 128;
    margo_addr_to_string(mid, addr_str, &addr_str_size, my_address);
    margo_addr_free(mid,my_address);
    fprintf(stderr, "Server running at address %s, with provider id 42", addr_str);

    struct symbiomon_provider_args args = SYMBIOMON_PROVIDER_ARGS_INIT;

    symbiomon_provider_t provider;
    symbiomon_provider_register(mid, 42, &args, &provider);

    symbiomon_taglist_t taglist, taglist2, taglist3;

    symbiomon_taglist_create(&taglist, 5, "tag1", "tag2", "tag3", "tag4", "tag5");
    symbiomon_metric_create("srini", "testmetric", SYMBIOMON_TYPE_COUNTER, "My first metric", taglist, &m, provider);


    symbiomon_taglist_create(&taglist2, 3, "tag1", "tag2", "tag3");
    symbiomon_metric_create("srini", "testmetric2", SYMBIOMON_TYPE_COUNTER, "My second metric", taglist2, &m2, provider);

    symbiomon_taglist_create(&taglist3, 0);
    symbiomon_metric_create("srini", "testmetric", SYMBIOMON_TYPE_COUNTER, "My third metric", taglist3, &m3, provider);


    signal(SIGALRM, metric_update);
    alarm(2);
    /*symbiomon_metric_destroy(m, provider);

    symbiomon_taglist_destroy(taglist);*/

    margo_wait_for_finalize(mid);

    return 0;
}
