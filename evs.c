#include <stdlib.h>
#include <stdarg.h>
#include <stddef.h>
#include <string.h>

#include "evs.h"


enum evs_mem_type {
    EVS_MEM_CB,
    EVS_MEM_SUBP,
    EVS_MEM_DELAY,
};

struct evs_subp_member {
    enum evs_mem_type type;
    union {
        evs_cb cb;
        struct evs_subp *subp;
        int ticks;
    };
    struct evs_subp_member *parent;
    struct evs_subp_member *next;
};

struct evs_prog {
    void *ctx;
    struct evs_prog *parent;
    struct evs_subp_member *members;
};

struct evs {
    struct evs_prog prog;
    struct evs_tickgen *tickgen;
    struct evs_subp_member *curmem;
    struct evs_ctx {
        void *cur;
        struct evs_ctx *prev;
    } *ctx;
    struct evs_repeats {
        int n;
        struct evs_repeats *prev;
    } *repeats;
    char cont;
};

struct evs_subp {
    struct evs_prog prog;
    int repeats;
};

struct evs_tickgen {
    void *ctx;
    struct evs_tickgen_vtable vtable;
};

static void evs_update_parent(struct evs_subp_member *members, struct evs_subp_member *parent);
static void evs_subp_free(struct evs_subp_member *members);
static void evs_insert_member(char is_main, struct evs_subp_member **list, enum evs_mem_type type, ...);
static struct evs_subp_member *evs_new_member(enum evs_mem_type type, ...);
static struct evs_subp_member *evs_new_member_v(enum evs_mem_type type, va_list va);
static int evs_progress(struct evs *evs);
static void evs_next(struct evs *evs);
static void evs_push_ctx(struct evs *evs, void *ctx);
static void evs_pop_ctx(struct evs *evs);
static void evs_push_repeat(struct evs *evs, int n);
static void evs_pop_repeat(struct evs *evs);


struct evs *evs_new(void)
{
    struct evs *evs = malloc(sizeof *evs);

    evs->prog.ctx = NULL;
    evs->prog.members = NULL;
    evs->prog.parent = NULL;
    evs->ctx = NULL;
    evs->cont = (char)1;
    evs->curmem = NULL;
    evs->tickgen = NULL;

    return evs;
}

struct evs_subp *evs_subp_new(void)
{
    struct evs_subp *evs_subp = malloc(sizeof *evs_subp);

    evs_subp->prog.ctx = NULL;
    evs_subp->prog.members = NULL;
    evs_subp->prog.parent = NULL;
    evs_subp->repeats = 0;

    return evs_subp;
}


void evs_free(struct evs *evs)
{
    evs_subp_free(evs->prog.members);
    free(evs);
}

void evs_tickgen_free(struct evs_tickgen *evs_tickgen)
{
    free(evs_tickgen);
}


void evs_start(struct evs *evs)
{
    int ticksToWait = 0;

    while (evs->cont) {
        int ticksWaited = evs->tickgen->vtable.waitUntilNextTick(ticksToWait, evs->tickgen->ctx);

        ticksToWait -= ticksWaited;

        if (ticksToWait <= 0) {
            ticksToWait += evs_progress(evs);

            evs_next(evs);
        }
    }
}

void evs_stop(struct evs *evs)
{
    evs->cont = (char)0;
}

void evs_add_subp(struct evs *evs, struct evs_subp *subp)
{
    evs_insert_member(1, &evs->prog.members, EVS_MEM_SUBP, subp);
}

void evs_add_func(struct evs *evs, evs_cb cb)
{
    evs_insert_member(1, &evs->prog.members, EVS_MEM_CB, cb);
}

void evs_add_delay(struct evs *evs, int ticks)
{
    evs_insert_member(1, &evs->prog.members, EVS_MEM_DELAY, ticks);
}

void evs_ctx(struct evs *evs, void *ctx)
{
    evs->prog.ctx = ctx;
    evs_push_ctx(evs, ctx);
}

