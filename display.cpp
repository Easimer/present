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

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <xcb/xcb.h>
#include <xcb/xcb_image.h>
#include "display.h"

struct display {
    xcb_connection_t* conn;
    xcb_screen_t* scr;
    xcb_drawable_t wnd;
    xcb_gcontext_t ctx; // black
    xcb_gcontext_t ctx_white;
    xcb_gcontext_t fontgc;
    uint16_t s_width, s_height;
};

display* display_open() {
    const char* font_name = "-misc-fixed-*-*-*-*-20-*-*-*-*-*-iso8859-2";
    display* ret = NULL;
    unsigned int values[3] = {0, 0};
    xcb_connection_t* conn;
    xcb_screen_t* scr;
    xcb_drawable_t wnd;
    xcb_gcontext_t fg, fontgc;
    xcb_font_t font;
    unsigned int mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;

    ret = (display*)malloc(sizeof(display));
    if(ret) {
        conn = xcb_connect(NULL, NULL);
        scr = xcb_setup_roots_iterator(xcb_get_setup(conn)).data;
        wnd = scr->root;
        fg = xcb_generate_id(conn);
        values[0] = scr->black_pixel;
        xcb_create_gc(conn, fg, wnd, mask, values);
        values[0] = scr->white_pixel;
        ret->ctx_white = xcb_generate_id(conn);
        xcb_create_gc(conn, ret->ctx_white, wnd, mask, values);

        wnd = xcb_generate_id(conn);
        mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        values[0] = scr->white_pixel;
        values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_KEY_RELEASE;

        xcb_create_window(conn, XCB_COPY_FROM_PARENT, wnd, scr->root,
                0, 0, 1280, 720, 2, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                scr->root_visual, mask, values);
        // TODO: implement fullscreen and ask xcb for display resolution

        // https://vincentsanders.blogspot.com/2010/04/xcb-programming-is-hard.html
        xcb_map_window(conn, wnd);
        xcb_flush(conn);

        // load a font
        font = xcb_generate_id(conn);
        xcb_open_font(conn, font, strlen(font_name), font_name);
        fontgc = xcb_generate_id(conn);
        mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
        values[0] = scr->black_pixel;
        values[1] = scr->white_pixel;
        values[2] = font;
        xcb_create_gc(conn, fontgc, wnd, mask, values);
        xcb_close_font(conn, font);

        ret->conn = conn;
        ret->wnd = wnd;
        ret->scr = scr;
        ret->ctx = fg;
        ret->fontgc = fontgc;
        ret->s_width = 1280;
        ret->s_height = 720;
    }

    return ret;
}

void display_close(display* disp) {
    assert(disp);
    if(disp) {
        if(disp->conn) {
            xcb_disconnect(disp->conn);
        }
        free(disp);
    }
}

bool display_fetch_event(display* disp, display_event* out) {
    bool ret = false;
    xcb_generic_event_t *e;
    assert(disp && out && disp->conn);
    if(disp && out) {
        e = xcb_wait_for_event(disp->conn);
        if(e) {
            switch(e->response_type & ~0x80) {
                case XCB_EXPOSE:
                    *out = DISPEV_NONE; // force redraw
                    ret = true;
                    break;
                case XCB_BUTTON_RELEASE: {
                    xcb_button_release_event_t *ev = (xcb_button_release_event_t *)e;
                    ret = true;
                    if(ev->detail == 1) {
                        *out = DISPEV_NEXT;
                    } else if(ev->detail == 3) {
                        *out = DISPEV_PREV;
                    } else {
                        ret = false;
                    }
                    break;
                }
                case XCB_KEY_RELEASE: {
                    xcb_key_release_event_t *ev = (xcb_key_release_event_t *)e;
                    ret = true;
                    switch(ev->detail) {
                        case 9: // ESC
                            *out = DISPEV_EXIT;
                            break;
                        case 65: // SPACE
                        case 114: // right cursor
                            *out = DISPEV_NEXT;
                            break;
                        case 113: // left cursor
                            *out = DISPEV_PREV;
                            break;
                        case 112: // Page up
                            *out = DISPEV_END;
                            break;
                        case 117: // Page down
                            *out = DISPEV_START;
                            break;
                        default:
                            ret = false;
                            break;
                    }
                    break;
                }
            }
        }
        free(e);
    }
    return ret;
}

static xcb_format_t* find_format(xcb_connection_t * c, uint8_t depth, uint8_t bpp)
{
    const xcb_setup_t *setup = xcb_get_setup(c);
    xcb_format_t *fmt = xcb_setup_pixmap_formats(setup);
    xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length(setup);
    for(; fmt != fmtend; ++fmt)
        if((fmt->depth == depth) && (fmt->bits_per_pixel == bpp)) {
            return fmt;
        }
    return 0;
}

void display_render_queue(display* disp, render_queue* rq) {
    assert(disp && rq && disp->conn);
    if(disp && rq && disp->conn) {
        xcb_rectangle_t rectangle = { 0, 0, disp->s_width, disp->s_height};
        rq_draw_cmd* cur = rq->commands;

        xcb_poly_fill_rectangle(disp->conn, disp->wnd, disp->ctx_white, 1, &rectangle);

        xcb_format_t* fmt = find_format(disp->conn, 24, 32);
        const xcb_setup_t *setup = xcb_get_setup(disp->conn);

        while(cur) {
            switch(cur->cmd) {
                case RQCMD_DRAW_TEXT: {
                    rq_draw_text* dtxt = (rq_draw_text*)cur;
                    xcb_image_text_8(disp->conn, dtxt->text_len, disp->wnd, disp->fontgc,
                            dtxt->x, dtxt->y,
                            dtxt->text);
                    break;
                }
                case RQCMD_DRAW_IMAGE: {
                    rq_draw_image* dimg = (rq_draw_image*)cur;
                    xcb_image_t* ximg;
                    ximg = xcb_image_create(dimg->width, dimg->height,
                            XCB_IMAGE_FORMAT_Z_PIXMAP, fmt->scanline_pad,
                            fmt->depth, fmt->bits_per_pixel, 0, (xcb_image_order_t)setup->image_byte_order,
                            XCB_IMAGE_ORDER_LSB_FIRST, NULL,
                            dimg->width * dimg->height * 4, (uint8_t*)dimg->buffer);
                    xcb_image_put(disp->conn, disp->wnd, disp->ctx, ximg, dimg->x, dimg->y, 0);
                    xcb_image_destroy(ximg);
                    break;
                }
                case RQCMD_DRAW_RECTANGLE: {
                    xcb_poly_fill_rectangle(disp->conn, disp->wnd, disp->ctx, 1, &rectangle);
                    break;
                }
                default:
                    break;
            }
            cur = cur->next;
        }
        xcb_flush(disp->conn);
    }
}
