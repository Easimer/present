#include <stdlib.h>
#include <assert.h>
#pragma warning(push)
#pragma warning(disable : 4458)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;
#pragma warning(pop)
#include "display.h"

#define WM_JUMPSTART (WM_USER + 0x0000)

struct display {
    HWND wnd;
    
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    float s_width, s_height;
    
    HPEN penInvis;
    HPEN penBlack;
    
    // For WndProc
    HDC backdc;
    display_event* ev_out;
    bool ev_res;
    render_queue* rq;
};

static void process_render_queue(display* disp, HWND hWnd, HDC hDC, const RECT* rClient, const render_queue* rq) {
    assert(rClient && rq);
    
    rq_draw_cmd* cur = rq->commands;
    HFONT fntCurrent = NULL;
    int font_size = 0;
    const char* font_name = NULL;
    // clear bg
    SelectObject(hDC, disp->penInvis);
    Rectangle(hDC, 0, 0, rClient->right, rClient->bottom);
    
    wchar_t* text_buffer = (wchar_t*)malloc(8192 * sizeof(wchar_t)); // ExtTextOut has a maximum string length of 8192
    SetBkMode(hDC, TRANSPARENT);
    SetTextAlign(hDC, TA_BOTTOM);
    
    while(cur) {
        switch(cur->cmd) {
            case RQCMD_DRAW_TEXT: {
                rq_draw_text* dtxt = (rq_draw_text*)cur;
                int r = (int)(dtxt->r * 255); int g = (int)(dtxt->g * 255);
                int b = (int)(dtxt->b * 255);
                int x = (int)(dtxt->x * disp->s_width);
                int y = (int)(dtxt->y * disp->s_height);
                int size = (int)(dtxt->size * disp->s_height);
                size_t wlen = mbstowcs(text_buffer, dtxt->text, 8192);
                
                // NOTE(easimer): dtxt->font_name is kinda unique so it's OK to compare pointers here
                if(!fntCurrent || (font_size != size && font_name != dtxt->font_name)) {
                    font_size = size;
                    if(fntCurrent) {
                        DeleteObject(fntCurrent);
                    }
                    const char* ffn = dtxt->font_name ? dtxt->font_name : "Calibri";
                    fntCurrent = CreateFontA(font_size, 0, 0, 0, FW_NORMAL,
                                             FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                                             CLEARTYPE_QUALITY, DEFAULT_PITCH,
                                             ffn);
                }
                SelectObject(hDC, fntCurrent);
                SetTextColor(hDC, RGB(r, g, b));
                
                ExtTextOutW(hDC, x, y, ETO_OPAQUE, NULL, text_buffer, (UINT)wlen, NULL);
                break;
            }
            case RQCMD_DRAW_IMAGE: {
                rq_draw_image* dimg = (rq_draw_image*)cur;
                int x = (int)(dimg->x * disp->s_width);
                int y = (int)(dimg->y * disp->s_height);
                int w = (int)(dimg->width);
                int h = (int)(dimg->height);
                // upload image to GDI
                BITMAPINFO bmpinf = {0};
                //memset(bmpinf, 0, sizeof(bmpinf));
                bmpinf.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmpinf.bmiHeader.biWidth = dimg->width;
                bmpinf.bmiHeader.biHeight = -dimg->height;
                bmpinf.bmiHeader.biPlanes = 1;
                bmpinf.bmiHeader.biBitCount = 32;
                bmpinf.bmiHeader.biCompression = BI_RGB;
                void* buffer = NULL;
                HBITMAP hDib = CreateDIBSection(hDC, &bmpinf,
                                                DIB_RGB_COLORS, &buffer,
                                                0, 0);
                if(buffer == NULL) {
                    DebugBreak();
                }
                HDC hDibDC = CreateCompatibleDC(hDC);
                SelectObject(hDibDC, hDib);
                
                // TODO(easimer): what about stride?
                memcpy(buffer, dimg->buffer, dimg->width * dimg->height * 4);
                
                (void)x, (void)y, (void)w, (void)h;
                
                BitBlt(hDC, x, y, w, h, hDibDC, 0, 0, SRCCOPY);
                
                
                ReleaseDC(disp->wnd, hDibDC);
                break;
            }
            case RQCMD_DRAW_RECTANGLE: {
                rq_draw_rect* drect = (rq_draw_rect*)cur;
                int r = (int)(drect->r * 255); int g = (int)(drect->g * 255);
                int b = (int)(drect->b * 255); //int a = (int)(drect->a * 255);
                int x0 = (int)(drect->x0 * disp->s_width);
                int y0 = (int)(drect->y0 * disp->s_height);
                int x1 = (int)(drect->x1 * disp->s_width);
                int y1 = (int)(drect->y1 * disp->s_height);
                HBRUSH brRect = CreateSolidBrush(RGB(r, g, b));
                SelectObject(hDC, brRect);
                SelectObject(hDC, disp->penInvis);
                Rectangle(hDC, x0, y0, x1, y1);
                DeleteObject(brRect);
                break;
            }
        }
        cur = cur->next;
    }
    if(fntCurrent) {
        DeleteObject(fntCurrent);
    }
    free(text_buffer);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    display* disp = (display*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    switch(uMsg) {
        case WM_CREATE: {
            CREATESTRUCT *pCreate = (CREATESTRUCT*)lParam;
            SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pCreate->lpCreateParams);
            break;
        }
        case WM_DESTROY: {
            PostQuitMessage(0);
            break;
        }
        case WM_QUIT: {
            if(disp->ev_out) {
                disp->ev_res = true;
                *disp->ev_out = DISPEV_EXIT;
            }
            break;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            RECT r;
            HDC hdc;
            if(disp->rq) {
                hdc = BeginPaint(disp->wnd, &ps);
                GetClientRect(disp->wnd, &r);
                process_render_queue(disp, hWnd, hdc, &r, disp->rq);
                EndPaint(disp->wnd, &ps);
            }
            break;
        }
        case WM_KEYUP: {
            if(disp->ev_out) {
                disp->ev_res = true;
                switch (wParam) {
                    case VK_LEFT:
                    *disp->ev_out = DISPEV_PREV;
                    break;
                    case VK_RIGHT:
                    case VK_SPACE:
                    *disp->ev_out = DISPEV_NEXT;
                    break;
                    case VK_PRIOR:
                    *disp->ev_out = DISPEV_START;
                    break;
                    case VK_NEXT:
                    *disp->ev_out = DISPEV_END;
                    break;
                    case VK_ESCAPE:
                    *disp->ev_out = DISPEV_EXIT;
                    break;
                    default:
                    disp->ev_res = false;
                    break;
                }
            }
            break;
        }
        case WM_SIZE: {
            if(disp->ev_out) {
                disp->ev_res = true;
                *disp->ev_out = DISPEV_NONE; // force redraw
            }
            disp->s_width = LOWORD(lParam);
            disp->s_height = HIWORD(lParam);
            break;
        }
        case WM_JUMPSTART: {
            if(disp->ev_out) {
                disp->ev_res = true;
                *disp->ev_out = DISPEV_NONE; // force redraw
            }
            break;
        }
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

struct display* display_open() {
    display* ret = (display*)malloc(sizeof(display));
    WNDCLASSA wc = {0};
    int screen_width, screen_height;
    
    if(ret) {
        ret->rq = NULL;
        ret->ev_out = NULL;
        
        GdiplusStartup(&ret->gdiplusToken, &ret->gdiplusStartupInput, NULL);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpszClassName = "PresentWindow";
        wc.hInstance = GetModuleHandle(NULL);
        wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
        wc.lpszMenuName = NULL;
        wc.lpfnWndProc = &WndProc;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        RegisterClassA(&wc);
        screen_width = GetSystemMetrics(SM_CXSCREEN);
        screen_height = GetSystemMetrics(SM_CYSCREEN);
        ret->s_width = (float)screen_width;
        ret->s_height = (float)screen_height;
        ret->wnd = CreateWindowExA(0, "PresentWindow", "Present",
                                   WS_POPUP | WS_VISIBLE,
                                   0, 0, screen_width, screen_height,
                                   NULL, NULL, wc.hInstance, ret);
        ShowWindow(ret->wnd, SW_SHOW);
        UpdateWindow(ret->wnd);
        ret->penBlack = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        ret->penInvis = CreatePen(PS_NULL, 1, RGB(0, 0, 0));
        
        PostMessage(ret->wnd, WM_JUMPSTART, 0, 0);
    }
    
    return ret;
}

void display_close(display* disp) {
    if(disp) {
        DeleteObject(disp->penInvis);
        DeleteObject(disp->penBlack);
        GdiplusShutdown(disp->gdiplusToken);
        DestroyWindow(disp->wnd);
    }
}

bool display_fetch_event(display* disp, display_event* out) {
    MSG msg;
    bool ret = false;
    
    if(disp) {
        disp->ev_res = false;
        disp->ev_out = out;
        if(PeekMessageA(&msg, disp->wnd, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        ret = disp->ev_res;
    }
    return ret;
}

void display_render_queue(display* disp, render_queue* rq) {
    MSG msg = {0};
    
    if(disp && rq) {
        assert(!disp->rq);
        disp->rq = rq;
        InvalidateRect(disp->wnd, NULL, TRUE);
        UpdateWindow(disp->wnd);
        disp->rq = NULL;
    }
}

bool display_swap_red_blue_channels() {
    return true;
}