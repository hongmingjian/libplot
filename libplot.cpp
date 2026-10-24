#include "libplot.hpp"

enum { TYPE_PLOT, TYPE_CURVE };
struct Handle {
    int type;
    void *obj;
};

EXPORT_C void *plt_create(int coord)
{
	Plot *plt = new Plot(coord);
	return new Handle{ TYPE_PLOT, plt };
}

EXPORT_C void plt_destroy(void *handle_)
{
	if (handle_) {
		Handle *handle = static_cast<Handle *>(handle_);
		if (handle->type == TYPE_PLOT) {
			Plot *plt = static_cast<Plot *>(handle->obj);
			delete plt;
			delete handle;
		}
	}
}

EXPORT_C int plt_set_option(void *handle_, const char *option, const char *value)
{
	if (handle_) {
		Handle *handle = static_cast<Handle *>(handle_);
		switch(handle->type) {
			case TYPE_PLOT:
				return static_cast<Plot *>(handle->obj)->set_option(option, value);
				break;
			case TYPE_CURVE:
				return static_cast<Curve *>(handle->obj)->set_option(option, value);
				break;
		}
	}
	return -1;
}

EXPORT_C void plt_zoom(void *plt_, int cx, int cy, float factor)
{
	if (plt_) {
		Handle *handle = static_cast<Handle *>(plt_);
		if (handle->type == TYPE_PLOT) {
			Plot *plt = static_cast<Plot *>(handle->obj);
			plt->zoom(cx, cy, factor);
		}
	}
}

EXPORT_C void plt_pan(void *plt_, int dx, int dy)
{
	if (plt_) {
		Handle *handle = static_cast<Handle *>(plt_);
		if (handle->type == TYPE_PLOT) {
			Plot *plt = static_cast<Plot *>(handle->obj);
			plt->pan(dx, dy);
		}
	}
}

EXPORT_C void plt_pix2math(void *plt_, int px, int py, float *x, float *y, float *y2)
{
	if (plt_) {
		Handle *handle = static_cast<Handle *>(plt_);
		if (handle->type == TYPE_PLOT) {
			Plot *plt = static_cast<Plot *>(handle->obj);
			plt->pix2math(px, py, x, y, y2);
		}
	}
}

EXPORT_C void *plt_curve(void *handle_, const float *x, const float *y, int count)
{
	if (handle_) {
		Handle *handle = static_cast<Handle *>(handle_);
		Plot *plt = nullptr;
		Curve *crv = nullptr;
		switch(handle->type) {
			case TYPE_PLOT: {
				plt = static_cast<Plot *>(handle->obj);
				crv = new Curve(plt);
				if (x && y && count > 0) {
					crv->xData.assign(x, x + count);
					crv->yData.assign(y, y + count);
				}
				plt->curves.push_back(crv);
				break;
            }
			case TYPE_CURVE: {
				crv = static_cast<Curve *>(handle->obj);
				if (x && y && count > 0) {
					crv->xData.assign(x, x + count);
					crv->yData.assign(y, y + count);
				}
				plt = crv->plot;
				break;
            }
		}

		if (!plt || !crv)
			return nullptr;

		std::vector<std::string> axes = {"X"};
		if (crv->axis == AXIS_SECONDARY)
			axes.push_back("Y2");
		else
			axes.push_back("Y");

		for (auto a : axes) {
			Axis *axis = nullptr;
			const float *v = nullptr;

			if (a == "X") {
				axis = &plt->axisX;
				v = x;
			} else if (a == "Y") {
				axis = &plt->axisY;
				v = y;
			} else if (a == "Y2") {
				axis = &plt->axisY2;
				v = y;
			}

			if (v && count > 0) {
				if (plt->autoscale[a]) {
					axis->min = v[0];
					axis->max = v[0];
					for (int i = 1; i < count; ++i) {
						if (v[i] < axis->min)
							axis->min = v[i];
						if (v[i] > axis->max)
							axis->max = v[i];
					}
					axis->applyMargin();
				}
			}
		}

		return (handle->type == TYPE_PLOT) ? new Handle{ TYPE_CURVE, crv } : handle;
	}

	return nullptr;
}

