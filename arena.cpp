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

void arena_push_frame(mem_arena* arena) {
    assert(arena);
    if(arena) {
        // A pointer must fit into our remaining space
        assert(arena->size - arena->used >= sizeof(uint8_t*));
        // Calculate start of new frame
        uint8_t* new_frame_ptr = arena->cursor + sizeof(uint8_t*);
        // Store previous frame pointer at cursor
        *(uint8_t**)(arena->cursor) = arena->frame_pointer;
        // Step cursor by a pointer's size
        arena->cursor = new_frame_ptr;
        // Set the new frame pointer
        arena->frame_pointer = arena->cursor;
    }
}

void arena_pop_frame(mem_arena* arena) {
    assert(arena);
    if(arena) {
        uint8_t* prev_frame_ptr = *(uint8_t**)(arena->frame_pointer - sizeof(uint8_t*));
        assert(prev_frame_ptr != arena->frame_pointer);
        if(prev_frame_ptr != arena->frame_pointer) {
            arena->frame_pointer = prev_frame_ptr;
            arena->cursor = arena->frame_pointer;
        }
    }
}

void* arena_get_frame_ptr(mem_arena* arena) {
    void* ret = NULL;
    assert(arena);
    if(arena) {
        ret = arena->frame_pointer;
    }
    return ret;
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
    }
    return ret;
}
