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

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include "render_queue.h"
#include "arena.h"

#define RQ_ARENA_SIZE (32 * 1024 * 1024) // 32MiB

Render_Queue* RQ_Alloc() {
    Render_Queue* ret = NULL;
    
    ret = (Render_Queue*)malloc(sizeof(Render_Queue));
    if(ret) {
        ret->mem = Arena_Create(RQ_ARENA_SIZE);
        ret->commands = ret->last = NULL;
    }
    
    return ret;
}

void RQ_Free(Render_Queue* rq) {
    assert(rq);
    if(rq) {
        Arena_Destroy(rq->mem);
        free(rq);
    }
}

void RQ_Clear(Render_Queue* rq) {
    assert(rq);
    if(rq) {
        Arena_Clear(rq->mem);
        rq->commands = NULL;
        rq->last = NULL;
    }
}

void* RQ_NewCmd(Render_Queue* rq, unsigned size) {
    void* ret = NULL;
    assert(rq && size > 0);
    if(rq && size > 0) {
        ret = Arena_Alloc(rq->mem, size);
        RQ_Draw_Cmd* hdr = (RQ_Draw_Cmd*)ret;
        hdr->cmd = RQCMD_INVALID;
        hdr->next = NULL;
        
        if(rq->last) {
            rq->last->next = (RQ_Draw_Cmd*)ret;
            rq->last = (RQ_Draw_Cmd*)ret;
        } else {
            rq->commands = rq->last = (RQ_Draw_Cmd*)ret;
        }
    }
    return ret;
}
