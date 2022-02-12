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

struct Mem_Arena {
    unsigned size;
    unsigned used;
};

Mem_Arena* Arena_Create(unsigned size) {
    Mem_Arena* ret = NULL;
    assert(size > 0);
    
    ret = (Mem_Arena*)malloc(sizeof(Mem_Arena) + size);
    if(ret) {
        memset(ret, 0, sizeof(Mem_Arena) + size);
        ret->size = size;
        ret->used = 0;
    }
    
    return ret;
}

void Arena_Destroy(Mem_Arena* arena) {
    assert(arena);
    if(arena) {
        free(arena);
    }
}

void Arena_Clear(Mem_Arena* arena) {
    assert(arena);
    if(arena) {
        arena->used = 0;
    }
}

void* Arena_Alloc(Mem_Arena* arena, unsigned size) {
    return Arena_Resolve(arena, Arena_AllocEx(arena, size));
}

unsigned Arena_Used(Mem_Arena* arena) {
    assert(arena);
    unsigned ret = 0;
    if(arena) {
        ret = arena->used;
    }
    return ret;
}

unsigned Arena_Size(Mem_Arena* arena) {
    assert(arena);
    unsigned ret = 0;
    if(arena) {
        ret = arena->size;
    }
    return ret;
}

Mem_Arena_Offset Arena_AllocEx(Mem_Arena* arena, unsigned size) {
    Mem_Arena_Offset ret;
    if(!(arena && size > 0 && arena->size - arena->used >= size)) {
        abort();
    }

    ret = arena->used;
    arena->used += size;

    return ret;
}

void* Arena_Resolve(Mem_Arena* arena, Mem_Arena_Offset idx) {
    if(!arena || idx >= arena->size) {
        abort();
    }

    return ((uint8_t*)(arena + 1)) + idx;
}