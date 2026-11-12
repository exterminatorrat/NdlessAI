/*
 * Diagrams for the relations where the answer has a shape: force arrows, a
 * projectile's strobe, and the area under a curve.
 *
 * Three rules hold across the drawings, and each of them is why a piece of the
 * code below looks the way it does:
 *
 *  - The picture is scaled from the numbers in the call, never from a fixed
 *    range. A 3 m launch and a 90 degree one have to fill the same 320x240
 *    frame, so every drawing finds its own extents first and maps them onto
 *    the plot rectangle.
 *  - A sign is a direction. A negative force draws to the left (or down),
 *    never as a shorter positive arrow, because the direction is the part of
 *    the answer a number cannot show.
 *  - Only vectors with the same unit share a length scale. Friction's N and f
 *    are both newtons, so they compare on one scale and a ratio of 0.4 looks
 *    like 0.4. A lever arm and a force are not, so torque draws both at fixed
 *    lengths and lets the labels carry the numbers.
 *
 * Nothing here uses float: the target has no FPU, and the same expressions are
 * exercised on the host where a soft-float call would be invisible.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "diagram.h"
#include "expr.h"
#include "screen.h"

#define D_TITLE_Y   4
#define D_FOOT_Y    216
#define D_PLOT_X    16
#define D_PLOT_Y    34
#define D_PLOT_W    288
#define D_PLOT_H    168

#define D_ARROW_MAX 96
#define D_ARROW_MIN 6
#define D_HEAD_LEN  7
#define D_HEAD_WING 4
#define D_BODY_HALF 8

/* pi/180 in Q16.16. The two relations that take an angle take degrees, the
 * same as the solvers and the step table. */
#define D_DEG 1144

enum {
	D_NONE = 0,
	D_FORCE,
	D_PROJ,
	D_INTEGRAL
};

typedef struct {
	const char *name;
	int kind;
	int nargs;
} drow_t;

/*
 * The relations that get a drawing. Kept in step-table order so a diff against
 * steptable.h reads straight down.
 */
static const drow_t d_rows[] = {
	{ "friction_force", D_FORCE, 2 },
	{ "integr_poly3", D_INTEGRAL, 6 },
	{ "kinematic_v", D_PROJ, 3 },
	{ "newton2", D_FORCE, 2 },
	{ "proj_range", D_PROJ, 3 },
	{ "torque", D_FORCE, 3 }
};

static const drow_t *find_row(const char *name)
{
	if (!name)
		return NULL;
	for (size_t i = 0; i < sizeof d_rows / sizeof d_rows[0]; i++)
		if (strcmp(d_rows[i].name, name) == 0)
			return &d_rows[i];
	return NULL;
}

int diagram_supports(const char *relation)
{
	return find_row(relation) != NULL;
}

/* ---- small integer helpers ---- */

static q16_t qabs(q16_t v)
{
	return v < 0 ? (q16_t)-v : v;
}

static int isqrt_i(int64_t v)
{
	int64_t lo = 0, hi = 46340;

	if (v <= 0)
		return 0;
	while (lo < hi) {
		int64_t mid = (lo + hi + 1) / 2;

		if (mid * mid <= v)
			lo = mid;
		else
			hi = mid - 1;
	}
	return (int)lo;
}

/*
 * a*b/c in Q16, carried in 64 bits. gq_mul saturates at Q16's own range, and a
 * 40 m/s launch squares to more than that; the drawing still has to place the
 * apex on screen.
 */
static q16_t qmuldiv(q16_t a, q16_t b, q16_t c)
{
	int64_t p;

	if (c == 0)
		return 0;
	p = (int64_t)a * b;
	p = p >= 0 ? (p + 32768) >> 16 : -(((-p) + 32768) >> 16);
	return gq_sat(p * 65536 / c);
}

/*
 * A gain of 1/256 px per world unit, so a curve whose values are a thousand
 * times the width of the plot still gets a scale finer than one pixel per
 * unit. Rounding, not truncation: a shaded column biased low by half a pixel
 * would make the area test argue with itself.
 */
