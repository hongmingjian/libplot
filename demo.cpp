//
// cl.exe /std:c++17 /EHsc /MDd /utf-8 /Zi /I. /D_DEBUG /DUNICODE /D_UNICODE demo.cpp plot.cpp /link /DEBUG /SUBSYSTEM:WINDOWS user32.lib gdi32.lib
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

#include "plot.hpp"

int g_plotWidth  = 550;
int g_plotHeight = 450;
Plot<float> *g_plt = nullptr;
std::vector<Curve<float> *> g_curves;
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
	COLORREF curPenColor;
    int curPenStyle;
    int curPenWidth;

	{
		LOGPEN lp = { 0 };
		GetObjectA(GetCurrentObject(hdc, OBJ_PEN), sizeof(LOGPEN), &lp);
		curPenColor = lp.lopnColor;
		curPenStyle = lp.lopnStyle;
		curPenWidth = lp.lopnWidth.x ? lp.lopnWidth.x : 1;
	}
    HPEN hCurPen = NULL;
    HPEN hOldPen = NULL;

    for (auto& cmd : cmds) {
        switch (cmd.op) {
        case OP_SET_PEN_COLOR:
        case OP_SET_PEN_STYLE:
        case OP_SET_PEN_WIDTH:
			if (cmd.op == OP_SET_PEN_COLOR) {
				curPenColor = convertColor(cmd.arg3);
                SetTextColor(hdc, curPenColor);
            } else if (cmd.op == OP_SET_PEN_STYLE)
				curPenStyle = convertLineStyle(cmd.arg3);
			else if (cmd.op == OP_SET_PEN_WIDTH) {
				curPenWidth = (int)cmd.arg1;
				if (curPenWidth < 1)
					curPenWidth = 1;
			}
			if (hOldPen)
				SelectObject(hdc, hOldPen);
			if (hCurPen)
				DeleteObject(hCurPen);
            hCurPen = CreateGDIRenderPen(curPenStyle, curPenWidth, curPenColor);
            hOldPen = (HPEN)SelectObject(hdc, hCurPen);
            break;
        case OP_MOVETO:
			MoveToEx(hdc, (int)cmd.x, (int)cmd.y, NULL);
			break;
        case OP_LINETO:
			LineTo(hdc, (int)cmd.x, (int)cmd.y);
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

		g_plt = new Plot<float>(g_coord);

		if (g_plt) {
			g_plt->set_option("plot.title", "Archimedean spiral");
			g_plt->set_option("plot.background.color", "#aaaaaa");
//			g_plt->set_option("plot.axisX.tick.visible", "true");
//			g_plt->set_option("plot.axisX.grid.visible", "false");
//			g_plt->set_option("plot.axisY.visible", "true");
//			g_plt->set_option("plot.axisY.color", "#0000FF");
//			g_plt->set_option("plot.axisY.tick.color", "#0000FF");
//			g_plt->set_option("plot.axisY.grid.color", "#0000FF");
//			g_plt->set_option("plot.axisY.range", "1,1e2");
//			g_plt->set_option("plot.axisY.scale", "linear");
			g_plt->set_option("plot.axisX.label", "Frequency");
			g_plt->set_option("plot.axisY.label", "a.u.");

			HDC hdc = GetDC(hWnd);
			if (hdc) {
				TEXTMETRIC tm;
				GetTextMetrics(hdc, &tm);
				ReleaseDC(hWnd, hdc);

				char buf[64];
				sprintf_s(buf, "%dx%d", tm.tmMaxCharWidth, tm.tmHeight);
				g_plt->set_option("plot.font.size", buf);
			}
		}

		break;
	}

    case WM_TIMER: {
		if (wParam == g_idTimer) {
			if (g_plt) {
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
					Curve<float> *curve = new Curve<float>(g_plt);
					if (curve) {
						g_curves.push_back(curve);
						curve->updateData(xData, yData);
						curve->set_option("curve.color", "#0000FF");
						curve->set_option("curve.width", "2");
//					    curve->set_option("curve.style", ":");
					    curve->set_option("curve.marker.shape", ".");
//					    curve->set_option("curve.marker.size", "8");
						curve->set_option("curve.marker.color", "#FF0000");
					}
				} else
					g_curves[0]->updateData(xData, yData);

                InvalidateRect(hWnd, NULL, FALSE);

				//KillTimer(hWnd, g_idTimer);
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
                if (g_plt) {
                    g_plt->pan(dx, dy);
                }

                g_lastMousePos.x = curX;
                g_lastMousePos.y = curY;

                InvalidateRect(hWnd, NULL, FALSE);
            }
        }

        if (g_plt) {
            float logicalX = 0.0f;
            float logicalY = 0.0f;
            float logicalY2 = 0.0f;

            g_plt->pix2math(curX, curY, &logicalX, &logicalY, &logicalY2);

#ifdef UNICODE
            wchar_t szTitle[256];
            swprintf_s(szTitle, L"Libplot demo: (%d, %d) -> (%g, %g, %g)", curX, curY, logicalX, logicalY, logicalY2);
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
        if (g_plt) {
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
					g_plt->zoom(ptMouse.x, ptMouse.y, factor);
					InvalidateRect(hWnd, NULL, FALSE);
				}
			}
        }
        return 0;
    }

    case WM_SIZE: {
        g_plotWidth  = LOWORD(lParam);
        g_plotHeight = HIWORD(lParam);

		if (g_plt) {
			char buf[64];
			sprintf_s(buf, "%dx%d", g_plotWidth, g_plotHeight);
			g_plt->set_option("plot.size", buf);
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
        HBITMAP hBitmapMem = CreateCompatibleBitmap(hdcScreen, g_plotWidth, g_plotHeight);
        HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmapMem);

        RECT rect = { 0, 0, g_plotWidth, g_plotHeight };
        HBRUSH hBgBrush = CreateSolidBrush(RGB(255, 255, 255));
        FillRect(hdcMem, &rect, hBgBrush);
        DeleteObject(hBgBrush);

        if (g_plt) {
			const int MAX_OUT_COMMANDS = 1024;
			std::vector<struct plt_render_cmd> out_cmds(MAX_OUT_COMMANDS);

			int actual_count = g_plt->render(out_cmds.data(), MAX_OUT_COMMANDS);

			if (actual_count > 0) {
				ExecuteDrawCommands(hdcMem, out_cmds);
			}
        } else {
            TextOutA(hdcMem, 20, 20, "DLL API not loaded!", 19);
        }

        BitBlt(hdcScreen, 0, 0, g_plotWidth, g_plotHeight, hdcMem, 0, 0, SRCCOPY);

        SelectObject(hdcMem, hOldBitmap);
        DeleteObject(hBitmapMem);
        DeleteDC(hdcMem);

        EndPaint(hWnd, &ps);
        break;
    }
    case WM_DESTROY:
		if (g_plt)
			delete g_plt;
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
        CW_USEDEFAULT, CW_USEDEFAULT, g_plotWidth, g_plotHeight, NULL, NULL, hInstance, NULL);

    if (!hWnd) return 0;

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
