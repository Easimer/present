// present
// Copyright (C) 2019 Daniel Meszaros <easimer@gmail.com>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#pragma once
#include "arena.h"

enum rq_cmd {
    RQCMD_INVALID = 0,
    RQCMD_DRAW_TEXT,
    RQCMD_DRAW_IMAGE,
    RQCMD_DRAW_RECTANGLE,
    RQCMD_MAX
};

struct rgba_color {
    float r, g, b, a;
};

struct rq_draw_cmd {
    rq_cmd cmd;
    rq_draw_cmd* next;
};

struct rq_draw_text {
    rq_draw_cmd hdr;
    float x, y; // text position [0,1] normalized
    float size; // text height in percentage of screen height
    int text_len;
    const char* text;
    rgba_color color;
    
    const char* font_name;
};

struct rq_draw_image {
    rq_draw_cmd hdr;
    float x, y;
    int width, height;
    void* buffer; // R8B8G8 format
};

struct rq_draw_rect {
    rq_draw_cmd hdr;
    float x0, y0, x1, y1; // [0, 1] normalized ss coords
    rgba_color color;
};

struct render_queue {
    mem_arena* mem;
    
    rq_draw_cmd* commands;
    rq_draw_cmd* last;
};

render_queue* rq_alloc();
void rq_free(render_queue* rq);
void rq_clear(render_queue* rq);
void* rq_new_cmd(render_queue* rq, unsigned size);

#ifdef __cplusplus
template<typename T>
inline T* rq_new_cmd(render_queue* rq, rq_cmd cmd = RQCMD_INVALID) {
    T* ret = (T*)rq_new_cmd(rq, sizeof(T));
    
    ret->hdr.cmd = cmd;
    ret->hdr.next = NULL;
    
    return ret;
}
#endif

#define VIRTUAL_X(val) ((val) / 1280.0f)
#define VIRTUAL_Y(val) ((val) / 720.0f)
#define SET_RGB(aggr, R, G, B)  \
(aggr).r = (R) / 255.0f; \
(aggr).g = (G) / 255.0f; \
(aggr).b = (B) / 255.0f; \
(aggr).a = 1;