void evs_bind_tickgen(struct evs *evs, struct evs_tickgen *tickgen)
{
    evs->tickgen = tickgen;
}


void evs_subp_add_subp(struct evs_subp *evs_subp, struct evs_subp *evs_subsubp)
{
    evs_insert_member(0, &evs_subp->prog.members, EVS_MEM_SUBP, evs_subsubp);
}

void evs_subp_add_func(struct evs_subp *evs_subp, evs_cb cb)
{
    evs_insert_member(0, &evs_subp->prog.members, EVS_MEM_CB, cb);
}

void evs_subp_add_delay(struct evs_subp *evs_subp, int ticks)
{
    evs_insert_member(0, &evs_subp->prog.members, EVS_MEM_DELAY, ticks);
}


void evs_subp_repeat(struct evs_subp *evs_subp, int n)
{
    evs_subp->repeats = n - 1;
}

void evs_subp_ctx(struct evs_subp *evs_subp, void *ctx)
{
    evs_subp->prog.ctx = ctx;
}


struct evs_tickgen *evs_tickgen_new(void)
{
    struct evs_tickgen *tickgen = malloc(sizeof *tickgen);

    tickgen->ctx = NULL;
    memset(&tickgen->vtable, 0, sizeof tickgen->vtable);

    return tickgen;
}

void evs_tickgen_ctx(struct evs_tickgen *evs_tickgen, void *ctx)
{
    evs_tickgen->ctx = ctx;
}

void evs_tickgen_vtable(struct evs_tickgen *evs_tickgen, struct evs_tickgen_vtable *vtable)
{
    evs_tickgen->vtable = *vtable;
}

static void evs_compose_v(struct evs_subp_member **members, va_list elements)
{
    struct evs_subp_member *member;
    while ((member = va_arg(elements, struct evs_subp_member*))) {
        *members = member;
        members = &(*members)->next;
    }
}

struct evs_subp *evs_compose(struct evs_subp *subp, ...)
{
    va_list elements;

    va_start(elements, subp);
    evs_compose_v(&subp->prog.members, elements);
    va_end(elements);

    return subp;
}

evs_compose_element evs_compose_delay(int ticks)
{
    return evs_new_member(EVS_MEM_DELAY, ticks);
}

evs_compose_element evs_compose_func(evs_cb cb)
{
    return evs_new_member(EVS_MEM_CB, cb);
}

evs_compose_element evs_compose_subp(void *ctx, int repeats, ...)
{
    struct evs_subp *subp = evs_subp_new();
    
    va_list va;
    evs_subp_repeat(subp, repeats); 
    evs_subp_ctx(subp, ctx);
    
    va_start(va, repeats);
    evs_compose_v(&subp->prog.members, va);
    va_end(va);

    return evs_new_member(EVS_MEM_SUBP, subp);
}

static void evs_insert_member(char is_main, struct evs_subp_member **listp, enum evs_mem_type type, ...)
{
    struct evs_subp_member **mem;
    // move to end of list
    for (mem = listp; *mem; mem = &(*mem)->next);
    va_list va;
    va_start(va, type);
    *mem = evs_new_member_v(type, va);
    (*mem)->parent = NULL;
    va_end(va);
}

static struct evs_subp_member *evs_new_member(enum evs_mem_type type, ...)
{
    va_list va;
    struct evs_subp_member *mem = malloc(sizeof *mem);

    mem->type = type;
    mem->next = NULL;

    va_start(va, type);
    switch (type) {
    case EVS_MEM_CB:
        mem->cb = va_arg(va, evs_cb);
        break;
    case EVS_MEM_SUBP:
        mem->subp = va_arg(va, struct evs_subp*);
        evs_update_parent(mem->subp->prog.members, mem);
        break;
    case EVS_MEM_DELAY:
        mem->ticks = va_arg(va, int);
        break;
    }

