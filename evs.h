

typedef void (*evs_cb)(void *ctx);


struct evs;
struct evs_subp;
struct evs_tickgen;


struct evs_tickgen_vtable {
    int (*waitUntilNextTick)(void *ctx);
};


struct evs *evs_new(void);
struct evs_subp *evs_subp_new(void);
struct evs_tickgen *evs_tickgen_new(void);

void evs_free(struct evs *evs);
void evs_tickgen_free(struct evs_tickgen *evs_tickgen);

void evs_start(struct evs *evs);
void evs_add_subp(struct evs *evs, struct evs_subp *subp);
void evs_add_func(struct evs *evs, evs_cb cb);
void evs_add_delay(struct evs *evs, int ticks);
void evs_ctx(struct evs *evs, void *ctx);
void evs_bind_tickgen(struct evs *evs, struct evs_tickgen *tickgen);


void evs_subp_add_subp(struct evs_subp *evs_subp, struct evs_subp *evs_subsubp);
void evs_subp_add_func(struct evs_subp *evs_subp, evs_cb cb);
void evs_subp_repeat(struct evs_subp *evs_subp, int n);
void evs_subp_add_delay(struct evs_subp *evs, int ticks);
void evs_subp_ctx(struct evs_subp *evs_subp, void *ctx);

void evs_tickgen_ctx(struct evs_tickgen *evs_tickgen, void *ctx);
void evs_tickgen_vtable(struct evs_tickgen *evs_tickgen, struct evs_tickgen_vtable *vtable);
