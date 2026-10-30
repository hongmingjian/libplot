//
// cl.exe /EHsc /MDd /utf-8 /Zi /I. /D_DEBUG /DUNICODE /D_UNICODE testlibplot.cpp /link /DEBUG /SUBSYSTEM:WINDOWS user32.lib gdi32.lib
//
#define _USE_MATH_DEFINES
#include <windows.h>
#include <vector>
#include <algorithm>
#include <string>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <random>

#include "libplot.h"

typedef void *(*FuncCreate)(int);
typedef void  (*FuncDestroy)(void*);
typedef void *(*FuncCurve)(void*, const float*, const float*, int);
typedef int   (*FuncSetOption)(void*, const char*, const char*);
typedef void  (*FuncPan)(void*, int, int);
typedef void  (*FuncZoom)(void*, int, int, float);
typedef void  (*FuncPix2Math)(void *, int, int, float*, float*, float*);
typedef int   (*FuncRender)(void*, struct plt_render_cmd *, int);

HMODULE            hPlotDll         = nullptr;
FuncCreate pfnCreate = nullptr;
FuncDestroy pfnDestroy = nullptr;
FuncCurve           pfnCurve          = nullptr;
FuncSetOption      pfnSetOption     = nullptr;
FuncPan            pfnPan           = nullptr;
FuncZoom           pfnZoom          = nullptr;
FuncPix2Math  pfnPix2Math = nullptr;
FuncRender           pfnRender          = nullptr;

int g_canvasWidth  = 550;
int g_canvasHeight = 450;
void *g_plt = nullptr;
std::vector<void *> g_curves;
int g_coord = COORD_POLAR;//CARTESIAN;
int g_idTimer = 1001;
static bool g_isPanning = false;
static POINT g_lastMousePos;

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
	if (s == "")     return PS_NULL;
    if (s == "-")    return PS_SOLID;
    if (s == "--")   return PS_DASH;
    if (s == ":")    return PS_DOT;
    if (s == "-.")   return PS_DASHDOT;
    if (s == "-..")  return PS_DASHDOTDOT;
    return PS_NULL;
}

HPEN CreateGDIRenderPen(int style, int width, COLORREF color) {
    if (style == PS_SOLID)
		return CreatePen(PS_SOLID, width, color);
    LOGBRUSH lb = { BS_SOLID, color, 0 };
    return ExtCreatePen(PS_GEOMETRIC | style, width, &lb, 0, NULL);
}

