/*
 * Host tests for the plotting module.
 *
 * The calculator cannot be debugged interactively, so the coordinate
 * transform, the clipper and the renderer are pinned down here. The render
 * tests draw into a real 320x240 buffer and inspect pixels, which is the
 * only way to catch a transform that is self-consistent but wrong.
 *
 *   ./test_graph            run the checks
 *   ./test_graph --ppm DIR  also write one PPM per function into DIR
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "graph.h"

static int failures;

static void test_from_call(void);
static void test_marker_pixels(void);
static void test_family(void);
static void test_field(void);

static void check(int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

static void near_q16(q16_t got, double want, double tol, const char *what)
{
	double d = (double)got / 65536.0 - want;

	if (d < 0)
		d = -d;
	if (d > tol) {
		printf("FAIL %s: got %.6f want %.6f\n", what, (double)got / 65536.0, want);
		failures++;
	}
}

static void near_int(int got, int want, int tol, const char *what)
{
	int d = got - want;

	if (d < 0)
		d = -d;
	if (d > tol) {
		printf("FAIL %s: got %d want %d\n", what, got, want);
		failures++;
	}
}

static q16_t q(double v)
{
	return (q16_t)(v * 65536.0 + (v < 0 ? -0.5 : 0.5));
}

static void test_arith(void)
{
	check(gq_mul(q(2.0), q(3.0)) == q(6.0), "mul 2*3");
	check(gq_mul(q(-2.0), q(3.0)) == q(-6.0), "mul -2*3");
	check(gq_mul(q(0.5), q(0.5)) == q(0.25), "mul 0.5*0.5");
	check(gq_div(q(6.0), q(3.0)) == q(2.0), "div 6/3");
	check(gq_div(q(1.0), q(0.0)) == Q16_MAX, "div by zero saturates");
	check(gq_div(q(-1.0), q(0.0)) == Q16_MIN, "div by zero saturates negative");
	check(gq_mul(Q16_MAX, Q16_MAX) == Q16_MAX, "mul saturates");
}

static void test_trig(void)
{
	near_q16(gq_sin(0), 0.0, 1e-4, "sin 0");
	near_q16(gq_sin(q(3.14159265 / 2)), 1.0, 1e-4, "sin pi/2");
	near_q16(gq_sin(q(3.14159265)), 0.0, 1e-4, "sin pi");
	near_q16(gq_sin(q(-3.14159265 / 2)), -1.0, 1e-4, "sin -pi/2");
	near_q16(gq_cos(0), 1.0, 1e-4, "cos 0");
	near_q16(gq_cos(q(3.14159265)), -1.0, 1e-4, "cos pi");
	near_q16(gq_tan(q(0.78539816)), 1.0, 1e-3, "tan pi/4");

	for (int i = -40; i <= 40; i++) {
		double x = i * 0.25;

		near_q16(gq_sin(q(x)), sin(x), 2e-4, "sin sweep");
		near_q16(gq_cos(q(x)), cos(x), 2e-4, "cos sweep");
	}
}

static void test_parse(void)
{
	gexpr_t e;
	char err[64];

	check(gexpr_parse("x^2-3x+1", &e, err, sizeof err) == 0, "parse quadratic");
	near_q16(gexpr_eval(&e, q(0)), 1.0, 1e-4, "quad at 0");
	near_q16(gexpr_eval(&e, q(1)), -1.0, 1e-4, "quad at 1");
	near_q16(gexpr_eval(&e, q(2)), -1.0, 1e-4, "quad at 2");
	near_q16(gexpr_eval(&e, q(3)), 1.0, 1e-4, "quad at 3");

	check(gexpr_parse("2x+1", &e, err, sizeof err) == 0, "parse implicit mul");
	near_q16(gexpr_eval(&e, q(4)), 9.0, 1e-4, "2x+1 at 4");

	check(gexpr_parse("2(x+1)", &e, err, sizeof err) == 0, "parse implicit paren");
	near_q16(gexpr_eval(&e, q(3)), 8.0, 1e-4, "2(x+1) at 3");

	check(gexpr_parse("-x^2", &e, err, sizeof err) == 0, "parse unary minus");
	near_q16(gexpr_eval(&e, q(3)), -9.0, 1e-4, "-x^2 at 3");

	check(gexpr_parse("x^3-2x", &e, err, sizeof err) == 0, "parse cubic");
	near_q16(gexpr_eval(&e, q(2)), 4.0, 1e-4, "cubic at 2");

	check(gexpr_parse("sin(x)", &e, err, sizeof err) == 0, "parse sin");
	near_q16(gexpr_eval(&e, q(1.5707963)), 1.0, 1e-3, "sin(x) at pi/2");

	check(gexpr_parse("cos(2x)", &e, err, sizeof err) == 0, "parse cos(2x)");
	near_q16(gexpr_eval(&e, q(0)), 1.0, 1e-4, "cos(2x) at 0");
	near_q16(gexpr_eval(&e, q(1.5707963)), -1.0, 1e-3, "cos(2x) at pi/2");

	check(gexpr_parse("2.5x-0.5", &e, err, sizeof err) == 0, "parse decimals");
	near_q16(gexpr_eval(&e, q(1)), 2.0, 1e-4, "2.5x-0.5 at 1");

	check(gexpr_parse("x^-1", &e, err, sizeof err) == 0, "parse negative exponent");
	near_q16(gexpr_eval(&e, q(4)), 0.25, 1e-4, "x^-1 at 4");

	check(gexpr_parse("sqrt(x)", &e, err, sizeof err) == 0, "parse sqrt");
	near_q16(gexpr_eval(&e, q(9)), 3.0, 1e-4, "sqrt(x) at 9");
	near_q16(gexpr_eval(&e, q(2)), 1.4142136, 1e-4, "sqrt(2) via x");

	check(gexpr_parse("ln(exp(x))", &e, err, sizeof err) == 0, "parse ln of exp");
	near_q16(gexpr_eval(&e, q(2)), 2.0, 1e-3, "ln(exp(2))");
	near_q16(gexpr_eval(&e, q(-1)), -1.0, 1e-3, "ln(exp(-1))");

	check(gexpr_parse("exp(x)", &e, err, sizeof err) == 0, "parse exp");
	near_q16(gexpr_eval(&e, q(0)), 1.0, 1e-4, "exp(0)");
	near_q16(gexpr_eval(&e, q(1)), 2.7182818, 1e-2, "exp(1)");

	check(gexpr_parse("abs(-3x)", &e, err, sizeof err) == 0, "parse abs");
	near_q16(gexpr_eval(&e, q(2)), 6.0, 1e-4, "abs(-3x) at 2");
	near_q16(gexpr_eval(&e, q(-2)), 6.0, 1e-4, "abs(-3x) at -2");

	check(gexpr_parse("2^0.5", &e, err, sizeof err) == 0, "parse fractional power");
	near_q16(gexpr_eval(&e, q(0)), 1.4142136, 1e-3, "2^0.5");

	check(gexpr_parse("x^10", &e, err, sizeof err) == 0, "parse large power");
	near_q16(gexpr_eval(&e, q(1.1)), 2.5937425, 5e-3, "x^10 at 1.1");

	check(gexpr_parse("pi", &e, err, sizeof err) == 0, "parse pi");
	near_q16(gexpr_eval(&e, q(0)), 3.1415927, 1e-3, "pi");

	check(gexpr_parse("e^2", &e, err, sizeof err) == 0, "parse e squared");
	near_q16(gexpr_eval(&e, q(0)), 7.3890561, 1e-2, "e^2");

	check(gexpr_parse("2pi", &e, err, sizeof err) == 0, "parse implicit constant");
	near_q16(gexpr_eval(&e, q(0)), 6.2831853, 1e-3, "2pi");

	check(gexpr_parse("sqrt(-1)", &e, err, sizeof err) == 0, "parse sqrt of negative");
	check(gexpr_eval(&e, q(0)) == Q16_MAX, "sqrt of a negative has no value");
	check(gexpr_parse("ln(0)", &e, err, sizeof err) == 0, "parse ln of zero");
	check(gexpr_eval(&e, q(0)) == Q16_MAX, "ln of zero has no value");

	check(gexpr_parse("", &e, err, sizeof err) < 0, "reject empty");
	check(gexpr_parse("x+", &e, err, sizeof err) < 0, "reject trailing op");
	check(gexpr_parse("sin x", &e, err, sizeof err) < 0, "reject sin without paren");
	check(gexpr_parse("x@2", &e, err, sizeof err) < 0, "reject bad char");
	check(gexpr_parse("((x)", &e, err, sizeof err) < 0, "reject unbalanced");
	check(gexpr_parse("x)2", &e, err, sizeof err) < 0, "reject trailing text");
}

static void test_transform(void)
{
	gview_t v;

	v.xmin = q(-10);
	v.xmax = q(10);
	v.ymin = q(-5);
	v.ymax = q(5);

	check(gmap_x(&v, q(-10)) == 0, "map xmin to 0");
	check(gmap_x(&v, q(10)) == G_PLOT_W, "map xmax to width");
	check(gmap_x(&v, q(0)) == G_PLOT_W / 2, "map x=0 to centre");
	check(gmap_y(&v, q(5)) == 0, "map ymax to 0");
	check(gmap_y(&v, q(-5)) == G_PLOT_H, "map ymin to height");
	check(gmap_y(&v, q(0)) == G_PLOT_H / 2, "map y=0 to centre");

	check(gunmap_x(&v, 0) == q(-10), "unmap 0 to xmin");
	check(gunmap_x(&v, G_PLOT_W) == q(10), "unmap width to xmax");
	check(gunmap_x(&v, G_PLOT_W / 2) == q(0), "unmap centre to 0");
	check(gunmap_y(&v, 0) == q(5), "unmap 0 to ymax");
	check(gunmap_y(&v, G_PLOT_H) == q(-5), "unmap height to ymin");

	for (int px = 0; px < G_PLOT_W; px++)
		near_int(gmap_x(&v, gunmap_x(&v, px)), px, 1, "x round trip");
	for (int py = 0; py < G_PLOT_H; py++)
		near_int(gmap_y(&v, gunmap_y(&v, py)), py, 1, "y round trip");

	/* Off-screen values must stay ordered and finite, not wrap. */
	check(gmap_x(&v, q(-1000)) < 0, "far left is negative");
	check(gmap_x(&v, q(1000)) > G_PLOT_W, "far right is past width");
	check(gmap_y(&v, q(1000)) < 0, "far up is negative");
	check(gmap_y(&v, q(-1000)) > G_PLOT_H, "far down is past height");

	/* A non-square view must still fill the rect exactly. */
	v.xmin = q(-1);
	v.xmax = q(3);
	v.ymin = q(-100);
	v.ymax = q(100);
	check(gmap_x(&v, q(-1)) == 0, "narrow xmin");
	check(gmap_x(&v, q(3)) == G_PLOT_W, "narrow xmax");
	check(gmap_y(&v, q(100)) == 0, "tall ymax");
	check(gmap_y(&v, q(-100)) == G_PLOT_H, "tall ymin");
}

