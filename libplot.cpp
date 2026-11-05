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
	Plot<real_t> *plt = new Plot<real_t>(coord);
	return new Handle{ TYPE_PLOT, plt };
}

EXPORT_C void plt_destroy(void *handle_)
{
	if (handle_) {
		Handle *handle = static_cast<Handle *>(handle_);
		if (handle->type == TYPE_PLOT) {
			Plot<real_t> *plt = static_cast<Plot<real_t> *>(handle->obj);
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
				return static_cast<Plot<real_t> *>(handle->obj)->set_option(option, value);
				break;
			case TYPE_CURVE:
				return static_cast<Curve<real_t> *>(handle->obj)->set_option(option, value);
				break;
		}
	}
	return -1;
}

EXPORT_C void plt_zoom(void *plt_, float cx, float cy, float factor)
{
	if (plt_) {
		Handle *handle = static_cast<Handle *>(plt_);
		if (handle->type == TYPE_PLOT) {
			Plot<real_t> *plt = static_cast<Plot<real_t> *>(handle->obj);
			plt->zoom(cx, cy, factor);
		}
	}
}

EXPORT_C void plt_pan(void *plt_, float dx, float dy)
{
	if (plt_) {
		Handle *handle = static_cast<Handle *>(plt_);
		if (handle->type == TYPE_PLOT) {
			Plot<real_t> *plt = static_cast<Plot<real_t> *>(handle->obj);
			plt->pan(dx, dy);
		}
	}
}

EXPORT_C void plt_pix2math(void *plt_, float px, float py, real_t *x, real_t *y, real_t *y2)
{
	if (plt_) {
		Handle *handle = static_cast<Handle *>(plt_);
		if (handle->type == TYPE_PLOT) {
			Plot<real_t> *plt = static_cast<Plot<real_t> *>(handle->obj);
			plt->pix2math(px, py, x, y, y2);
		}
	}
}

EXPORT_C void *plt_curve(void *handle_, const real_t *x, const real_t *y, int count)
{
	if (handle_) {
		Handle *handle = static_cast<Handle *>(handle_);
		Plot<real_t> *plt = nullptr;
		Curve<real_t> *crv = nullptr;
		switch(handle->type) {
			case TYPE_PLOT: {
				plt = static_cast<Plot<real_t> *>(handle->obj);
				crv = new Curve<real_t>(plt);
				if (x && y && count > 0) {
					crv->updateData({x, x+count}, {y, y+count});
				}
				break;
            }
			case TYPE_CURVE: {
				crv = static_cast<Curve<real_t> *>(handle->obj);
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

EXPORT_C void plt_append(void *crv_, const real_t *x, const real_t *y, int count)
{
	if (crv_ && x && y && count > 0) {
    	Handle *handle = static_cast<Handle *>(crv_);
		if (handle->type == TYPE_CURVE) {
			Curve<real_t> *crv = static_cast<Curve<real_t> *>(handle->obj);
			crv->appendData({x, x+count}, {y, y+count});
		}
	}
}

EXPORT_C void plt_remove(void *crv_)
{
	if (crv_) {
		Handle *handle = static_cast<Handle *>(crv_);
		if (handle->type == TYPE_CURVE) {
			Curve<real_t> *crv = static_cast<Curve<real_t> *>(handle->obj);
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
			Plot<real_t> *plt = static_cast<Plot<real_t> *>(handle->obj);
			return plt->render(out, nout);
		}
	}

	return 0;
}