#define GAIN_SHIFT 24
#define D_GAIN_MAX  (1 << 22)   /* 16384 px per world unit, far past useful */

static int gain_px(q16_t v, int g)
{
	int64_t p = (int64_t)v * g;

	return (int)(p >= 0 ? (p + (1 << (GAIN_SHIFT - 1))) >> GAIN_SHIFT
	                    : -(((-p) + (1 << (GAIN_SHIFT - 1))) >> GAIN_SHIFT));
}

/* ---- primitives ---- */

static void arrow(uint16_t *fb, int x0, int y0, int x1, int y1)
{
	int dx = x1 - x0, dy = y1 - y0;
	int len = isqrt_i((int64_t)dx * dx + (int64_t)dy * dy);
	int bx, by, wx, wy;

	screen_line(fb, x0, y0, x1, y1, DIAGRAM_CURVE);
	if (len < 2)
		return;
	/* The head is built from the unit vector so it stays a head at any
	 * length: a fixed pixel offset would swallow an arrow that is almost
	 * all head, and at 3 m the launch arrow is short. */
	bx = x1 - dx * D_HEAD_LEN / len;
	by = y1 - dy * D_HEAD_LEN / len;
	wx = -dy * D_HEAD_WING / len;
	wy = dx * D_HEAD_WING / len;
	screen_line(fb, x1, y1, bx + wx, by + wy, DIAGRAM_HEAD);
	screen_line(fb, x1, y1, bx - wx, by - wy, DIAGRAM_HEAD);
	screen_line(fb, bx + wx, by + wy, bx - wx, by - wy, DIAGRAM_HEAD);
}

/*
 * Text is clamped instead of clipped: a label that would leave the frame is
 * pushed back inside it, because a force magnitude cut in half reads as a
 * different number and the framebuffer would clip it silently.
 */
static void label(uint16_t *fb, int x, int y, const char *s)
{
	int w = screen_text_w(s);

	if (x + w > SCREEN_W - 2)
		x = SCREEN_W - 2 - w;
	if (x < 2)
		x = 2;
	if (y > SCREEN_HEIGHT - 9)
		y = SCREEN_HEIGHT - 9;
	if (y < 2)
		y = 2;
	screen_text(fb, x, y, s, DIAGRAM_TEXT);
}

static void label_value(uint16_t *fb, int x, int y, const char *name, q16_t v)
{
	char num[24];
	char buf[40];

	gexpr_format(v, num, sizeof num);
	snprintf(buf, sizeof buf, "%s=%s", name, num);
	label(fb, x, y, buf);
}

static void title(uint16_t *fb, const char *s)
{
	screen_text(fb, 6, D_TITLE_Y, s, DIAGRAM_TEXT);
}

/* The longest vector in a drawing maps to D_ARROW_MAX; the rest keep their
 * ratio to it, which is what "to scale" can mean within one picture. */
static int scale_len(q16_t mag, q16_t unit, int maxlen)
{
	int64_t len;

	if (mag <= 0 || unit <= 0)
		return 0;
	len = (int64_t)mag * maxlen / unit;
	if (len > maxlen)
		len = maxlen;
	/* Below a few pixels the head is the whole arrow and the direction
	 * stops being readable, so a non-zero quantity keeps a visible stub. */
	if (len < D_ARROW_MIN)
		len = D_ARROW_MIN;
	return (int)len;
}

static void body(uint16_t *fb, int cx, int cy)
{
	screen_fill(fb, cx - D_BODY_HALF, cy - D_BODY_HALF,
	            D_BODY_HALF * 2 + 1, D_BODY_HALF * 2 + 1, DIAGRAM_BODY);
}

/* ---- call parsing ---- */

#define D_ARGS 6

typedef struct {
	const char *name;
	const char *arg[D_ARGS];
	int nargs;
} dcall_t;

