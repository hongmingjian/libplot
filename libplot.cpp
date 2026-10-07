#include "libplot.hpp"

EXPORT_C void *create_figure(void)
{
	return static_cast<void *>(new Figure());
}

EXPORT_C void destroy_figure(void *fig_)
{
	if (fig_) {
		Figure *fig = static_cast<Figure *>(fig_);
		delete fig;
	}
}

EXPORT_C int set_option(void *fig_, const char *option, const char *value)
{
	if (fig_) {
		Figure *fig = static_cast<Figure *>(fig_);
		return fig->set_option(option, value);
	}
	return -1;
}

EXPORT_C void zoom(void *fig_, int cx, int cy, float factor)
{
	if (fig_) {
		Figure *fig = static_cast<Figure *>(fig_);
		fig->zoom(cx, cy, factor);
	}
}

EXPORT_C void pan(void *fig_, int dx, int dy)
{
	if (fig_) {
		Figure *fig = static_cast<Figure *>(fig_);
		fig->pan(dx, dy);
	}
}

EXPORT_C void pixel2logical(void *fig_, int px, int py, float *lx, float *ly, float *ly2)
{
	if (fig_) {
		Figure *fig = static_cast<Figure *>(fig_);
		fig->pixel2logical(px, py, lx, ly, ly2);
	}
}

EXPORT_C int plot(void *fig_, const float* x, const float* y, int n, struct draw_command* out, int nout)
{
	if (fig_) {
		Figure *fig = static_cast<Figure *>(fig_);
		return fig->plot(std::vector<float>(x, x+n), std::vector<float>(y, y+n), out, nout);
	}
	return -1;
}

