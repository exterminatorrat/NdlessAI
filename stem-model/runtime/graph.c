/*
 * Integer-only plotting for the TI-Nspire CX II.
 *
 * The CPU has no FPU, so every coordinate and function value is Q16.16
 * fixed point. The per-column evaluation loop is the hot path: it runs 320
 * times per frame and must not pull in soft-float helpers. Multiplication
 * goes through int64 (a single smull on ARM926EJ-S) and saturates rather
 * than wrapping, so a steep curve leaves the screen instead of folding
 * back into it.
 *
 * A call the pipeline produced plots the relation it named. The formula comes
 * from steptable.h, the same table pipeline_steps renders and check_steps.py
 * proves against the solvers, with the call's arguments substituted for its
 * @1..@n placeholders - so the curve is the relation the answer came from
 * rather than an expression the student had to derive first.
 */
#include <string.h>

#include "expr.h"
#include <stdio.h>

#include "graph.h"
#include "screen.h"
#include "steptable.h"


#define G_BG         0x0000
#define G_GRID       0x2104
#define G_AXIS       0x8410
#define G_CURVE      0x07E0
#define G_ROOT       0xF800
#define G_VERTEX     0xFFE0
#define G_TRACE      0xFFFF
#define G_TRACE_DIM  0x4208
#define G_TEXT       0xFFFF
#define G_MARK       0x07FF
#define G_FIELD_LINE 0x4D7F
#define G_SOLUTION   0xFD20

/* One colour per curve, so a family's members can be told apart and their
 * labels match the line they belong to. */
static const uint16_t g_family_colour[G_MAX_CURVES] = {
	G_CURVE, 0x07FF, 0xFFE0, 0xF81F
};

/* The evaluator's own node pool bounds the text it can hold; a rewritten step
 * template plus eight arguments stays well inside this. */
#define G_EXPR_TEXT 384


static int64_t fdiv(int64_t a, int64_t b)
{
	int64_t q = a / b;

	if ((a % b) != 0 && ((a < 0) != (b < 0)))
		q--;
	return q;
}

static void copy_str(char *dst, int n, const char *src)
{
	int i = 0;

	if (n < 1)
		return;
	while (src && src[i] && i < n - 1) {
		dst[i] = src[i];
		i++;
	}
	dst[i] = 0;
}

/* ---- view ---- */

/*
 * The padding and the degenerate-span guard both autoscalers need. A single
 * expression and a family differ only in how ymin and ymax are found, so they
 * cannot drift into fitting the screen differently.
 */
static void fit_window(gview_t *v, q16_t x0, q16_t x1, q16_t ymin, q16_t ymax,
                       int found)
{
	int64_t span, pad;

	if (!found) {
		ymin = -10 * Q16_ONE;
		ymax = 10 * Q16_ONE;
	}
	span = (int64_t)ymax - ymin;
	pad = span / 10;
	if (pad < Q16_ONE / 2)
		pad = Q16_ONE / 2;
	ymin = gq_sat((int64_t)ymin - pad);
	ymax = gq_sat((int64_t)ymax + pad);
	if ((int64_t)ymax - ymin < Q16_ONE) {
		int64_t c = ((int64_t)ymin + ymax) / 2;

		ymin = gq_sat(c - Q16_ONE);
		ymax = gq_sat(c + Q16_ONE);
	}
	v->xmin = x0;
	v->xmax = x1;
	v->ymin = ymin;
	v->ymax = ymax;
}

void gview_autoscale(gview_t *v, const gexpr_t *e)
{
	q16_t x0 = -10 * Q16_ONE, x1 = 10 * Q16_ONE;
	q16_t ymin = Q16_MAX, ymax = Q16_MIN;
	int found = 0;

	for (int i = 0; i <= 256; i++) {
		q16_t x = gq_sat((int64_t)x0 + (int64_t)(x1 - x0) * i / 256);
		q16_t y = gexpr_eval(e, x);

		if (y == Q16_MAX || y == Q16_MIN)
			continue;
		if (!found) {
			ymin = ymax = y;
			found = 1;
		} else {
			if (y < ymin)
				ymin = y;
			if (y > ymax)
				ymax = y;
		}
	}
	fit_window(v, x0, x1, ymin, ymax, found);
}

int gmap_x(const gview_t *v, q16_t x)
{
	int64_t span = (int64_t)v->xmax - v->xmin;
	int64_t px;

	if (span <= 0)
		return 0;
	px = fdiv(((int64_t)x - v->xmin) * G_PLOT_W, span);
	if (px < -32768)
		px = -32768;
	if (px > 32767)
		px = 32767;
	return (int)px;
}

int gmap_y(const gview_t *v, q16_t y)
{
	int64_t span = (int64_t)v->ymax - v->ymin;
	int64_t py;

	if (span <= 0)
		return 0;
	py = fdiv(((int64_t)v->ymax - y) * G_PLOT_H, span);
	if (py < -32768)
		py = -32768;
	if (py > 32767)
		py = 32767;
	return (int)py;
}

q16_t gunmap_x(const gview_t *v, int px)
{
	int64_t span = (int64_t)v->xmax - v->xmin;

	return gq_sat((int64_t)v->xmin + fdiv((int64_t)px * span, G_PLOT_W));
}

q16_t gunmap_y(const gview_t *v, int py)
{
	int64_t span = (int64_t)v->ymax - v->ymin;

	return gq_sat((int64_t)v->ymax - fdiv((int64_t)py * span, G_PLOT_H));
}

/* ---- clipping ---- */

static int outcode(int x, int y, int w, int h)
{
	int c = 0;

	if (x < 0)
		c |= 1;
	else if (x >= w)
		c |= 2;
	if (y < 0)
		c |= 4;
	else if (y >= h)
		c |= 8;
	return c;
}

int gclip_seg(int *x0, int *y0, int *x1, int *y1, int w, int h)
{
	int ax = *x0, ay = *y0, bx = *x1, by = *y1;
	int ca = outcode(ax, ay, w, h);
	int cb = outcode(bx, by, w, h);

	for (int i = 0; i < 8; i++) {
		int c;
		int64_t nx, ny;

		if (!(ca | cb)) {
			*x0 = ax;
			*y0 = ay;
			*x1 = bx;
			*y1 = by;
			return 1;
		}
		if (ca & cb)
			return 0;
		c = ca ? ca : cb;
		if (c & 1) {
			if (bx == ax)
				return 0;
			nx = 0;
			ny = ay + (int64_t)(by - ay) * (0 - ax) / (bx - ax);
		} else if (c & 2) {
			if (bx == ax)
				return 0;
			nx = w - 1;
			ny = ay + (int64_t)(by - ay) * ((w - 1) - ax) / (bx - ax);
		} else if (c & 4) {
			if (by == ay)
				return 0;
			ny = 0;
			nx = ax + (int64_t)(bx - ax) * (0 - ay) / (by - ay);
		} else {
			if (by == ay)
				return 0;
			ny = h - 1;
			nx = ax + (int64_t)(bx - ax) * ((h - 1) - ay) / (by - ay);
		}
		if (c == ca) {
			ax = (int)nx;
			ay = (int)ny;
			ca = outcode(ax, ay, w, h);
		} else {
			bx = (int)nx;
			by = (int)ny;
			cb = outcode(bx, by, w, h);
		}
	}
	return 0;
}

