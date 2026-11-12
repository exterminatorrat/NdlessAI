/*
 * Ndless side of the plotting screen: keys, the LCD, and the loop.
 * All plotting logic lives in runtime/graph.c so the host tests can run it.
 */
#include <libndls.h>
#include <stdlib.h>

#include "graph.h"
#include "graph_ui.h"

static uint16_t fb[G_SCREEN_W * G_SCREEN_H];

static int poll_key(void)
{
	if (isKeyPressed(KEY_NSPIRE_ESC))
		return GK_QUIT;
	if (isKeyPressed(KEY_NSPIRE_LEFT))
		return GK_LEFT;
	if (isKeyPressed(KEY_NSPIRE_RIGHT))
		return GK_RIGHT;
	if (isKeyPressed(KEY_NSPIRE_UP))
		return GK_UP;
	if (isKeyPressed(KEY_NSPIRE_DOWN))
		return GK_DOWN;
	if (isKeyPressed(KEY_NSPIRE_PLUS))
		return GK_ZOOM_IN;
	if (isKeyPressed(KEY_NSPIRE_MINUS))
		return GK_ZOOM_OUT;
	if (isKeyPressed(KEY_NSPIRE_T))
		return GK_TRACE;
	if (isKeyPressed(KEY_NSPIRE_R))
		return GK_RESET;
	return GK_NONE;
}

/*
 * Draws a prepared state until the student leaves, then hands the display back
 * in the mode the rest of the program draws in. Both entries below end here, so
 * the screen mode is restored on every path, including the one where lcd_init
 * refuses the mode in the first place.
 */
static void plot_screen(gstate_t *g)
{
	if (!lcd_init(SCR_320x240_565)) {
		show_msgbox("Graph", "This screen mode is not available.");
		return;
	}

	for (;;) {
		int k;

		graph_render(g, fb);
		lcd_blit(fb, SCR_320x240_565);
		do {
			k = poll_key();
			if (k != GK_NONE)
				break;
			idle();
		} while (1);
		wait_no_key_pressed();
		if (graph_key(g, k))
			break;
	}

	lcd_init(SCR_TYPE_INVALID);
}

/* What the graph screen's own input line handed over. The line was checked
 * against this same parser before the plot opened, so a failure here means the
 * two disagreed rather than that the student typed something odd. */
int graph_run_text(const char *text)
{
	char err[64];
	gstate_t *g;

	if (!text || !*text)
		return 0;
	g = malloc(sizeof *g);
	if (!g)
		return 0;
	if (graph_init(g, text, err, sizeof err) < 0) {
		show_msgbox("Graph", err);
		free(g);
		return 0;
	}
	plot_screen(g);
	free(g);
	return 0;
}

/* What the answer screen handed over: the curve behind the call the answer
 * came from, with the answer marked on it. */
int graph_run_call(const char *call)
{
	char err[64];
	gstate_t *g;

	if (!call || !*call)
		return 0;
	g = malloc(sizeof *g);
	if (!g)
		return 0;
	if (graph_from_call(g, call, 0, err, sizeof err) != G_CALL_OK) {
		show_msgbox("Graph", err);
		free(g);
		return 0;
	}
	plot_screen(g);
	free(g);
	return 0;
}
