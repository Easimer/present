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

#include <stdio.h>
#include <locale.h>
#include <assert.h>
#include "display.h"
#include "present.h"
#include "render_queue.h"

static void render_loop(const char* filename) {
    Display* disp;
    present_file* file;
    Display_Event ev;
    bool requested_exit = false;
    render_queue* rq = NULL;
    
    // Open presentation file
    file = present_open(filename);
    if(file) {
        // Open a window
        disp = DisplayOpen();
        if(disp) {
            // Loop until the presentation is over or
            // the user has requested an exit (by pressing ESC)
            while(!present_over(file) && !requested_exit) {
                // Wait for an event
                if(DisplayFetchEvent(disp, ev)) {
                    int f;
                    switch(ev) {
                        case DISPEV_PREV:
                        f = present_seek(file, -1);
                        break;
                        case DISPEV_NEXT:
                        f = present_seek(file, 1);
                        break;
                        case DISPEV_START:
                        f = present_seek_to(file, 0);
                        break;
                        case DISPEV_END:
                        f = present_seek_to(file, -1);
                        break;
                        case DISPEV_EXIT:
                        requested_exit = true;
                        break;
                        case DISPEV_NONE:
                        break;
                        default:
                        assert(0);
                        break;
                    }
                    // Allocate an empty render buffer
                    rq = rq_alloc();
                    // Only re-render if something happened
                    present_fill_render_queue(file, rq);
                    // Display render queue
                    DisplayRenderQueue(disp, rq);
                    // Free render queue
                    rq_free(rq);
                    // NOTE(easimer): previously we allocated an RQ once at startup
                    // then used that at every redraw, clearing it after the 
                    // DisplayRenderQueue call. But due to images, render queues
                    // can get quite large and we don't want to hold megabytes of memory 
                    // hostage while not even using it.
                }
            }
            DisplayClose(disp);
        }
        present_close(file);
    }
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "en_US.utf8");
    if(argc == 2) {
        render_loop(argv[1]);
    } else {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
    }
    return 0;
}
