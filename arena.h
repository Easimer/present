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

struct Mem_Arena;

Mem_Arena* Arena_Create(unsigned size);
void Arena_Destroy(Mem_Arena* arena);
void Arena_Clear(Mem_Arena* arena);
void* Arena_Alloc(Mem_Arena* arena, unsigned size);
unsigned Arena_Used(Mem_Arena* arena);
unsigned Arena_Size(Mem_Arena* arena);