/*
 * Splits "CALL name a b c" in place. This mirrors pipeline.c's parse_call
 * deliberately: the text this module receives is the text the pipeline built,
 * and two splitters that disagree about spaces would fail only on the device.
 */
static int split_call(const char *call, char *work, size_t worklen, dcall_t *c)
{
	char *parts[D_ARGS + 2];
	int n = 0;
	size_t len = strlen(call);

	if (len + 1 > worklen)
		return -1;
	memcpy(work, call, len + 1);

	for (size_t i = 0; i < len && n < D_ARGS + 2;) {
		while (i < len && work[i] == ' ')
			work[i++] = '\0';
		if (i >= len)
			break;
		parts[n++] = work + i;
		while (i < len && work[i] != ' ')
			i++;
	}
	if (n < 2 || strcmp(parts[0], "CALL") != 0)
		return -1;
	if (n - 2 > D_ARGS)
		return -1;
	c->name = parts[1];
	c->nargs = n - 2;
	for (int i = 0; i < c->nargs; i++)
		c->arg[i] = parts[i + 2];
	return 0;
}

static int parse_q16(const char *s, q16_t *out)
{
	int64_t ip = 0, frac = 0, scale = 1;
	int sign = 1, seen = 0;

	if (*s == '-' || *s == '+') {
		sign = *s == '-' ? -1 : 1;
		s++;
	}
	while (*s >= '0' && *s <= '9') {
		seen = 1;
		if (ip < 1000000)
			ip = ip * 10 + (*s - '0');
		s++;
	}
	if (*s == '.') {
		s++;
		while (*s >= '0' && *s <= '9') {
			seen = 1;
			/* Past five fractional digits the rest is below one
			 * Q16 step, so it is dropped rather than rounded into
			 * a value the picture would move for. */
			if (scale < 65536) {
				frac = frac * 10 + (*s - '0');
				scale *= 10;
			}
			s++;
		}
	}
	if (!seen || *s != 0)
		return -1;
	*out = gq_sat(sign * (ip * 65536 + frac * 65536 / scale));
	return 0;
}

static int fail(char *err, int errlen, const char *why)
{
	if (err && errlen > 0)
		snprintf(err, (size_t)errlen, "%s", why);
	return -1;
}

/* ---- force drawings ---- */

static int draw_friction(const q16_t *a, uint16_t *fb)
{
	q16_t n = a[1];
	q16_t f = gq_mul(a[0], n);
	q16_t mag = qabs(n) > qabs(f) ? qabs(n) : qabs(f);
	int bx = SCREEN_W / 2, by = 128;
	int len_n = scale_len(qabs(n), mag, D_ARROW_MAX);
	int len_f = scale_len(qabs(f), mag, D_ARROW_MAX);
	int ndir = n < 0 ? 1 : -1;   /* screen y grows down */
	int fdir = f < 0 ? -1 : 1;
	int ny, fy;

	/* Only a downward normal can run into the footer line; the upward one
	 * has the whole plot above the body. */
	if (ndir > 0 && by + D_BODY_HALF + len_n > D_FOOT_Y - 12)
		len_n = D_FOOT_Y - 12 - by - D_BODY_HALF;

	/* The floor is what the normal force is normal to; without it an
	 * upward arrow measures nothing. */
	screen_hline(fb, 24, by + D_BODY_HALF + 2, SCREEN_W - 48, DIAGRAM_AXIS);
	body(fb, bx, by);

	ny = by + ndir * D_BODY_HALF;
	arrow(fb, bx, ny, bx, ny + ndir * len_n);
	label_value(fb, bx + 8, ndir < 0 ? ny - len_n - 3 : ny + len_n + 3, "N", n);

	fy = bx + fdir * (D_BODY_HALF + 2);
	arrow(fb, fy, by, fy + fdir * len_f, by);
	label_value(fb, fdir > 0 ? fy + len_f - 28 : fy - len_f, by - 13, "f", f);

	title(fb, "friction: f = mu*N");
	return 0;
}

