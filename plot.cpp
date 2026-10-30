#include "plot.hpp"

#include <sstream>
#include <iomanip>

static std::string FormatNumber(float val, int scale, std::string unit = "")
{
    std::ostringstream oss;

    if (unit.empty()) {
        if (scale == SCALE_LOG && (val >= 1000.0f || val <= 0.001f)) {
            oss << std::scientific << std::setprecision(0) << val;
            return oss.str();
        }
        oss << std::setprecision(4) << val;
        return oss.str();
    }

    if (val == 0.0f) {
        oss << std::setprecision(4) << 0.0f << " " << unit;
        return oss.str();
    }

    if (scale == SCALE_LOG && (val >= 1000.0f || val <= 0.001f)) {
        oss << std::scientific << std::setprecision(0) << val << " " << unit;
        return oss.str();
    }

    static const char* prefixes[] = { "p", "u", "m", "", "k", "M", "G", "T" };

    float scaled_val = val;
    int index = 3; // "" (10^0)

    while (std::abs(scaled_val) >= 1000.0f && index < 7) {
        scaled_val /= 1000.0f;
        index++;
    }
    while (std::abs(scaled_val) < 1.0f && std::abs(scaled_val) > 0.0f && index > 0) {
        scaled_val *= 1000.0f;
        index--;
    }

    oss << std::setprecision(4) << scaled_val << " " << prefixes[index] << unit;
    return oss.str();
}

inline struct plt_render_cmd plt_render_cmd_(
		unsigned int op, unsigned int zIndex, unsigned int flags,
		float x, float y, float arg1, float arg2, const std::string& arg3)
{
    struct plt_render_cmd cmd{op, zIndex, flags, x, y, arg1, arg2, ""};
    strncpy_s(cmd.arg3, sizeof(cmd.arg3), arg3.c_str(), _TRUNCATE);
    return cmd;
}

