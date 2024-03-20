#include <stdio.h>
#include <unistd.h>

#include "evs.h"

void foo(void *ctx)
{
    printf("%s\n", (char*)ctx);
}

void bar(void *ctx)
{
    printf("%s\n", (char*)ctx);
}

int ftick(void *ctx)
{
    sleep(1);

    return 1;
}


int main(void)
{
    struct evs *evs = evs_new();
    struct evs_subp *evs_subp1 = evs_subp_new();
    struct evs_subp *evs_subp2 = evs_subp_new();
    struct evs_tickgen *tickgen = evs_tickgen_new();
    struct evs_tickgen_vtable vtable = {
        .waitUntilNextTick = ftick
    };

    evs_tickgen_vtable(tickgen, &vtable);
    evs_bind_tickgen(evs, tickgen);

    evs_subp_add_func(evs_subp1, foo);
    evs_subp_repeat(evs_subp1, 4);
    evs_subp_ctx(evs_subp1, "Foo");

    evs_subp_add_func(evs_subp2, foo);
    evs_subp_repeat(evs_subp2, 1);
    evs_subp_ctx(evs_subp2, "subp2");

    evs_add_subp(evs, evs_subp1);
    evs_add_delay(evs, 2);
    evs_add_func(evs, bar);
    evs_add_delay(evs, 4);
    evs_add_subp(evs, evs_subp2);
    evs_ctx(evs, "Bar");

    evs_start(evs);
    evs_free(evs);
    evs_tickgen_free(tickgen);

    return 0;
}