/* ---- drawing ---- */

static void px_set(uint16_t *fb, int x, int y, uint16_t c)
{
	screen_px(fb, x, y, c);
}

static void line(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t c)
{
	int dx = x1 - x0, dy = y1 - y0;
	int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
	int err;

	if (dx < 0)
		dx = -dx;
	if (dy < 0)
		dy = -dy;
	err = dx - dy;
	for (;;) {
		int e2;

		px_set(fb, x0, y0, c);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = 2 * err;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
}

static void box(uint16_t *fb, int x, int y, uint16_t c)
{
	for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
			px_set(fb, x + dx, y + dy, c);
}

/*
 * A filled disc, two pixels of radius. It has to read as a point on a curve
 * that is already two pixels wide, so a single pixel would disappear into the
 * line it is meant to mark.
 */
static void disc(uint16_t *fb, int x, int y, uint16_t c)
{
	for (int oy = -2; oy <= 2; oy++)
		for (int ox = -2; ox <= 2; ox++)
			if (ox * ox + oy * oy <= 5)
				px_set(fb, x + ox, y + oy, c);
}

/*
 * Q16.16 to a decimal string, trailing zeros trimmed. Three places for an axis
 * label; six for a value substituted back into a formula, which has to round
 * to the same fixed-point number it came from.
 */
static void fmt_fixed(q16_t v, char *out, int n, int digits)
{
	int64_t a = v;
	int64_t ip, fr;
	int64_t scale = 1;
	char tmp[12];
	int t = 0, i = 0, k;

	if (n < 1)
		return;
	if (a < 0) {
		if (i < n - 1)
			out[i++] = '-';
		a = -a;
	}
	for (k = 0; k < digits; k++)
		scale *= 10;
	ip = a >> 16;
	fr = ((a & 0xFFFF) * scale + 32768) >> 16;
	if (fr >= scale) {
		ip++;
		fr = 0;
	}
	if (ip == 0)
		tmp[t++] = '0';
	while (ip > 0) {
		tmp[t++] = (char)('0' + (int)(ip % 10));
		ip /= 10;
	}
	while (t > 0 && i < n - 1)
		out[i++] = tmp[--t];
	if (fr) {
		char d[12];
		int nd = digits;

		for (k = 0; k < digits; k++) {
			int64_t p10 = 1;

			for (int j = 0; j < digits - 1 - k; j++)
				p10 *= 10;
			d[k] = (char)('0' + (int)((fr / p10) % 10));
		}
		while (nd > 0 && d[nd - 1] == '0')
			nd--;
		if (i < n - 1)
			out[i++] = '.';
		for (k = 0; k < nd && i < n - 1; k++)
			out[i++] = d[k];
	}
	out[i] = 0;
}

typedef struct {
	char *b;
	int n;
	int i;
} sbuf_t;

static void sb_ch(sbuf_t *s, char c)
{
	if (s->i < s->n - 1)
		s->b[s->i++] = c;
}

static void sb_str(sbuf_t *s, const char *t)
{
	while (*t)
		sb_ch(s, *t++);
}

static void sb_q16(sbuf_t *s, q16_t v)
{
	char tmp[16];

	fmt_fixed(v, tmp, (int)sizeof tmp, 3);
	sb_str(s, tmp);
}

static const q16_t g_p10[9] = {
	7, 66, 655, 6554, 65536, 655360, 6553600, 65536000, 655360000
};

static q16_t nice_step(q16_t span, int ticks)
{
	int64_t raw;
	int k = 0;
	q16_t cand[4];
	q16_t best;
	int64_t best_err;

	if (span <= 0 || ticks <= 0)
		return Q16_ONE;
	raw = span / ticks;
	if (raw <= g_p10[0])
		return g_p10[0];
	for (int i = 8; i >= 0; i--) {
		if (raw >= g_p10[i]) {
			k = i;
			break;
		}
	}
	if (k == 8)
		return g_p10[8];

	/*
	 * Pick the 1/2/5/10 multiple whose tick count lands closest to the
	 * target. Taking the smallest step that is at least span/ticks instead
	 * would leave a 120-unit y range with only three grid lines.
	 */
	cand[0] = g_p10[k];
	cand[1] = (q16_t)(g_p10[k] * 2);
	cand[2] = (q16_t)(g_p10[k] * 5);
	cand[3] = (q16_t)(g_p10[k] * 10);
	best = cand[0];
	best_err = span / cand[0] - ticks;
	if (best_err < 0)
		best_err = -best_err;
	for (int i = 1; i < 4; i++) {
		int64_t err = span / cand[i] - ticks;

		if (err < 0)
			err = -err;
		if (err < best_err) {
			best = cand[i];
			best_err = err;
		}
	}
	return best;
}

static int64_t first_tick(q16_t lo, q16_t step)
{
	int64_t k = fdiv(lo, step);

	if (k * step < lo)
		k++;
	return k;
}

static void draw_axes(const gstate_t *g, uint16_t *fb)
{
	const gview_t *v = &g->view;
	int64_t xspan = (int64_t)v->xmax - v->xmin;
	int64_t yspan = (int64_t)v->ymax - v->ymin;
	q16_t xs, ys;
	int ax, ay, xaxis, yaxis;

	if (xspan <= 0 || yspan <= 0)
		return;
	xs = nice_step((q16_t)xspan, 6);
	ys = nice_step((q16_t)yspan, 5);
	ax = gmap_x(v, 0);
	ay = gmap_y(v, 0);
	xaxis = ay >= 0 && ay < G_PLOT_H;
	yaxis = ax >= 0 && ax < G_PLOT_W;

	for (int64_t k = first_tick(v->xmin, xs); k * xs <= v->xmax; k++) {
		int64_t t = k * xs;
		int px = gmap_x(v, (q16_t)t);
		int ty, ly;
		char buf[16];
		int w;

		if (px < 0 || px >= G_PLOT_W)
			continue;
		if (t != 0)
			for (int y = 0; y < G_PLOT_H; y++)
				px_set(fb, px, y, G_GRID);
		ty = xaxis ? ay : G_PLOT_H - 1;
		for (int d = 1; d <= 3; d++) {
			px_set(fb, px, ty - d, G_AXIS);
			px_set(fb, px, ty + d, G_AXIS);
		}
		fmt_fixed((q16_t)t, buf, (int)sizeof buf, 3);
		w = (int)strlen(buf) * SCREEN_ADVANCE - 1;
		ly = xaxis ? ay + 4 : G_PLOT_H - 10;
		if (ly + SCREEN_GLYPH_H > G_PLOT_H)
			ly = G_PLOT_H - SCREEN_GLYPH_H - 1;
		screen_text(fb, px - w / 2, ly, buf, G_TEXT);
	}

	for (int64_t k = first_tick(v->ymin, ys); k * ys <= v->ymax; k++) {
		int64_t t = k * ys;
		int py = gmap_y(v, (q16_t)t);
		int tx, lx;
		char buf[16];

		if (py < 0 || py >= G_PLOT_H)
			continue;
		if (t != 0)
			for (int x = 0; x < G_PLOT_W; x++)
				px_set(fb, x, py, G_GRID);
		tx = yaxis ? ax : 0;
		for (int d = 1; d <= 3; d++) {
			px_set(fb, tx - d, py, G_AXIS);
			px_set(fb, tx + d, py, G_AXIS);
		}
		if (t == 0)
			continue;
		fmt_fixed((q16_t)t, buf, (int)sizeof buf, 3);
		lx = yaxis ? ax + 4 : 2;
		screen_text(fb, lx, py - 3, buf, G_TEXT);
	}

	if (xaxis)
		for (int x = 0; x < G_PLOT_W; x++)
			px_set(fb, x, ay, G_AXIS);
	if (yaxis)
		for (int y = 0; y < G_PLOT_H; y++)
			px_set(fb, ax, y, G_AXIS);
}

static void draw_curve(const gstate_t *g, const gcurve_t *c, uint16_t *fb)
{
	const gview_t *v = &g->view;
	int prev_py = 0, have = 0;
	int end_x = -1, end_y = 0;

	for (int px = 0; px < G_PLOT_W; px++) {
		q16_t x = gunmap_x(v, px);
		q16_t y = gexpr_eval(&c->expr, x);
		int py = gmap_y(v, y);

		if (have) {
			int x0 = px - 1, y0 = prev_py, x1 = px, y1 = py;
			int jump = py - prev_py;

			if (jump < 0)
				jump = -jump;
			/* A jump taller than the screen is an asymptote, not a line. */
			if (jump <= 2 * G_PLOT_H &&
			    gclip_seg(&x0, &y0, &x1, &y1, G_PLOT_W, G_PLOT_H))
				line(fb, x0, y0, x1, y1, c->colour);
		}
		prev_py = py;
		have = 1;
		if (py >= 0 && py < G_PLOT_H) {
			end_x = px;
			end_y = py;
		}
	}

	/*
	 * A family's members are only told apart by their labels, and the right
	 * end is where a family separates: three curves through one origin are
	 * a smudge at the left edge.
	 */
	if (c->label[0] && end_x >= 0) {
		int w = screen_text_w(c->label);
		int lx = end_x - w - 2;
		int ly = end_y - SCREEN_LINE_H - 1;

		if (lx < 0)
			lx = 0;
		if (lx + w > G_PLOT_W)
			lx = G_PLOT_W - w;
		if (ly < 0)
			ly = end_y + 3;
		if (ly + SCREEN_GLYPH_H > G_PLOT_H)
			ly = G_PLOT_H - SCREEN_GLYPH_H;
		screen_text(fb, lx, ly, c->label, c->colour);
	}
}

static void draw_markers(const gstate_t *g, uint16_t *fb)
{
	int py0 = gmap_y(&g->view, 0);

	for (int i = 0; i < g->nroots; i++) {
		int px = gmap_x(&g->view, g->roots[i]);

		if (px >= 0 && px < G_PLOT_W && py0 >= 0 && py0 < G_PLOT_H)
			box(fb, px, py0, G_ROOT);
	}
	for (int i = 0; i < g->nextrema; i++) {
		int px = gmap_x(&g->view, g->ext_x[i]);
		int py = gmap_y(&g->view, g->ext_y[i]);

		if (px >= 0 && px < G_PLOT_W && py >= 0 && py < G_PLOT_H)
			box(fb, px, py, G_VERTEX);
	}
}

/*
 * The answer the call asked about, where it falls on its own curve. Drawn last
 * of the plot passes so the curve it marks cannot paint over it: the point of
 * the feature is that the number and the picture visibly agree.
 */
static void draw_answer(const gstate_t *g, uint16_t *fb)
{
	int px, py, w, lx, ly;

	if (!g->has_mark)
		return;
	px = gmap_x(&g->view, g->mark_x);
	py = gmap_y(&g->view, g->mark_y);
	if (px < 0 || px >= G_PLOT_W || py < 0 || py >= G_PLOT_H)
		return;
	disc(fb, px, py, G_MARK);

	w = screen_text_w(g->mark_label);
	lx = px + 5;
	ly = py - SCREEN_GLYPH_H + 1;
	if (lx + w > G_PLOT_W)
		lx = px - w - 5;
	if (lx < 0)
		lx = 0;
	if (ly < 0)
		ly = py + 4;
	if (ly + SCREEN_GLYPH_H > G_PLOT_H)
		ly = G_PLOT_H - SCREEN_GLYPH_H;
	screen_text(fb, lx, ly, g->mark_label, G_MARK);
}

static void draw_trace(const gstate_t *g, uint16_t *fb)
{
	int px, py;

	if (!g->trace || g->ncurves < 1)
		return;
	px = g->cursor_px;
	py = gmap_y(&g->view, gexpr_eval(&g->curve[0].expr, gunmap_x(&g->view, px)));
	for (int y = 0; y < G_PLOT_H; y++)
		px_set(fb, px, y, G_TRACE_DIM);
	if (py >= 0 && py < G_PLOT_H)
		box(fb, px, py, G_TRACE);
}

static void draw_status(const gstate_t *g, uint16_t *fb)
{
	char buf[G_TEXT_MAX + 24];
	sbuf_t s;

	for (int y = G_STATUS_Y; y < G_SCREEN_H; y++)
		for (int x = 0; x < G_SCREEN_W; x++)
			fb[y * G_SCREEN_W + x] = G_BG;

	s.b = buf;
	s.n = (int)sizeof buf;
	s.i = 0;
	if (g->trace) {
		q16_t x = gunmap_x(&g->view, g->cursor_px);

		sb_str(&s, "X=");
		sb_q16(&s, x);
		sb_str(&s, "  Y=");
		sb_q16(&s, gexpr_eval(&g->curve[0].expr, x));
	} else {
		sb_str(&s, g->text);
	}
	buf[s.i] = 0;
	screen_text(fb, 2, G_STATUS_Y + 4, buf, G_TEXT);
}

/* ---- analysis ---- */

static void add_root(gstate_t *g, q16_t x)
{
	int64_t span = (int64_t)g->view.xmax - g->view.xmin;

	if (g->nroots >= G_MAX_ROOTS)
		return;
	if (g->nroots > 0) {
		int64_t d = (int64_t)x - g->roots[g->nroots - 1];

		if (d < 0)
			d = -d;
		if (d * 1024 < span)
			return;
	}
	g->roots[g->nroots++] = x;
}

static void add_extremum(gstate_t *g, q16_t x, q16_t y)
{
	if (g->nextrema >= G_MAX_EXTREMA)
		return;
	g->ext_x[g->nextrema] = x;
	g->ext_y[g->nextrema] = y;
	g->nextrema++;
}

static q16_t refine_extremum(const gexpr_t *e, q16_t lo, q16_t hi, int want_max)
{
	for (int i = 0; i < 24; i++) {
		int64_t d = ((int64_t)hi - lo) / 3;
		q16_t m1, m2, f1, f2;

		if (d <= 0)
			break;
		m1 = gq_sat((int64_t)lo + d);
		m2 = gq_sat((int64_t)hi - d);
		f1 = gexpr_eval(e, m1);
		f2 = gexpr_eval(e, m2);
		if (want_max ? (f1 < f2) : (f1 > f2))
			lo = m1;
		else
			hi = m2;
	}
	return gq_sat(((int64_t)lo + hi) / 2);
}

static void analyze(gstate_t *g)
{
	const gview_t *v = &g->view;
	int64_t span = (int64_t)v->xmax - v->xmin;
	const gexpr_t *e;
	q16_t x0, y0, x1, y1;

	g->nroots = 0;
	g->nextrema = 0;
	if (g->ncurves < 1 || span <= 0)
		return;
	e = &g->curve[0].expr;

	x0 = v->xmin;
	y0 = gexpr_eval(e, x0);
	x1 = gq_sat((int64_t)v->xmin + span / 512);
	y1 = gexpr_eval(e, x1);
	for (int i = 2; i <= 512 && g->nextrema < G_MAX_EXTREMA; i++) {
		q16_t x2 = gq_sat((int64_t)v->xmin + span * i / 512);
		q16_t y2 = gexpr_eval(e, x2);
		int d1 = (y1 > y0) - (y1 < y0);
		int d2 = (y2 > y1) - (y2 < y1);

		if (d1 != 0 && d2 != 0 && d1 != d2) {
			q16_t ex = refine_extremum(e, x0, x2, d1 > 0);

			add_extremum(g, ex, gexpr_eval(e, ex));
		}
		x0 = x1;
		y0 = y1;
		x1 = x2;
		y1 = y2;
	}

	x0 = v->xmin;
	y0 = gexpr_eval(e, x0);
	for (int i = 1; i <= 512 && g->nroots < G_MAX_ROOTS; i++) {
		q16_t x = gq_sat((int64_t)v->xmin + span * i / 512);
		q16_t y = gexpr_eval(e, x);

		if (y0 == 0) {
			add_root(g, x0);
		} else if (y != 0 && ((y0 < 0) != (y < 0))) {
			q16_t lo = x0, hi = x, flo = y0;

			for (int j = 0; j < 24; j++) {
				q16_t mid = gq_sat(((int64_t)lo + hi) / 2);
				q16_t fm = gexpr_eval(e, mid);

				if (fm == 0) {
					lo = hi = mid;
					break;
				}
				if ((flo < 0) != (fm < 0)) {
					hi = mid;
				} else {
					lo = mid;
					flo = fm;
				}
			}
			add_root(g, gq_sat(((int64_t)lo + hi) / 2));
		}
		x0 = x;
		y0 = y;
	}

	/*
	 * A double root touches zero without changing sign, so it is only
	 * visible as a turning point sitting on the axis. The test is absolute,
	 * not a fraction of the y span: a cubic's local extremum can be a few
	 * units off the axis while the span is thousands of units tall, and
	 * scaling by the span would call that a root.
	 */
	for (int i = 0; i < g->nextrema; i++) {
		int64_t ay = g->ext_y[i] < 0 ? -(int64_t)g->ext_y[i] : g->ext_y[i];

		if (ay < 16)
			add_root(g, g->ext_x[i]);
	}
}

/* ---- calls ---- */

/*
 * Splits "CALL name a b c" in place. The call text is the only thing this
 * screen has to go on - the same string pipeline_steps renders and the solvers
 * were given - so the curve and the answer cannot disagree about which numbers
 * they mean.
 */
static int split_call(char *work, const char *call, const char **name,
                      const char **args, int *nargs)
{
	char *parts[G_MAX_ARGS + 2];
	size_t len = strlen(call);
	int n = 0;

	if (len + 1 > G_CALL_MAX)
		return -1;
	memcpy(work, call, len + 1);
	for (size_t i = 0; i < len && n < G_MAX_ARGS + 2;) {
		while (i < len && work[i] == ' ')
			work[i++] = 0;
		if (i >= len)
			break;
		parts[n++] = work + i;
		while (i < len && work[i] != ' ')
			i++;
	}
	if (n < 2 || strcmp(parts[0], "CALL") != 0)
		return -1;
	if (n - 2 > G_MAX_ARGS)
		return -1;
	*name = parts[1];
	*nargs = n - 2;
	for (int k = 0; k < *nargs; k++)
		args[k] = parts[k + 2];
	return 0;
}

/*
 * An argument's value, as the fixed-point number the text names.
 *
 * Only plain decimals are accepted. The marker is placed at the argument's own
 * value, so an argument read as something with a different value would put the
 * point in the wrong place - which is worse than refusing to plot it.
 */
static int arg_value(const char *s, q16_t *out)
{
	int64_t ip = 0, frac = 0, den = 1, v;
	int neg = 0, fdig = 0, any = 0;

	if (*s == '-') {
		neg = 1;
		s++;
	}
	while (*s >= '0' && *s <= '9') {
		if (ip < 100000)
			ip = ip * 10 + (*s - '0');
		s++;
		any = 1;
	}
	if (*s == '.') {
		s++;
		while (*s >= '0' && *s <= '9') {
			if (fdig < 9) {
				frac = frac * 10 + (*s - '0');
				den *= 10;
				fdig++;
			}
			s++;
			any = 1;
		}
	}
	if (!any || *s)
		return -1;
	v = (ip << 16) + (frac << 16) / den;
	if (v > Q16_MAX)
		return -1;
	*out = (q16_t)(neg ? -v : v);
	return 0;
}

/* The arguments a template names, as one bit per 1-based index. */
static unsigned template_args(const char *tmpl)
{
	unsigned mask = 0;
	int i = 0;

	while (tmpl[i]) {
		int idx = 0;

		if (tmpl[i] != '@') {
			i++;
			continue;
		}
		i++;
		if (tmpl[i] < '0' || tmpl[i] > '9')
			continue;
		while (tmpl[i] >= '0' && tmpl[i] <= '9')
			idx = idx * 10 + (tmpl[i++] - '0');
		if (idx >= 1 && idx < 32)
			mask |= 1u << idx;
	}
	return mask;
}

/* The last argument a template uses, which is what a sweep of 0 picks: it is
 * the one the relation is usually written to be read against. */
static int template_last_arg(const char *tmpl)
{
	unsigned m = template_args(tmpl);
	int last = 0;

	for (int i = 1; i < 32; i++)
		if (m & (1u << i))
			last = i;
	return last;
}

static int put_s(char **at, char *end, const char *s)
{
	while (*s && *at < end)
		*(*at)++ = *s++;
	return *s ? -1 : 0;
}

static int put_c(char **at, char *end, char c)
{
	if (*at >= end)
		return -1;
	*(*at)++ = c;
	return 0;
}

/*
 * Rewrites a step template into the text the evaluator parses: "@k" becomes
 * the call's k-th argument, and the swept argument becomes x.
 *
 * Every value is bracketed, so a negative argument is squared as a whole
 * rather than read as a negation of the square, and so a number cannot run
 * into the letter after it. An exponent in the template is folded into a power
 * of ten, because the evaluator has no exponent notation and reads "1e-6" as
 * 1*e*(-6) - a different formula from the one the solver used.
 *
 * Returns 0, or -1 when an argument is missing or the text does not fit.
 */
static int build_text(char *out, int outlen, const char *tmpl,
                      const char *const *args, int nargs, int sweep)
{
	char *at = out, *end = out + outlen - 1;
	const char *p = tmpl;

	while (*p) {
		if (*p == '@') {
			int idx = 0;

			p++;
			if (*p < '0' || *p > '9')
				return -1;
			while (*p >= '0' && *p <= '9')
				idx = idx * 10 + (*p++ - '0');
			if (idx < 1 || idx > nargs)
				return -1;
			if (idx == sweep) {
				if (put_s(&at, end, "(x)") < 0)
					return -1;
			} else if (put_c(&at, end, '(') < 0 ||
			           put_s(&at, end, args[idx - 1]) < 0 ||
			           put_c(&at, end, ')') < 0) {
				return -1;
			}
			continue;
		}
		if (*p >= '0' && *p <= '9') {
			const char *q = p;
			const char *e;

			while (*q >= '0' && *q <= '9')
				q++;
			if (*q == '.') {
				q++;
				while (*q >= '0' && *q <= '9')
					q++;
			}
			e = q;
			if ((*e == 'e' || *e == 'E') &&
			    (e[1] == '+' || e[1] == '-' ||
			     (e[1] >= '0' && e[1] <= '9'))) {
				const char *dig = e + 1;

				if (*dig == '+' || *dig == '-')
					dig++;
				if (*dig >= '0' && *dig <= '9') {
					const char *k;

					while (*dig >= '0' && *dig <= '9')
						dig++;
					if (put_c(&at, end, '(') < 0)
						return -1;
					for (k = p; k < e; k++)
						if (put_c(&at, end, *k) < 0)
							return -1;
					if (put_s(&at, end, "*10^") < 0)
						return -1;
					for (k = e + 1; k < dig; k++)
						if (put_c(&at, end, *k) < 0)
							return -1;
					if (put_c(&at, end, ')') < 0)
						return -1;
					p = dig;
					continue;
				}
			}
			for (const char *k = p; k < q; k++)
				if (put_c(&at, end, *k) < 0)
					return -1;
			p = q;
			continue;
		}
		if (put_c(&at, end, *p++) < 0)
			return -1;
	}
	*at = 0;
	return 0;
}

static const step_row_t *find_step(const char *name)
{
	for (int i = 0; i < STEP_NUM_ROWS; i++)
		if (strcmp(step_rows[i].name, name) == 0)
			return &step_rows[i];
	return NULL;
}

/*
 * Adds one curve: the relation `name`, its arguments, swept over `sweep`. A
 * lone plot and every member of a parameter family come through here, so the
 * substitution is written once.
 */
static int curve_add(gstate_t *g, const char *name, const char *const *args,
                     int nargs, int sweep, const char *label, uint16_t colour,
                     char *err, int errlen)
{
	char text[G_EXPR_TEXT];
	const step_row_t *row;
	gexpr_t e;
	int idx;

	row = find_step(name);
	if (!row || !row->expr || !row->expr[0]) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "no formula for %s", name);
		return G_CALL_UNKNOWN;
	}
	if (sweep < 1 || sweep > nargs) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "%s has no argument %d", name, sweep);
		return G_CALL_NO_ARG;
	}
	if (!(template_args(row->expr) & (1u << sweep))) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "%s does not use argument %d",
			         name, sweep);
		return G_CALL_NO_SWEEP;
	}
	if (g->ncurves >= G_MAX_CURVES) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "no room for another curve");
		return G_CALL_NO_ARG;
	}
	if (build_text(text, (int)sizeof text, row->expr, args, nargs, sweep) < 0) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "%s needs more numbers", name);
		return G_CALL_NO_ARG;
	}
	if (gexpr_parse(text, &e, err, errlen) < 0)
		return G_CALL_NO_FORM;
	/* A formula in y describes a differential equation, not a curve of x. */
	if (gexpr_has_y(&e)) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "%s is an equation in y", name);
		return G_CALL_NO_FORM;
	}
	idx = g->ncurves++;
	g->curve[idx].expr = e;
	copy_str(g->curve[idx].label, G_LABEL_MAX, label);
	g->curve[idx].colour = colour;
	return G_CALL_OK;
}

