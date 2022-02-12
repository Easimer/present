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

#include <cstdint>

// A handle to a memory arena
struct Mem_Arena;

using Mem_Arena_Offset = uint32_t;

#define MEM_ARENA_INVALID_OFFSET ((Mem_Arena_Offset)-1)

// Creates a new area of a given size
Mem_Arena* Arena_Create(unsigned size);

// Frees an arena
void Arena_Destroy(Mem_Arena* arena);

// Clears the contents of an arena
void Arena_Clear(Mem_Arena* arena);

// Allocates a block in the arena, returning it's address
[[deprecated]]
void* Arena_Alloc(Mem_Arena* arena, unsigned size);

// Returns how many bytes have been allocated in the arena
unsigned Arena_Used(Mem_Arena* arena);

// Returns how big is the arena
unsigned Arena_Size(Mem_Arena* arena);

Mem_Arena_Offset Arena_AllocEx(Mem_Arena* arena, unsigned size);

void* Arena_Resolve(Mem_Arena* arena, Mem_Arena_Offset idx);