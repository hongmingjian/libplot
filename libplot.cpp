#include "libplot.hpp"
#include <windows.h>

enum { TYPE_FIGURE, TYPE_CURVE };

struct Handle {
    int type;
    void *obj;
};

EXPORT_C void *create_figure(int coord)
{
#ifdef _DEBUG
    if (GetConsoleWindow() != NULL) {
        FILE* pFile;
        freopen_s(&pFile, "CONOUT$", "w", stdout);
        freopen_s(&pFile, "CONOUT$", "w", stderr);
    }
#endif
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
