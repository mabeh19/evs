#include <stdio.h>
#include <unistd.h>
#include <time.h>

#include "evs.h"

void print(void *ctx)
{
    printf("%s\n", (char*)ctx);
}

void bar(void *ctx)
{
    printf("%s\n", (char*)ctx);
}

void baz(void *ctx)
{
    printf("%d\n", 2 * (int)ctx);
}

void stop(void *ctx)
{
    evs_stop(ctx);
}

int ftick(int ticks, void *ctx)
{
    if (ticks > 0) {
        long sec  = (long)ticks / 1000l;
        long nsec = (long)(ticks - (sec * 1000l)) * (long)1e6;
        struct timespec ts = { sec, nsec }, rem;
        nanosleep(&ts, &rem);
        return ticks;
    }

    return 0;
}

void test1(struct evs *evs)
{
    struct evs_subp *evs_subp1 = evs_subp_new();
    struct evs_subp *evs_subp2 = evs_subp_new();
    struct evs_subp *evs_subp3 = evs_subp_new();

    evs_subp_add_func(evs_subp3, baz);
    evs_subp_add_delay(evs_subp3, 200);
    evs_subp_ctx(evs_subp3, (void*)4);
    evs_subp_repeat(evs_subp3, 4);

    evs_subp_add_func(evs_subp1, print);
    evs_subp_add_subp(evs_subp1, evs_subp3);
    evs_subp_repeat(evs_subp1, 4);
    evs_subp_ctx(evs_subp1, "subp1");

    evs_subp_add_func(evs_subp2, print);
    evs_subp_add_delay(evs_subp2, 1000);
    evs_subp_repeat(evs_subp2, 1);
    evs_subp_ctx(evs_subp2, "subp2");

    evs_add_subp(evs, evs_subp1);
    evs_add_delay(evs, 500);
    evs_add_func(evs, bar);
    evs_add_delay(evs, 10000);
    evs_add_subp(evs, evs_subp2);
    evs_ctx(evs, "main");
}


void test_compose(struct evs *evs)
{
    struct evs_subp *subp = evs_subp_new();
    evs_subp_ctx(subp, "foo");
    evs_subp_repeat(subp, 0);
    evs_compose(
        subp,
        evs_compose_func(print),
        evs_compose_delay(200),
        evs_compose_subp(
            (void*)4, 2,
            evs_compose_func(baz),
            evs_compose_subp(
                "bar", 10,
                evs_compose_func(print),
                evs_compose_delay(100),
                NULL
            ),
            evs_compose_delay(500),
            NULL
        ),
        evs_compose_subp(
            "*pause*", 1,
            evs_compose_func(print),
            evs_compose_delay(2000),
            NULL
        ),
        evs_compose_delay(500),
        evs_compose_subp(
            "baz", 2,
            evs_compose_func(print),
            evs_compose_delay(5),
            NULL
        ),
        evs_compose_delay(2000),
        evs_compose_subp(
            evs, 0,
            evs_compose_func(stop),
            NULL
        ),
        NULL
    );

    evs_add_subp(evs, subp);
}


int main(void)
{
    struct evs *evs = evs_new();
    struct evs_tickgen *tickgen = evs_tickgen_new();
    struct evs_tickgen_vtable vtable = {
        .waitUntilNextTick = ftick
    };
    evs_tickgen_vtable(tickgen, &vtable);
    evs_bind_tickgen(evs, tickgen);

    //test1(evs);
    test_compose(evs);

    evs_start(evs);
    evs_free(evs);
    evs_tickgen_free(tickgen);

    return 0;
}