/* Fits the window to every curve on the screen. */
static void autoscale_curves(gview_t *v, const gstate_t *g, q16_t x0, q16_t x1)
{
	q16_t ymin = Q16_MAX, ymax = Q16_MIN;
	int found = 0;

	for (int i = 0; i < g->ncurves; i++) {
		for (int k = 0; k <= 128; k++) {
			q16_t x = gq_sat((int64_t)x0 + ((int64_t)x1 - x0) * k / 128);
			q16_t y = gexpr_eval(&g->curve[i].expr, x);

			if (y == Q16_MAX || y == Q16_MIN)
				continue;
			if (!found) {
				ymin = ymax = y;
				found = 1;
			} else {
				if (y < ymin)
					ymin = y;
				if (y > ymax)
					ymax = y;
			}
		}
	}
	fit_window(v, x0, x1, ymin, ymax, found);
}

/*
 * The window a call opens on: the student's own value doubled, so their value
 * sits in the middle with the same again on either side. A sweep value of zero
 * has no scale to work from, so it falls back to the usual ten units.
 */
static void call_window(gstate_t *g)
{
	int64_t lo, hi;

	if (g->mark_x > 0) {
		lo = 0;
		hi = (int64_t)g->mark_x * 2;
	} else if (g->mark_x < 0) {
		lo = (int64_t)g->mark_x * 2;
		hi = 0;
	} else {
		lo = -10 * Q16_ONE;
		hi = 10 * Q16_ONE;
	}
	autoscale_curves(&g->view, g, gq_sat(lo), gq_sat(hi));
}

