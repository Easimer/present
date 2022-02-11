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

static void RenderLoop(const char* filename) {
    Display* disp;
    Present_File* file;
    Display_Event ev;
    bool requested_exit = false;
    Render_Queue* rq = NULL;

    ImageLoader_Init();

    // Open presentation file
    file = Present_Open(filename);
    if(file) {
        // Open a window
        disp = Display_Open();
        if(disp) {
            // Loop until the presentation is over or
            // the user has requested an exit (by pressing ESC)
            while(!Present_Over(file) && !requested_exit) {
                // Wait for an event
                if(Display_FetchEvent(disp, ev)) {
                    int f;
                    bool redraw = true;
                    switch(ev) {
                        case DISPEV_PREV:
                        f = Present_Seek(file, -1);
                        break;
                        case DISPEV_NEXT:
                        f = Present_Seek(file, 1);
                        break;
                        case DISPEV_START:
                        f = Present_SeekTo(file, 0);
                        break;
                        case DISPEV_END:
                        f = Present_SeekTo(file, -1);
                        break;
                        case DISPEV_EXIT:
                        requested_exit = true;
                        break;
                        case DISPEV_NONE:
                        break;
                        case DISPEV_FOCUS:
                        Display_Focus(disp);
                        redraw = false;
                        break;
                        default:
                        assert(0);
                        break;
                        }
                    if(redraw) {
                        // Allocate an empty render buffer
                        rq = RQ_Alloc();
                        // Only re-render if something happened
                        Present_FillRenderQueue(file, rq);
                        // Display render queue
                        Display_RenderQueue(disp, rq);
                        // Free render queue
                        RQ_Free(rq);
                        // NOTE(easimer): previously we allocated an RQ once at startup
                        // then used that at every redraw, clearing it after the 
                        // DisplayRenderQueue call. But due to images, render queues
                        // can get quite large and we don't want to hold megabytes of memory 
                        // hostage while not even using it.
                    }
                }
            }
            Display_Close(disp);
        }
        Present_Close(file);
    }

    ImageLoader_Shutdown();
}

int main(int argc, char** argv) {
    setlocale(LC_ALL, "en_US.utf8");
    if(argc == 2) {
        RenderLoop(argv[1]);
    } else {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
    }
    return 0;
}