EXPORT_C void plt_append(void *handle_, const float *x, const float *y, int count)
{
	if (handle_ && x && y && count > 0) {
    	Handle *handle = static_cast<Handle *>(handle_);
		if (handle->type == TYPE_CURVE) {
			Curve* curve = static_cast<Curve *>(handle->obj);
			curve->xData.insert(curve->xData.end(), x, x + count);
			curve->yData.insert(curve->yData.end(), y, y + count);
		}
	}
}

EXPORT_C void plt_remove(void *plt_, void *crv_)
{
	if (plt_ && crv_) {
		Handle *hplt = static_cast<Handle *>(plt_);
		Handle *hcrv = static_cast<Handle *>(crv_);
		if (hplt->type == TYPE_PLOT && hcrv->type == TYPE_CURVE) {
			Plot *plt = static_cast<Plot *>(hplt->obj);
			Curve *curve = static_cast<Curve *>(hcrv->obj);

			auto it = std::find(plt->curves.begin(), plt->curves.end(), curve);
			if (it != plt->curves.end()) {
				delete curve;//*it;
				plt->curves.erase(it);
				delete hcrv;
			}
		}
    }
}

EXPORT_C int plt_render(void *plt_, struct plt_render_cmd *out, int nout)
{
	if (plt_) {
		Handle *handle = static_cast<Handle *>(plt_);
		if (handle->type == TYPE_PLOT) {
			Plot *plt = static_cast<Plot *>(handle->obj);
			return plt->render(out, nout);
		}
	}

	return 0;
}

#include <sstream>
#include <iomanip>

