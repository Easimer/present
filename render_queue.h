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
    RQCMD_MAX
};

struct rq_draw_cmd {
    rq_cmd cmd;
    rq_draw_cmd* next;
};

struct rq_draw_text {
    rq_draw_cmd hdr;
    int x, y;
    int size;
    const char* text;
};

struct rq_draw_image {
    rq_draw_cmd hdr;
    int x, y;
    int width, height;
    void* rgba_buffer; // R8B8G8 format
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

#ifdef __cplusplus__
template<typename T>
inline T* rq_new_cmd(render_queue* rq, rq_cmd cmd = RQCMD_INVALID) {
    T* ret = rq_new_cmd(rq, sizeof(T));

    ret->hdr.cmd = cmd;
    ret->hdr.next = NULL;

    return ret;
}
#endif