int graph_from_call(gstate_t *g, const char *call, int sweep, char *err, int errlen)
{
	char work[G_CALL_MAX];
	const char *name = NULL;
	const char *args[G_MAX_ARGS];
	const step_row_t *row;
	q16_t value, other;
	int nargs = 0, rc;

	memset(g, 0, sizeof *g);
	g->cursor_px = G_PLOT_W / 2;
	if (!call || split_call(work, call, &name, args, &nargs) < 0) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "that is not a call");
		return G_CALL_SYNTAX;
	}
	row = find_step(name);
	if (!row || !row->expr || !row->expr[0]) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "no formula for %s", name);
		return G_CALL_UNKNOWN;
	}
	if (sweep <= 0)
		sweep = template_last_arg(row->expr);
	if (sweep < 1 || sweep > nargs) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "%s has no argument %d", name, sweep);
		return G_CALL_NO_ARG;
	}
	for (int k = 0; k < nargs; k++) {
		if (arg_value(args[k], &other) < 0) {
			if (err && errlen > 0)
				snprintf(err, (size_t)errlen, "%s is not a number", args[k]);
			return G_CALL_SYNTAX;
		}
	}
	if (arg_value(args[sweep - 1], &value) < 0) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "%s is not a number",
			         args[sweep - 1]);
		return G_CALL_SYNTAX;
	}
	rc = curve_add(g, name, args, nargs, sweep, NULL, G_CURVE, err, errlen);
	if (rc != G_CALL_OK)
		return rc;

	copy_str(g->text, G_TEXT_MAX, call);
	copy_str(g->call, G_CALL_MAX, call);
	g->sweep = sweep;
	g->mark_x = value;
	g->mark_y = gexpr_eval(&g->curve[0].expr, value);
	if (g->mark_y != Q16_MAX && g->mark_y != Q16_MIN) {
		g->has_mark = 1;
		if (gexpr_format(g->mark_y, g->mark_label, G_LABEL_MAX) < 0)
			fmt_fixed(g->mark_y, g->mark_label, G_LABEL_MAX, 3);
	}
	call_window(g);
	g->home = g->view;
	analyze(g);
	return G_CALL_OK;
}

