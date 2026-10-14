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

static char *safe_strncpy(char *buf, const char *src, size_t bufsize)
{
	std::strncpy(buf, src, bufsize - 1);
	buf[bufsize - 1] = '\0';
	return buf;
}

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

int Figure::show(struct draw_command *out, size_t nout) const
{
	std::vector<struct draw_command> cmds;
	int xTicksCount = 10;//static_cast<int>(GetOpt("xTargetTicks", 6.0));
	int yTicksCount = 5;//static_cast<int>(GetOpt("leftYTargetTicks", 5.0));

	if (coord == COORD_POLAR) {
		auto [cx, cy] = ToPolarPixel(0.0, 0.0);

		auto yTickValues = axisY.calculateTicks(yTicksCount);
		for (float r : yTickValues) {
			if (r <= axisY.min)
				continue;
			std::pair<float, float> edge = ToPolarPixel(0.0, r);
			float pixelR = std::abs(edge.first - cx);
			if (pixelR > 0) {
				cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
				cmds.push_back(draw_command{OP_DRAW_CIRCLE, cx, cy, pixelR, "", 10});
				cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#C0C0C0", 10});
				cmds.push_back(draw_command{OP_DRAW_TEXT, edge.first, edge.second-2.0f, 0.0f, "", 10});
				safe_strncpy(cmds.back().argc1, FormatNumber(r, SCALE_LINEAR).c_str(), sizeof(draw_command::argc1));
			}
		}

		auto xTickValues = axisX.calculateTicks(xTicksCount);
		for (float theta : xTickValues) {
			if (theta <= axisX.min)
				continue;
			std::pair<float, float> outer_edge = ToPolarPixel(theta, axisY.max);

			cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#C0C0C0", 10});
			cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
			cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 10});
			cmds.push_back(draw_command{OP_LINETO, outer_edge.first, outer_edge.second, 0.0f, "", 10});

			float text_px = outer_edge.first + 15.0f * static_cast<float>(std::cos(theta));
			float text_py = outer_edge.second - 15.0f * static_cast<float>(std::sin(theta));
			cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#000000", 10});
			cmds.push_back(draw_command{OP_DRAW_TEXT, text_px, text_py, 0.0f, "", 10});
			safe_strncpy(cmds.back().argc1, FormatNumber(180 * theta / M_PI, SCALE_LINEAR).c_str(), sizeof(draw_command::argc1));
		}

		std::pair<float, float> max_edge = ToPolarPixel(0.0, axisY.max);
		float max_pixel_r = std::abs(max_edge.first - cx);
		if (max_pixel_r > 0) {
			cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#000000", 50});
			cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, "-", 50});
			cmds.push_back(draw_command{OP_SET_LINE_WIDTH, 0.0f, 0.0f, 1.5f, "", 50});
			cmds.push_back(draw_command{OP_DRAW_CIRCLE, cx, cy, max_pixel_r, "", 50});

			std::pair<float, float> xx;

			xx = ToPolarPixel(0, axisY.max);
			cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 50});
			cmds.push_back(draw_command{OP_LINETO, xx.first, xx.second, 0.0f, "", 50});

			xx = ToPolarPixel(M_PI / 2, axisY.max);
			cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 50});
			cmds.push_back(draw_command{OP_LINETO, xx.first, xx.second, 0.0f, "", 50});

			xx = ToPolarPixel(M_PI, axisY.max);
			cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 50});
			cmds.push_back(draw_command{OP_LINETO, xx.first, xx.second, 0.0f, "", 50});

			xx = ToPolarPixel(3 * M_PI / 2, axisY.max);
			cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 50});
			cmds.push_back(draw_command{OP_LINETO, xx.first, xx.second, 0.0f, "", 50});
		}

	} else if (coord ==  COORD_CARTESIAN) {

		float pxMin = ToPixelX(axisX.min);
		float pxMax = ToPixelX(axisX.max);
		float pyMin = ToPixelY(axisY.min);
		float pyMax = ToPixelY(axisY.max);

		auto xTickValues = axisX.calculateTicks(xTicksCount);
		for (float x : xTickValues) {
			float px = ToPixelX(x);

			cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#C0C0C0", 10});
			cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
			cmds.push_back(draw_command{OP_MOVETO, px, pyMin+1, 0.0f, "", 10});
			cmds.push_back(draw_command{OP_LINETO, px, pyMax, 0.0f, "", 10});

			cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#333333", 10});
			cmds.push_back(draw_command{OP_DRAW_TEXT, px-6.0f, pyMin+15.0f, 0.0f, "", 10});
			safe_strncpy(cmds.back().argc1, FormatNumber(x, axisX.scale).c_str(), sizeof(draw_command::argc1));
		}

		auto yTickValues = axisY.calculateTicks(yTicksCount);
		for (float y : yTickValues) {
			float py = ToPixelY(y);

			cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#C0C0C0", 10});
			cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
			cmds.push_back(draw_command{OP_MOVETO, pxMin+1, py, 0.0f, "", 10});
			cmds.push_back(draw_command{OP_LINETO, pxMax, py, 0.0f, "", 10});

			cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#0000FF", 10});
			cmds.push_back(draw_command{OP_DRAW_TEXT, pxMin-45.0f, py-6.0f, 0.0f, "", 10});
			safe_strncpy(cmds.back().argc1, FormatNumber(y, axisY.scale).c_str(), sizeof(draw_command::argc1));
		}

		if (axisY2.visible) {
			auto y2TickValues = axisY2.calculateTicks(yTicksCount);

			for (float y2 : y2TickValues) {
				float py = ToPixelY(y2, AXIS_SECONDARY);

				cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#008000", 10});
				cmds.push_back(draw_command{OP_DRAW_TEXT, pxMax+10.0f, py-6.0f, 0.0f, "", 10});
				safe_strncpy(cmds.back().argc1, FormatNumber(y2, axisY2.scale).c_str(), sizeof(draw_command::argc1));
			}
		}

		cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, "-", 50});
		cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#000000", 50});
		cmds.push_back(draw_command{OP_SET_LINE_WIDTH, 0.0f, 0.0f, 1.5f, "", 50});
		cmds.push_back(draw_command{OP_MOVETO, pxMin, pyMax, 0.0f, "", 50});
		cmds.push_back(draw_command{OP_LINETO, pxMin, pyMin, 0.0f, "", 50});
		cmds.push_back(draw_command{OP_LINETO, pxMax, pyMin, 0.0f, "", 50});
		if (axisY2.visible)
			cmds.push_back(draw_command{OP_LINETO, pxMax, pyMax, 0.0f, "", 50});
	}

	int nret = std::min(cmds.size(), nout);
	for (int i = 0; i < nret; i++) {
		out[i] = cmds.data()[i];
	}
	return nret;
}

