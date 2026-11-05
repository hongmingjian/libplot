#ifndef LIBPLOT_H
#define LIBPLOT_H

/**
 * NOTE: Change here to use float or double for your coordinate
 */
typedef float real_t;
//typedef double real_t;

#ifndef LIBPLOT_EXPORTS
/**
 * create and destroy a plot.
 */
void *plt_create(int coord);
void plt_destroy(void *plt);

/**
 * The `handle' is a handle of plot/curve.
 * Please look at Plot::set_option and Curve::set_option for options.
 */
int plt_set_option(void *handle, const char *option, const char *value);

/**
 * dx, dy - delta in x, y direction.
 * cx, cy - x, y of center for zoom in/out.
 * factor - <1 for zoom out, >1 for zoom in
 *
 * dx, dy, cx, cy are all in pixels.
 */
void plt_pan(void *plt, float dx, float dy);
void plt_zoom(void *plt, float cx, float cy, float factor);

/**
 * Get coordinates for given screen position (px, py) in pixels.
 *
 * If secondary(right) Y axis is not visible, y2 is not touched.
 */
void plt_pix2math(void *plt, float px, float py, real_t *x, real_t *y, real_t *y2);

/**
 * If `handle' is a handle of plot, this function will create a new curve within the plot and
 * return the handle of newly created curve.
 *
 * And if `handle' is a handle of curve, it will update the data of specified curve.
 */
void *plt_curve(void *handle, const real_t *x, const real_t *y, int count);

/**
 * Append data to the end of an existing curve.
 */
void plt_append(void *crv, const real_t *x, const real_t *y, int count);

/**
 * Remove the curve and destroy it.
 */
void plt_remove(void *crv);

/**
 * Render the coordinate system and curves, then output the instructions.
 */
int plt_render(void *plt, struct plt_render_cmd *out, int nout);
#endif

struct plt_render_cmd {
    unsigned int op     : 8;
    unsigned int zIndex : 8;
    unsigned int unused : 16;
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

#endif // LIBPLOT_H