int graph_call_plottable(const char *call)
{
	char work[G_CALL_MAX];
	const char *name = NULL;
	const char *args[G_MAX_ARGS];
	const step_row_t *row;
	q16_t value;
	int nargs = 0, sweep;

	if (!call || split_call(work, call, &name, args, &nargs) < 0)
		return 0;
	row = find_step(name);
	if (!row || !row->expr || !row->expr[0])
		return 0;
	sweep = template_last_arg(row->expr);
	if (sweep < 1 || sweep > nargs)
		return 0;
	for (int k = 0; k < nargs; k++)
		if (arg_value(args[k], &value) < 0)
			return 0;
	return 1;
}

int graph_family_add(gstate_t *g, int arg, q16_t value, char *err, int errlen)
{
	char work[G_CALL_MAX];
	char text[24];
	const char *name = NULL;
	const char *args[G_MAX_ARGS];
	const char *given[G_MAX_ARGS];
	uint16_t colour;
	int nargs = 0, rc;

	if (!g->call[0] || split_call(work, g->call, &name, args, &nargs) < 0) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "no call to vary");
		return G_CALL_SYNTAX;
	}
	if (arg < 1 || arg > nargs) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "there is no argument %d", arg);
		return G_CALL_NO_ARG;
	}
	if (arg == g->sweep) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "argument %d is the x axis", arg);
		return G_CALL_NO_ARG;
	}
	for (int k = 0; k < nargs; k++)
		given[k] = args[k];
	fmt_fixed(value, text, (int)sizeof text, 6);
	given[arg - 1] = text;
	colour = g_family_colour[g->ncurves < G_MAX_CURVES ? g->ncurves : 0];
	rc = curve_add(g, name, given, nargs, g->sweep, text, colour, err, errlen);
	if (rc != G_CALL_OK)
		return rc;
	call_window(g);
	g->home = g->view;
	analyze(g);
	return G_CALL_OK;
}

