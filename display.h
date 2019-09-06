#pragma once
#include "render_queue.h"

struct display;
enum display_event {
    DISPEV_NONE = 0,
    DISPEV_PREV,
    DISPEV_NEXT,
    DISPEV_START,
    DISPEV_END,
    DISPEV_EXIT,
    DISPEV_MAX
};

display* display_open();
void display_close(display*);
bool display_fetch_event(display*, display_event* out);
void display_render_queue(display*, render_queue*);
