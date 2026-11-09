/*
 * Integer-only function plotting for the TI-Nspire CX II.
 *
 * The CPU has no FPU, so every coordinate and function value is Q16.16
 * fixed point. The per-column evaluation loop is the hot path: it runs 320
 * times per frame and must not pull in soft-float helpers. Multiplication
 * goes through int64 (a single smull on ARM926EJ-S) and saturates rather
 * than wrapping, so a steep curve leaves the screen instead of folding
 * back into it.
 */
#include <string.h>

#include "expr.h"
#include "graph.h"


#define G_BG        0x0000
#define G_GRID      0x2104
#define G_AXIS      0x8410
#define G_CURVE     0x07E0
#define G_ROOT      0xF800
#define G_VERTEX    0xFFE0
#define G_TRACE     0xFFFF
#define G_TRACE_DIM 0x4208
#define G_TEXT      0xFFFF

/* 5x7 font, ASCII 32..95, one byte per column, bit 0 = top row. */
static const uint8_t gfont[64][5] = {
	{ 0x00, 0x00, 0x00, 0x00, 0x00 }, /* ' ' */
	{ 0x00, 0x00, 0x5F, 0x00, 0x00 }, /* '!' */
	{ 0x00, 0x03, 0x00, 0x03, 0x00 }, /* '"' */
	{ 0x14, 0x7F, 0x14, 0x7F, 0x14 }, /* '#' */
	{ 0x24, 0x2A, 0x7F, 0x2A, 0x12 }, /* '$' */
	{ 0x63, 0x13, 0x08, 0x64, 0x63 }, /* '%' */
	{ 0x36, 0x49, 0x55, 0x22, 0x50 }, /* '&' */
	{ 0x00, 0x00, 0x03, 0x00, 0x00 }, /* '\'' */
	{ 0x00, 0x1C, 0x22, 0x41, 0x00 }, /* '(' */
	{ 0x00, 0x41, 0x22, 0x1C, 0x00 }, /* ')' */
	{ 0x14, 0x08, 0x3E, 0x08, 0x14 }, /* '*' */
	{ 0x08, 0x08, 0x3E, 0x08, 0x08 }, /* '+' */
	{ 0x00, 0x40, 0x30, 0x10, 0x00 }, /* ',' */
	{ 0x08, 0x08, 0x08, 0x08, 0x08 }, /* '-' */
	{ 0x00, 0x00, 0x60, 0x60, 0x00 }, /* '.' */
	{ 0x40, 0x30, 0x08, 0x06, 0x01 }, /* '/' */
	{ 0x3E, 0x51, 0x49, 0x45, 0x3E }, /* '0' */
	{ 0x00, 0x42, 0x7F, 0x40, 0x00 }, /* '1' */
	{ 0x42, 0x61, 0x51, 0x49, 0x46 }, /* '2' */
	{ 0x21, 0x41, 0x45, 0x4B, 0x31 }, /* '3' */
	{ 0x18, 0x14, 0x12, 0x7F, 0x10 }, /* '4' */
	{ 0x27, 0x45, 0x45, 0x45, 0x39 }, /* '5' */
	{ 0x3C, 0x4A, 0x49, 0x49, 0x30 }, /* '6' */
	{ 0x01, 0x71, 0x09, 0x05, 0x03 }, /* '7' */
	{ 0x36, 0x49, 0x49, 0x49, 0x36 }, /* '8' */
	{ 0x06, 0x49, 0x49, 0x29, 0x1E }, /* '9' */
	{ 0x00, 0x00, 0x36, 0x36, 0x00 }, /* ':' */
	{ 0x00, 0x40, 0x36, 0x16, 0x00 }, /* ';' */
	{ 0x08, 0x14, 0x22, 0x41, 0x00 }, /* '<' */
	{ 0x14, 0x14, 0x14, 0x14, 0x14 }, /* '=' */
	{ 0x00, 0x41, 0x22, 0x14, 0x08 }, /* '>' */
	{ 0x02, 0x01, 0x51, 0x09, 0x06 }, /* '?' */
	{ 0x32, 0x49, 0x79, 0x41, 0x3E }, /* '@' */
	{ 0x7E, 0x09, 0x09, 0x09, 0x7E }, /* 'A' */
	{ 0x7F, 0x49, 0x49, 0x49, 0x36 }, /* 'B' */
	{ 0x3E, 0x41, 0x41, 0x41, 0x22 }, /* 'C' */
	{ 0x7F, 0x41, 0x41, 0x22, 0x1C }, /* 'D' */
	{ 0x7F, 0x49, 0x49, 0x49, 0x41 }, /* 'E' */
	{ 0x7F, 0x09, 0x09, 0x09, 0x01 }, /* 'F' */
	{ 0x3E, 0x41, 0x49, 0x49, 0x7A }, /* 'G' */
	{ 0x7F, 0x08, 0x08, 0x08, 0x7F }, /* 'H' */
	{ 0x00, 0x41, 0x7F, 0x41, 0x00 }, /* 'I' */
	{ 0x20, 0x40, 0x41, 0x3F, 0x01 }, /* 'J' */
	{ 0x7F, 0x08, 0x14, 0x22, 0x41 }, /* 'K' */
	{ 0x7F, 0x40, 0x40, 0x40, 0x40 }, /* 'L' */
	{ 0x7F, 0x02, 0x0C, 0x02, 0x7F }, /* 'M' */
	{ 0x7F, 0x04, 0x08, 0x10, 0x7F }, /* 'N' */
	{ 0x3E, 0x41, 0x41, 0x41, 0x3E }, /* 'O' */
	{ 0x7F, 0x09, 0x09, 0x09, 0x06 }, /* 'P' */
	{ 0x3E, 0x41, 0x51, 0x21, 0x5E }, /* 'Q' */
	{ 0x7F, 0x09, 0x19, 0x29, 0x46 }, /* 'R' */
	{ 0x46, 0x49, 0x49, 0x49, 0x31 }, /* 'S' */
	{ 0x01, 0x01, 0x7F, 0x01, 0x01 }, /* 'T' */
	{ 0x3F, 0x40, 0x40, 0x40, 0x3F }, /* 'U' */
	{ 0x1F, 0x20, 0x40, 0x20, 0x1F }, /* 'V' */
	{ 0x3F, 0x40, 0x38, 0x40, 0x3F }, /* 'W' */
	{ 0x63, 0x14, 0x08, 0x14, 0x63 }, /* 'X' */
	{ 0x03, 0x04, 0x78, 0x04, 0x03 }, /* 'Y' */
	{ 0x61, 0x51, 0x49, 0x45, 0x43 }, /* 'Z' */
	{ 0x00, 0x7F, 0x41, 0x41, 0x00 }, /* '[' */
	{ 0x01, 0x06, 0x08, 0x30, 0x40 }, /* '\\' */
	{ 0x00, 0x41, 0x41, 0x7F, 0x00 }, /* ']' */
	{ 0x04, 0x02, 0x01, 0x02, 0x04 }, /* '^' */
	{ 0x40, 0x40, 0x40, 0x40, 0x40 }, /* '_' */
};