/*
 * The sweep key: the same call against its next argument. Arguments the
 * formula does not use are skipped, because sweeping one would draw a flat
 * line - the sort of plot this screen refuses elsewhere.
 */
static int call_next_sweep(gstate_t *g)
{
	char work[G_CALL_MAX];
	char saved[G_CALL_MAX];
	const char *name = NULL;
	const char *args[G_MAX_ARGS];
	const step_row_t *row;
	unsigned used;
	int nargs = 0;

	if (!g->call[0] || split_call(work, g->call, &name, args, &nargs) < 0)
		return G_CALL_SYNTAX;
	row = find_step(name);
	if (!row || !row->expr || !row->expr[0])
		return G_CALL_UNKNOWN;
	used = template_args(row->expr);
	/* graph_from_call rebuilds the state, so the call text it matches against
	 * has to outlive it. */
	copy_str(saved, G_CALL_MAX, g->call);
	for (int k = 1; k <= nargs; k++) {
		int want = g->sweep + k;

		if (want > nargs)
			want = (want - 1) % nargs + 1;
		if (!(used & (1u << want)))
			continue;
		return graph_from_call(g, saved, want, NULL, 0);
	}
	return G_CALL_NO_ARG;
}

/*
 * The family key: three curves at half, the call's own value and double it, on
 * the first argument the formula uses that is not the x axis. An argument that
 * is zero there would draw the same line three times, so the values step 1, 2
 * and 3 instead.
 */