int Plot::render(struct plt_render_cmd *out, size_t nout) const
{
	std::vector<struct plt_render_cmd> cmds;

	if (coord == COORD_POLAR) {
		auto [cx, cy] = toPolarPixel(0.0, 0.0);

		std::pair<float, float> max_edge = toPolarPixel(0.0, axisY.max);
		float max_pixel_r = std::abs(max_edge.first - cx);
		if (max_pixel_r > 0) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 5, 0, 0.0f, 0.0f, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 5, 0, cx, cy, max_pixel_r, max_pixel_r, backgroundColor));
		}

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0, 0.0f, 0.0f, 1.0f, 0.0f, ""));

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.tick.color));
		for (float theta : axisX.calculateTicks()) {
			if (theta <= axisX.min)
				continue;
			std::pair<float, float> outer_edge = toPolarPixel(theta, axisY.max);

			if (axisX.grid.visible) {
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, 0, cx, cy, 0.0f, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, 0, outer_edge.first, outer_edge.second, 0.0f, 0.0f, ""));
			}

			if (axisX.tick.visible) {
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, outer_edge.first, outer_edge.second, 0,
							((abs(theta-M_PI_2)<std::numeric_limits<float>::epsilon()||abs(theta-3*M_PI_2)<std::numeric_limits<float>::epsilon())?ALIGN_HCENTER:((theta>M_PI_2&&theta<3*M_PI_2)?ALIGN_RIGHT:ALIGN_LEFT)) |
							((abs(theta)<std::numeric_limits<float>::epsilon()||abs(theta-M_PI)<std::numeric_limits<float>::epsilon())?ALIGN_VCENTER:((theta>M_PI)?ALIGN_TOP:ALIGN_BOTTOM)),
							FormatNumber(180 * theta / M_PI, SCALE_LINEAR)));
			}
		}

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.tick.color));
		for (float r : axisY.calculateTicks()) {
			if (r <= axisY.min)
				continue;
			std::pair<float, float> edge = toPolarPixel(0.0, r);
			float pixelR = std::abs(edge.first - cx);
			if (pixelR > 0) {
				if (axisY.grid.visible) {
					cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 10, 0, cx, cy, pixelR, pixelR, ""));
				}
				if (axisY.tick.visible) {
					cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, edge.first, edge.second, 0.0f, ALIGN_HCENTER | ALIGN_TOP, FormatNumber(r, SCALE_LINEAR, axisY.unit)));
				}
			}
		}

		if (axisX.visible) {
			if (max_pixel_r > 0) {
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisX.width, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.style));
				cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 50, 0, cx, cy, max_pixel_r, max_pixel_r, ""));
			}
		}

		if (axisY.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisY.width, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.style));

			std::pair<float, float> max_edge2;

			//max_edge = toPolarPixel(0.0, axisY.max);
			max_edge2 = toPolarPixel(M_PI, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, max_edge.first, max_edge.second, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, max_edge2.first, max_edge2.second, 0.0f, 0.0f, ""));

			max_edge = toPolarPixel(M_PI_2, axisY.max);
			max_edge2 = toPolarPixel(3 * M_PI_2, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, max_edge.first, max_edge.second, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, max_edge2.first, max_edge2.second, 0.0f, 0.0f, ""));
		}

		if (!title.empty()) {
			max_edge = toPolarPixel(M_PI_2, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));
			float yPos = max_edge.second - 2 * fontHeight;
			if (yPos < 0) yPos = 0;
			cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, 0, max_edge.first, yPos, 0, ALIGN_HCENTER | ALIGN_TOP, title));
		}

	} else if (coord == COORD_CARTESIAN) {

		float pxMin = toPixelX(axisX.min);
		float pxMax = toPixelX(axisX.max);
		float pyMin = toPixelY(axisY.max);//!
		float pyMax = toPixelY(axisY.min);//!

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 5, 0, 0.0f, 0.0f, 0.0f, 0.0f, ""));
		cmds.push_back(plt_render_cmd_(OP_DRAW_RECT, 5, 0, pxMin, pyMin, pxMax-pxMin, pyMax-pyMin, backgroundColor));

		int nCharsYTicks = 0, nCharsY2Ticks = 0;

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0, 0.0f, 0.0f, 1.0f, 0.0f, ""));

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.tick.color));
		for (float x : axisX.calculateTicks()) {
			float px = toPixelX(x);

			if (axisX.grid.visible) {
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, 0, px, pyMin, 0.0f, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, 0, px, pyMax, 0.0f, 0.0f, ""));
			}

			if (axisX.tick.visible) {
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, px, pyMax, 0.0f, ALIGN_HCENTER | ALIGN_TOP, FormatNumber(x, axisX.scale, axisX.unit)));
			}
		}

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.tick.color));
		for (float y : axisY.calculateTicks()) {
			float py = toPixelY(y);

			if (axisY.grid.visible || (axisY2.visible && axisY2.grid.visible)) {
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, 0, pxMin, py, 0.0f, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, 0, pxMax, py, 0.0f, 0.0f, ""));
			}

			if (axisY.tick.visible) {
				std::string s = FormatNumber(y, axisY.scale, axisY.unit);
				if (s.size() > nCharsYTicks)
					nCharsYTicks = s.size();
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, pxMin, py, 0.0f, ALIGN_RIGHT | ALIGN_VCENTER, s));
			}
		}

		if (axisY2.visible) {
			for (float y2 : axisY2.calculateTicks()) {
				float py = toPixelY(y2, AXIS_SECONDARY);
				if (axisY.tick.visible) {
					cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.tick.color));

					std::string s = FormatNumber(y2, axisY2.scale, axisY2.unit);
					if (s.size() > nCharsY2Ticks)
						nCharsY2Ticks = s.size();
					cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, pxMax, py, 0.0f, ALIGN_LEFT | ALIGN_VCENTER, s));
				}
			}
		}

		if (axisX.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisX.width, 0.0f, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, pxMin, pyMax, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, pxMax, pyMax, 0.0f, 0.0f, ""));

			if (!axisX.label.empty()) {
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));

				float yPos = pyMax + fontHeight;
				if (yPos > plotHeight) yPos = plotHeight;
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, 0, (pxMin+pxMax)/2, yPos, 0.0f, ALIGN_HCENTER | ALIGN_TOP, axisX.label));
			}
		}
		if (axisY.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisY.width, 0.0f, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, pxMin, pyMin, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, pxMin, pyMax, 0.0f, 0.0f, ""));

			if (!axisY.label.empty()) {
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.color));
				float xPos = pxMin-(1+nCharsYTicks)*fontWidth;
				if (xPos < 0) xPos = 0;
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, 0, xPos, (pyMin+pyMax)/2, M_PI_2, ALIGN_HCENTER | ALIGN_TOP, axisY.label));
			}
		}
		if (axisY2.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisY2.width, 0.0f, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, pxMax, pyMin, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, pxMax, pyMax, 0.0f, 0.0f, ""));

			if (!axisY2.label.empty()) {
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.color));

				float xPos = pxMax+nCharsY2Ticks*fontWidth;
				if (xPos > plotWidth) xPos = plotWidth;
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, 0, xPos, (pyMin+pyMax)/2, M_PI_2, ALIGN_HCENTER | ALIGN_BOTTOM, axisY2.label));
			}
		}

		if (!title.empty()) {
			cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));
			float yPos = pyMin - 2 * fontHeight;
			if (yPos < 0) yPos = 0;
			cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, 0, (pxMin+pxMax)/2, yPos, 0, ALIGN_HCENTER | ALIGN_TOP, title));
		}
	}

	for (const auto* curve : curves) {
		const std::vector<float> &xData = curve->getXData();
		const std::vector<float> &yData = curve->getYData();

		if (!curve || xData.empty() || yData.empty())
			continue;
		if (!curve->visible)
			continue;

		std::vector<std::pair<float, float>> pxpy;

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 20, 0, 0.0f, 0.0f, curve->width, 0.0f, ""));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 20, 0, 0.0f, 0.0f, 0.0f, 0.0f, curve->color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 20, 0, 0.0f, 0.0f, 0.0f, 0.0f, curve->style));

		bool firstPoint = true;
		for (size_t i = 0; i < std::min(xData.size(), yData.size()); ++i) {
			if (xData[i] < axisX.min || xData[i] > axisX.max) {
				firstPoint = true;
				continue;
			}

			if (const Axis& a = (curve->axisY == AXIS_SECONDARY) ? axisY2 : axisY;
					yData[i] < a.min || yData[i] > a.max) {
				firstPoint = true;
				continue;
			}

			float px;
			float py;
			if (coord == COORD_CARTESIAN) {
				px = toPixelX(xData[i]);
				py = toPixelY(yData[i], curve->axisY);
			} else if (coord == COORD_POLAR) {
				auto p = toPolarPixel(xData[i], yData[i]);
				px = p.first;
				py = p.second;
			}

			if (curve->markerShape != "")
				pxpy.push_back({px, py});

			if (firstPoint) {
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 20, 0, px, py, 0.0f, 0.0f, ""));
				firstPoint = false;
			} else {
				cmds.push_back(plt_render_cmd_(OP_LINETO, 20, 0, px, py, 0.0f, 0.0f, ""));
			}
		}

		if (!pxpy.empty()) {
			float h = curve->markerSize / 2.0f;
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 30, 0, 0.0f, 0.0f, 0.0f, 0.0f, curve->markerColor));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 30, 0, 0.0f, 0.0f, 0.0f, 0.0f, "-"));
			for (auto [px, py] : pxpy) {
				if (curve->markerShape == "x") {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px - h, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px - h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py - h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == ".") {
					cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 30, 0, px, py, h, h, curve->markerColor));
				} else if (curve->markerShape == "+") {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px - h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px, py + h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == "o") {
					cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 30, 0, px, py, h, h, ""));
				} else if (curve->markerShape == "s") {
					cmds.push_back(plt_render_cmd_(OP_DRAW_RECT, 30, 0, px-h, py-h, 2 * h, 2 * h, ""));
					//cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px - h, py - h, 0.0f, 0.0f, ""));
					//cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py - h, 0.0f, 0.0f, ""));
					//cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py + h, 0.0f, 0.0f, ""));
					//cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px - h, py + h, 0.0f, 0.0f, ""));
					//cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px - h, py - h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == "^") {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px - h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == "d") {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px - h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
				}
			}
		}
	}

	printf("Num. of cmds: %d\n", cmds.size());
	fflush(stdout);
	int nret = std::min(cmds.size(), nout);
	for (int i = 0; i < nret; i++) {
		out[i] = cmds.data()[i];
	}
	return nret;
}

