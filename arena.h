#pragma once

struct mem_arena;

mem_arena* arena_create(unsigned size);
void arena_destroy(mem_arena* arena);
void arena_clear(mem_arena* arena);
void arena_push_frame(mem_arena* arena);
void arena_pop_frame(mem_arena* arena);
void* arena_get_frame_ptr(mem_arena* arena);
void* arena_alloc(mem_arena* arena, unsigned size);