static int64_t fdiv(int64_t a, int64_t b)
{
	int64_t q = a / b;

	if ((a % b) != 0 && ((a < 0) != (b < 0)))
		q--;
	return q;
}

/* ---- view ---- */

void gview_autoscale(gview_t *v, const gexpr_t *e)
{
	q16_t x0 = -10 * Q16_ONE, x1 = 10 * Q16_ONE;
	q16_t ymin = Q16_MAX, ymax = Q16_MIN;
	int found = 0;
	int64_t span, pad;

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
	if (x >= 0 && x < G_SCREEN_W && y >= 0 && y < G_SCREEN_H)
		fb[y * G_SCREEN_W + x] = c;
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

static void text(uint16_t *fb, int x, int y, const char *s, uint16_t c)
{
	for (; *s; s++) {
		int ch = (unsigned char)*s;
		const uint8_t *gl;

		if (ch >= 'a' && ch <= 'z')
			ch -= 32;
		if (ch < 32 || ch > 95)
			ch = '?';
		gl = gfont[ch - 32];
		for (int col = 0; col < 5; col++)
			for (int row = 0; row < 7; row++)
				if (gl[col] & (1 << row))
					px_set(fb, x + col, y + row, c);
		x += 6;
	}
}

static void fmt_q16(q16_t v, char *out, int n)
{
	int64_t a = v;
	int64_t ip, fr;
	char tmp[12];
	int t = 0, i = 0;

	if (a < 0) {
		if (i < n - 1)
			out[i++] = '-';
		a = -a;
	}
	ip = a >> 16;
	fr = ((a & 0xFFFF) * 1000 + 32768) >> 16;
	if (fr >= 1000) {
		ip++;
		fr = 0;
	}
	if (ip == 0)
		tmp[t++] = '0';
	while (ip > 0) {
		tmp[t++] = (char)('0' + (ip % 10));
		ip /= 10;
	}
	while (t > 0 && i < n - 1)
		out[i++] = tmp[--t];
	if (fr) {
		char d[3];
		int k = 3;

		d[0] = (char)('0' + (fr / 100) % 10);
		d[1] = (char)('0' + (fr / 10) % 10);
		d[2] = (char)('0' + fr % 10);
		while (k > 0 && d[k - 1] == '0')
			k--;
		if (i < n - 1)
			out[i++] = '.';
		for (int j = 0; j < k && i < n - 1; j++)
			out[i++] = d[j];
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

	fmt_q16(v, tmp, (int)sizeof tmp);
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
		fmt_q16((q16_t)t, buf, (int)sizeof buf);
		w = (int)strlen(buf) * 6 - 1;
		ly = xaxis ? ay + 4 : G_PLOT_H - 10;
		if (ly + 7 > G_PLOT_H)
			ly = G_PLOT_H - 8;
		text(fb, px - w / 2, ly, buf, G_TEXT);
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
		fmt_q16((q16_t)t, buf, (int)sizeof buf);
		lx = yaxis ? ax + 4 : 2;
		text(fb, lx, py - 3, buf, G_TEXT);
	}

	if (xaxis)
		for (int x = 0; x < G_PLOT_W; x++)
			px_set(fb, x, ay, G_AXIS);
	if (yaxis)
		for (int y = 0; y < G_PLOT_H; y++)
			px_set(fb, ax, y, G_AXIS);
}

static void draw_curve(const gstate_t *g, uint16_t *fb)
{
	const gview_t *v = &g->view;
	int prev_py = 0, have = 0;

	for (int px = 0; px < G_PLOT_W; px++) {
		q16_t x = gunmap_x(v, px);
		q16_t y = gexpr_eval(&g->expr, x);
		int py = gmap_y(v, y);

		if (have) {
			int x0 = px - 1, y0 = prev_py, x1 = px, y1 = py;
			int jump = py - prev_py;

			if (jump < 0)
				jump = -jump;
			/* A jump taller than the screen is an asymptote, not a line. */
			if (jump <= 2 * G_PLOT_H &&
			    gclip_seg(&x0, &y0, &x1, &y1, G_PLOT_W, G_PLOT_H))
				line(fb, x0, y0, x1, y1, G_CURVE);
		}
		prev_py = py;
		have = 1;
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

static void draw_trace(const gstate_t *g, uint16_t *fb)
{
	int px, py;

	if (!g->trace)
		return;
	px = g->cursor_px;
	py = gmap_y(&g->view, gexpr_eval(&g->expr, gunmap_x(&g->view, px)));
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
		sb_q16(&s, gexpr_eval(&g->expr, x));
	} else {
		sb_str(&s, g->text);
	}
	buf[s.i] = 0;
	text(fb, 2, G_STATUS_Y + 4, buf, G_TEXT);
}

void graph_render(const gstate_t *g, uint16_t *fb)
{
	for (int i = 0; i < G_SCREEN_W * G_SCREEN_H; i++)
		fb[i] = G_BG;
	draw_axes(g, fb);
	draw_curve(g, fb);
	draw_markers(g, fb);
	draw_trace(g, fb);
	draw_status(g, fb);
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
	q16_t x0, y0, x1, y1;

	g->nroots = 0;
	g->nextrema = 0;
	if (span <= 0)
		return;

	x0 = v->xmin;
	y0 = gexpr_eval(&g->expr, x0);
	x1 = gq_sat((int64_t)v->xmin + span / 512);
	y1 = gexpr_eval(&g->expr, x1);
	for (int i = 2; i <= 512 && g->nextrema < G_MAX_EXTREMA; i++) {
		q16_t x2 = gq_sat((int64_t)v->xmin + span * i / 512);
		q16_t y2 = gexpr_eval(&g->expr, x2);
		int d1 = (y1 > y0) - (y1 < y0);
		int d2 = (y2 > y1) - (y2 < y1);

		if (d1 != 0 && d2 != 0 && d1 != d2) {
			q16_t ex = refine_extremum(&g->expr, x0, x2, d1 > 0);

			add_extremum(g, ex, gexpr_eval(&g->expr, ex));
		}
		x0 = x1;
		y0 = y1;
		x1 = x2;
		y1 = y2;
	}

	x0 = v->xmin;
	y0 = gexpr_eval(&g->expr, x0);
	for (int i = 1; i <= 512 && g->nroots < G_MAX_ROOTS; i++) {
		q16_t x = gq_sat((int64_t)v->xmin + span * i / 512);
		q16_t y = gexpr_eval(&g->expr, x);

		if (y0 == 0) {
			add_root(g, x0);
		} else if (y != 0 && ((y0 < 0) != (y < 0))) {
			q16_t lo = x0, hi = x, flo = y0;

			for (int j = 0; j < 24; j++) {
				q16_t mid = gq_sat(((int64_t)lo + hi) / 2);
				q16_t fm = gexpr_eval(&g->expr, mid);

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

/* ---- state ---- */

int graph_init(gstate_t *g, const char *text, char *err, int errlen)
{
	int i;

	memset(g, 0, sizeof *g);
	if (gexpr_parse(text, &g->expr, err, errlen) < 0)
		return -1;
	for (i = 0; text[i] && i < G_TEXT_MAX - 1; i++)
		g->text[i] = text[i];
	g->text[i] = 0;
	gview_autoscale(&g->view, &g->expr);
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
	case GK_RESET:
		gview_autoscale(v, &g->expr);
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
	analyze(g);
	return 0;
}