static void test_clip(void)
{
	int x0, y0, x1, y1;

	x0 = 10; y0 = 20; x1 = 100; y1 = 200;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 1, "inside accepted");
	check(x0 == 10 && y0 == 20 && x1 == 100 && y1 == 200, "inside unchanged");

	x0 = -50; y0 = 10; x1 = -10; y1 = 20;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 0, "left rejected");

	x0 = 400; y0 = 10; x1 = 500; y1 = 20;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 0, "right rejected");

	x0 = 10; y0 = -50; x1 = 20; y1 = -10;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 0, "top rejected");

	x0 = 10; y0 = 300; x1 = 20; y1 = 400;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 0, "bottom rejected");

	x0 = -10; y0 = 50; x1 = 10; y1 = 50;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 1, "cross left accepted");
	check(x0 == 0 && y0 == 50 && x1 == 10 && y1 == 50, "cross left clipped");

	x0 = 310; y0 = 50; x1 = 400; y1 = 50;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 1, "cross right accepted");
	check(x0 == 310 && y0 == 50 && x1 == 319 && y1 == 50, "cross right clipped");

	x0 = 50; y0 = -10; x1 = 50; y1 = 10;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 1, "cross top accepted");
	check(x0 == 50 && y0 == 0 && x1 == 50 && y1 == 10, "cross top clipped");

	x0 = 50; y0 = 220; x1 = 50; y1 = 300;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 1, "cross bottom accepted");
	check(x0 == 50 && y0 == 220 && x1 == 50 && y1 == 223, "cross bottom clipped");

	/* Diagonal through the rect from outside both ends. */
	x0 = -100; y0 = -100; x1 = 400; y1 = 400;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 1, "diagonal accepted");
	check(x0 >= 0 && y0 >= 0 && x1 <= 319 && y1 <= 223, "diagonal inside rect");

	/* Zero-length segment. */
	x0 = 5; y0 = 5; x1 = 5; y1 = 5;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 1, "point accepted");
	x0 = -5; y0 = 5; x1 = -5; y1 = 5;
	check(gclip_seg(&x0, &y0, &x1, &y1, 320, 224) == 0, "point rejected");
}

