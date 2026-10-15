#include "libplot.hpp"

enum { TYPE_FIGURE, TYPE_CURVE };
struct Handle {
    int type;
    void *obj;
};

EXPORT_C void *create_figure(int coord)
{
	Figure *fig = new Figure(coord);
	return new Handle{ TYPE_FIGURE, fig };
}

EXPORT_C void destroy_figure(void *handle_)
{
	if (handle_) {
		Handle *handle = static_cast<Handle *>(handle_);
		if (handle->type == TYPE_FIGURE) {
			Figure *fig = static_cast<Figure *>(handle->obj);
			delete fig;
			delete handle;
		}
	}
}

EXPORT_C int set_option(void *handle_, const char *option, const char *value)
{
	if (handle_) {
		Handle *handle = static_cast<Handle *>(handle_);
		switch(handle->type) {
			case TYPE_FIGURE:
				return static_cast<Figure *>(handle->obj)->set_option(option, value);
				break;
			case TYPE_CURVE:
				return static_cast<Curve *>(handle->obj)->set_option(option, value);
				break;
		}
	}
	return -1;
}

EXPORT_C void zoom(void *fig_, int cx, int cy, float factor)
{
	if (fig_) {
		Handle *handle = static_cast<Handle *>(fig_);
		if (handle->type == TYPE_FIGURE) {
			Figure *fig = static_cast<Figure *>(handle->obj);
			fig->zoom(cx, cy, factor);
		}
	}
}

EXPORT_C void pan(void *fig_, int dx, int dy)
{
	if (fig_) {
		Handle *handle = static_cast<Handle *>(fig_);
		if (handle->type == TYPE_FIGURE) {
			Figure *fig = static_cast<Figure *>(handle->obj);
			fig->pan(dx, dy);
		}
	}
}

EXPORT_C void pixel2logical(void *fig_, int px, int py, float *lx, float *ly, float *ly2)
{
	if (fig_) {
		Handle *handle = static_cast<Handle *>(fig_);
		if (handle->type == TYPE_FIGURE) {
			Figure *fig = static_cast<Figure *>(handle->obj);
			fig->pixel2logical(px, py, lx, ly, ly2);
		}
	}
}

