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
	if (handle_ && x && y && count > 0) {
		Handle *handle = static_cast<Handle *>(handle_);
		switch(handle->type) {
			case TYPE_PLOT: {
				Plot *plt = static_cast<Plot *>(handle->obj);
				Curve *curve = new Curve();
				curve->plot = plt;
				curve->xData.assign(x, x + count);
				curve->yData.assign(y, y + count);
				plt->curves.push_back(curve);
				return new Handle{ TYPE_CURVE, curve };
            }
			case TYPE_CURVE: {
				Curve *curve = static_cast<Curve *>(handle->obj);
				curve->xData.assign(x, x + count);
				curve->yData.assign(y, y + count);
				return handle_;
            }
		}
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
		int op, unsigned int zIndex, unsigned int flags,
		float x, float y, float arg1, const std::string& arg2)
{
    struct plt_render_cmd cmd{op, zIndex, flags, x, y, arg1, ""};
    strncpy_s(cmd.arg2, sizeof(cmd.arg2), arg2.c_str(), _TRUNCATE);
    return cmd;
}

int Plot::render(struct plt_render_cmd *out, size_t nout) const
{
	std::vector<struct plt_render_cmd> cmds;

	if (coord == COORD_POLAR) {
		auto [cx, cy] = ToPolarPixel(0.0, 0.0);

		for (float theta : axisX.calculateTicks()) {
			if (theta <= axisX.min)
				continue;
			std::pair<float, float> outer_edge = ToPolarPixel(theta, axisY.max);

			if (axisX.grid.visible) {
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, axisX.grid.color));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, axisX.grid.style));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0, 0.0f, 0.0f, 1, ""));
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, 0, cx, cy, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, 0, outer_edge.first, outer_edge.second, 0.0f, ""));
			}

			if (axisX.tick.visible) {
				float text_px = outer_edge.first + 15.0f * static_cast<float>(std::cos(theta));
				float text_py = outer_edge.second - 15.0f * static_cast<float>(std::sin(theta));
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, axisX.tick.color));
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_FONT, 10, 0, 0.0f, 0.0f, 0.0f, axisX.tick.font));
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, text_px, text_py, theta+(theta>M_PI?M_PI_2:-M_PI_2), FormatNumber(180 * theta / M_PI, SCALE_LINEAR)));
			}
		}

		for (float r : axisY.calculateTicks()) {
			if (r <= axisY.min)
				continue;
			std::pair<float, float> edge = ToPolarPixel(0.0, r);
			float pixelR = std::abs(edge.first - cx);
			if (pixelR > 0) {
				if (axisY.grid.visible) {
					cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, axisY.grid.color));
					cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, axisY.grid.style));
					cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0, 0.0f, 0.0f, 1, ""));
					cmds.push_back(plt_render_cmd_(OP_DRAW_CIRCLE, 10, 0, cx, cy, pixelR, ""));
				}
				if (axisY.tick.visible) {
					cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, axisY.tick.color));
					cmds.push_back(plt_render_cmd_(OP_SET_TEXT_FONT, 10, 0, 0.0f, 0.0f, 0.0f, axisY.tick.font));
					cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, edge.first, edge.second-2.0f, 0.0f, FormatNumber(r, SCALE_LINEAR)));
				}
			}
		}

		std::pair<float, float> max_edge = ToPolarPixel(0.0, axisY.max);
		if (axisX.visible) {
			float max_pixel_r = std::abs(max_edge.first - cx);
			if (max_pixel_r > 0) {
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, axisX.color));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, axisX.style));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisX.width, ""));
				cmds.push_back(plt_render_cmd_(OP_DRAW_CIRCLE, 50, 0, cx, cy, max_pixel_r, ""));
			}
		}

		if (axisY.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, axisY.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, axisY.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisY.width, ""));

			std::pair<float, float> max_edge2;

			//max_edge = ToPolarPixel(0.0, axisY.max);
			max_edge2 = ToPolarPixel(M_PI, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, max_edge.first, max_edge.second, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, max_edge2.first, max_edge2.second, 0.0f, ""));

			max_edge = ToPolarPixel(M_PI_2, axisY.max);
			max_edge2 = ToPolarPixel(3 * M_PI_2, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, max_edge.first, max_edge.second, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, max_edge2.first, max_edge2.second, 0.0f, ""));
		}

	} else if (coord ==  COORD_CARTESIAN) {

		float pxMin = ToPixelX(axisX.min);
		float pxMax = ToPixelX(axisX.max);
		float pyMin = ToPixelY(axisY.max);//!
		float pyMax = ToPixelY(axisY.min);//!

		for (float x : axisX.calculateTicks()) {
			float px = ToPixelX(x);

			if (axisX.grid.visible) {
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, axisX.grid.color));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, axisX.grid.style));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0, 0.0f, 0.0f, 1, ""));
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, 0, px, pyMin, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, 0, px, pyMax, 0.0f, ""));
			}

			if (axisX.tick.visible) {
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, axisX.tick.color));
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_FONT, 10, 0, 0.0f, 0.0f, 0.0f, axisX.tick.font));
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, px-6.0f, pyMax+15.0f, 0.0f, FormatNumber(x, axisX.scale)));
			}
		}

		for (float y : axisY.calculateTicks()) {
			float py = ToPixelY(y);

			if (axisY.grid.visible || (axisY2.visible && axisY2.grid.visible)) {
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, axisY.grid.color));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0, 0.0f, 0.0f, 0.0f, axisY.grid.style));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0, 0.0f, 0.0f, 1, ""));
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, 0, pxMin, py, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, 0, pxMax, py, 0.0f, ""));
			}

			if (axisY.tick.visible) {
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, axisY.tick.color));
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_FONT, 10, 0, 0.0f, 0.0f, 0.0f, axisY.tick.font));
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, pxMin-45.0f, py-6.0f, 0.0f, FormatNumber(y, axisY.scale)));
			}
		}

		if (axisY2.visible) {
			for (float y2 : axisY2.calculateTicks()) {
				float py = ToPixelY(y2, AXIS_SECONDARY);
				if (axisY.tick.visible) {
					cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0, 0.0f, 0.0f, 0.0f, axisY2.tick.color));
					cmds.push_back(plt_render_cmd_(OP_SET_TEXT_FONT, 10, 0, 0.0f, 0.0f, 0.0f, axisY2.tick.font));
					cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, 0, pxMax+10.0f, py-6.0f, 0.0f, FormatNumber(y2, axisY2.scale)));
				}
			}
		}

		if (axisX.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, axisX.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, axisX.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisX.width, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, pxMin, pyMax, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, pxMax, pyMax, 0.0f, ""));
		}
		if (axisY.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, axisY.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, axisY.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisY.width, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, pxMin, pyMin, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, pxMin, pyMax, 0.0f, ""));
		}
		if (axisY2.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0, 0.0f, 0.0f, 0.0f, axisY2.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0, 0.0f, 0.0f, 0.0f, axisY2.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0, 0.0f, 0.0f, axisY2.width, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, 0, pxMax, pyMin, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, 0, pxMax, pyMax, 0.0f, ""));
		}
	}

	int nret = std::min(cmds.size(), nout);
	for (int i = 0; i < nret; i++) {
		out[i] = cmds.data()[i];
	}
	return nret;
}

