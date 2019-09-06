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

struct present_file;

present_file* present_open(const char* filename);
void present_close(present_file* file);

bool present_over(present_file* file);

int present_seek(present_file* file, int off);
int present_seek_to(present_file* file, int idx);
void present_fill_render_queue(present_file* file, render_queue* rq);