EXPORT_C void *plot_curve(void *handle_, const float *x, const float *y, int count)
{
	if (handle_ && x && y && count > 0) {
		Handle *handle = static_cast<Handle *>(handle_);
		switch(handle->type) {
			case TYPE_FIGURE: {
				Figure *fig = static_cast<Figure *>(handle->obj);
				Curve *curve = new Curve();
				curve->figure = fig;
				curve->xData.assign(x, x + count);
				curve->yData.assign(y, y + count);
				fig->curves.push_back(curve);
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

EXPORT_C void append_points(void *handle_, const float *x, const float *y, int count)
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

EXPORT_C int remove_curve(void *fig_, void *curve_)
{
	if (fig_ && curve_) {
		Handle *hfig = static_cast<Handle *>(fig_);
		Handle *hcurve = static_cast<Handle *>(curve_);
		if (hfig->type == TYPE_FIGURE && hcurve->type == TYPE_CURVE) {
			Figure *fig = static_cast<Figure *>(hfig->obj);
			Curve *curve = static_cast<Curve *>(hcurve->obj);

			auto it = std::find(fig->curves.begin(), fig->curves.end(), curve);
			if (it != fig->curves.end()) {
				delete curve;//*it;
				fig->curves.erase(it);
				delete hcurve;
				return 0;
			}
		}
    }
    return -1;
}

EXPORT_C int show(void *fig_, struct draw_command *out, int nout)
{
	if (fig_) {
		Handle *handle = static_cast<Handle *>(fig_);
		if (handle->type == TYPE_FIGURE) {
			Figure *fig = static_cast<Figure *>(handle->obj);
			return fig->show(out, nout);
		}
	}

	return 0;
}

#include <sstream>
#include <iomanip>

static std::string FormatNumber(double val, int scale)
{
	std::ostringstream oss;
	if (scale == SCALE_LOG && (val >= 1000.0 || val <= 0.001)) {
		oss << std::scientific << std::setprecision(0) << val;
		return oss.str();
	}
	oss << std::setprecision(4) << val;
	return oss.str();
}

inline struct draw_command draw_command_(int op, float x, float y, float argf1, const std::string& argc1, int zIndex)
{
    struct draw_command cmd{op, x, y, argf1, "", zIndex };
    strncpy_s(cmd.argc1, sizeof(cmd.argc1), argc1.c_str(), _TRUNCATE);
    return cmd;
}

int Figure::show(struct draw_command *out, size_t nout) const
{
	std::vector<struct draw_command> cmds;

	if (coord == COORD_POLAR) {
		auto [cx, cy] = ToPolarPixel(0.0, 0.0);

		for (float theta : axisX.calculateTicks()) {
			if (theta <= axisX.min)
				continue;
			std::pair<float, float> outer_edge = ToPolarPixel(theta, axisY.max);

			if (axisX.grid.visible) {
				cmds.push_back(draw_command_(OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, axisX.grid.color, 10));
				cmds.push_back(draw_command_(OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, axisX.grid.style, 10));
				cmds.push_back(draw_command_(OP_SET_LINE_WIDTH, 0.0f, 0.0f, 1, "", 10));
				cmds.push_back(draw_command_(OP_MOVETO, cx, cy, 0.0f, "", 10));
				cmds.push_back(draw_command_(OP_LINETO, outer_edge.first, outer_edge.second, 0.0f, "", 10));
			}

			if (axisX.tick.visible) {
				float text_px = outer_edge.first + 15.0f * static_cast<float>(std::cos(theta));
				float text_py = outer_edge.second - 15.0f * static_cast<float>(std::sin(theta));
				cmds.push_back(draw_command_(OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, axisX.tick.color, 10));
				cmds.push_back(draw_command_(OP_SET_TEXT_FONT, 0.0f, 0.0f, 0.0f, axisX.tick.font, 10));
				cmds.push_back(draw_command_(OP_DRAW_TEXT, text_px, text_py, theta+(theta>M_PI?M_PI_2:-M_PI_2), FormatNumber(180 * theta / M_PI, SCALE_LINEAR), 10));
			}
		}

		for (float r : axisY.calculateTicks()) {
			if (r <= axisY.min)
				continue;
			std::pair<float, float> edge = ToPolarPixel(0.0, r);
			float pixelR = std::abs(edge.first - cx);
			if (pixelR > 0) {
				if (axisY.grid.visible) {
					cmds.push_back(draw_command_(OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, axisY.grid.color, 10));
					cmds.push_back(draw_command_(OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, axisY.grid.style, 10));
					cmds.push_back(draw_command_(OP_SET_LINE_WIDTH, 0.0f, 0.0f, 1, "", 10));
					cmds.push_back(draw_command_(OP_DRAW_CIRCLE, cx, cy, pixelR, "", 10));
				}
				if (axisY.tick.visible) {
					cmds.push_back(draw_command_(OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, axisY.tick.color, 10));
					cmds.push_back(draw_command_(OP_SET_TEXT_FONT, 0.0f, 0.0f, 0.0f, axisY.tick.font, 10));
					cmds.push_back(draw_command_(OP_DRAW_TEXT, edge.first, edge.second-2.0f, 0.0f, FormatNumber(r, SCALE_LINEAR), 10));
				}
			}
		}

		std::pair<float, float> max_edge = ToPolarPixel(0.0, axisY.max);
		if (axisX.visible) {
			float max_pixel_r = std::abs(max_edge.first - cx);
			if (max_pixel_r > 0) {
				cmds.push_back(draw_command_(OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, axisX.color, 50));
				cmds.push_back(draw_command_(OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, axisX.style, 50));
				cmds.push_back(draw_command_(OP_SET_LINE_WIDTH, 0.0f, 0.0f, axisX.width, "", 50));
				cmds.push_back(draw_command_(OP_DRAW_CIRCLE, cx, cy, max_pixel_r, "", 50));
			}
		}

		if (axisY.visible) {
			cmds.push_back(draw_command_(OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, axisY.color, 50));
			cmds.push_back(draw_command_(OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, axisY.style, 50));
			cmds.push_back(draw_command_(OP_SET_LINE_WIDTH, 0.0f, 0.0f, axisY.width, "", 50));

			std::pair<float, float> max_edge2;

			//max_edge = ToPolarPixel(0.0, axisY.max);
			max_edge2 = ToPolarPixel(M_PI, axisY.max);
			cmds.push_back(draw_command_(OP_MOVETO, max_edge.first, max_edge.second, 0.0f, "", 50));
			cmds.push_back(draw_command_(OP_LINETO, max_edge2.first, max_edge2.second, 0.0f, "", 50));

			max_edge = ToPolarPixel(M_PI_2, axisY.max);
			max_edge2 = ToPolarPixel(3 * M_PI_2, axisY.max);
			cmds.push_back(draw_command_(OP_MOVETO, max_edge.first, max_edge.second, 0.0f, "", 50));
			cmds.push_back(draw_command_(OP_LINETO, max_edge2.first, max_edge2.second, 0.0f, "", 50));
		}

	} else if (coord ==  COORD_CARTESIAN) {

		float pxMin = ToPixelX(axisX.min);
		float pxMax = ToPixelX(axisX.max);
		float pyMin = ToPixelY(axisY.max);//!
		float pyMax = ToPixelY(axisY.min);//!

		for (float x : axisX.calculateTicks()) {
			float px = ToPixelX(x);

			if (axisX.grid.visible) {
				cmds.push_back(draw_command_(OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, axisX.grid.color, 10));
				cmds.push_back(draw_command_(OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, axisX.grid.style, 10));
				cmds.push_back(draw_command_(OP_SET_LINE_WIDTH, 0.0f, 0.0f, 1, "", 10));
				cmds.push_back(draw_command_(OP_MOVETO, px, pyMin, 0.0f, "", 10));
				cmds.push_back(draw_command_(OP_LINETO, px, pyMax, 0.0f, "", 10));
			}

			if (axisX.tick.visible) {
				cmds.push_back(draw_command_(OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, axisX.tick.color, 10));
				cmds.push_back(draw_command_(OP_SET_TEXT_FONT, 0.0f, 0.0f, 0.0f, axisX.tick.font, 10));
				cmds.push_back(draw_command_(OP_DRAW_TEXT, px-6.0f, pyMax+15.0f, 0.0f, FormatNumber(x, axisX.scale), 10));
			}
		}

		for (float y : axisY.calculateTicks()) {
			float py = ToPixelY(y);

			if (axisY.grid.visible || (axisY2.visible && axisY2.grid.visible)) {
				cmds.push_back(draw_command_(OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, axisY.grid.color, 10));
				cmds.push_back(draw_command_(OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, axisY.grid.style, 10));
				cmds.push_back(draw_command_(OP_SET_LINE_WIDTH, 0.0f, 0.0f, 1, "", 10));
				cmds.push_back(draw_command_(OP_MOVETO, pxMin, py, 0.0f, "", 10));
				cmds.push_back(draw_command_(OP_LINETO, pxMax, py, 0.0f, "", 10));
			}

			if (axisY.tick.visible) {
				cmds.push_back(draw_command_(OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, axisY.tick.color, 10));
				cmds.push_back(draw_command_(OP_SET_TEXT_FONT, 0.0f, 0.0f, 0.0f, axisY.tick.font, 10));
				cmds.push_back(draw_command_(OP_DRAW_TEXT, pxMin-45.0f, py-6.0f, 0.0f, FormatNumber(y, axisY.scale), 10));
			}
		}

		if (axisY2.visible) {
			for (float y2 : axisY2.calculateTicks()) {
				float py = ToPixelY(y2, AXIS_SECONDARY);
				if (axisY.tick.visible) {
					cmds.push_back(draw_command_(OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, axisY2.tick.color, 10));
					cmds.push_back(draw_command_(OP_SET_TEXT_FONT, 0.0f, 0.0f, 0.0f, axisY2.tick.font, 10));
					cmds.push_back(draw_command_(OP_DRAW_TEXT, pxMax+10.0f, py-6.0f, 0.0f, FormatNumber(y2, axisY2.scale), 10));
				}
			}
		}

		if (axisX.visible) {
			cmds.push_back(draw_command_(OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, axisX.color, 50));
			cmds.push_back(draw_command_(OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, axisX.style, 50));
			cmds.push_back(draw_command_(OP_SET_LINE_WIDTH, 0.0f, 0.0f, axisX.width, "", 50));

			cmds.push_back(draw_command_(OP_MOVETO, pxMin, pyMax, 0.0f, "", 50));
			cmds.push_back(draw_command_(OP_LINETO, pxMax, pyMax, 0.0f, "", 50));
		}
		if (axisY.visible) {
			cmds.push_back(draw_command_(OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, axisY.color, 50));
			cmds.push_back(draw_command_(OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, axisY.style, 50));
			cmds.push_back(draw_command_(OP_SET_LINE_WIDTH, 0.0f, 0.0f, axisY.width, "", 50));

			cmds.push_back(draw_command_(OP_MOVETO, pxMin, pyMin, 0.0f, "", 50));
			cmds.push_back(draw_command_(OP_LINETO, pxMin, pyMax, 0.0f, "", 50));
		}
		if (axisY2.visible) {
			cmds.push_back(draw_command_(OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, axisY2.color, 50));
			cmds.push_back(draw_command_(OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, axisY2.style, 50));
			cmds.push_back(draw_command_(OP_SET_LINE_WIDTH, 0.0f, 0.0f, axisY2.width, "", 50));

			cmds.push_back(draw_command_(OP_MOVETO, pxMax, pyMin, 0.0f, "", 50));
			cmds.push_back(draw_command_(OP_LINETO, pxMax, pyMax, 0.0f, "", 50));
		}
	}

	int nret = std::min(cmds.size(), nout);
	for (int i = 0; i < nret; i++) {
		out[i] = cmds.data()[i];
	}
	return nret;
}

