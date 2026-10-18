#ifndef LIBPLOT_H
#define LIBPLOT_H

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

struct plt_render_cmd {
    int op;
	unsigned int zIndex : 8;
	unsigned int flags  : 24;
    float x;
    float y;
	float arg1;
    char arg2[64];
};
enum {
    OP_NONE = 0,

    OP_SET_TEXT_COLOR,
    OP_SET_TEXT_FONT,
    OP_DRAW_TEXT,
    OP_SET_LINE_COLOR,
    OP_SET_LINE_STYLE,
    OP_SET_LINE_WIDTH,
    OP_MOVETO,
    OP_LINETO,
    OP_DRAW_CIRCLE,

	OP_COUNT
};

enum {
	COORD_CARTESIAN = 0,
	COORD_POLAR = 1
};

enum {
	AXIS_PRIMARY = 0,
	AXIS_SECONDARY = 1
};

enum {
	SCALE_LINEAR = 0,
	SCALE_LOG = 1
};

EXPORT_C void *plt_create(int coord);
EXPORT_C void plt_destroy(void *plt);

EXPORT_C int plt_set_option(void *handle, const char *option, const char *value);

EXPORT_C void plt_pan(void *plt, int dx, int dy);
EXPORT_C void plt_zoom(void *plt, int cx, int cy, float factor);
EXPORT_C void plt_pix2math(void *plt, int px, int py, float *x, float *y, float *y2);

EXPORT_C void *plt_curve(void *handle, const float *x, const float *y, int count);
EXPORT_C void plt_append(void *crv, const float *x, const float *y, int count);
EXPORT_C void plt_remove(void *plt, void *crv);

EXPORT_C int plt_render(void *plt, struct plt_render_cmd *out, int nout);

#endif // LIBPLOT_H