    va_end(va);

    return mem;
}

static struct evs_subp_member *evs_new_member_v(enum evs_mem_type type, va_list va)
{
    struct evs_subp_member *mem = malloc(sizeof *mem);

    mem->type = type;
    mem->next = NULL;

    switch (type) {
    case EVS_MEM_CB:
        mem->cb = va_arg(va, evs_cb);
        break;
    case EVS_MEM_SUBP:
        mem->subp = va_arg(va, struct evs_subp*);
        evs_update_parent(mem->subp->prog.members, mem);
        break;
    case EVS_MEM_DELAY:
        mem->ticks = va_arg(va, int);
        break;
    }

    return mem;
}


static int evs_progress(struct evs *evs)
{
    if (!evs->curmem) evs->curmem = evs->prog.members;
    struct evs_subp_member *cm = evs->curmem;
    switch (cm->type) {
    case EVS_MEM_CB:
        cm->cb(evs->ctx->cur);
        break;
    case EVS_MEM_SUBP:
        evs->curmem = cm->subp->prog.members;
        evs_push_ctx(evs, cm->subp->prog.ctx);
        evs_push_repeat(evs, cm->subp->repeats);
        evs_progress(evs);
        break;
    case EVS_MEM_DELAY:
        return cm->ticks;
    }

    return 0;
}

static void evs_next(struct evs *evs)
{
    if (evs->curmem->next) {
        evs->curmem = evs->curmem->next;
        return;
    }

    struct evs_subp_member *parent = evs->curmem->parent;

    // If no parent, we must be at the end of the most outer loop
    if (!parent) {
        evs->curmem = NULL;
        return;
    }

    if (evs->repeats && evs->repeats->n) {
        evs->curmem = parent->subp->prog.members;
        evs->repeats->n--;
    }
    else {
        for (; !parent->next && parent->parent; parent = parent->parent) {
            evs_pop_ctx(evs);
            evs_pop_repeat(evs);
        }
        evs->curmem = parent->next;
        evs_pop_ctx(evs);
        evs_pop_repeat(evs);
    }
}


static void evs_push_ctx(struct evs *evs, void *ctx)
{
    if (!evs->ctx) {
        evs->ctx = malloc(sizeof *evs->ctx);
        evs->ctx->cur = ctx;
        evs->ctx->prev = NULL;
        return;
    }

    struct evs_ctx *nctx = malloc(sizeof *nctx);
    nctx->prev = evs->ctx;
    nctx->cur = ctx;
    evs->ctx = nctx;
}

static void evs_pop_ctx(struct evs *evs)
{
    if (!evs->ctx) return;

    struct evs_ctx *prev = evs->ctx->prev;
    free(evs->ctx);
    evs->ctx = prev;
}


static void evs_subp_free(struct evs_subp_member *members)
{
    for (struct evs_subp_member *mem = members; mem;) {
        struct evs_subp_member *n = mem->next;
        if (mem->type == EVS_MEM_SUBP) {
            evs_subp_free(mem->subp->prog.members);
            free(mem->subp);
        }
        free(mem);
        mem = n;
    }
}

static void evs_update_parent(struct evs_subp_member *members, struct evs_subp_member *parent)
{
    for (; members; members = members->next)
        members->parent = parent;
}


static void evs_push_repeat(struct evs *evs, int n)
{
    if (!evs->repeats) {
        evs->repeats = malloc(sizeof *evs->repeats);
        evs->repeats->n = n;
        evs->repeats->prev = NULL;
        return;
    }

    struct evs_repeats *repeats = malloc(sizeof *repeats);
    repeats->prev = evs->repeats;
    repeats->n = n;
    evs->repeats = repeats;
}

static void evs_pop_repeat(struct evs *evs)
{
    if (!evs->repeats) return;

    struct evs_repeats *prev = evs->repeats->prev;
    free(evs->repeats);
    evs->repeats = prev;
}
