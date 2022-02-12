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
#include "render_queue.h"

struct Present_File;

// Load a presentation file into memory
Present_File* Present_Open(const char* filename);

// Close a presentation file
void Present_Close(Present_File* file);

// Returns whether the presentation is over
bool Present_Over(Present_File* file);

// Jumps forward `off` slides
int Present_Seek(Present_File* file, int off);

// Jumps to a given slide.
// idx == -1 --> goes to last slide
// idx < 0   --> goes to title slide
// idx > len --> goes to the last black slide
// otherwise --> goes to `idx`th slide
int Present_SeekTo(Present_File* file, int idx);

// Fill a render queue with draw commands
void Present_FillRenderQueue(Present_File* file, Render_Queue* rq);

void Present_ExecuteCommandOnCurrentSlide(Present_File* file);