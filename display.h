// present
// Copyright (C) 2019-2020 Daniel Meszaros <easimer@gmail.com>
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

// A display handle.
// The contents of this struct are platform-dependent
// and clients of this API should not need to know
// about them.
struct Display;

// User input actions.
enum Display_Event {
    // No event
    DISPEV_NONE = 0,
    // User wants to see the previous slide
    DISPEV_PREV,
    // User wants to see the next slide
    DISPEV_NEXT,
    // User wants to see the title slide
    DISPEV_START,
    // User wants to see the last slide
    DISPEV_END,
    // User wants to exit
    DISPEV_EXIT,
    // Invalid event
    DISPEV_MAX
};

// Tries to open a display.
// Return non-NULL on success.
Display* DisplayOpen();

// Close the display.
//
// If display is NULL, this is a no-op.
void DisplayClose(Display* display);

// Tries to fetch an event from the event queue. If there was an
// event, it is written to out and true will be returned.
// If nothing happened since the last call, false will be returned.
//
// If display is NULL, this is a no-op and will return false.
bool DisplayFetchEvent(Display* display, Display_Event& out);

// Draws a Render_Queue to the display.
void DisplayRenderQueue(Display* display, render_queue* rq);

// Returns whether images queued to be drawn should
// have their red and blue channels swapped.
// Used in present.cpp when loading an image.
bool DisplaySwapRedBlueChannels();