static void test_analysis(void)
{
	gstate_t g;
	char err[64];

	/*
	 * Roots found by a sign change are exact to the bisection width. A
	 * vertex, and a double root found through it, are only located to the
	 * width of the Q16.16 flat region: f(x) = x^2 rounds to the same value
	 * for |x| < 1/256, so 0.004 units is the floor. That is 1/16 of a pixel
	 * at the default zoom, so it does not move the marker on screen.
	 */
	check(graph_init(&g, "x^2-4", err, sizeof err) == 0, "init x^2-4");
	check(g.nroots == 2, "x^2-4 has two roots");
	if (g.nroots == 2) {
		near_q16(g.roots[0], -2.0, 1e-3, "root -2");
		near_q16(g.roots[1], 2.0, 1e-3, "root 2");
	}
	check(g.nextrema == 1, "x^2-4 has one vertex");
	if (g.nextrema == 1) {
		near_q16(g.ext_x[0], 0.0, 0.01, "vertex x");
		near_q16(g.ext_y[0], -4.0, 1e-3, "vertex y");
	}

	check(graph_init(&g, "x^2-4x+3", err, sizeof err) == 0, "init x^2-4x+3");
	check(g.nroots == 2, "x^2-4x+3 has two roots");
	if (g.nroots == 2) {
		near_q16(g.roots[0], 1.0, 1e-3, "root 1");
		near_q16(g.roots[1], 3.0, 1e-3, "root 3");
	}
	if (g.nextrema == 1) {
		near_q16(g.ext_x[0], 2.0, 0.01, "vertex x of x^2-4x+3");
		near_q16(g.ext_y[0], -1.0, 1e-3, "vertex y of x^2-4x+3");
	} else {
		check(0, "x^2-4x+3 has one vertex");
	}

	check(graph_init(&g, "x^2+1", err, sizeof err) == 0, "init x^2+1");
	check(g.nroots == 0, "x^2+1 has no real roots");

	/* A double root touches zero without a sign change. */
	check(graph_init(&g, "x^2-2x+1", err, sizeof err) == 0, "init double root");
	check(g.nroots == 1, "double root found");
	if (g.nroots == 1)
		near_q16(g.roots[0], 1.0, 0.01, "double root at 1");

	check(graph_init(&g, "sin(x)", err, sizeof err) == 0, "init sin");
	check(g.nroots >= 3, "sin has roots in view");

	/* A cubic's turning points are not roots even when they sit near y=0. */
	check(graph_init(&g, "x^3-3x", err, sizeof err) == 0, "init cubic");
	check(g.nroots == 3, "x^3-3x has exactly three roots");
	if (g.nroots == 3) {
		near_q16(g.roots[0], -1.7320508, 1e-3, "cubic root -sqrt3");
		near_q16(g.roots[1], 0.0, 1e-3, "cubic root 0");
		near_q16(g.roots[2], 1.7320508, 1e-3, "cubic root sqrt3");
	}
}

