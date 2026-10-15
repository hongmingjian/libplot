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

struct draw_command {
    int op;
    float x;
    float y;
	float argf1;
    char argc1[64];
    int zIndex;
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

EXPORT_C void *create_figure(int coord);
EXPORT_C void destroy_figure(void *fig);

EXPORT_C int set_option(void *obj, const char *option, const char *value);

EXPORT_C void pan(void *fig, int dx, int dy);
EXPORT_C void zoom(void *fig, int cx, int cy, float factor);
EXPORT_C void pixel2logical(void *fig, int px, int py, float *lx, float *ly, float *ly2);

EXPORT_C void *plot_curve(void *obj, const float *x, const float *y, int count);
EXPORT_C void append_points(void *line, const float *x, const float *y, int count);
EXPORT_C int remove_curve(void *fig, void *line);

EXPORT_C int show(void *fig, struct draw_command *out, int nout);

#endif // LIBPLOT_H