static int call_family(gstate_t *g)
{
	char work[G_CALL_MAX];
	char text[3][24];
	const char *name = NULL;
	const char *args[G_MAX_ARGS];
	const char *given[G_MAX_ARGS];
	const step_row_t *row;
	unsigned used;
	q16_t base, vals[3];
	int nargs = 0, arg = 0, rc;

	if (!g->call[0] || split_call(work, g->call, &name, args, &nargs) < 0)
		return G_CALL_SYNTAX;
	row = find_step(name);
	if (!row || !row->expr || !row->expr[0])
		return G_CALL_UNKNOWN;
	used = template_args(row->expr) & ~(1u << g->sweep);
	for (int k = 1; k <= nargs; k++) {
		if (used & (1u << k)) {
			arg = k;
			break;
		}
	}
	if (!arg)
		return G_CALL_NO_ARG;
	if (arg_value(args[arg - 1], &base) < 0)
		return G_CALL_SYNTAX;
	if (base > 0) {
		vals[0] = gq_sat((int64_t)base / 2);
		vals[1] = base;
		vals[2] = gq_sat((int64_t)base * 2);
	} else if (base < 0) {
		vals[0] = gq_sat((int64_t)base * 2);
		vals[1] = base;
		vals[2] = gq_sat((int64_t)base / 2);
	} else {
		vals[0] = Q16_ONE;
		vals[1] = 2 * Q16_ONE;
		vals[2] = 3 * Q16_ONE;
	}
	for (int k = 0; k < nargs; k++)
		given[k] = args[k];
	g->ncurves = 0;
	for (int k = 0; k < 3; k++) {
		fmt_fixed(vals[k], text[k], (int)sizeof text[k], 6);
		given[arg - 1] = text[k];
		rc = curve_add(g, name, given, nargs, g->sweep, text[k],
		               g_family_colour[k], NULL, 0);
		if (rc != G_CALL_OK) {
			g->ncurves = 0;
			return rc;
		}
	}
	call_window(g);
	g->home = g->view;
	analyze(g);
	return G_CALL_OK;
}

/* ---- slope fields ---- */

/* Cell centres in world coordinates, in the order gexpr_slope_field fills its
 * segments: column c left to right, row r from the top down. */
static q16_t cell_x(const gview_t *v, int c, int cols)
{
	return gq_sat((int64_t)v->xmin +
	              ((int64_t)v->xmax - v->xmin) * (2 * c + 1) / (2 * cols));
}

static q16_t cell_y(const gview_t *v, int r, int rows)
{
	return gq_sat((int64_t)v->ymax -
	              ((int64_t)v->ymax - v->ymin) * (2 * r + 1) / (2 * rows));
}

/* 1/64-pixel half-extents to whole pixels, rounding away from zero so a short
 * segment does not vanish between them. */
static int half_px(int16_t v)
{
	int a = v < 0 ? -v : v;
	int n = (a + 32) / 64;

	return v < 0 ? -n : n;
}

static void push_trace(gfield_t *f, q16_t x, q16_t y)
{
	if (f->ntrace < G_FIELD_TRACE_MAX) {
		f->tx[f->ntrace] = x;
		f->ty[f->ntrace] = y;
		f->ntrace++;
	}
}

/*
 * The solution through the initial condition, as one polyline ordered by x:
 * backwards to the left edge, then forwards to the right one.
 *
 * gexpr_rk4 returns only the y values, so each point's x is rebuilt here with
 * the same integer step it divides by, and the points therefore sit where the
 * run put them. A step that saturates ends the line at the blow-up rather than
 * joining across it.
 */
static void build_trace(gstate_t *g)
{
	gfield_t *f = &g->field;
	q16_t back[G_FIELD_STEPS];
	int n = f->steps > G_FIELD_STEPS ? G_FIELD_STEPS : f->steps;
	int nb = 0;

	f->ntrace = 0;
	if (n < 1)
		return;

	if (f->ic_x > g->view.xmin) {
		q16_t h = gq_sat(((int64_t)g->view.xmin - f->ic_x) / n);
		q16_t ys[G_FIELD_STEPS + 1];

		if (h && gexpr_rk4(&f->expr, f->ic_x, f->ic_y, g->view.xmin, n, ys,
		                   n + 1) == GEXPR_DIFF_OK) {
			for (int i = 1; i <= n && nb < G_FIELD_STEPS; i++) {
				if (ys[i] == Q16_MAX || ys[i] == Q16_MIN)
					break;
				back[nb++] = ys[i];
			}
			for (int i = nb - 1; i >= 0; i--)
				push_trace(f, gq_sat((int64_t)f->ic_x + (int64_t)h * (i + 1)),
				           back[i]);
		}
	}

	push_trace(f, f->ic_x, f->ic_y);

	if (f->ic_x < g->view.xmax) {
		q16_t h = gq_sat(((int64_t)g->view.xmax - f->ic_x) / n);
		q16_t ys[G_FIELD_STEPS + 1];

		if (h && gexpr_rk4(&f->expr, f->ic_x, f->ic_y, g->view.xmax, n, ys,
		                   n + 1) == GEXPR_DIFF_OK)
			for (int i = 1; i <= n; i++) {
				if (ys[i] == Q16_MAX || ys[i] == Q16_MIN)
					break;
				push_trace(f, gq_sat((int64_t)f->ic_x + (int64_t)h * i), ys[i]);
			}
	}
}

/* The segments are a property of the window, so moving the window rebuilds
 * them; nothing else in the state has to be told. */
static int field_build(gstate_t *g)
{
	gfield_t *f = &g->field;

	f->cols = G_FIELD_COLS;
	f->rows = G_FIELD_ROWS;
	if (gexpr_slope_field(&f->expr, g->view.xmin, g->view.xmax, g->view.ymin,
	                      g->view.ymax, f->cols, f->rows, f->dx,
	                      f->dy) != GEXPR_DIFF_OK)
		return -1;
	build_trace(g);
	return 0;
}

int graph_field(gstate_t *g, const char *text, const gview_t *window,
                q16_t ic_x, q16_t ic_y, int steps, char *err, int errlen)
{
	memset(g, 0, sizeof *g);
	g->cursor_px = G_PLOT_W / 2;
	if (!text || gexpr_parse(text, &g->field.expr, err, errlen) < 0)
		return G_CALL_NO_FORM;
	if (window) {
		g->view = *window;
	} else {
		g->view.xmin = -5 * Q16_ONE;
		g->view.xmax = 5 * Q16_ONE;
		g->view.ymin = -5 * Q16_ONE;
		g->view.ymax = 5 * Q16_ONE;
	}
	if (g->view.xmax <= g->view.xmin || g->view.ymax <= g->view.ymin) {
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "the window has no area");
		return G_CALL_NO_FORM;
	}
	g->home = g->view;
	g->field.on = 1;
	g->field.ic_x = ic_x;
	g->field.ic_y = ic_y;
	g->field.steps = steps;
	copy_str(g->text, G_TEXT_MAX, text);
	if (field_build(g) < 0) {
		g->field.on = 0;
		if (err && errlen > 0)
			snprintf(err, (size_t)errlen, "the field does not fit this window");
		return G_CALL_NO_FORM;
	}
	return G_CALL_OK;
}

