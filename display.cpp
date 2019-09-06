#include <stdlib.h>
#include <assert.h>
#include "display.h"

struct display {
};

display* display_open() {
    display* ret = NULL;

    ret = (display*)malloc(sizeof(display));
    if(ret) {
    }

    return ret;
}

void display_close(display* disp) {
    assert(disp);
    if(disp) {
        free(disp);
    }
}

bool display_fetch_event(display* disp, display_event* out) {
    bool ret = false;
    assert(disp && out);
    if(disp && out) {
    }
    return ret;
}

void display_render_queue(display* disp, render_queue* rq) {
    assert(disp && rq);
    if(disp && rq) {
    }
}