void ExecuteDrawCommands(HDC hdc, std::vector<struct plt_render_cmd>& cmds) {
    std::stable_sort(cmds.begin(), cmds.end(), [](const struct plt_render_cmd& a, const struct plt_render_cmd& b) {
        return a.zIndex < b.zIndex;
    });

    COLORREF oldTextColor = GetTextColor(hdc);
	COLORREF curLineColor;
    int curLineStyle;
    int curLineWidth;

	{
		LOGPEN lp = { 0 };
		GetObjectA(GetCurrentObject(hdc, OBJ_PEN), sizeof(LOGPEN), &lp);
		curLineColor = lp.lopnColor;
		curLineStyle = lp.lopnStyle;
		curLineWidth = lp.lopnWidth.x ? lp.lopnWidth.x : 1;
	}
    HPEN hCurPen = NULL;
    HPEN hOldPen = NULL;

    for (auto& cmd : cmds) {
        switch (cmd.op) {
        case OP_SET_TEXT_COLOR:
            SetTextColor(hdc, convertColor(cmd.arg3));
            break;
        case OP_DRAW_TEXT: {
		    LOGFONTA lf = { 0 };
		    GetObjectA(GetCurrentObject(hdc, OBJ_FONT), sizeof(LOGFONTA), &lf);

		    long gdiAngle = static_cast<long>(10 * cmd.arg1 * 180 / M_PI);
		    lf.lfEscapement = gdiAngle;
		    lf.lfOrientation = gdiAngle;

		    HFONT hCurFont = CreateFontIndirectA(&lf);
		    if (hCurFont) {
				HFONT hOldFont = (HFONT)SelectObject(hdc, hCurFont);
				int oldBkMode = SetBkMode(hdc, TRANSPARENT);

				unsigned int pltAlign = static_cast<unsigned int>(cmd.arg2);
				UINT gdiAlign = TA_LEFT | TA_TOP;
				if (pltAlign & ALIGN_HCENTER)
					gdiAlign = (gdiAlign & ~TA_LEFT) | TA_CENTER;
				else if (pltAlign & ALIGN_RIGHT)
					gdiAlign = (gdiAlign & ~TA_LEFT) | TA_RIGHT;
                if (pltAlign & ALIGN_BASELINE)
					gdiAlign = (gdiAlign & ~TA_TOP) | TA_BASELINE;
                else if (pltAlign & ALIGN_BOTTOM)
				 	gdiAlign = (gdiAlign & ~TA_TOP) | TA_BOTTOM;
                else if (pltAlign & ALIGN_VCENTER) {
					SIZE sz;
					GetTextExtentPoint32A(hdc, cmd.arg3, strlen(cmd.arg3), &sz);
					cmd.y -= sz.cy / 2;
                    gdiAlign = (gdiAlign & ~TA_TOP) | TA_TOP;
				}
				SetTextAlign(hdc, gdiAlign);

				TextOutA(hdc, cmd.x, cmd.y, cmd.arg3, strlen(cmd.arg3));

				SetBkMode(hdc, oldBkMode);
				SelectObject(hdc, hOldFont);
				DeleteObject(hCurFont);
			}
            break;
        }
        case OP_SET_LINE_COLOR:
        case OP_SET_LINE_STYLE:
        case OP_SET_LINE_WIDTH:
			if (cmd.op == OP_SET_LINE_COLOR)
				curLineColor = convertColor(cmd.arg3);
			else if (cmd.op == OP_SET_LINE_STYLE)
				curLineStyle = convertLineStyle(cmd.arg3);
			else if (cmd.op == OP_SET_LINE_WIDTH) {
				curLineWidth = (int)cmd.arg1;
				if (curLineWidth < 1)
					curLineWidth = 1;
			}
			if (hOldPen)
				SelectObject(hdc, hOldPen);
			if (hCurPen)
				DeleteObject(hCurPen);
            hCurPen = CreateGDIRenderPen(curLineStyle, curLineWidth, curLineColor);
            hOldPen = (HPEN)SelectObject(hdc, hCurPen);
            break;
        case OP_MOVETO:
			MoveToEx(hdc, (int)cmd.x, (int)cmd.y, NULL);
			break;
        case OP_LINETO:
			LineTo(hdc, (int)cmd.x, (int)cmd.y);
			break;
        case OP_DRAW_ELLIPSE: {
			HBRUSH hCurBrush = NULL;
			HBRUSH hOldBrush = NULL;
			if (std::string(cmd.arg3) == "") {
				hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
			} else {
				hCurBrush = CreateSolidBrush(convertColor(cmd.arg3));
				hOldBrush = (HBRUSH)SelectObject(hdc, hCurBrush);
			}

            Ellipse(hdc, (int)(cmd.x - cmd.arg1), (int)(cmd.y - cmd.arg2),
					     (int)(cmd.x + cmd.arg1), (int)(cmd.y + cmd.arg2));

			if (hOldBrush)
				SelectObject(hdc, hOldBrush);
			if (hCurBrush)
				DeleteObject(hCurBrush);
            break;
            }
        case OP_DRAW_RECT: {
			HBRUSH hCurBrush = NULL;
			HBRUSH hOldBrush = NULL;
			if (std::string(cmd.arg3) == "") {
				hOldBrush = (HBRUSH)SelectObject(hdc, GetStockObject(NULL_BRUSH));
			} else {
				hCurBrush = CreateSolidBrush(convertColor(cmd.arg3));
				hOldBrush = (HBRUSH)SelectObject(hdc, hCurBrush);
			}

            Rectangle(hdc, (int)(cmd.x), (int)(cmd.y),
					       (int)(cmd.x + cmd.arg1), (int)(cmd.y + cmd.arg2));

			if (hOldBrush)
				SelectObject(hdc, hOldBrush);
			if (hCurBrush)
				DeleteObject(hCurBrush);
            break;
            }
        }
    }
	if (hOldPen)
		SelectObject(hdc, hOldPen);
	if (hCurPen)
		DeleteObject(hCurPen);
	SetTextColor(hdc, oldTextColor);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE: {
		SetTimer(hWnd, g_idTimer, 1000, NULL);

        if (pfnCreate)
			g_plt = pfnCreate(g_coord);

		if (g_plt && pfnSetOption) {
//			pfnSetOption(g_plt, "plot.axisX.tick.visible", "true");
//			pfnSetOption(g_plt, "plot.axisX.grid.visible", "false");
//			pfnSetOption(g_plt, "plot.axisY.visible", "true");
//			pfnSetOption(g_plt, "plot.axisY.color", "#0000FF");
//			pfnSetOption(g_plt, "plot.axisY.tick.color", "#0000FF");
//			pfnSetOption(g_plt, "plot.axisY.grid.color", "#0000FF");
//			pfnSetOption(g_plt, "plot.axisY.range", "1,1e2");
//			pfnSetOption(g_plt, "plot.axisY.scale", "linear");
		}

		break;
	}

    case WM_TIMER: {
		if (wParam == g_idTimer) {
			if (g_plt && pfnCurve && pfnSetOption) {
				std::vector<float> xData;
				std::vector<float> yData;

				std::random_device rd;
				std::mt19937 gen(rd());

				float a = 0, b = 1;
				std::uniform_int_distribution<int> distrib(3, 6);
				int nturns = distrib(gen);
				int npoints_per_turn = 72;
				int npoints = nturns * npoints_per_turn;
				float step = 2.0f * M_PI / npoints_per_turn;
				for (int i = 0; i < npoints+1; ++i) {
					float theta = i * step;
					float r = a + b * theta;
					if (g_coord == COORD_CARTESIAN) {
						xData.push_back(r * std::cos(theta));
						yData.push_back(r * std::sin(theta));
					} else {
						xData.push_back(std::fmod(theta, 2.0f * M_PI));
						yData.push_back(r);
					}
				}

				if (g_curves.empty()) {
					void *curve = pfnCurve(g_plt, xData.data(), yData.data(), xData.size());
					if (curve) {
						g_curves.push_back(curve);
						pfnSetOption(curve, "curve.color", "#0000FF");
						pfnSetOption(curve, "curve.width", "2");
//					    pfnSetOption(curve, "curve.style", ":");
//					    pfnSetOption(curve, "curve.marker.shape", ".");
//					    pfnSetOption(curve, "curve.marker.size", "8");
//						pfnSetOption(curve, "curve.marker.color", "#FF0000");
					}
				} else
					pfnCurve(g_curves[0], xData.data(), yData.data(), xData.size());

                InvalidateRect(hWnd, NULL, FALSE);

				KillTimer(hWnd, g_idTimer);
			}
		}
		return 0;
	}
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
                if (g_plt && pfnPan) {
                    pfnPan(g_plt, dx, dy);
                }

                g_lastMousePos.x = curX;
                g_lastMousePos.y = curY;

                InvalidateRect(hWnd, NULL, FALSE);
            }
        }

        if (g_plt && pfnPix2Math) {
            float logicalX = 0.0f;
            float logicalY = 0.0f;
            float logicalY2 = 0.0f;

            pfnPix2Math(g_plt, curX, curY, &logicalX, &logicalY, &logicalY2);

#ifdef UNICODE
            wchar_t szTitle[256];
            swprintf_s(szTitle, L"Libplot demo: (%d, %d) -> (%.g, %g, %g)", curX, curY, logicalX, logicalY, logicalY2);
            SetWindowTextW(hWnd, szTitle);
#else
            char szTitle[256];
            sprintf_s(szTitle, "Libplot demo: (%d, %d) -> (%g, %g, %g)", curX, curY, logicalX, logicalY, logicalY2);
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
        if (g_plt && pfnZoom) {
			if (GET_KEYSTATE_WPARAM(wParam) & MK_CONTROL) {
				short zDelta = GET_WHEEL_DELTA_WPARAM(wParam);

				int screenX = (int)(short)LOWORD(lParam);
				int screenY = (int)(short)HIWORD(lParam);

				POINT ptMouse = { screenX, screenY };
				ScreenToClient(hWnd, &ptMouse);

				int clicks = zDelta / WHEEL_DELTA;
				float factor = 1.0f;
				if (clicks > 0) {
					factor = 1.25f;
				} else if (clicks < 0) {
					factor = 0.8f;
				}

				if (factor != 1.0f) {
					pfnZoom(g_plt, ptMouse.x, ptMouse.y, factor);
					InvalidateRect(hWnd, NULL, FALSE);
				}
			}
        }
        return 0;
    }

    case WM_SIZE: {
        g_canvasWidth  = LOWORD(lParam);
        g_canvasHeight = HIWORD(lParam);

		if (g_plt && pfnSetOption) {
			char buf[64];
			sprintf_s(buf, "%dx%d", g_canvasWidth, g_canvasHeight);
			pfnSetOption(g_plt, "plot.size", buf);
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

		if (g_plt && pfnSetOption) {
			TEXTMETRIC tm;
			GetTextMetrics(hdcScreen, &tm);
			char buf[64];
			sprintf_s(buf, "%dx%d", tm.tmMaxCharWidth, tm.tmHeight);
			pfnSetOption(g_plt, "plot.font.size", buf);
		}
        if (g_plt && pfnRender) {
			const int MAX_OUT_COMMANDS = 1024;
			std::vector<struct plt_render_cmd> out_cmds(MAX_OUT_COMMANDS);

			int actual_count = pfnRender(g_plt, out_cmds.data(), MAX_OUT_COMMANDS);

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
		if (g_plt && pfnDestroy)
			pfnDestroy(g_plt);
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
        pfnCreate = (FuncCreate)GetProcAddress(hPlotDll, "plt_create");
        pfnDestroy = (FuncDestroy)GetProcAddress(hPlotDll, "plt_destroy");
        pfnCurve          = (FuncCurve)GetProcAddress(hPlotDll, "plt_curve");
        pfnSetOption     = (FuncSetOption)GetProcAddress(hPlotDll, "plt_set_option");
		pfnPan           = (FuncPan)GetProcAddress(hPlotDll, "plt_pan");
		pfnZoom          = (FuncZoom)GetProcAddress(hPlotDll, "plt_zoom");
		pfnPix2Math = (FuncPix2Math)GetProcAddress(hPlotDll, "plt_pix2math");
		pfnRender          = (FuncRender)GetProcAddress(hPlotDll, "plt_render");
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

    HWND hWnd = CreateWindowW(szClassName, L"Libplot demo", WS_OVERLAPPEDWINDOW,
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
