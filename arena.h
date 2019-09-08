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
// This file is part of present.

#pragma once

struct mem_arena;

mem_arena* arena_create(unsigned size);
void arena_destroy(mem_arena* arena);
void arena_clear(mem_arena* arena);
void* arena_alloc(mem_arena* arena, unsigned size);
unsigned arena_used(mem_arena* arena);
unsigned arena_size(mem_arena* arena);