static void draw_field(const gstate_t *g, uint16_t *fb)
{
	const gfield_t *f = &g->field;

	for (int r = 0; r < f->rows; r++) {
		for (int c = 0; c < f->cols; c++) {
			int i = r * f->cols + c;
			int cx = gmap_x(&g->view, cell_x(&g->view, c, f->cols));
			int cy = gmap_y(&g->view, cell_y(&g->view, r, f->rows));
			int hx = half_px(f->dx[i]);
			int hy = half_px(f->dy[i]);

			/* dy is measured with y upward, and the pixel axis runs
			 * downward, so the segment's ends swap rows. */
			if (hx == 0 && hy == 0)
				continue;
			line(fb, cx - hx, cy + hy, cx + hx, cy - hy, G_FIELD_LINE);
		}
	}

	for (int i = 1; i < f->ntrace; i++) {
		int x0 = gmap_x(&g->view, f->tx[i - 1]);
		int y0 = gmap_y(&g->view, f->ty[i - 1]);
		int x1 = gmap_x(&g->view, f->tx[i]);
		int y1 = gmap_y(&g->view, f->ty[i]);

		if (gclip_seg(&x0, &y0, &x1, &y1, G_PLOT_W, G_PLOT_H))
			line(fb, x0, y0, x1, y1, G_SOLUTION);
	}
}

/* ---- render ---- */

void graph_render(const gstate_t *g, uint16_t *fb)
{
	for (int i = 0; i < G_SCREEN_W * G_SCREEN_H; i++)
		fb[i] = G_BG;
	draw_axes(g, fb);
	if (g->field.on) {
		draw_field(g, fb);
	} else {
		for (int i = 0; i < g->ncurves; i++)
			draw_curve(g, &g->curve[i], fb);
	}
	draw_markers(g, fb);
	draw_trace(g, fb);
	draw_answer(g, fb);
	draw_status(g, fb);
}

/* ---- state ---- */

int graph_init(gstate_t *g, const char *text, char *err, int errlen)
{
	int i;

	memset(g, 0, sizeof *g);
	if (gexpr_parse(text, &g->curve[0].expr, err, errlen) < 0)
		return -1;
	g->ncurves = 1;
	g->curve[0].colour = G_CURVE;
	for (i = 0; text[i] && i < G_TEXT_MAX - 1; i++)
		g->text[i] = text[i];
	g->text[i] = 0;
	gview_autoscale(&g->view, &g->curve[0].expr);
	g->home = g->view;
	g->cursor_px = G_PLOT_W / 2;
	analyze(g);
	return 0;
}

static void pan(gview_t *v, int64_t dx, int64_t dy)
{
	v->xmin = gq_sat((int64_t)v->xmin + dx);
	v->xmax = gq_sat((int64_t)v->xmax + dx);
	v->ymin = gq_sat((int64_t)v->ymin + dy);
	v->ymax = gq_sat((int64_t)v->ymax + dy);
}

static void zoom(gview_t *v, int in)
{
	int64_t ax, ay, nxmin, nxmax, nymin, nymax, xs, ys;

	/*
	 * Anchor on the origin when it is on screen. A parabola autoscales to
	 * the height of its arms at the edges, so its centre is far above the
	 * vertex; zooming about the centre would walk away from the part the
	 * user is looking at.
	 */
	if (v->xmin <= 0 && v->xmax >= 0 && v->ymin <= 0 && v->ymax >= 0) {
		ax = 0;
		ay = 0;
	} else {
		ax = ((int64_t)v->xmin + v->xmax) / 2;
		ay = ((int64_t)v->ymin + v->ymax) / 2;
	}
	if (in) {
		nxmin = ax + ((int64_t)v->xmin - ax) / 2;
		nxmax = ax + ((int64_t)v->xmax - ax) / 2;
		nymin = ay + ((int64_t)v->ymin - ay) / 2;
		nymax = ay + ((int64_t)v->ymax - ay) / 2;
	} else {
		nxmin = ax + ((int64_t)v->xmin - ax) * 2;
		nxmax = ax + ((int64_t)v->xmax - ax) * 2;
		nymin = ay + ((int64_t)v->ymin - ay) * 2;
		nymax = ay + ((int64_t)v->ymax - ay) * 2;
	}
	xs = nxmax - nxmin;
	ys = nymax - nymin;
	if (xs < (1 << 12) || xs > ((int64_t)1 << 26)) {
		xs = xs < (1 << 12) ? (1 << 12) : ((int64_t)1 << 26);
		nxmin = ax - xs / 2;
		nxmax = ax + xs / 2;
	}
	if (ys < (1 << 12) || ys > ((int64_t)1 << 26)) {
		ys = ys < (1 << 12) ? (1 << 12) : ((int64_t)1 << 26);
		nymin = ay - ys / 2;
		nymax = ay + ys / 2;
	}
	v->xmin = gq_sat(nxmin);
	v->xmax = gq_sat(nxmax);
	v->ymin = gq_sat(nymin);
	v->ymax = gq_sat(nymax);
}

int graph_key(gstate_t *g, int key)
{
	gview_t *v = &g->view;
	int64_t xs = (int64_t)v->xmax - v->xmin;
	int64_t ys = (int64_t)v->ymax - v->ymin;

	switch (key) {
	case GK_QUIT:
		return 1;
	case GK_TRACE:
		g->trace = !g->trace;
		break;
	case GK_SWEEP:
		if (g->call[0])
			call_next_sweep(g);
		break;
	case GK_FAMILY:
		if (g->call[0])
			call_family(g);
		break;
	case GK_RESET:
		g->view = g->home;
		g->cursor_px = G_PLOT_W / 2;
		break;
	case GK_LEFT:
		if (g->trace) {
			if (g->cursor_px > 0)
				g->cursor_px--;
		} else {
			pan(v, -xs / 8, 0);
		}
		break;
	case GK_RIGHT:
		if (g->trace) {
			if (g->cursor_px < G_PLOT_W - 1)
				g->cursor_px++;
		} else {
			pan(v, xs / 8, 0);
		}
		break;
	case GK_UP:
		if (g->trace) {
			g->cursor_px -= 8;
			if (g->cursor_px < 0)
				g->cursor_px = 0;
		} else {
			pan(v, 0, ys / 8);
		}
		break;
	case GK_DOWN:
		if (g->trace) {
			g->cursor_px += 8;
			if (g->cursor_px > G_PLOT_W - 1)
				g->cursor_px = G_PLOT_W - 1;
		} else {
			pan(v, 0, -ys / 8);
		}
		break;
	case GK_ZOOM_IN:
		zoom(v, 1);
		break;
	case GK_ZOOM_OUT:
		zoom(v, 0);
		break;
	default:
		return 0;
	}
	if (g->field.on)
		field_build(g);
	analyze(g);
	return 0;
}
