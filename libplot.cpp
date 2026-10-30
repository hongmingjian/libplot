#include "plot.hpp"

#ifdef __cplusplus
#define EXTERN_C extern "C"
#else
#define EXTERN_C
#endif
#ifdef _MSC_VER
#define EXPORT_C EXTERN_C __declspec( dllexport )
#else
#define EXPORT_C EXTERN_C __attribute__((visibility("default")))
#endif

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
					crv->updateData({x, x+count}, {y, y+count});
				}
				break;
            }
			case TYPE_CURVE: {
				crv = static_cast<Curve *>(handle->obj);
				plt = crv->plot;
				if (x && y && count > 0) {
					crv->updateData({x, x+count}, {y, y+count});
				}
				break;
            }
		}

		return (handle->type == TYPE_PLOT) ? new Handle{ TYPE_CURVE, crv } : handle;
	}

	return nullptr;
}

EXPORT_C void plt_append(void *crv_, const float *x, const float *y, int count)
{
	if (crv_ && x && y && count > 0) {
    	Handle *handle = static_cast<Handle *>(crv_);
		if (handle->type == TYPE_CURVE) {
			Curve *crv = static_cast<Curve *>(handle->obj);
			crv->appendData({x, x+count}, {y, y+count});
		}
	}
}

EXPORT_C void plt_remove(void *crv_)
{
	if (crv_) {
		Handle *handle = static_cast<Handle *>(crv_);
		if (handle->type == TYPE_CURVE) {
			Curve *crv = static_cast<Curve *>(handle->obj);
			delete crv;
			delete handle;
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

