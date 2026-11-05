#ifndef GRAPH_H
#define GRAPH_H

#include <stdint.h>

/*
 * Function plotting for the calculator: parse y = f(x), map it onto the
 * 320x240 framebuffer, and drive pan, zoom and trace.
 *
 * Everything here is integer-only. The CPU has no FPU, so coordinates and
 * function values are Q16.16 fixed point and the per-column evaluation loop
 * never touches a float. Nothing in this file depends on Ndless, so the host
 * tests exercise the same transform, clipping and rendering code the
 * calculator runs.
 */

#define G_SCREEN_W 320
#define G_SCREEN_H 240
#define G_PLOT_W   320
#define G_PLOT_H   224
#define G_STATUS_Y 224

#define G_TEXT_MAX    64
#define G_MAX_NODES   48
#define G_MAX_ROOTS   8
#define G_MAX_EXTREMA 4

typedef int32_t q16_t;

#define Q16_ONE ((q16_t)0x00010000)
#define Q16_MAX ((q16_t)0x7FFFFFFF)
#define Q16_MIN ((q16_t)(-0x7FFFFFFF))

enum {
	G_NUM, G_X, G_ADD, G_SUB, G_MUL, G_DIV, G_POW, G_SIN, G_COS, G_TAN, G_NEG
};

typedef struct {
	int16_t op;
	int16_t a;
	int16_t b;
	int32_t num;
} gnode_t;

typedef struct {
	gnode_t node[G_MAX_NODES];
	int n;
	int root;
} gexpr_t;

typedef struct {
	q16_t xmin, xmax, ymin, ymax;
} gview_t;

enum {
	GK_NONE, GK_UP, GK_DOWN, GK_LEFT, GK_RIGHT,
	GK_ZOOM_IN, GK_ZOOM_OUT, GK_TRACE, GK_RESET, GK_QUIT
};

typedef struct {
	gexpr_t expr;
	char text[G_TEXT_MAX];
	gview_t view;
	int trace;
	int cursor_px;
	q16_t roots[G_MAX_ROOTS];
	int nroots;
	q16_t ext_x[G_MAX_EXTREMA];
	q16_t ext_y[G_MAX_EXTREMA];
	int nextrema;
} gstate_t;

/* Q16.16 arithmetic. mul and div saturate instead of wrapping. */
q16_t gq_mul(q16_t a, q16_t b);
q16_t gq_div(q16_t a, q16_t b);
q16_t gq_sin(q16_t x);
q16_t gq_cos(q16_t x);
q16_t gq_tan(q16_t x);

/*
 * Parses an expression in x. Supports + - * / ^, parentheses, decimal
 * numbers, sin/cos/tan, and implicit multiplication ("2x", "3sin(x)").
 * Returns 0 on success; on failure returns -1 and writes a short message
 * into err.
 */
int gexpr_parse(const char *text, gexpr_t *e, char *err, int errlen);
q16_t gexpr_eval(const gexpr_t *e, q16_t x);

/* Picks a view that contains the curve over x in [-10, 10]. */
void gview_autoscale(gview_t *v, const gexpr_t *e);

/*
 * World <-> pixel mapping. Pixel coordinates are relative to the plot rect
 * (0..G_PLOT_W-1, 0..G_PLOT_H-1); y grows downward. Results are clamped to
 * a range that is safe to clip, not to the rect.
 */
int gmap_x(const gview_t *v, q16_t x);
int gmap_y(const gview_t *v, q16_t y);
q16_t gunmap_x(const gview_t *v, int px);
q16_t gunmap_y(const gview_t *v, int py);

/*
 * Cohen-Sutherland clip of a segment to [0,w-1] x [0,h-1]. Returns 1 and
 * rewrites the endpoints when any part is visible, 0 when it is not.
 */
int gclip_seg(int *x0, int *y0, int *x1, int *y1, int w, int h);

/* Parses, autoscales, and finds roots and turning points. */
int graph_init(gstate_t *g, const char *text, char *err, int errlen);

/* Applies one key. Returns 1 when the caller should leave graph mode. */
int graph_key(gstate_t *g, int key);

/* Draws the whole screen into a 320x240 RGB565 buffer. */
void graph_render(const gstate_t *g, uint16_t *fb);

#endif