static int draw_newton2(const q16_t *a, uint16_t *fb)
{
	q16_t force = gq_mul(a[0], a[1]);
	int dir = force < 0 ? -1 : 1;
	int bx = SCREEN_W / 2, by = 120;
	int sx = bx + dir * (D_BODY_HALF + 2);
	int ex = sx + dir * (force == 0 ? 0 : D_ARROW_MAX);
	char m[24], acc[24], prod[24], buf[96];

	/* One vector has no ratio to show, so its length is fixed and the
	 * label carries the magnitude; the direction is what the picture adds
	 * to the number the answer pane already prints. */
	body(fb, bx, by);
	arrow(fb, sx, by, ex, by);
	label_value(fb, dir > 0 ? ex + 6 : ex - 32, by - 14, "F", force);

	gexpr_format(a[0], m, sizeof m);
	gexpr_format(a[1], acc, sizeof acc);
	gexpr_format(force, prod, sizeof prod);
	snprintf(buf, sizeof buf, "m=%s  a=%s  F=m*a=%s", m, acc, prod);
	label(fb, 6, D_FOOT_Y, buf);

	title(fb, "F = m*a");
	return 0;
}

static int draw_torque(const q16_t *a, uint16_t *fb)
{
	q16_t force = a[0], arm = a[1];
	q16_t rad = gq_mul(a[2], D_DEG);
	q16_t sa = gq_sin(rad), ca = gq_cos(rad);
	q16_t tau = gq_mul(gq_mul(force, arm), sa);
	int px = 84, py = 120;
	int ax = px + 104;
	int dx = force < 0 ? (q16_t)-ca : ca;
	int dy = force < 0 ? sa : (q16_t)-sa;
	int len = D_ARROW_MAX / 2;
	int hx = ax + (int)(((int64_t)len * dx) >> 16);
	int hy = py + (int)(((int64_t)len * dy) >> 16);

	/* A lever arm and a force are different units, so the arm is a fixed
	 * line and the angle between them is what the drawing adds. */
	screen_line(fb, px, py, ax, py, DIAGRAM_AXIS);
	screen_line(fb, px, py + 1, ax, py + 1, DIAGRAM_AXIS);
	screen_fill(fb, px - 2, py - 2, 5, 5, DIAGRAM_BODY);
	arrow(fb, ax, py, hx, hy);

	label_value(fb, (px + ax) / 2 - 12, py + 8, "r", arm);
	label_value(fb, hx + 6, hy - 3, "F", force);
	{
		char num[24], buf[48];

		gexpr_format(tau, num, sizeof num);
		snprintf(buf, sizeof buf, "tau = F*r*sin(theta) = %s", num);
		label(fb, 6, D_FOOT_Y, buf);
	}

	title(fb, "tau = F*r*sin(theta)");
	return 0;
}

/* ---- projectile drawings ---- */

