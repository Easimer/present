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
#include "image_load.h"
#include <stddef.h>

// Render command kind
enum RQ_Cmd {
    // Invalid command
    RQCMD_INVALID = 0,
    // Draw text
    RQCMD_DRAW_TEXT,
    // Draw an image
    RQCMD_DRAW_IMAGE,
    // Draw a rectangle
    RQCMD_DRAW_RECTANGLE,
    RQCMD_MAX
};

// RGBA Color
struct RGBA_Color {
    float r, g, b, a;
};

// Draw command - common fields
struct RQ_Draw_Cmd {
    RQ_Cmd cmd;
    RQ_Draw_Cmd* next;
};

// Draw text command
struct RQ_Draw_Text {
    RQ_Draw_Cmd hdr;
    float x, y; // text position [0,1] normalized
    float size; // text height in percentage of screen height
    int text_len;
    const char* text;
    RGBA_Color color;
    
    const char* font_name;
};

// Draw image command
struct RQ_Draw_Image {
    RQ_Draw_Cmd hdr;
    float x, y; // position [0, 1] screen space
    float w, h; // size [0, 1]
    int width, height; // Image size in pixels TODO(easimer): rename these
    void* buffer; // R8B8G8 format
};

// Draw rectangle command
struct RQ_Draw_Rect {
    RQ_Draw_Cmd hdr;
    float x0, y0, x1, y1; // [0, 1] normalized ss coords
    RGBA_Color color;
};

// Render queue
struct Render_Queue {
    Mem_Arena* mem;
    
    RQ_Draw_Cmd* commands;
    RQ_Draw_Cmd* last;
};

// Tries to allocate a new render queue
Render_Queue* RQ_Alloc();

// Deallocates the render queue
void RQ_Free(Render_Queue* rq);
void RQ_Clear(Render_Queue* rq);
// Creates a new command of a given size and appends it to the end of
// the render queue.
void* RQ_NewCmd(Render_Queue* rq, unsigned size);

#ifdef __cplusplus
// Creates a new command of a given type and appends it to the end of
// the render queue.
template<typename T>
inline T* RQ_NewCmd(Render_Queue* rq, RQ_Cmd cmd = RQCMD_INVALID) {
    T* ret = (T*)RQ_NewCmd(rq, sizeof(T));
    
    ret->hdr.cmd = cmd;
    ret->hdr.next = NULL;
    
    return ret;
}
#endif

// NOTE(easimer): most drawing commands specify sizes of things in term of percentage
// of the screen size instead of pixels to ensure that they remain the same
// size on all kinds of screen resolutions. But sometimes in code we need to say
// things like 'I want this text to be 72px tall on a 720p screen!' For these
// cases we have two macros: VIRTUAL_X and VIRTUAL_Y.
// TODO(easimer): This doesn't work on 4:3 displays.
// TODO(easimer): Images only partially use this system. They are not scaled.

#define VIRTUAL_X(val) ((val) / 1280.0f)
#define VIRTUAL_Y(val) ((val) / 720.0f)

#define SET_RGB(aggr, R, G, B)  \
(aggr).r = (R) / 255.0f; \
(aggr).g = (G) / 255.0f; \
(aggr).b = (B) / 255.0f; \
(aggr).a = 1;
