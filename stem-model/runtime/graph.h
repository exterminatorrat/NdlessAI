#ifndef GRAPH_H
#define GRAPH_H

#include <stdint.h>

#include "expr.h"

/*
 * Function plotting for the calculator: parse y = f(x), map it onto the
 * 320x240 framebuffer, and drive pan, zoom and trace.
 *
 * The parser and evaluator live in expr.h; this module owns the view, the
 * clipper, the analysis and the renderer. Nothing here depends on Ndless, so
 * the host tests exercise the same transform, clipping and rendering code the
 * calculator runs.
 */

#define G_SCREEN_W 320
#define G_SCREEN_H 240
#define G_PLOT_W   320
#define G_PLOT_H   224
#define G_STATUS_Y 224

#define G_TEXT_MAX    64
#define G_MAX_ROOTS   8
#define G_MAX_EXTREMA 4

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
