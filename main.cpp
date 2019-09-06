#include "display.h"
#include "present.h"
#include "render_queue.h"

static void render_loop(const char* filename) {
    display* disp;
    present_file* file;
    display_event ev;
    bool requested_exit = false;
    render_queue* rq = NULL;

    // Open presentation file
    file = present_open(filename);
    if(file) {
        // Open a window
        disp = display_open();
        if(disp) {
            // Allocate an empty render buffer
            rq = rq_alloc();
            // Loop until the presentation is over or
            // the user has requested an exit (by pressing ESC)
            while(present_over(file) && !requested_exit) {
                // Wait for an event
                if(display_fetch_event(disp, &ev)) {
                    switch(ev) {
                        case DISPEV_PREV:
                            present_seek(file, -1);
                            break;
                        case DISPEV_NEXT:
                            present_seek(file, 1);
                            break;
                        case DISPEV_START:
                            present_seek_to(file, 0);
                            break;
                        case DISPEV_END:
                            present_seek_to(file, -1);
                            break;
                        case DISPEV_EXIT:
                            requested_exit = true;
                            break;
                    }
                    // Only re-render if something happened
                    present_fill_render_queue(file, &rq);
                    // Display render queue
                    display_render_queue(disp, rq);
                    // Clear render queue
                    rq_clear(rq);
                }
            }
            rq_free(rq);
            display_close(disp);
        }
        present_close(file);
    }
}

int main(int argc, char** argv) {
    if(argc == 2) {
        render_loop(argv[1]);
    } else {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
    }
    return 0;
}
