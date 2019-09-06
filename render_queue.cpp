#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include "render_queue.h"
#include "arena.h"

#define RQ_ARENA_SIZE (4 * 1024) // 4KiB

render_queue* rq_alloc() {
    render_queue* ret = NULL;

    ret = (render_queue*)malloc(sizeof(render_queue));
    if(ret) {
        ret->mem = arena_create(RQ_ARENA_SIZE);
        ret->commands = ret->last = NULL;
    }

    return ret;
}

void rq_free(render_queue* rq) {
    assert(rq);
    if(rq) {
        arena_destroy(rq->mem);
        free(rq);
    }
}

void rq_clear(render_queue* rq) {
    assert(rq);
    if(rq) {
        arena_clear(rq->mem);
        rq->commands = NULL;
    }
}

void* rq_new_cmd(render_queue* rq, unsigned size) {
    void* ret = NULL;
    assert(rq && size > 0);
    if(rq && size > 0) {
        ret = arena_alloc(rq->mem, size);
        rq_draw_cmd* hdr = (rq_draw_cmd*)ret;
        hdr->cmd = RQCMD_INVALID;
        hdr->next = NULL;

        if(rq->last) {
            rq->last->next = (rq_draw_cmd*)ret;
            rq->last = (rq_draw_cmd*)ret;
        } else {
            rq->commands = rq->last = (rq_draw_cmd*)ret;
        }
    }
    return ret;
}