static int has_color(const uint16_t *fb, int x, int y, uint16_t c, int r)
{
	for (int dy = -r; dy <= r; dy++)
		for (int dx = -r; dx <= r; dx++) {
			int px = x + dx, py = y + dy;

			if (px >= 0 && px < G_SCREEN_W && py >= 0 && py < G_SCREEN_H &&
			    fb[py * G_SCREEN_W + px] == c)
				return 1;
		}
	return 0;
}

static void test_zoom_pan(void)
{
	gstate_t g;
	char err[64];
	int ox, oy;
	int64_t span;

	check(graph_init(&g, "x^2-4x+3", err, sizeof err) == 0, "zoom init");
	ox = gmap_x(&g.view, 0);
	oy = gmap_y(&g.view, 0);
	graph_key(&g, GK_ZOOM_IN);
	check(gmap_x(&g.view, 0) == ox, "zoom keeps the origin column");
	check(gmap_y(&g.view, 0) == oy, "zoom keeps the origin row");
	span = (int64_t)g.view.xmax - g.view.xmin;
	check(span < 20 * Q16_ONE, "zoom in halves the x span");
	graph_key(&g, GK_ZOOM_OUT);
	graph_key(&g, GK_ZOOM_OUT);
	span = (int64_t)g.view.xmax - g.view.xmin;
	check(span > 20 * Q16_ONE, "zoom out widens the x span");

	check(graph_init(&g, "x^2-4", err, sizeof err) == 0, "pan init");
	ox = gmap_x(&g.view, 0);
	oy = gmap_y(&g.view, 0);
	graph_key(&g, GK_RIGHT);
	check(gmap_x(&g.view, 0) < ox, "pan right moves the origin left");
	graph_key(&g, GK_LEFT);
	check(gmap_x(&g.view, 0) == ox, "pan left restores the origin");
	graph_key(&g, GK_UP);
	check(gmap_y(&g.view, 0) > oy, "pan up moves the origin down");
}