static int draw_range(const q16_t *a, uint16_t *fb, char *err, int errlen)
{
	q16_t v0 = a[0], g = a[2];
	q16_t th = gq_mul(a[1], D_DEG);
	q16_t sa = gq_sin(th), ca = gq_cos(th);
	q16_t vx = gq_mul(v0, ca), vy = gq_mul(v0, sa);
	q16_t flight, span, apex;
	int ground = D_PLOT_Y + D_PLOT_H - 10;
	int top = D_PLOT_Y + 6;
	int rise = ground - top;
	int vertical, px0 = 0, py0 = 0, have = 0;

	if (g <= 0)
		return fail(err, errlen, "a range needs gravity above zero");
	if (v0 <= 0 || sa <= 0)
		return fail(err, errlen, "a range needs an upward launch");

	flight = qmuldiv(gq_mul(v0, sa), 2 * Q16_ONE, g);
	span = gq_mul(vx, flight);
	apex = qmuldiv(gq_mul(vy, vy), Q16_ONE, 2 * g);
	if (apex <= 0 || flight <= 0)
		return fail(err, errlen, "this launch has no trajectory to draw");
	/* A 90 degree launch has a range of zero and a path that is a vertical
	 * line. Dividing by the range would leave nothing to draw, so the
	 * straight-up case gets its own x. */
	vertical = span <= 0;

	screen_hline(fb, D_PLOT_X - 8, ground, D_PLOT_W + 16, DIAGRAM_AXIS);

	for (int k = 0; k <= 48; k++) {
		int64_t u = (int64_t)k * Q16_ONE / 48;
		q16_t t = gq_mul(flight, (q16_t)u);
		q16_t x = gq_mul(vx, t);
		q16_t y = gq_mul(vy, t) -
		          gq_div(gq_mul(gq_mul(g, t), t), 2 * Q16_ONE);
		int px = vertical ? D_PLOT_X + D_PLOT_W / 2
		                  : D_PLOT_X + (int)((int64_t)x * D_PLOT_W / span);
		int py = ground - (int)((int64_t)y * rise / apex);

		if (have)
			screen_line(fb, px0, py0, px, py, DIAGRAM_CURVE);
		px0 = px;
		py0 = py;
		have = 1;
	}

	/* Equal time steps, so the gap between marks is the distance covered
	 * in one step: wide at launch, closing as the projectile slows. */
	for (int k = 0; k <= 8; k++) {
		int64_t u = (int64_t)k * Q16_ONE / 8;
		q16_t t = gq_mul(flight, (q16_t)u);
		q16_t x = gq_mul(vx, t);
		q16_t y = gq_mul(vy, t) -
		          gq_div(gq_mul(gq_mul(g, t), t), 2 * Q16_ONE);
		int px = vertical ? D_PLOT_X + D_PLOT_W / 2
		                  : D_PLOT_X + (int)((int64_t)x * D_PLOT_W / span);
		int py = ground - (int)((int64_t)y * rise / apex);

		screen_fill(fb, px - 1, py - 1, 3, 3, DIAGRAM_MARK);
	}

	label_value(fb, D_PLOT_X, ground + 6, "R", vertical ? 0 : span);
	{
		char num[24], buf[32];

		gexpr_format(a[1], num, sizeof num);
		snprintf(buf, sizeof buf, "theta=%s", num);
		label(fb, SCREEN_W - 80, D_PLOT_Y - 22, buf);
	}
	title(fb, "range R = v0^2*sin(2*theta)/g");
	return 0;
}

static int draw_velocity(const q16_t *a, uint16_t *fb, char *err, int errlen)
{
	q16_t v0 = a[0], acc = a[1], t_end = a[2];
	q16_t pos[9], v;
	q16_t lo, hi, span;
	int track = D_PLOT_Y + D_PLOT_H - 40;
	int xleft = 28, width = SCREEN_W - 56;

	if (t_end <= 0)
		return fail(err, errlen, "a strobe needs a positive duration");

	for (int k = 0; k <= 8; k++) {
		int64_t u = (int64_t)k * Q16_ONE / 8;
		q16_t t = gq_mul(t_end, (q16_t)u);

		pos[k] = gq_sat((int64_t)gq_mul(v0, t) +
		                gq_div(gq_mul(gq_mul(acc, t), t), 2 * Q16_ONE));
	}
	lo = hi = pos[0];
	for (int k = 1; k <= 8; k++) {
		if (pos[k] < lo)
			lo = pos[k];
		if (pos[k] > hi)
			hi = pos[k];
	}
	span = gq_sat((int64_t)hi - lo);
	v = gq_sat((int64_t)v0 + gq_mul(acc, t_end));

	/* A tick per equal time step. The spacing is the distance travelled in
	 * one step, which is what makes a deceleration visible: the ticks close
	 * up as the body slows. */
	for (int k = 0; k <= 8; k++) {
		int px = span > 0
		         ? xleft + (int)((int64_t)(pos[k] - lo) * width / span)
		         : xleft + width / 2;

		screen_line(fb, px, track - 8, px, track + 8, DIAGRAM_MARK);
		if (k == 8)
			screen_fill(fb, px - 2, track - 2, 5, 5, DIAGRAM_MARK);
	}
	screen_hline(fb, xleft - 6, track + 14, width + 12, DIAGRAM_AXIS);

	label_value(fb, 6, track - 26, "v0", v0);
	label_value(fb, SCREEN_W - 78, track - 26, "v", v);
	{
		char num[24], buf[48];

		gexpr_format(v, num, sizeof num);
		snprintf(buf, sizeof buf, "v = v0 + a*t = %s", num);
		label(fb, 6, D_FOOT_Y, buf);
	}

	title(fb, "v = v0 + a*t");
	return 0;
}