static std::string FormatNumber(float val, int scale)
{
	std::ostringstream oss;
	if (scale == SCALE_LOG && (val >= 1000.0 || val <= 0.001)) {
		oss << std::scientific << std::setprecision(0) << val;
		return oss.str();
	}
	oss << std::setprecision(4) << val;
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
		auto [cx, cy] = ToPolarPixel(0.0, 0.0);

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0, 0.0f, 0.0f, 1.0f, 0.0f, ""));

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.tick.color));
		for (float theta : axisX.calculateTicks()) {
			if (theta <= axisX.min)
				continue;
			std::pair<float, float> outer_edge = ToPolarPixel(theta, axisY.max);

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
			std::pair<float, float> edge = ToPolarPixel(0.0, r);
			float pixelR = std::abs(edge.first - cx);
			if (pixelR > 0) {
				if (axisY.grid.visible) {
					cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 10, 0, cx, cy, pixelR, pixelR, ""));
				}
				if (axisY.tick.visible) {
					cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, edge.first, edge.second, 0.0f, ALIGN_HCENTER | ALIGN_TOP, FormatNumber(r, SCALE_LINEAR)));
				}
			}
		}

		std::pair<float, float> max_edge = ToPolarPixel(0.0, axisY.max);
		if (axisX.visible) {
			float max_pixel_r = std::abs(max_edge.first - cx);
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

			//max_edge = ToPolarPixel(0.0, axisY.max);
			max_edge2 = ToPolarPixel(M_PI, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, max_edge.first, max_edge.second, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, max_edge2.first, max_edge2.second, 0.0f, 0.0f, ""));

			max_edge = ToPolarPixel(M_PI_2, axisY.max);
			max_edge2 = ToPolarPixel(3 * M_PI_2, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, max_edge.first, max_edge.second, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, max_edge2.first, max_edge2.second, 0.0f, 0.0f, ""));
		}

	} else if (coord == COORD_CARTESIAN) {

		float pxMin = ToPixelX(axisX.min);
		float pxMax = ToPixelX(axisX.max);
		float pyMin = ToPixelY(axisY.max);//!
		float pyMax = ToPixelY(axisY.min);//!

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0, 0.0f, 0.0f, 1.0f, 0.0f, ""));

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.tick.color));
		for (float x : axisX.calculateTicks()) {
			float px = ToPixelX(x);

			if (axisX.grid.visible) {
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, 0, px, pyMin, 0.0f, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, 0, px, pyMax, 0.0f, 0.0f, ""));
			}

			if (axisX.tick.visible) {
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, px, pyMax, 0.0f, ALIGN_HCENTER | ALIGN_TOP, FormatNumber(x, axisX.scale)));
			}
		}

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.tick.color));
		for (float y : axisY.calculateTicks()) {
			float py = ToPixelY(y);

			if (axisY.grid.visible || (axisY2.visible && axisY2.grid.visible)) {
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, 0, pxMin, py, 0.0f, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, 0, pxMax, py, 0.0f, 0.0f, ""));
			}

			if (axisY.tick.visible) {
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, pxMin, py, 0.0f, ALIGN_RIGHT | ALIGN_VCENTER, FormatNumber(y, axisY.scale)));
			}
		}

		if (axisY2.visible) {
			for (float y2 : axisY2.calculateTicks()) {
				float py = ToPixelY(y2, AXIS_SECONDARY);
				if (axisY.tick.visible) {
					cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.tick.color));
					cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, pxMax, py, 0.0f, ALIGN_LEFT | ALIGN_VCENTER, FormatNumber(y2, axisY2.scale)));
				}
			}
		}

		if (axisX.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisX.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisX.width, 0.0f, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, pxMin, pyMax, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, pxMax, pyMax, 0.0f, 0.0f, ""));
		}
		if (axisY.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisY.width, 0.0f, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, pxMin, pyMin, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, pxMin, pyMax, 0.0f, 0.0f, ""));
		}
		if (axisY2.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisY2.width, 0.0f, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, pxMax, pyMin, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, pxMax, pyMax, 0.0f, 0.0f, ""));
		}
	}

	for (const auto* curve : curves) {
		if (!curve || curve->xData.empty() || curve->yData.empty())
			continue;

		std::vector<std::pair<float, float>> pxpy;

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 20, 0, 0.0f, 0.0f, curve->width, 0.0f, ""));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 20, 0, 0.0f, 0.0f, 0.0f, 0.0f, curve->color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 20, 0, 0.0f, 0.0f, 0.0f, 0.0f, curve->style));

		bool firstPoint = true;
		for (size_t i = 0; i < std::min(curve->xData.size(), curve->yData.size()); ++i) {
			if (curve->xData[i] < axisX.min || curve->xData[i] > axisX.max) {
				firstPoint = true;
				continue;
			}

			if (const Axis& a = (curve->axis == AXIS_SECONDARY) ? axisY2 : axisY;
					curve->yData[i] < a.min || curve->yData[i] > a.max) {
				firstPoint = true;
				continue;
			}

			float px;
			float py;
			if (coord == COORD_CARTESIAN) {
				px = ToPixelX(curve->xData[i]);
				py = ToPixelY(curve->yData[i], curve->axis);
			} else if (coord == COORD_POLAR) {
				auto p = ToPolarPixel(curve->xData[i], curve->yData[i]);
				px = p.first;
				py = p.second;
			}

			if (curve->markerShape != MARKER_NONE)
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
				if (curve->markerShape == MARKER_X) {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px - h, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px - h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py - h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == MARKER_DOT) {
					cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 30, 0, px, py, h, h, curve->markerColor));
				} else if (curve->markerShape == MARKER_PLUS) {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px - h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px, py + h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == MARKER_CIRCLE) {
					cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 30, 0, px, py, h, h, ""));
				} else if (curve->markerShape == MARKER_SQUARE) {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px - h, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px - h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px - h, py - h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == MARKER_TRIANGLE) {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px - h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == MARKER_DIAMOND) {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px + h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px - h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, 0, px, py - h, 0.0f, 0.0f, ""));
				}
			}
		}
	}

	int nret = std::min(cmds.size(), nout);
	for (int i = 0; i < nret; i++) {
		out[i] = cmds.data()[i];
	}
	return nret;
}

