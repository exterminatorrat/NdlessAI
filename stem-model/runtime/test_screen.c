/*
 * Host tests for the text renderer.
 *
 * The font and the primitives are the only pixels every screen passes
 * through, so they are pinned down here: the glyphs are checked against the
 * bitmap the table claims, the advance is checked to be exactly six columns,
 * and every primitive is called at and past each framebuffer edge to prove it
 * clips instead of writing into the neighbouring row or off the array.
 *
 * The framebuffer is passed as a pointer into a larger arena with canaries on
 * both sides, because a missing clipping test in screen_px() writes one row
 * down rather than outside the array: checking neighbours inside the buffer
 * and the guard words outside it catches both.
 *
 *   ./test_screen
 */
#include <stdio.h>
#include <string.h>

#include "screen.h"

#define PAD 16
#define CANARY 0xA5A5u

static uint16_t arena[PAD + SCREEN_W * SCREEN_HEIGHT + PAD];
static uint16_t *const fb = arena + PAD;
static int failures;

static void check(int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

static void reset(uint16_t fill)
{
	memset(arena, 0xA5, sizeof arena);
	for (int i = 0; i < SCREEN_W * SCREEN_HEIGHT; i++)
		fb[i] = fill;
}

static int guards_ok(void)
{
	for (int i = 0; i < PAD; i++)
		if (arena[i] != CANARY ||
		    arena[PAD + SCREEN_W * SCREEN_HEIGHT + i] != CANARY)
			return 0;
	return 1;
}

static uint16_t px(int x, int y)
{
	return fb[y * SCREEN_W + x];
}

static int dirty(void)
{
	int n = 0;

	for (int i = 0; i < SCREEN_W * SCREEN_HEIGHT; i++)
		if (fb[i] != SCREEN_WHITE)
			n++;
	return n;
}

static void test_metrics(void)
{
	check(screen_text_w("abc") == 18, "width of abc is 18");
	check(screen_text_w("") == 0, "width of empty is 0");
	check(screen_text_w(NULL) == 0, "width of NULL is 0");

	check(screen_text_fit("abcdef", 0) == 0, "fit 0px is 0");
	check(screen_text_fit("abcdef", -5) == 0, "fit negative is 0");
	check(screen_text_fit("abcdef", 6) == 1, "fit 6px is 1");
	check(screen_text_fit("abcdef", 11) == 1, "fit 11px is 1");
	check(screen_text_fit("abcdef", 12) == 2, "fit 12px is 2");
	check(screen_text_fit("abcdef", 100) == 6, "fit past the end is all");
	check(screen_text_fit(NULL, 100) == 0, "fit NULL is 0");
}

static void test_glyph_shape(void)
{
	/* The column bytes screen.c documents for I: 0x00, 0x41, 0x7F, 0x41, 0x00. */
	static const uint8_t eye[5] = { 0x00, 0x41, 0x7F, 0x41, 0x00 };

	reset(SCREEN_WHITE);
	screen_text(fb, 20, 20, "I", SCREEN_BLACK);
	for (int col = 0; col < 5; col++)
		for (int row = 0; row < 7; row++) {
			int want = (eye[col] >> row) & 1;
			int got = px(20 + col, 20 + row) == SCREEN_BLACK;

			if (want != got) {
				printf("FAIL I pixel (%d,%d): got %d want %d\n",
				       col, row, got, want);
				failures++;
			}
		}

	/* '-' is one row of five, three rows down. */
	reset(SCREEN_WHITE);
	screen_text(fb, 30, 10, "-", SCREEN_BLACK);
	for (int col = 0; col < 5; col++)
		for (int row = 0; row < 7; row++)
			check((px(30 + col, 10 + row) == SCREEN_BLACK) == (row == 3),
			      "'-' is a line at row 3");

	/* '.' sits at the baseline in the middle two columns. */
	reset(SCREEN_WHITE);
	screen_text(fb, 40, 10, ".", SCREEN_BLACK);
	for (int col = 0; col < 5; col++)
		for (int row = 0; row < 7; row++)
			check((px(40 + col, 10 + row) == SCREEN_BLACK) ==
			      ((col == 2 || col == 3) && row >= 5),
			      "'.' is two dots at the baseline");
}

static void test_glyph_table(void)
{
	uint16_t one[5 * 7], two[5 * 7];

	for (int ch = 32; ch <= 95; ch++) {
		char s[2] = { (char)ch, 0 };
		int ink = 0;

		reset(SCREEN_WHITE);
		screen_text(fb, 10, 10, s, SCREEN_BLACK);
		check(guards_ok(), "a glyph stays inside the framebuffer");

		/* Nothing outside the 5x7 box, whatever the glyph draws inside. */
		for (int y = 0; y < SCREEN_HEIGHT; y++)
			for (int x = 0; x < SCREEN_W; x++) {
				int inside = x >= 10 && x < 15 && y >= 10 && y < 17;

				if (!inside && px(x, y) != SCREEN_WHITE) {
					printf("FAIL glyph %d draws outside its box at %d,%d\n",
					       ch, x, y);
					failures++;
					y = SCREEN_HEIGHT;
					break;
				}
			}

		for (int i = 0; i < 5 * 7; i++) {
			one[i] = px(10 + i % 5, 10 + i / 5);
			if (one[i] != SCREEN_WHITE)
				ink++;
		}
		/* Every printable glyph except the space marks some pixel; a blank
		 * entry in the table would otherwise read as a clipped glyph. */
		check(ink > 0 || ch == ' ', "a glyph has ink unless it is the space");

		/* The same glyph at another position must be the same shape. */
		reset(SCREEN_WHITE);
		screen_text(fb, 100, 50, s, SCREEN_BLACK);
		for (int i = 0; i < 5 * 7; i++)
			two[i] = px(100 + i % 5, 50 + i / 5);
		if (memcmp(one, two, sizeof one) != 0) {
			printf("FAIL glyph %d is not translation invariant\n", ch);
			failures++;
		}
	}

	/* Lowercase is the uppercase glyph: the table has one case, and the
	 * renderer folds rather than drawing a second alphabet. */
	for (int ch = 'a'; ch <= 'z'; ch++) {
		char lower[2] = { (char)ch, 0 };
		char upper[2] = { (char)(ch - 'a' + 'A'), 0 };

		reset(SCREEN_WHITE);
		screen_text(fb, 0, 0, lower, SCREEN_BLACK);
		memcpy(one, fb, sizeof one);
		reset(SCREEN_WHITE);
		screen_text(fb, 0, 0, upper, SCREEN_BLACK);
		if (memcmp(one, fb, sizeof one) != 0) {
			printf("FAIL lowercase %c differs from %c\n", ch, upper[0]);
			failures++;
		}
	}

	/* Anything outside the table draws '?' rather than nothing. */
	reset(SCREEN_WHITE);
	screen_text(fb, 0, 0, "?", SCREEN_BLACK);
	memcpy(one, fb, sizeof one);
	reset(SCREEN_WHITE);
	screen_text(fb, 0, 0, "~", SCREEN_BLACK);
	check(memcmp(one, fb, sizeof one) == 0, "'~' draws '?'");
	reset(SCREEN_WHITE);
	screen_text(fb, 0, 0, "\x80", SCREEN_BLACK);
	check(memcmp(one, fb, sizeof one) == 0, "a high byte draws '?'");
}

static void test_advance(void)
{
	reset(SCREEN_WHITE);
	screen_text(fb, 10, 10, "--", SCREEN_BLACK);
	check(px(10, 13) == SCREEN_BLACK, "first dash starts at x");
	check(px(14, 13) == SCREEN_BLACK, "first dash is five wide");
	check(px(15, 13) == SCREEN_WHITE, "one column of spacing follows");
	check(px(16, 13) == SCREEN_BLACK, "second dash starts six columns on");
	check(px(20, 13) == SCREEN_BLACK, "second dash is five wide");

	/* Two calls at the advance equal one call with two characters. */
	reset(SCREEN_WHITE);
	screen_text(fb, 0, 0, "A", SCREEN_BLACK);
	screen_text(fb, SCREEN_ADVANCE, 0, "B", SCREEN_BLACK);
	{
		uint16_t split[SCREEN_W * SCREEN_HEIGHT];

		memcpy(split, fb, sizeof split);
		reset(SCREEN_WHITE);
		screen_text(fb, 0, 0, "AB", SCREEN_BLACK);
		check(memcmp(split, fb, sizeof split) == 0,
		      "the advance is exactly six columns");
	}
}

static void test_clip(void)
{
	reset(SCREEN_WHITE);
	screen_px(fb, -1, 0, SCREEN_BLACK);
	screen_px(fb, 0, -1, SCREEN_BLACK);
	screen_px(fb, SCREEN_W, 0, SCREEN_BLACK);
	screen_px(fb, 0, SCREEN_HEIGHT, SCREEN_BLACK);
	screen_px(fb, SCREEN_W, SCREEN_HEIGHT, SCREEN_BLACK);
	check(dirty() == 0, "off-screen pixels are dropped");
	check(guards_ok(), "off-screen pixels stay inside the buffer");

	reset(SCREEN_WHITE);
	screen_px(fb, 319, 239, SCREEN_BLACK);
	check(px(319, 239) == SCREEN_BLACK, "the last pixel is drawable");
	check(dirty() == 1, "the last pixel is the only one drawn");
	check(guards_ok(), "the last pixel stays inside the buffer");

	/* A fill that starts off the top-left keeps only its visible corner. */
	reset(SCREEN_WHITE);
	screen_fill(fb, -5, -5, 20, 20, SCREEN_BLACK);
	check(dirty() == 15 * 15, "fill keeps its visible corner");
	check(px(0, 0) == SCREEN_BLACK, "fill covers the origin");
	check(px(14, 14) == SCREEN_BLACK, "fill covers its visible last pixel");
	check(px(15, 0) == SCREEN_WHITE, "fill stops at its own edge");
	check(px(0, 15) == SCREEN_WHITE, "fill stops at its own bottom edge");
	check(guards_ok(), "fill at a negative origin stays inside");

	/* And one that runs off the bottom-right keeps only what fits. */
	reset(SCREEN_WHITE);
	screen_fill(fb, 310, 230, 20, 20, SCREEN_BLACK);
	check(dirty() == 10 * 10, "fill keeps its visible tail");
	check(px(319, 239) == SCREEN_BLACK, "fill reaches the last pixel");
	check(guards_ok(), "fill at the far corner stays inside");

	/* A rectangle whose top and left edges are off screen still draws the
	 * two edges that are visible, and only those. */
	reset(SCREEN_WHITE);
	screen_rect(fb, -2, -2, 10, 10, SCREEN_BLACK);
	check(px(0, 7) == SCREEN_BLACK, "rect draws its visible bottom edge");
	check(px(7, 0) == SCREEN_BLACK, "rect draws its visible right edge");
	check(px(7, 7) == SCREEN_BLACK, "rect draws the visible corner");
	check(px(0, 0) == SCREEN_WHITE, "rect leaves the clipped corner empty");
	check(px(3, 3) == SCREEN_WHITE, "rect stays hollow");
	check(guards_ok(), "rect at a negative origin stays inside");

	reset(SCREEN_WHITE);
	screen_rect(fb, 310, 230, 20, 20, SCREEN_BLACK);
	check(px(310, 230) == SCREEN_BLACK, "rect keeps its visible origin");
	check(px(319, 230) == SCREEN_BLACK, "rect's top edge is clipped");
	check(guards_ok(), "rect at the far corner stays inside");

	reset(SCREEN_WHITE);
	screen_hline(fb, -10, 5, 20, SCREEN_BLACK);
	check(px(0, 5) == SCREEN_BLACK, "hline starts where the screen does");
	check(px(9, 5) == SCREEN_BLACK, "hline keeps the part that fits");
	check(px(10, 5) == SCREEN_WHITE, "hline stops at its own width");
	check(dirty() == 10, "hline draws exactly its visible pixels");
	check(guards_ok(), "hline at a negative origin stays inside");

	reset(SCREEN_WHITE);
	screen_hline(fb, 315, 5, 20, SCREEN_BLACK);
	check(px(315, 5) == SCREEN_BLACK, "a short hline at the edge draws");
	check(px(319, 5) == SCREEN_BLACK, "hline reaches the last column");
	check(dirty() == 5, "hline clips at the right edge");
	check(guards_ok(), "hline past the right edge stays inside");

	/* Entirely off-screen lines draw nothing at all. */
	reset(SCREEN_WHITE);
	screen_line(fb, -20, 120, -1, 120, SCREEN_BLACK);
	screen_line(fb, 400, 0, 500, 240, SCREEN_BLACK);
	screen_line(fb, 0, 240, 320, 500, SCREEN_BLACK);
	check(dirty() == 0, "lines outside the screen draw nothing");
	check(guards_ok(), "lines outside the screen stay inside");

	reset(SCREEN_WHITE);
	screen_line(fb, -10, 0, 10, 0, SCREEN_BLACK);
	check(px(0, 0) == SCREEN_BLACK, "a clipped line keeps its visible part");
	check(px(10, 0) == SCREEN_BLACK, "a clipped line reaches its end");
	check(guards_ok(), "a clipped line stays inside");

	/* Text clips per glyph, so half a word at either edge is drawn. */
	reset(SCREEN_WHITE);
	screen_text(fb, -2, 5, "I", SCREEN_BLACK);
	check(dirty() > 0, "text at a negative x keeps its visible columns");
	check(px(0, 5) == SCREEN_BLACK, "the visible column of that glyph draws");
	check(guards_ok(), "text at a negative x stays inside");

	reset(SCREEN_WHITE);
	screen_text(fb, 316, 5, "III", SCREEN_BLACK);
	check(dirty() > 0 && dirty() <= 4 * 7, "text at the right edge clips");
	check(guards_ok(), "text at the right edge stays inside");

	reset(SCREEN_WHITE);
	screen_text(fb, -100, -100, "ABC", SCREEN_BLACK);
	check(dirty() == 0, "text off the top-left draws nothing");
	check(guards_ok(), "text off the top-left stays inside");

	/* Degenerate sizes must not read or write anywhere. */
	reset(SCREEN_WHITE);
	screen_fill(fb, 0, 0, 0, 0, SCREEN_BLACK);
	screen_rect(fb, 0, 0, 0, 0, SCREEN_BLACK);
	screen_hline(fb, 0, 0, -4, SCREEN_BLACK);
	check(guards_ok(), "empty primitives stay inside");
}

static void test_clear(void)
{
	reset(SCREEN_WHITE);
	screen_clear(fb, 0x1234);
	check(px(0, 0) == 0x1234 && px(319, 239) == 0x1234,
	      "clear fills both corners");
	{
		int n = 0;

		for (int i = 0; i < SCREEN_W * SCREEN_HEIGHT; i++)
			n += fb[i] == 0x1234;
		check(n == SCREEN_W * SCREEN_HEIGHT, "clear fills every pixel");
	}
	check(guards_ok(), "clear writes only the framebuffer");
}

int main(void)
{
	test_metrics();
	test_glyph_shape();
	test_glyph_table();
	test_advance();
	test_clip();
	test_clear();

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("screen: all checks passed\n");
	return 0;
}