static void test_render(void)
{
	static uint16_t fb[G_SCREEN_W * G_SCREEN_H];
	gstate_t g;
	char err[64];
	int missing = 0;

	check(graph_init(&g, "x^2-4", err, sizeof err) == 0, "render init");
	graph_render(&g, fb);

	/* The curve must appear at the transform's position in every column. */
	for (int px = 0; px < G_PLOT_W; px++) {
		q16_t x = gunmap_x(&g.view, px);
		int py = gmap_y(&g.view, gexpr_eval(&g.curve[0].expr, x));

		if (py < 0 || py >= G_PLOT_H)
			continue;
		if (!has_color(fb, px, py, 0x07E0, 2))
			missing++;
	}
	check(missing == 0, "curve drawn in every column");
	if (missing)
		printf("  %d columns without a curve pixel\n", missing);

	/* Axes: the origin sits on both axis lines. */
	check(fb[gmap_y(&g.view, 0) * G_SCREEN_W + gmap_x(&g.view, 0)] == 0x8410,
	      "origin is axis coloured");

	/* Root markers at y = 0. */
	check(has_color(fb, gmap_x(&g.view, q(-2)), gmap_y(&g.view, 0), 0xF800, 1),
	      "root marker at -2");
	check(has_color(fb, gmap_x(&g.view, q(2)), gmap_y(&g.view, 0), 0xF800, 1),
	      "root marker at 2");

	/* Vertex marker. */
	check(has_color(fb, gmap_x(&g.view, 0), gmap_y(&g.view, q(-4)), 0xFFE0, 1),
	      "vertex marker");

	/* Status strip is cleared and the plot does not bleed into it. */
	check(fb[(G_SCREEN_H - 1) * G_SCREEN_W + G_SCREEN_W - 1] == 0x0000,
	      "status strip background");

	/* Grid density: enough lines to read the scale, not a solid mesh. */
	{
		int gcols = 0, grows = 0;

		for (int x = 0; x < G_PLOT_W; x++) {
			int n = 0;

			for (int y = 0; y < G_PLOT_H; y++)
				if (fb[y * G_SCREEN_W + x] == 0x2104)
					n++;
			if (n > G_PLOT_H / 2)
				gcols++;
		}
		for (int y = 0; y < G_PLOT_H; y++) {
			int n = 0;

			for (int x = 0; x < G_PLOT_W; x++)
				if (fb[y * G_SCREEN_W + x] == 0x2104)
					n++;
			if (n > G_PLOT_W / 2)
				grows++;
		}
		check(gcols >= 3 && gcols <= 12, "vertical grid line count");
		check(grows >= 3 && grows <= 12, "horizontal grid line count");
	}

	/* Trace mode draws a cursor column and reports coordinates. */
	g.trace = 1;
	g.cursor_px = 100;
	graph_render(&g, fb);
	check(fb[10 * G_SCREEN_W + 100] == 0x4208, "trace column drawn");
	check(has_color(fb, 100, gmap_y(&g.view, gexpr_eval(&g.curve[0].expr, gunmap_x(&g.view, 100))),
	                0xFFFF, 1), "trace cursor on curve");

	/* A pole must not be bridged by a vertical line. */
	check(graph_init(&g, "1/x", err, sizeof err) == 0, "render 1/x");
	graph_render(&g, fb);
	{
		int px = gmap_x(&g.view, 0);
		int hits = 0;

		for (int y = 0; y < G_PLOT_H; y++)
			if (fb[y * G_SCREEN_W + px] == 0x07E0)
				hits++;
		check(hits < 4, "no vertical line through the pole");
	}
}

