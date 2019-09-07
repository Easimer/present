#include <stdlib.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include "display.h"

struct display {
    HWND wnd;
    
    // For WndProc
    display_event* ev_out;
    bool ev_res;
};

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
        case WM_PAINT: {
            if (disp->ev_out) {
                disp->ev_res = true;
                *disp->ev_out = DISPEV_NONE; // Force redraw
            }
            break;
        }
        case WM_KEYUP: {
            if(disp->ev_out) {
                disp->ev_res = true;
                *disp->ev_out = DISPEV_NEXT;
            }
            break;
        }
    }
    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

struct display* display_open() {
    display* ret = (display*)malloc(sizeof(display));
    WNDCLASSA wc = {0};
    
    if(ret) {
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpszClassName = "PresentWindow";
        wc.hInstance = GetModuleHandle(NULL);
        wc.hbrBackground = GetSysColorBrush(COLOR_3DFACE);
        wc.lpszMenuName = NULL;
        wc.lpfnWndProc = &WndProc;
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        RegisterClassA(&wc);
        ret->wnd = CreateWindowExA(0, "PresentWindow", "Present",
                                   WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                   0, 0, 1280, 720, NULL, NULL, wc.hInstance, ret);
        ShowWindow(ret->wnd, SW_SHOW);
        UpdateWindow(ret->wnd);
    }
    
    return ret;
}

void display_close(display* disp) {
    if(disp) {
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
    PAINTSTRUCT ps;
    RECT r;
    HDC hdc;
    
    if(disp && rq) {
        GetClientRect(disp->wnd, &r);
        if(r.bottom == 0) {
            return;
        }
        
        hdc = BeginPaint(disp->wnd, &ps);
        
        SetPixel(hdc, 0, 0, RGB(255, 0, 0));
        
        EndPaint(disp->wnd, &ps);
    }
}

bool display_swap_red_blue_channels() {
    return false;
}