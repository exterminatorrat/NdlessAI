/*
 * Host tests for the diagram module.
 *
 * There is no calculator on this machine, so the only way to know a drawing is
 * right is to read its pixels back. These checks pin down the three things the
 * wireframe plan asks the drawings to guarantee: every mark lands inside the
 * frame, an arrow follows the sign of its quantity, and the shading measures
 * the integral instead of decorating it.
 *
 *   ./test_diagram            run the checks
 *   ./test_diagram --ppm DIR  also write one PPM per drawing into DIR
 */
#include <stdio.h>
#include <string.h>

#include "diagram.h"
#include "screen.h"

static uint16_t fb[SCREEN_W * SCREEN_HEIGHT];
static int failures;

static void check(int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

static int count_colour(uint16_t c)
{
	int n = 0;

	for (int i = 0; i < SCREEN_W * SCREEN_HEIGHT; i++)
		if (fb[i] == c)
			n++;
	return n;
}

/*
 * Bounding box of one colour. `hit` tells the caller whether the colour was
 * there at all, since a missing element and an element at the origin would
 * otherwise both report zeroes.
 */
static int bbox(uint16_t c, int *x0, int *y0, int *x1, int *y1)
{
	int hit = 0;

	*x0 = *y0 = SCREEN_W + SCREEN_HEIGHT;
	*x1 = *y1 = -1;
	for (int y = 0; y < SCREEN_HEIGHT; y++)
		for (int x = 0; x < SCREEN_W; x++)
			if (fb[y * SCREEN_W + x] == c) {
				if (x < *x0)
					*x0 = x;
				if (x > *x1)
					*x1 = x;
				if (y < *y0)
					*y0 = y;
				if (y > *y1)
					*y1 = y;
				hit = 1;
			}
	return hit;
}

/* Bounding box of a colour inside a band of rows, for the one drawing with two
 * arrows that share a colour. */
static int bbox_band(uint16_t c, int ry0, int ry1, int *x0, int *x1)
{
	int hit = 0;

	*x0 = SCREEN_W;
	*x1 = -1;
	for (int y = ry0; y <= ry1; y++)
		for (int x = 0; x < SCREEN_W; x++)
			if (fb[y * SCREEN_W + x] == c) {
				if (x < *x0)
					*x0 = x;
				if (x > *x1)
					*x1 = x;
				hit = 1;
			}
	return hit;
}

/* Columns that hold at least one pixel of a colour, run-length collapsed, so
 * one strobe tick reads as one entry rather than as its width. */
static int column_runs(uint16_t c, int *centre, int max)
{
	int runs = 0, start = -1;

	for (int x = 0; x <= SCREEN_W; x++) {
		int on = 0;

		if (x < SCREEN_W)
			for (int y = 0; y < SCREEN_HEIGHT; y++)
				if (fb[y * SCREEN_W + x] == c) {
					on = 1;
					break;
				}
		if (on && start < 0)
			start = x;
		if (!on && start >= 0) {
			if (runs < max)
				centre[runs] = (start + x - 1) / 2;
			runs++;
			start = -1;
		}
	}
	return runs > max ? max : runs;
}

static int render_ok(const char *call, const char *what)
{
	char err[64] = "";

	/* The sentinel catches a renderer that draws over whatever the caller
	 * left behind instead of owning the whole frame. */
	fb[0] = 0xDEAD;
	if (diagram_render(call, fb, err, sizeof err) != 0) {
		printf("FAIL %s: %s: %s\n", what, call, err);
		failures++;
		return 0;
	}
	if (fb[0] == 0xDEAD) {
		printf("FAIL %s: %s left the framebuffer uncleared\n", what, call);
		failures++;
		return 0;
	}
	return 1;
}

static void test_supports(void)
{
	static const char *yes[] = {
		"friction_force", "newton2", "torque",
		"proj_range", "kinematic_v", "integr_poly3"
	};
	static const char *no[] = {
		"ohm_i", "quad_solve", "kinetic_energy", "average", ""
	};

	for (size_t i = 0; i < sizeof yes / sizeof yes[0]; i++)
		check(diagram_supports(yes[i]) != 0, yes[i]);
	for (size_t i = 0; i < sizeof no / sizeof no[0]; i++)
		check(diagram_supports(no[i]) == 0, no[i]);
	check(diagram_supports(NULL) == 0, "no relation has no diagram");
}

/*
 * Everything drawn has to sit inside the frame with a margin on every side. A
 * drawing that ran past an edge would still be "clipped" by screen_px and look
 * plausible, so the check is that nothing touches the outermost row or column.
 */
static void check_inside(const char *what)
{
	static const uint16_t palette[] = {
		DIAGRAM_TEXT, DIAGRAM_AXIS, DIAGRAM_CURVE,
		DIAGRAM_BODY, DIAGRAM_HEAD, DIAGRAM_MARK, DIAGRAM_SHADE
	};

	for (size_t i = 0; i < sizeof palette / sizeof palette[0]; i++) {
		int x0, y0, x1, y1;

		if (!bbox(palette[i], &x0, &y0, &x1, &y1))
			continue;
		if (x0 < 2 || y0 < 2 || x1 > SCREEN_W - 3 || y1 > SCREEN_HEIGHT - 3) {
			printf("FAIL %s: colour %04x box %d,%d..%d,%d\n",
			       what, palette[i], x0, y0, x1, y1);
			failures++;
		}
	}
}

static void test_frame(void)
{
	static const char *calls[] = {
		"CALL friction_force 0.4 10",
		"CALL newton2 5 -2",
		"CALL torque 20 0.5 90",
		"CALL proj_range 20 45 9.8",
		"CALL proj_range 3 30 9.8",
		"CALL proj_range 20 90 9.8",
		"CALL kinematic_v 20 -2 5",
		"CALL integr_poly3 3 2 2 -1 1 4",
		"CALL integr_poly3 0.5 0 0 0 0 1"
	};

	for (size_t i = 0; i < sizeof calls / sizeof calls[0]; i++)
		if (render_ok(calls[i], "frame"))
			check_inside(calls[i]);
}

/* The body is the anchor every force arrow is measured against. */
static int body_centre(int *cx)
{
	int x0, y0, x1, y1;

	if (!bbox(DIAGRAM_BODY, &x0, &y0, &x1, &y1))
		return 0;
	*cx = (x0 + x1) / 2;
	return 1;
}

static void test_arrow_signs(void)
{
	int cx, head0, head1, tmp;

	/* Newton's second law with a positive acceleration pushes right. */
	if (!render_ok("CALL newton2 5 2", "newton2 +a"))
		return;
	if (!body_centre(&cx) || !bbox(DIAGRAM_HEAD, &head0, &tmp, &head1, &tmp))
		return;
	check(head1 > cx, "positive force points right of the body");

	/* The same mass with a negative acceleration must land on the other
	 * side of the same body. A shorter positive arrow would fail this,
	 * which is the point: the sign is a direction, not a length. */
	if (!render_ok("CALL newton2 5 -2", "newton2 -a"))
		return;
	if (!body_centre(&cx) || !bbox(DIAGRAM_HEAD, &head0, &tmp, &head1, &tmp))
		return;
	check(head1 < cx - 8, "negative force points left of the body");

	/*
	 * Friction is the drawing with two vectors on one scale, so it also
	 * proves the lateral arrow reverses without moving the normal force.
	 */
	if (!render_ok("CALL friction_force 0.4 10", "friction +mu"))
		return;
	{
		int bx, by, bx1, by1;

		if (!bbox(DIAGRAM_BODY, &bx, &by, &bx1, &by1))
			return;
		cx = (bx + bx1) / 2;
		if (!bbox_band(DIAGRAM_HEAD, by, by1, &head0, &head1))
			return;
		check(head0 > bx1, "positive friction points right of the body");

		if (!render_ok("CALL friction_force -0.4 10", "friction -mu"))
			return;
		if (!bbox(DIAGRAM_BODY, &bx, &by, &bx1, &by1))
			return;
		check((bx + bx1) / 2 == cx, "the body stays put when mu changes sign");
		if (!bbox_band(DIAGRAM_HEAD, by, by1, &head0, &head1))
			return;
		check(head1 < bx, "negative friction points left of the body");
	}
}

static void test_strobe(void)
{
	int centre[16], n, gaps[16];

	if (!render_ok("CALL kinematic_v 20 -2 5", "strobe"))
		return;
	n = column_runs(DIAGRAM_MARK, centre, 16);
	check(n == 9, "kinematic_v draws one tick per equal time step");

	for (int i = 1; i < n; i++)
		gaps[i - 1] = centre[i] - centre[i - 1];
	for (int i = 1; i + 1 < n; i++)
		check(gaps[i] <= gaps[i - 1] + 1,
		      "each gap is no wider than the one before while decelerating");
	if (n >= 2) {
		printf("  strobe gaps: %d..%d px\n", gaps[0], gaps[n - 2]);
		check(gaps[0] > gaps[n - 2], "the first gap is wider than the last");
		check(gaps[0] - gaps[n - 2] >= 8,
		      "the deceleration is visible, not a rounding artefact");
	}
}

static void test_shading(void)
{
	int small, large;
	int sx0, sy0, sx1, sy1, cx0, cy0, cx1, cy1;
	long diff;

	check(render_ok("CALL integr_poly3 0.25 0 0 0 0 1", "area small"),
	      "area small: rendered");
	small = count_colour(DIAGRAM_SHADE);
	check(bbox(DIAGRAM_SHADE, &sx0, &sy0, &sx1, &sy1) != 0,
	      "area small: something is shaded");
	check(bbox(DIAGRAM_CURVE, &cx0, &cy0, &cx1, &cy1) != 0,
	      "area small: the curve is drawn");
	check(sx0 > cx0 + 4 && sx1 < cx1 - 4,
	      "shading stops at the bounds, short of the curve's ends");
	check(sy1 - sy0 > 4, "the shaded region has a visible height");
	check(cy1 > cy0 + 4, "the curve is not a flat line");
	(void)cy0;
	(void)cy1;

	check(render_ok("CALL integr_poly3 0.5 0 0 0 0 1", "area large"),
	      "area large: rendered");
	large = count_colour(DIAGRAM_SHADE);

	/*
	 * Doubling every coefficient doubles the area, and the renderer is not
	 * allowed to answer that by stretching the curve to the same height.
	 * A pixel of rounding per column over 240 columns is the whole error
	 * budget, so a few percent is as close as pixels can get.
	 */
	printf("  shaded pixels: %d and %d\n", small, large);
	check(small > 100 && large > small, "both cases shade a real region");
	diff = (long)large - 2L * small;
	if (diff < 0)
		diff = -diff;
	check(diff * 100 <= 2L * small * 8, "the shaded area doubles with the integrand");
}

static void test_errors(void)
{
	char err[64];

	check(diagram_render("CALL ohm_i 2 3", fb, err, sizeof err) < 0,
	      "a relation with no diagram is refused");
	check(err[0] != 0, "the refusal says why");
	check(diagram_render("CALL newton2 5", fb, err, sizeof err) < 0,
	      "a short call is refused");
	check(diagram_render("CALL newton2 x 2", fb, err, sizeof err) < 0,
	      "an argument that is not a number is refused");
	check(diagram_render("not a call", fb, err, sizeof err) < 0,
	      "text without CALL is refused");
	check(diagram_render(NULL, fb, err, sizeof err) < 0,
	      "no call is refused");
	check(diagram_render("CALL newton2 5 2", NULL, err, sizeof err) < 0,
	      "no framebuffer is refused");
	check(diagram_render("CALL integr_poly3 1 0 0 0 2 1", fb, err,
	                     sizeof err) < 0,
	      "reversed bounds are refused");
	check(diagram_render("CALL proj_range 20 45 0", fb, err, sizeof err) < 0,
	      "a launch without gravity is refused");
}

static void write_ppm(const char *path)
{
	FILE *f = fopen(path, "wb");

	if (!f)
		return;
	fprintf(f, "P6\n%d %d\n255\n", SCREEN_W, SCREEN_HEIGHT);
	for (int i = 0; i < SCREEN_W * SCREEN_HEIGHT; i++) {
		uint16_t c = fb[i];
		unsigned char rgb[3];

		rgb[0] = (unsigned char)(((c >> 11) & 0x1F) * 255 / 31);
		rgb[1] = (unsigned char)(((c >> 5) & 0x3F) * 255 / 63);
		rgb[2] = (unsigned char)((c & 0x1F) * 255 / 31);
		fwrite(rgb, 1, 3, f);
	}
	fclose(f);
}

static void dump(const char *dir)
{
	static const char *names[] = {
		"friction", "newton2", "torque", "range45", "range90", "strobe",
		"area-small", "area-large"
	};
	static const char *calls[] = {
		"CALL friction_force 0.4 10",
		"CALL newton2 5 -2",
		"CALL torque 20 0.5 90",
		"CALL proj_range 20 45 9.8",
		"CALL proj_range 20 90 9.8",
		"CALL kinematic_v 20 -2 5",
		"CALL integr_poly3 0.25 0 0 0 0 1",
		"CALL integr_poly3 0.5 0 0 0 0 1"
	};

	for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) {
		char path[512];
		char err[64];

		if (diagram_render(calls[i], fb, err, sizeof err) != 0) {
			printf("dump: %s failed: %s\n", calls[i], err);
			continue;
		}
		snprintf(path, sizeof path, "%s/%s.ppm", dir, names[i]);
		write_ppm(path);
		printf("wrote %s\n", path);
	}
}

int main(int argc, char **argv)
{
	test_supports();
	test_frame();
	test_arrow_signs();
	test_strobe();
	test_shading();
	test_errors();

	if (argc >= 3 && strcmp(argv[1], "--ppm") == 0)
		dump(argv[2]);

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("diagram: all checks passed\n");
	return 0;
}
