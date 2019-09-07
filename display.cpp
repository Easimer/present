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
#include <xcb/xcb_ewmh.h>
#include <cairo.h>
#include <cairo-xcb.h>
#include "display.h"

struct display {
    xcb_connection_t* conn;
    xcb_screen_t* scr;
    xcb_drawable_t wnd;
    xcb_gcontext_t ctx; // black
    xcb_gcontext_t fontgc;
    uint16_t s_width, s_height;

    cairo_surface_t* surf;
    cairo_t* cr;
};

static xcb_visualtype_t *find_visual(xcb_connection_t *c, xcb_visualid_t visual)
{
    xcb_screen_iterator_t screen_iter = xcb_setup_roots_iterator(xcb_get_setup(c));

    for (; screen_iter.rem; xcb_screen_next(&screen_iter)) {
        xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(screen_iter.data);
        for (; depth_iter.rem; xcb_depth_next(&depth_iter)) {
            xcb_visualtype_iterator_t visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
            for (; visual_iter.rem; xcb_visualtype_next(&visual_iter))
                if (visual == visual_iter.data->visual_id)
                    return visual_iter.data;
        }
    }

    return NULL;
}

display* display_open() {
    const char* font_name = "-misc-fixed-*-*-*-*-20-*-*-*-*-*-iso8859-2";
    display* ret = NULL;
    unsigned int values[3] = {0, 0};
    xcb_connection_t* conn;
    xcb_screen_t* scr;
    xcb_drawable_t wnd;
    xcb_gcontext_t fg, fontgc;
    xcb_font_t font;
    xcb_visualtype_t* visual;
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

        wnd = xcb_generate_id(conn);
        mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
        values[0] = scr->white_pixel;
        values[1] = XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_KEY_RELEASE |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY;

        xcb_create_window(conn, XCB_COPY_FROM_PARENT, wnd, scr->root,
                0, 0, 1280, 720, 2, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                scr->root_visual, mask, values);
        // TODO: implement fullscreen and ask xcb for display resolution

        // https://vincentsanders.blogspot.com/2010/04/xcb-programming-is-hard.html
        xcb_map_window(conn, wnd);
        xcb_flush(conn);

        visual = find_visual(conn, scr->root_visual);

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

        // send fullscreen hint
        xcb_ewmh_connection_t EWMH;
        xcb_intern_atom_cookie_t* cookie = xcb_ewmh_init_atoms(conn, &EWMH);
        if(xcb_ewmh_init_atoms_replies(&EWMH, cookie, NULL)) {
            xcb_change_property(conn, XCB_PROP_MODE_REPLACE, wnd,
                    EWMH._NET_WM_STATE, XCB_ATOM_ATOM, 32, 1, &(EWMH._NET_WM_STATE_FULLSCREEN));
        }

        ret->conn = conn;
        ret->wnd = wnd;
        ret->scr = scr;
        ret->ctx = fg;
        ret->fontgc = fontgc;
        ret->s_width = 1280;
        ret->s_height = 720;

        ret->surf = cairo_xcb_surface_create(conn, wnd, visual, ret->s_width, ret->s_height);
        ret->cr = cairo_create(ret->surf);
    }

    return ret;
}

void display_close(display* disp) {
    assert(disp);
    if(disp) {
        if(disp->conn) {
            cairo_destroy(disp->cr);
            cairo_surface_finish(disp->surf);
            cairo_surface_destroy(disp->surf);

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
                case XCB_CONFIGURE_NOTIFY: {
                    xcb_configure_notify_event_t* ev = (xcb_configure_notify_event_t*)e;
                    cairo_xcb_surface_set_size(disp->surf, ev->width, ev->height);
                    disp->s_width = ev->width;
                    disp->s_height = ev->height;
                    break;
                }
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

void display_render_queue(display* disp, render_queue* rq) {
    assert(disp && rq && disp->conn);
    if(disp && rq && disp->conn) {
        rq_draw_cmd* cur = rq->commands;

        // clear backbuf
        cairo_set_source_rgb(disp->cr, 1, 1, 1);
        cairo_paint(disp->cr);

        // setup text drawing
        cairo_select_font_face(disp->cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_source_rgb(disp->cr, 0, 0, 0);

        while(cur) {
            switch(cur->cmd) {
                case RQCMD_DRAW_TEXT: {
                    rq_draw_text* dtxt = (rq_draw_text*)cur;
                    cairo_set_font_size(disp->cr, dtxt->size * disp->s_height);
                    cairo_move_to(disp->cr, dtxt->x * disp->s_width, dtxt->y * disp->s_height);
                    cairo_set_source_rgba(disp->cr, dtxt->r, dtxt->g, dtxt->b, dtxt->a);
                    cairo_show_text(disp->cr, dtxt->text);
                    break;
                }
                case RQCMD_DRAW_IMAGE: {
                    rq_draw_image* dimg = (rq_draw_image*)cur;
                        cairo_surface_t* imgsurf;
                    cairo_save(disp->cr);
                    imgsurf = cairo_image_surface_create_for_data(
                            (unsigned char*)dimg->buffer,
                            CAIRO_FORMAT_ARGB32,
                            dimg->width, dimg->height,
                            dimg->width * 4);
                    cairo_set_source_surface(disp->cr, imgsurf, dimg->x * disp->s_width, dimg->y * disp->s_height);
                    cairo_paint(disp->cr);
                    cairo_surface_destroy(imgsurf);
                    cairo_restore(disp->cr);
                    break;
                }
                case RQCMD_DRAW_RECTANGLE: {
                    rq_draw_rect* drect = (rq_draw_rect*)cur;
                    int x, y, w, h;
                    x = drect->x0 * disp->s_width;
                    y = drect->y0 * disp->s_height;
                    w = drect->x1 * disp->s_width - x;
                    h = drect->y1 * disp->s_height - y;
                    cairo_set_source_rgba(disp->cr, drect->r, drect->g, drect->b, drect->a);
                    cairo_rectangle(disp->cr, x, y, w, h);
                    cairo_fill(disp->cr);
                    break;
                }
                default:
                    break;
            }
            cur = cur->next;
        }
        cairo_surface_flush(disp->surf);
        xcb_flush(disp->conn);
    }
}

bool display_swap_red_blue_channels() {
    return true;
}
