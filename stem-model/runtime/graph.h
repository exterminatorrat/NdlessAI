#ifndef GRAPH_H
#define GRAPH_H

#include <stdint.h>

#include "expr.h"

/*
 * Plotting for the calculator: parse y = f(x), map it onto the 320x240
 * framebuffer, and drive pan, zoom and trace.
 *
 * Two ways in. A typed expression is plotted as given. A call the pipeline
 * produced is plotted as the relation it names, against one of its own
 * arguments - the formula comes from steptable.h, which check_steps.py already
 * proves reproduces the solver's answer, so the curve and the number are the
 * same relation rather than two attempts at it.
 *
 * The parser and evaluator live in expr.h; this module owns the view, the
 * clipper, the analysis and the renderer. Nothing here depends on Ndless, so
 * the host tests exercise the same transform, clipping and rendering code the
 * calculator runs.
 */

#define G_SCREEN_W 320
#define G_SCREEN_H 240

/*
 * The plot is framed the way the rest of the program is: a title bar on top and
 * two rows of hints underneath. Every function below works in plot coordinates
 * (0..G_PLOT_W-1 across, 0..G_PLOT_H-1 down) and the drawing helpers add
 * G_PLOT_Y on the way to the framebuffer, so mapping, clipping and the analysis
 * kept their meanings when the chrome arrived.
 */
#define G_HEADER_H 13
#define G_FOOTER_H 26
#define G_PLOT_W   320
#define G_PLOT_H   (G_SCREEN_H - G_HEADER_H - G_FOOTER_H)
#define G_PLOT_Y   G_HEADER_H
#define G_FOOTER_Y (G_PLOT_Y + G_PLOT_H)

#define G_TEXT_MAX    64
#define G_MAX_ROOTS   8
#define G_MAX_EXTREMA 4

/* The call this screen can be opened from is kept as text; a call carries at
 * most eight arguments, as solvers_solve accepts. */
#define G_CALL_MAX 128
#define G_MAX_ARGS 8

/* A plot plus the members of a parameter family. */
#define G_MAX_CURVES 4
#define G_LABEL_MAX  24

typedef struct {
	q16_t xmin, xmax, ymin, ymax;
} gview_t;

enum {
	GK_NONE, GK_UP, GK_DOWN, GK_LEFT, GK_RIGHT,
	GK_ZOOM_IN, GK_ZOOM_OUT, GK_TRACE, GK_RESET,
	/* Only meaningful when the screen was opened from a call. */
	GK_SWEEP, GK_FAMILY,
	GK_QUIT
};

/*
 * One plotted relation. A typed expression has one; a call has one per
 * parameter value, so the substitution is written once and the renderer walks
 * the array.
 */
typedef struct {
	gexpr_t expr;
	char label[G_LABEL_MAX];
	uint16_t colour;
} gcurve_t;

/*
 * A slope field is its own grid rather than a change to the curve list: the
 * segments are a property of the window, not of a function of x, and they are
 * recomputed when pan or zoom moves the window.
 */
#define G_FIELD_COLS 16
#define G_FIELD_ROWS 12
#define G_FIELD_STEPS 64
#define G_FIELD_TRACE_MAX (2 * G_FIELD_STEPS + 1)

typedef struct {
	gexpr_t expr;                             /* y' = f(x, y) */
	int on;
	int cols, rows;
	int16_t dx[G_FIELD_COLS * G_FIELD_ROWS];  /* half-extents in 1/64 pixel */
	int16_t dy[G_FIELD_COLS * G_FIELD_ROWS];
	q16_t ic_x, ic_y;                         /* where the traced solution starts */
	int steps;                                /* RK4 steps each way, 0 for none */
	int ntrace;
	q16_t tx[G_FIELD_TRACE_MAX];
	q16_t ty[G_FIELD_TRACE_MAX];
} gfield_t;

typedef struct {
	gcurve_t curve[G_MAX_CURVES];
	int ncurves;
	char text[G_TEXT_MAX];
	gview_t view;
	gview_t home;   /* what GK_RESET goes back to */
	int trace;
	int cursor_px;
	q16_t roots[G_MAX_ROOTS];
	int nroots;
	q16_t ext_x[G_MAX_EXTREMA];
	q16_t ext_y[G_MAX_EXTREMA];
	int nextrema;

	/* The answer a call names, as a point on its own curve. */
	int has_mark;
	q16_t mark_x, mark_y;
	char mark_label[G_LABEL_MAX];

	/* The call this screen was opened from, kept so a key can replot it. */
	char call[G_CALL_MAX];
	int sweep;      /* 1-based argument index; 0 when nothing is swept */

	gfield_t field;
} gstate_t;

enum {
	G_CALL_OK = 0,
	G_CALL_SYNTAX = -1,   /* not a CALL line, or an argument is not a number */
	G_CALL_UNKNOWN = -2,  /* no step template for that relation */
	G_CALL_NO_ARG = -3,   /* the template uses an argument the call did not give */
	G_CALL_NO_SWEEP = -4, /* the template does not use the argument to sweep */
	G_CALL_NO_FORM = -5   /* the formula is outside what the evaluator accepts */
};

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

/*
 * Plots the relation a call named, sweeping argument `sweep` over a window
 * around the call's own value and marking the point the call asked about.
 * `sweep` is a 1-based argument index; 0 means the last argument the
 * relation's formula uses.
 *
 * Returns G_CALL_OK, or a negative G_CALL_* code with a sentence in `err` when
 * the call has no plottable form.
 */
int graph_from_call(gstate_t *g, const char *call, int sweep, char *err, int errlen);

/*
 * Whether graph_from_call would accept `call`: the same checks without a graph
 * state, so a screen can offer the plot key only for an answer that has one.
 */
int graph_call_plottable(const char *call);

/*
 * Adds the same call with argument `arg` at `value`, one curve each, so a
 * relation can be seen at several values of a parameter at once. The swept
 * argument stays the x axis. Returns G_CALL_OK or a negative G_CALL_* code.
 */
int graph_family_add(gstate_t *g, int arg, q16_t value, char *err, int errlen);

/*
 * Plots y' = f(x,y) as a slope field over `window`, and traces the solution
 * through (ic_x, ic_y) when `steps` is positive. A NULL window means the
 * default [-5,5] x [-5,5]. Returns G_CALL_OK, or G_CALL_NO_FORM when the
 * text does not parse or the window has no area.
 */
int graph_field(gstate_t *g, const char *text, const gview_t *window,
                q16_t ic_x, q16_t ic_y, int steps, char *err, int errlen);

/* Applies one key. Returns 1 when the caller should leave graph mode. */
int graph_key(gstate_t *g, int key);

/* Draws the whole screen into a 320x240 RGB565 buffer, chrome included. */
void graph_render(const gstate_t *g, uint16_t *fb);

#endif
