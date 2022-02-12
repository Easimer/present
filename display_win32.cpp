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

// Pseudo-event to unblock display_fetch_event after startup and draw the title slide
// Posted in display_open()
// WPARAM and LPARAM are always zero.
#define WM_JUMPSTART (WM_USER + 0x0000)

#define HOTKEY_FOCUS (0)
#define HOTKEY_FOCUS_VK (0x46) // F key
#define HOTKEY_FOCUS_FLAGS (MOD_SHIFT | MOD_ALT)

struct Display {
    HWND wnd;
    
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    float s_width, s_height;
    
    HPEN penInvis;
    HPEN penBlack;
    
    // For WndProc
    HDC backdc;
    Display_Event* ev_out;
    bool ev_res;
    Render_Queue* rq;
};

void Display_ExecuteCommandLine(Display* disp, const char* cmdline);

static void ProcessRenderQueue(Display* disp, HWND hWnd, HDC hDC, const RECT* rClient, const Render_Queue* rq) {
    assert(rClient && rq);
    
    Mem_Arena_Offset hCur = rq->commands;
    HFONT fntCurrent = NULL;
    int font_size = 0;
    const char* font_name = NULL;
    // clear bg
    SelectObject(hDC, disp->penInvis);
    Rectangle(hDC, 0, 0, rClient->right, rClient->bottom);
    
    wchar_t* text_buffer = (wchar_t*)malloc(8192 * sizeof(wchar_t)); // ExtTextOut has a maximum string length of 8192
    SetBkMode(hDC, TRANSPARENT);
    SetTextAlign(hDC, TA_BOTTOM);
    
    while(hCur != MEM_ARENA_INVALID_OFFSET) {
        auto* cur = (RQ_Draw_Cmd*)Arena_Resolve(rq->mem, hCur);

        switch(cur->cmd) {
            case RQCMD_DRAW_TEXT: {
                RQ_Draw_Text* dtxt = (RQ_Draw_Text*)cur;
                int r = (int)(dtxt->color.r * 255); int g = (int)(dtxt->color.g * 255);
                int b = (int)(dtxt->color.b * 255);
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
                RQ_Draw_Image* dimg = (RQ_Draw_Image*)cur;
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
                
                SetStretchBltMode(hDC, HALFTONE);
                float aspect = dimg->h / dimg->w;
                int destW = (int)(disp->s_width * dimg->w);
                int destH = (int)(disp->s_height * aspect);
                StretchBlt(hDC, x, y, destW, destH, hDibDC, 0, 0, dimg->width, dimg->height, SRCCOPY);

                ReleaseDC(disp->wnd, hDibDC);
                break;
            }
            case RQCMD_DRAW_RECTANGLE: {
                RQ_Draw_Rect* drect = (RQ_Draw_Rect*)cur;
                int r = (int)(drect->color.r * 255); int g = (int)(drect->color.g * 255);
                int b = (int)(drect->color.b * 255); //int a = (int)(drect->a * 255);
                int x0 = (int)(drect->x0 * disp->s_width);
                int y0 = (int)(drect->y0 * disp->s_height);
                int x1 = (int)(drect->x1 * disp->s_width);
                int y1 = (int)(drect->y1 * disp->s_height);
                HBRUSH brRect = CreateSolidBrush(RGB(r, g, b));
                SelectObject(hDC, brRect);
                SelectObject(hDC, disp->penInvis);
                
                // NOTE(easimer): as per MSDN: "The rectangle that is drawn excludes the bottom and right edges." Thus we need to add +1 to the bottom-right coordinate
                Rectangle(hDC, x0, y0, x1 + 1, y1 + 1);
                DeleteObject(brRect);
                break;
            }
            default: {
                fprintf(stderr, "Unknown render command type '%d'\n", cur->cmd);
                break;
            }
        }
        hCur = cur->next;
    }
    if(fntCurrent) {
        DeleteObject(fntCurrent);
    }
    free(text_buffer);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Display* disp = (Display*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
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
                ProcessRenderQueue(disp, hWnd, hdc, &r, disp->rq);
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
                    case 'E':
                    *disp->ev_out = DISPEV_EXEC;
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
        case WM_HOTKEY: {
            auto flags = LOWORD(lParam);
            auto vk = HIWORD(lParam);
            printf("win32: received hotkey event %x %x\n", vk, flags);
            if (vk == HOTKEY_FOCUS_VK && ((flags & HOTKEY_FOCUS_FLAGS) == HOTKEY_FOCUS_FLAGS)) {
                if(disp->ev_out) {
                    disp->ev_res = true;
                    *disp->ev_out = DISPEV_FOCUS;
                }
            }
            return 0;
        }
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

struct Monitor_Enumeration_Result {
    bool headless = true;
    HMONITOR hMonitor = NULL;
    RECT rect;
};

static BOOL CALLBACK MonitorEnumerationProc(HMONITOR hMonitor, HDC hDC, LPRECT rect, LPARAM lParam) {
    MONITORINFO minf;
    minf.cbSize = sizeof(minf);
    auto res = (Monitor_Enumeration_Result*)lParam;
    
    res->headless = false;
    assert(hMonitor != NULL);
    
    if (GetMonitorInfoA(hMonitor, &minf)) {
        if ((minf.dwFlags & MONITORINFOF_PRIMARY) == 0) {
            // Found a non-primary display
            res->hMonitor = hMonitor;
            res->rect = minf.rcMonitor;
            fprintf(stderr, "display_win32: found a secondary display (%d %d %d %d)\n", res->rect.left, res->rect.top, res->rect.right, res->rect.bottom);
        } else {
            if (!res->hMonitor) {
                // Set the display to this one (a primary one), only if we haven't found an
                // other display yet
                res->hMonitor = hMonitor;
                res->rect = minf.rcMonitor;
                fprintf(stderr, "display_win32: found the primary display (%d %d %d %d)\n", res->rect.left, res->rect.top, res->rect.right, res->rect.bottom);
            }
        }
    } else {
        fprintf(stderr, "display_win32: GetMonitorInfo has failed\n");
    }
    
    return TRUE;
}

struct Display* Display_Open() {
    Display* ret = NULL;
    WNDCLASSA wc = {0};
    LONG win_x = -1, win_y = -1, win_w = -1, win_h = -1;
    Monitor_Enumeration_Result monenum;
    
    if (EnumDisplayMonitors(NULL, NULL, MonitorEnumerationProc, (LPARAM)&monenum)) {
        if (!monenum.headless && monenum.hMonitor) {
            win_x = monenum.rect.left;
            win_y = monenum.rect.top;
            win_w = monenum.rect.right - monenum.rect.left;
            win_h = monenum.rect.bottom - monenum.rect.top;
            ret = (Display*)malloc(sizeof(Display));
        } else {
            fprintf(stdout, "display_win32: couldn't find displays\n");
        }
    } else {
        fprintf(stdout, "display_win32: failed to enumerate displays\n");
    }
    
    if(ret) {
        fprintf(stdout, "display_win32: [%d %d %d %d]\n", win_x, win_y, win_w, win_h);
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
        ret->s_width = (float)win_w;
        ret->s_height = (float)win_h;
        ret->wnd = CreateWindowExA(0, "PresentWindow", "Present",
                                   WS_POPUP | WS_VISIBLE,
                                   win_x, win_y, win_w, win_h,
                                   NULL, NULL, wc.hInstance, ret);
        ShowWindow(ret->wnd, SW_SHOW);
        UpdateWindow(ret->wnd);
        ret->penBlack = CreatePen(PS_SOLID, 1, RGB(0, 0, 0));
        ret->penInvis = CreatePen(PS_NULL, 1, RGB(0, 0, 0));

        RegisterHotKey(ret->wnd, HOTKEY_FOCUS, HOTKEY_FOCUS_FLAGS, HOTKEY_FOCUS_VK);

        PostMessage(ret->wnd, WM_JUMPSTART, 0, 0);
    }
    
    return ret;
}

void Display_Close(Display* disp) {
    if(disp) {
        DeleteObject(disp->penInvis);
        DeleteObject(disp->penBlack);
        GdiplusShutdown(disp->gdiplusToken);
        DestroyWindow(disp->wnd);
    }
}

bool Display_FetchEvent(Display* disp, Display_Event& out) {
    MSG msg;
    bool ret = false;
    
    if(disp) {
        disp->ev_res = false;
        disp->ev_out = &out;
        if(GetMessageA(&msg, disp->wnd, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        ret = disp->ev_res;
    }
    return ret;
}

void Display_RenderQueue(Display* disp, Render_Queue* rq) {
    MSG msg = {0};
    
    if(disp && rq) {
        assert(!disp->rq);
        disp->rq = rq;
        InvalidateRect(disp->wnd, NULL, TRUE);
        UpdateWindow(disp->wnd);
        disp->rq = NULL;
    }
}

bool Display_SwapRedBlueChannels() {
    return true;
}

void Display_Focus(Display* display) {
    if(!display) {
        return;
    }

    HWND hCurWnd = GetForegroundWindow();
    DWORD dwMyID = GetCurrentThreadId();
    DWORD dwCurID = GetWindowThreadProcessId(hCurWnd, NULL);
    AttachThreadInput(dwCurID, dwMyID, TRUE);
    SetWindowPos(display->wnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    SetWindowPos(display->wnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOMOVE);
    SetForegroundWindow(display->wnd);
    SetFocus(display->wnd);
    SetActiveWindow(display->wnd);
    AttachThreadInput(dwCurID, dwMyID, FALSE);
}