static void write_ppm(const char *path, const uint16_t *fb)
{
	FILE *f = fopen(path, "wb");

	if (!f) {
		printf("could not write %s\n", path);
		return;
	}
	fprintf(f, "P6\n%d %d\n255\n", G_SCREEN_W, G_SCREEN_H);
	for (int i = 0; i < G_SCREEN_W * G_SCREEN_H; i++) {
		uint16_t c = fb[i];
		unsigned char rgb[3];

		rgb[0] = (unsigned char)(((c >> 11) & 0x1F) * 255 / 31);
		rgb[1] = (unsigned char)(((c >> 5) & 0x3F) * 255 / 63);
		rgb[2] = (unsigned char)((c & 0x1F) * 255 / 31);
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
	printf("wrote %s\n", path);
}

static void dump(const char *dir)
{
	static uint16_t fb[G_SCREEN_W * G_SCREEN_H];
	gstate_t g;
	char err[64];
	char path[512];
	const char *names[] = { "poly", "trig", "trace", "zoom" };
	const char *exprs[] = { "x^2-4", "sin(x)", "x^3-3x", "x^2-4x+3" };

	for (int i = 0; i < 4; i++) {
		if (graph_init(&g, exprs[i], err, sizeof err) < 0) {
			printf("dump: %s failed: %s\n", exprs[i], err);
			continue;
		}
		if (i == 2) {
			g.trace = 1;
			g.cursor_px = 200;
		}
		if (i == 3) {
			graph_key(&g, GK_ZOOM_IN);
			graph_key(&g, GK_ZOOM_IN);
		}
		graph_render(&g, fb);
		snprintf(path, sizeof path, "%s/%s.ppm", dir, names[i]);
		write_ppm(path, fb);
	}
}

/*
 * 4.6a: a call the pipeline produced plots as the relation it named.
 *
 * The curve has to be the relation's own formula, not a lookalike: kinetic
 * energy at 5 kg evaluated at 3 m/s must be the 22.5 the solver returns. If
 * this drifts, the graph and the answer on the same screen disagree, which is
 * worse than having no graph at all.
 */
static void test_from_call(void)
{
	gstate_t g;
	char err[64];
	q16_t at3;

	check(graph_from_call(&g, "CALL kinetic_energy 5 3", 0, err, sizeof err) ==
	      G_CALL_OK, "call plots");
	check(g.ncurves == 1, "one curve from a call");
	check(g.sweep == 2, "sweeps the last argument by default");
	at3 = gexpr_eval(&g.curve[0].expr, 3 * Q16_ONE);
	near_q16(at3, 22.5, 0.01, "curve is the relation it named");

	/* 4.6b: the student's own point, marked where the curve says it is. */
	check(g.has_mark == 1, "answer marked");
	near_q16(g.mark_x, 3.0, 0.001, "mark at the swept value");
	near_q16(g.mark_y, 22.5, 0.01, "mark at the answer");
	check(g.mark_label[0] != 0, "mark carries its label");

	/* A formula the template does not have, and a line that is not a call. */
	check(graph_from_call(&g, "CALL no_such_relation 1 2", 0, err, sizeof err) < 0,
	      "unknown relation refused");
	check(graph_from_call(&g, "kinetic energy", 0, err, sizeof err) < 0,
	      "text that is not a call refused");
	check(graph_from_call(&g, "CALL kinetic_energy 5", 0, err, sizeof err) < 0,
	      "a call with too few arguments refused");

	/* A typed expression still plots on the old path. */
	check(graph_init(&g, "x^2-4", err, sizeof err) == 0, "typed path still works");
	check(g.ncurves == 1 && g.has_mark == 0, "a typed curve has no answer mark");
}

/* 4.6b as pixels: the mark is drawn where the view mapping puts it. */
static void test_marker_pixels(void)
{
	static uint16_t fb[G_SCREEN_W * G_SCREEN_H];
	gstate_t g;
	char err[64];
	int px, py;

	check(graph_from_call(&g, "CALL kinetic_energy 5 3", 0, err, sizeof err) ==
	      G_CALL_OK, "marker: call plots");
	graph_render(&g, fb);
	px = gmap_x(&g.view, g.mark_x);
	py = gmap_y(&g.view, g.mark_y);
	check(px >= 0 && px < G_PLOT_W && py >= 0 && py < G_PLOT_H,
	      "answer point inside the plot");
	if (px >= 0 && px < G_PLOT_W && py >= 0 && py < G_PLOT_H)
		check(has_color(fb, px, py, 0x07FF, 1), "answer disc drawn at its point");
}

/* 4.6d: a family is the same relation at several values of one argument. */
static void test_family(void)
{
	gstate_t g;
	char err[64];
	q16_t m5, m10;

	check(graph_from_call(&g, "CALL kinetic_energy 5 3", 0, err, sizeof err) ==
	      G_CALL_OK, "family: base curve");
	check(graph_family_add(&g, 1, 10 * Q16_ONE, err, sizeof err) == G_CALL_OK,
	      "family: second member");
	check(g.ncurves == 2, "family holds both curves");
	m5 = gexpr_eval(&g.curve[0].expr, 4 * Q16_ONE);
	m10 = gexpr_eval(&g.curve[1].expr, 4 * Q16_ONE);
	near_q16(m5, 40.0, 0.01, "first member at 4 m/s");
	near_q16(m10, 80.0, 0.01, "second member at 4 m/s");
	check(strcmp(g.curve[0].label, g.curve[1].label) != 0,
	      "family members are labelled apart");
	check(graph_family_add(&g, 99, Q16_ONE, err, sizeof err) < 0,
	      "family entry past the last argument refused");
}

/* 4.6f: a slope field is the evaluator's slopes, drawn. */
static void test_field(void)
{
	gstate_t g;
	gview_t v;
	char err[64];
	int bad = 0, inside = 0;

	v.xmin = -2 * Q16_ONE;
	v.xmax = 2 * Q16_ONE;
	v.ymin = -2 * Q16_ONE;
	v.ymax = 2 * Q16_ONE;
	check(graph_field(&g, "2y", &v, 0, Q16_ONE, G_FIELD_STEPS, err,
	                  sizeof err) == G_CALL_OK, "field builds");
	check(g.field.on == 1 && g.field.cols == G_FIELD_COLS &&
	      g.field.rows == G_FIELD_ROWS, "field grid");

	for (int r = 0; r < g.field.rows; r++) {
		q16_t y = (q16_t)((int64_t)v.ymax -
		                  (((int64_t)v.ymax - v.ymin) * (2 * r + 1)) /
		                  (2 * g.field.rows));
		for (int c = 0; c < g.field.cols; c++) {
			q16_t x = (q16_t)((int64_t)v.xmin +
			                  (((int64_t)v.xmax - v.xmin) * (2 * c + 1)) /
			                  (2 * g.field.cols));
			int i = r * g.field.cols + c;
			double m = gexpr_eval2(&g.field.expr, x, y) / (double)Q16_ONE;
			double got, tol;

			if (g.field.dx[i] == 0)
				continue;
			got = (double)g.field.dy[i] / (double)g.field.dx[i];
			tol = 0.05 * (1.0 + (m < 0 ? -m : m));
			if (got < m - tol || got > m + tol)
				bad++;
		}
	}
	check(bad == 0, "every field segment follows the evaluator");
	if (bad)
		printf("  %d segments off the slope\n", bad);

	/* The traced solution is y = e^(2x) from (0, 1), to a few percent. */
	for (int i = 0; i < g.field.ntrace; i++) {
		double x = g.field.tx[i] / (double)Q16_ONE;
		double y = g.field.ty[i] / (double)Q16_ONE;
		double want = exp(2.0 * x);

		if (y > want * 0.9 && y < want * 1.1)
			inside++;
	}
	check(g.field.ntrace > 0 && inside == g.field.ntrace,
	      "traced solution follows the closed form");

	check(graph_field(&g, "x+y+z", &v, 0, 0, G_FIELD_STEPS, err,
	                  sizeof err) < 0, "an unparseable field is refused");
}

int main(int argc, char **argv)
{
	test_arith();
	test_trig();
	test_parse();
	test_transform();
	test_clip();
	test_analysis();
	test_zoom_pan();
	test_render();
	test_from_call();
	test_marker_pixels();
	test_family();
	test_field();

	if (argc >= 3 && strcmp(argv[1], "--ppm") == 0)
		dump(argv[2]);

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("graph: all checks passed\n");
	return 0;
}
