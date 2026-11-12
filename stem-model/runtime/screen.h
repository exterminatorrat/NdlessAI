#ifndef UI_SCREEN_H
#define UI_SCREEN_H

#include <stdint.h>

/*
 * The one text renderer on this target.
 *
 * A 5x7 font in a byte per column, drawn straight into the 320x240 565
 * framebuffer the display reads. graph.c kept a private copy of this while it
 * was the only screen in the program; with an interface as well there would be
 * two fonts that could drift, and two places to fix a glyph.
 *
 * Nothing here depends on Ndless, so the host tests draw into an array and
 * check pixels the same way the calculator draws into the LCD buffer.
 */

#define SCREEN_W 320
#define SCREEN_HEIGHT 240
#define SCREEN_GLYPH_W 5
#define SCREEN_GLYPH_H 7
#define SCREEN_ADVANCE 6     /* glyph plus one column of spacing */
#define SCREEN_LINE_H 9      /* glyph plus two rows of leading */

/*
 * The app's name and build stamp, as the calculator shows them. Every screen
 * prints this string in its title bar, so it is defined once: a stale .tns is
 * otherwise indistinguishable from a broken fix, and two screens drifting apart
 * would make the stamp worthless on whichever one nobody looked at. Bump the
 * stamp whenever a screen's behaviour changes.
 */
#define SCREEN_TITLE "STEM HELPER"
#define SCREEN_STAMP "V11"

/* Colour in 565, the format lcd_init(SCR_320x240_565) gives the program. */
#define SCREEN_WHITE 0xFFFFu
#define SCREEN_BLACK 0x0000u
#define SCREEN_DIM   0x8410u
#define SCREEN_ACCENT 0x07E0u

void screen_clear(uint16_t *fb, uint16_t c);
void screen_px(uint16_t *fb, int x, int y, uint16_t c);
void screen_text(uint16_t *fb, int x, int y, const char *s, uint16_t c);
void screen_fill(uint16_t *fb, int x, int y, int w, int h, uint16_t c);
void screen_rect(uint16_t *fb, int x, int y, int w, int h, uint16_t c);
void screen_hline(uint16_t *fb, int x, int y, int w, uint16_t c);
void screen_line(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t c);

/* Columns a string occupies, and the characters that fit a width. */
int screen_text_w(const char *s);
int screen_text_fit(const char *s, int px);

#endif /* UI_SCREEN_H */