/* ---- definite integral ---- */

static q16_t poly3(const q16_t *k, q16_t x)
{
	int64_t p = k[0];

	p = ((p * x) >> 16) + k[1];
	p = ((p * x) >> 16) + k[2];
	p = ((p * x) >> 16) + k[3];
	return gq_sat(p);
}

static int draw_integral(const q16_t *a, uint16_t *fb, char *err, int errlen)
{
	q16_t lo = a[4], hi = a[5];
	int64_t xspan = (int64_t)hi - lo;
	q16_t x0, x1, fmin = 0, fmax = 0;
	int64_t pad_span;
	int gx, gfit, g, band, top, axis_y;
	int lo_px, hi_px, prev_py = 0, have = 0;
	char num[24];

	if (xspan <= 0)
		return fail(err, errlen, "the upper bound must be above the lower");

	pad_span = xspan / 6;
	x0 = gq_sat((int64_t)lo - pad_span);
	x1 = gq_sat((int64_t)hi + pad_span);
	xspan = (int64_t)x1 - x0;

	/* The scale comes from the interval being integrated, not from the
	 * padded ends: a tail drawn for context must not decide how tall the
	 * measured region is. */
	for (int px = 0; px < D_PLOT_W; px++) {
		q16_t x = (q16_t)(x0 + (int64_t)px * xspan / D_PLOT_W);
		q16_t y;

		if (x < lo || x > hi)
			continue;
		y = poly3(a, x);
		if (y < fmin)
			fmin = y;
		if (y > fmax)
			fmax = y;
	}
	if (fmin > 0)
		fmin = 0;
	if (fmax < 0)
		fmax = 0;

	/*
	 * The vertical scale is the horizontal one (one world unit in x is one
	 * in y) until that would push the region out of the plot. The region is
	 * not stretched to fill the frame, because the shading is the
	 * measurement: a small integral has to look small beside a large one.
	 * Only a curve too tall for the plot is rescaled, and then only enough
	 * to fit.
	 */
	gx = (int)(((int64_t)D_PLOT_W << GAIN_SHIFT) / xspan);
	if (gx > D_GAIN_MAX)
		gx = D_GAIN_MAX;
	if (gx < 1)
		gx = 1;
	{
		q16_t mag = qabs(fmin) > qabs(fmax) ? qabs(fmin) : qabs(fmax);
		int64_t fit;

		g = gx;
		if (mag > 0) {
			fit = (int64_t)D_PLOT_H * 85 * 65536 / 100 * 256 / mag;
			gfit = fit > D_GAIN_MAX ? D_GAIN_MAX : (int)fit;
			if (gfit < g)
				g = gfit;
		}
	}
	if (g < 1)
		g = 1;

	band = gain_px(fmax, g) - gain_px(fmin, g);
	top = D_PLOT_Y + (D_PLOT_H - band) / 2;
	axis_y = top + gain_px(fmax, g);

	lo_px = D_PLOT_X + (int)(((int64_t)(lo - x0) * D_PLOT_W) / xspan);
	hi_px = D_PLOT_X + (int)(((int64_t)(hi - x0) * D_PLOT_W) / xspan);

	/*
	 * Fill between the curve and the axis inside the bounds. The axis and
	 * the curve are drawn afterwards so they stay whole on top of it, and
	 * the pixel count of this one colour is what the test reads as the
	 * area.
	 */
	for (int px = lo_px; px <= hi_px; px++) {
		q16_t x = (q16_t)(x0 + (int64_t)(px - D_PLOT_X) * xspan / D_PLOT_W);
		int py = axis_y - gain_px(poly3(a, x), g);
		int from = py < axis_y ? py : axis_y;
		int to = py < axis_y ? axis_y : py;

		screen_fill(fb, px, from, 1, to - from + 1, DIAGRAM_SHADE);
	}

	/*
	 * The curve is drawn only while it is inside the plot. Outside the
	 * bounds a steep polynomial leaves the frame, and a segment pinned to
	 * the edge would read as a plateau instead of a curve going off.
	 */
	for (int px = 0; px < D_PLOT_W; px++) {
		q16_t x = (q16_t)(x0 + (int64_t)px * xspan / D_PLOT_W);
		int py = axis_y - gain_px(poly3(a, x), g);
		int in = py >= D_PLOT_Y && py < D_PLOT_Y + D_PLOT_H;

		if (have && in && prev_py >= D_PLOT_Y && prev_py < D_PLOT_Y + D_PLOT_H)
			screen_line(fb, D_PLOT_X + px - 1, prev_py,
			            D_PLOT_X + px, py, DIAGRAM_CURVE);
		prev_py = py;
		have = 1;
	}

	screen_hline(fb, D_PLOT_X - 8, axis_y, D_PLOT_W + 16, DIAGRAM_AXIS);
	screen_line(fb, lo_px, axis_y - 3, lo_px, axis_y + 3, DIAGRAM_TEXT);
	screen_line(fb, hi_px, axis_y - 3, hi_px, axis_y + 3, DIAGRAM_TEXT);
	gexpr_format(lo, num, sizeof num);
	label(fb, lo_px - screen_text_w(num) / 2, axis_y + 6, num);
	gexpr_format(hi, num, sizeof num);
	label(fb, hi_px - screen_text_w(num) / 2, axis_y + 6, num);
	label(fb, D_PLOT_X + D_PLOT_W - 6, D_PLOT_Y - 22, "x");

	title(fb, "area under a*x^3+b*x^2+c*x+d");
	return 0;
}

