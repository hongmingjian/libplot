//
// cl.exe /EHsc /utf-8 /Zi /I. /DUNICODE /D_UNICODE testlibplot.cpp /link /DEBUG /SUBSYSTEM:WINDOWS user32.lib gdi32.lib
//
#include <windows.h>
#include <vector>
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdio>
#include <iostream>

#include "libplot.h"

typedef void* (*FuncCreateFigure)(int);
typedef void  (*FuncDestroyFigure)(void*);
//typedef int   (*FuncAddCurve)(void*, const float*, const float*, int);
typedef int   (*FuncSetOption)(void*, const char*, const char*);
typedef void  (*FuncPan)(void*, int, int);
typedef void  (*FuncZoom)(void*, int, int, float);
typedef void  (*FuncPixel2Logical)(void *, int, int, float*, float*, float*);
typedef int   (*FuncShow)(void*, struct draw_command *, int);

HMODULE            hPlotDll         = nullptr;
FuncCreateFigure   pfnCreateFigure  = nullptr;
FuncDestroyFigure  pfnDestroyFigure = nullptr;
//FuncPlot           pfnPlot          = nullptr;
FuncSetOption      pfnSetOption     = nullptr;
FuncPan            pfnPan           = nullptr;
FuncZoom           pfnZoom          = nullptr;
FuncPixel2Logical  pfnPixel2Logical = nullptr;
FuncShow           pfnShow          = nullptr;

int g_canvasWidth  = 550;
int g_canvasHeight = 450;
void *g_fig = nullptr;
static bool g_isPanning = false;  // 是否正在拖动
static POINT g_lastMousePos;       // 记录上一次鼠标的像素坐标

COLORREF convertColor(const char* str) {
    if (!str || strlen(str) == 0)
		return RGB(0, 0, 0);
    std::string s(str);
    s.erase(0, s.find_first_not_of(" "));
    s.erase(s.find_last_not_of(" ") + 1);
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "red")     return RGB(255, 0, 0);
    if (s == "green")   return RGB(0, 128, 0);
    if (s == "blue")    return RGB(0, 0, 255);
    if (s == "black")   return RGB(0, 0, 0);
    if (s == "white")   return RGB(255, 255, 255);
    if (s == "gray")    return RGB(128, 128, 128);
    if (s == "yellow")  return RGB(255, 255, 0);

    if (!s.empty() && s[0] == '#')
		s = s.substr(1);
    if (s.length() == 6) {
        unsigned long colorVal = strtoul(s.c_str(), NULL, 16);
        return RGB((colorVal >> 16) & 0xFF, (colorVal >> 8) & 0xFF, colorVal & 0xFF);
    }
    return RGB(0, 0, 0);
}

int convertLineStyle(const char* styleStr) {
    std::string s(styleStr);
    if (s == "-" || s == "solid")     return PS_SOLID;
    if (s == "--" || s == "dash")     return PS_DASH;
    if (s == ":" || s == "dot")       return PS_DOT;
    if (s == "-." || s == "dashdot")   return PS_DASHDOT;
    return PS_SOLID;
}

HPEN CreateGDIRenderPen(int style, int width, COLORREF color) {
    if (style == PS_SOLID)
		return CreatePen(PS_SOLID, width, color);
    LOGBRUSH lb = { BS_SOLID, color, 0 };
    return ExtCreatePen(PS_GEOMETRIC | style, width, &lb, 0, NULL);
}

