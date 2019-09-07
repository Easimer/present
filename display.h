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

struct display;
enum display_event {
    DISPEV_NONE = 0,
    DISPEV_PREV,
    DISPEV_NEXT,
    DISPEV_START,
    DISPEV_END,
    DISPEV_EXIT,
    DISPEV_MAX
};

display* display_open();
void display_close(display*);
bool display_fetch_event(display*, display_event* out);
void display_render_queue(display*, render_queue*);

bool display_swap_red_blue_channels();
