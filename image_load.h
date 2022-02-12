// present
// Copyright (C) 2020 Daniel Meszaros <easimer@gmail.com>
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

struct Promised_Image;

struct Loaded_Image {
    char* buffer;
    int width, height;
};

void ImageLoader_Init();
void ImageLoader_Shutdown();
Promised_Image* ImageLoader_Request(const char* path);
Loaded_Image* ImageLoader_Await(Promised_Image* pimg);
//void ImageLoader_Free(Promised_Image* pimg);
void ImageLoader_Free(Loaded_Image* limg);