void ExecuteDrawCommands(HDC hdc, std::vector<struct draw_command>& cmds) {
    std::sort(cmds.begin(), cmds.end(), [](const struct draw_command& a, const struct draw_command& b) {
        return a.zIndex < b.zIndex;
    });

    COLORREF curColor = RGB(0, 0, 0);
    int curLineStyle = PS_SOLID;
    int curLineWidth = 1;

    HPEN hCurPen = CreateGDIRenderPen(curLineStyle, curLineWidth, curColor);
    HPEN hOldPen = (HPEN)SelectObject(hdc, hCurPen);
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
    HFONT hCurFont = NULL;
    HFONT hOldFont = (HFONT)GetCurrentObject(hdc, OBJ_FONT);

    for (const auto& cmd : cmds) {
        switch (cmd.op) {
        case OP_SET_TEXT_COLOR:
            SetTextColor(hdc, convertColor(cmd.argc1));
            break;
        case OP_SET_LINE_COLOR:
            curColor = convertColor(cmd.argc1);
            SelectObject(hdc, hOldPen);
			DeleteObject(hCurPen);
            hCurPen = CreateGDIRenderPen(curLineStyle, curLineWidth, curColor);
            hOldPen = (HPEN)SelectObject(hdc, hCurPen);
            break;
        case OP_SET_LINE_WIDTH:
            curLineWidth = (int)cmd.argf1;
			if (curLineWidth < 1)
				curLineWidth = 1;
            SelectObject(hdc, hOldPen);
			DeleteObject(hCurPen);
            hCurPen = CreateGDIRenderPen(curLineStyle, curLineWidth, curColor);
            hOldPen = (HPEN)SelectObject(hdc, hCurPen);
            break;
        case OP_SET_LINE_STYLE:
            curLineStyle = convertLineStyle(cmd.argc1);
            SelectObject(hdc, hOldPen);
			DeleteObject(hCurPen);
            hCurPen = CreateGDIRenderPen(curLineStyle, curLineWidth, curColor);
            hOldPen = (HPEN)SelectObject(hdc, hCurPen);
            break;
        case OP_SET_TEXT_FONT:
            if (hCurFont) {
				SelectObject(hdc, hOldFont);
				DeleteObject(hCurFont);
			}
#ifdef UNICODE
            wchar_t wFontName[64]; MultiByteToWideChar(CP_ACP, 0, cmd.argc1, -1, wFontName, 64);
            hCurFont = CreateFontW(-(int)cmd.argf1, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, wFontName);
#else
            hCurFont = CreateFontA(-(int)cmd.argf1, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET, 0, 0, 0, 0, cmd.argc1);
#endif
            hOldFont = (HFONT)SelectObject(hdc, hCurFont);
            break;
        case OP_MOVETO:   MoveToEx(hdc, (int)cmd.x, (int)cmd.y, NULL); break;
        case OP_LINETO:   LineTo(hdc, (int)cmd.x, (int)cmd.y); break;
        case OP_DRAW_CIRCLE: Ellipse(hdc, (int)(cmd.x - cmd.argf1), (int)(cmd.y - cmd.argf1), (int)(cmd.x + cmd.argf1), (int)(cmd.y + cmd.argf1)); break;
        case OP_DRAW_TEXT: {
            int mode = SetBkMode(hdc, TRANSPARENT);
#ifdef UNICODE
            wchar_t wText[64]; MultiByteToWideChar(CP_ACP, 0, cmd.argc1, -1, wText, 64);
            TextOutW(hdc, (int)cmd.x, (int)cmd.y, wText, lstrlenW(wText));
#else
            TextOutA(hdc, (int)cmd.x, (int)cmd.y, cmd.argc1, (int)strlen(cmd.argc1));
#endif
            SetBkMode(hdc, mode);
            break;
        }
        }
    }
    SelectObject(hdc, hOldPen);
	DeleteObject(hCurPen);
	SelectObject(hdc, hOldBrush);
    if (hCurFont) {
		SelectObject(hdc, hOldFont);
		DeleteObject(hCurFont);
	}
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        if (pfnCreateFigure)
			g_fig = pfnCreateFigure(COORD_CARTESIAN);

		break;

    case WM_LBUTTONDOWN: {
        g_isPanning = true;
        g_lastMousePos.x = LOWORD(lParam);
        g_lastMousePos.y = HIWORD(lParam);

        SetCapture(hWnd);
        return 0;
    }

    case WM_MOUSEMOVE: {
		int curX = LOWORD(lParam);
		int curY = HIWORD(lParam);

        if (g_isPanning) {
            int dx = curX - g_lastMousePos.x;
            int dy = curY - g_lastMousePos.y;

            if (dx != 0 || dy != 0) {
                if (g_fig && pfnPan) {
                    pfnPan(g_fig, dx, dy);
                }

                g_lastMousePos.x = curX;
                g_lastMousePos.y = curY;

                InvalidateRect(hWnd, NULL, FALSE);
            }
        }

        if (g_fig && pfnPixel2Logical) {
            float logicalX = 0.0f;
            float logicalY = 0.0f;
            float logicalY2 = 0.0f;

            pfnPixel2Logical(g_fig, curX, curY, &logicalX, &logicalY, &logicalY2);

#ifdef UNICODE
            wchar_t szTitle[128];
            swprintf_s(szTitle, L"Test libplot | X: %.4f, Y: %.4f, Y2: %.4f", logicalX, logicalY, logicalY2);
            SetWindowTextW(hWnd, szTitle);
#else
            char szTitle[128];
            sprintf_s(szTitle, "Test libplot | X: %.4f, Y: %.4f, Y2: %.4f", logicalX, logicalY, logicalY2);
            SetWindowTextA(hWnd, szTitle);
#endif
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (g_isPanning) {
            g_isPanning = false;
            ReleaseCapture();
        }
        return 0;
    }

    case WM_MOUSEWHEEL: {
        if (g_fig && pfnZoom) {
            // A. 获取滚轮滚动的距离
            // GET_WHEEL_DELTA_WPARAM 获取的值通常是 120 (向上滚) 或 -120 (向下滚)
            short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

            // B. 获取滚轮触发时鼠标的 屏幕绝对坐标
			int screenX = (int)(short)LOWORD(lParam);
			int screenY = (int)(short)HIWORD(lParam);

            // C. 关键步骤：将屏幕绝对坐标转换为当前窗口画布的 相对像素坐标
            POINT ptMouse = { screenX, screenY };
            ScreenToClient(hWnd, &ptMouse); // 转换后，ptMouse.x 和 ptMouse.y 就是相对于画布左上角的像素位置

			int clicks = zDelta / WHEEL_DELTA;
            float factor = 1.0f;
            if (clicks > 0) {
                // 向上滚：按 0.8 的步长放大（每次放大 20%）
                // 如果你想让幅度小一点，可以用标准对数：factor = std::pow(0.8f, clicks);
                factor = 0.8f;
            } else if (clicks < 0) {
                // 向下滚：完美无误差的 1.25 缩小！
                // 1 / 0.8f 刚好等于 1.25f，无论是 float 还是 double 都极其精准
                factor = 1.25f;
            }

            if (factor != 1.0f) {
//				printf("factor=%f\n", factor);
				fflush(stdout);
                pfnZoom(g_fig, ptMouse.x, ptMouse.y, factor);
                InvalidateRect(hWnd, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_SIZE: {
        g_canvasWidth  = LOWORD(lParam);
        g_canvasHeight = HIWORD(lParam);

		if (g_fig && pfnSetOption) {
			char buf[64];
			sprintf_s(buf, "%dx%d", g_canvasWidth, g_canvasHeight);
			pfnSetOption(g_fig, "figure.size", buf);
		}

        InvalidateRect(hWnd, NULL, FALSE);
        return 0;
    }

    case WM_ERASEBKGND:
        return TRUE;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdcScreen = BeginPaint(hWnd, &ps);

        HDC hdcMem = CreateCompatibleDC(hdcScreen);
        HBITMAP hBitmapMem = CreateCompatibleBitmap(hdcScreen, g_canvasWidth, g_canvasHeight);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmapMem);

        RECT rect = { 0, 0, g_canvasWidth, g_canvasHeight };
        HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdcMem, &rect, hBgBrush);
        DeleteObject(hBgBrush);

        if (g_fig && pfnShow) {
			const int MAX_OUT_COMMANDS = 1024;
			std::vector<struct draw_command> out_cmds(MAX_OUT_COMMANDS);

			int actual_count = pfnShow(g_fig, out_cmds.data(), MAX_OUT_COMMANDS);

			if (actual_count > 0) {
				ExecuteDrawCommands(hdcMem, out_cmds);
			}
        } else {
            TextOutA(hdcMem, 20, 20, "DLL API not loaded!", 19);
        }

        BitBlt(hdcScreen, 0, 0, g_canvasWidth, g_canvasHeight, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmapMem);
        DeleteDC(hdcMem);

        EndPaint(hWnd, &ps);
        break;
    }
    case WM_DESTROY:
		if (g_fig && pfnDestroyFigure)
			pfnDestroyFigure(g_fig);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
#ifdef _DEBUG
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif

    hPlotDll = LoadLibrary(L"debug\\libplot.dll");
    if (hPlotDll) {
        pfnCreateFigure  = (FuncCreateFigure)GetProcAddress(hPlotDll, "create_figure");
        pfnDestroyFigure = (FuncDestroyFigure)GetProcAddress(hPlotDll, "destroy_figure");
//        pfnPlot          = (FuncPlot)GetProcAddress(hPlotDll, "plot");
        pfnSetOption     = (FuncSetOption)GetProcAddress(hPlotDll, "set_option");
		pfnPan           = (FuncPan)GetProcAddress(hPlotDll, "pan");
		pfnZoom          = (FuncZoom)GetProcAddress(hPlotDll, "zoom");
		pfnPixel2Logical = (FuncPixel2Logical)GetProcAddress(hPlotDll, "pixel2logical");
		pfnShow          = (FuncShow)GetProcAddress(hPlotDll, "show");
    }

    const wchar_t szClassName[] = L"TestLibPlotClass";
    WNDCLASSEXW wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = szClassName;

    if (!RegisterClassExW(&wcex)) return 0;

    HWND hWnd = CreateWindowW(szClassName, L"Test libplot", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, g_canvasWidth, g_canvasHeight, NULL, NULL, hInstance, NULL);

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    if (hPlotDll)
		FreeLibrary(hPlotDll);
    return (int)msg.wParam;
}