/* ---- entry point ---- */

int diagram_render(const char *call, uint16_t *fb, char *err, int errlen)
{
	char work[128];
	dcall_t c;
	q16_t a[D_ARGS];
	const drow_t *row;
	int rc = 0;

	if (err && errlen > 0)
		err[0] = 0;
	if (!fb)
		return fail(err, errlen, "no framebuffer");
	if (!call || call[0] == 0)
		return fail(err, errlen, "there is no call to draw");
	if (split_call(call, work, sizeof work, &c) < 0)
		return fail(err, errlen, "could not read the call");

	row = find_row(c.name);
	if (!row)
		return fail(err, errlen, "this relation has no diagram");
	if (c.nargs != row->nargs)
		return fail(err, errlen, "the call has the wrong number of arguments");
	for (int i = 0; i < c.nargs; i++)
		if (parse_q16(c.arg[i], &a[i]) < 0)
			return fail(err, errlen, "an argument is not a number");

	screen_clear(fb, DIAGRAM_BG);
	switch (row->kind) {
	case D_FORCE:
		if (strcmp(c.name, "friction_force") == 0)
			rc = draw_friction(a, fb);
		else if (strcmp(c.name, "newton2") == 0)
			rc = draw_newton2(a, fb);
		else
			rc = draw_torque(a, fb);
		break;
	case D_PROJ:
		if (strcmp(c.name, "proj_range") == 0)
			rc = draw_range(a, fb, err, errlen);
		else
			rc = draw_velocity(a, fb, err, errlen);
		break;
	case D_INTEGRAL:
		rc = draw_integral(a, fb, err, errlen);
		break;
	default:
		rc = fail(err, errlen, "this relation has no diagram");
		break;
	}
	return rc;
}
