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
    unsigned int op     : 8;
    unsigned int zIndex : 8;
    unsigned int flags  : 16;
    float x;
    float y;
    float arg1;
    float arg2;
    char arg3[64];
};
enum {
    OP_NONE = 0,

    OP_SET_TEXT_COLOR, // arg3: color
    OP_DRAW_TEXT,      // x, y: position, arg1: angle, arg2: alignment, arg3: text
    OP_SET_LINE_COLOR, // arg3: color
    OP_SET_LINE_STYLE, // arg3: style
    OP_SET_LINE_WIDTH, // arg1: width
    OP_MOVETO,         // x, y: position
    OP_LINETO,         // x, y: position
    OP_DRAW_ELLIPSE,   // x, y: center, arg1/2: length of semi-major/minor, arg3: fill color
	OP_DRAW_RECT,      // x, y: left/top, arg1/2: width/height, arg3: fill color

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

enum {
	ALIGN_LEFT = 0x00000000,
	ALIGN_HCENTER = 0x00000001,
	ALIGN_RIGHT = 0x00000002,

	ALIGN_TOP = 0x00000000,
	ALIGN_VCENTER = 0x00010000,
	ALIGN_BOTTOM = 0x00020000,
	ALIGN_BASELINE = 0x00040000
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
