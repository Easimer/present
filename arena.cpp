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

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include "arena.h"

struct mem_arena {
    unsigned size;
    unsigned used;
    uint8_t* frame_pointer;
    uint8_t* cursor;
    uint8_t* content;
};

mem_arena* arena_create(unsigned size) {
    mem_arena* ret = NULL;
    assert(size > 0);
    
    ret = (mem_arena*)malloc(sizeof(mem_arena) + size);
    if(ret) {
        memset(ret, 0, sizeof(mem_arena) + size);
        ret->size = size;
        ret->used = 0;
        ret->content = ret->cursor = (uint8_t*)(ret + 1);
        ret->frame_pointer = ret->content;
    }
    
    return ret;
}

void arena_destroy(mem_arena* arena) {
    assert(arena);
    if(arena) {
        free(arena);
    }
}

void arena_clear(mem_arena* arena) {
    assert(arena);
    if(arena) {
        arena->used = 0;
        arena->cursor = arena->content;
        arena->frame_pointer = arena->content;
    }
}

void* arena_alloc(mem_arena* arena, unsigned size) {
    void* ret = NULL;
    assert(arena && size > 0 && arena->size - arena->used >= size);
    // - Arena must exist
    // - Size must be non-zero
    // - Atleast 'size' bytes must be available
    if(arena && size > 0 && arena->size - arena->used >= size) {
        ret = arena->cursor;
        arena->cursor += size;
        arena->used += size;
    }
    return ret;
}

unsigned arena_used(mem_arena* arena) {
    assert(arena);
    unsigned ret = 0;
    if(arena) {
        ret = arena->used;
    }
    return ret;
}

unsigned arena_size(mem_arena* arena) {
    assert(arena);
    unsigned ret = 0;
    if(arena) {
        ret = arena->size;
    }
    return ret;
}