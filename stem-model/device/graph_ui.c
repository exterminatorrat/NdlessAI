/*
 * Ndless side of the graphing feature: keys, the LCD, and the prompt.
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

int graph_run(void)
{
	char *input = NULL;
	gstate_t *g;
	char err[64];
	int len;

	len = show_msg_user_input("Graph", "Enter y = f(x):", "x^2-3x+1", &input);
	if (len < 0 || !input)
		return 0;

	g = malloc(sizeof *g);
	if (!g) {
		free(input);
		return 0;
	}
	if (graph_init(g, input, err, sizeof err) < 0) {
		show_msgbox("Graph", err);
		free(input);
		free(g);
		return 0;
	}
	free(input);

	if (!lcd_init(SCR_320x240_565)) {
		show_msgbox("Graph", "This screen mode is not available.");
		free(g);
		return 0;
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
	free(g);
	return 0;
}
