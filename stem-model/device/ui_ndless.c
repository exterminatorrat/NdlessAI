/*
 * Ndless side of the screen: read the keypad, blit the model's framebuffer.
 *
 * Every decision — what a key means, what the pane says, where the window
 * sits — belongs to runtime/ui.c, where the host tests can drive it with
 * synthetic keys. This file is the only place that knows the calculator has
 * keys at all.
 */
#include <libndls.h>
#include <stdint.h>

#include "graph_ui.h"
#include "screen.h"
#include "ui.h"

static uint16_t fb[SCREEN_W * SCREEN_HEIGHT];

typedef struct {
	const t_key *key;
	char ch;
} key_char_t;

/*
 * Printable keys. The SDK's isKeyPressed macro takes the address of its
 * argument, so a table of keys has to be dereferenced and re-addressed at the
 * call site; writing the key inline for each of the fifty entries instead
 * would be fifty chances to bind the wrong character.
 */
static const key_char_t char_keys[] = {
	{ &KEY_NSPIRE_A, 'a' }, { &KEY_NSPIRE_B, 'b' }, { &KEY_NSPIRE_C, 'c' },
	{ &KEY_NSPIRE_D, 'd' }, { &KEY_NSPIRE_E, 'e' }, { &KEY_NSPIRE_F, 'f' },
	{ &KEY_NSPIRE_G, 'g' }, { &KEY_NSPIRE_H, 'h' }, { &KEY_NSPIRE_I, 'i' },
	{ &KEY_NSPIRE_J, 'j' }, { &KEY_NSPIRE_K, 'k' }, { &KEY_NSPIRE_L, 'l' },
	{ &KEY_NSPIRE_M, 'm' }, { &KEY_NSPIRE_N, 'n' }, { &KEY_NSPIRE_O, 'o' },
	{ &KEY_NSPIRE_P, 'p' }, { &KEY_NSPIRE_Q, 'q' }, { &KEY_NSPIRE_R, 'r' },
	{ &KEY_NSPIRE_S, 's' }, { &KEY_NSPIRE_T, 't' }, { &KEY_NSPIRE_U, 'u' },
	{ &KEY_NSPIRE_V, 'v' }, { &KEY_NSPIRE_W, 'w' }, { &KEY_NSPIRE_X, 'x' },
	{ &KEY_NSPIRE_Y, 'y' }, { &KEY_NSPIRE_Z, 'z' },
	{ &KEY_NSPIRE_0, '0' }, { &KEY_NSPIRE_1, '1' }, { &KEY_NSPIRE_2, '2' },
	{ &KEY_NSPIRE_3, '3' }, { &KEY_NSPIRE_4, '4' }, { &KEY_NSPIRE_5, '5' },
	{ &KEY_NSPIRE_6, '6' }, { &KEY_NSPIRE_7, '7' }, { &KEY_NSPIRE_8, '8' },
	{ &KEY_NSPIRE_9, '9' },
	{ &KEY_NSPIRE_SPACE, ' ' }, { &KEY_NSPIRE_PERIOD, '.' },
	{ &KEY_NSPIRE_COMMA, ',' }, { &KEY_NSPIRE_PLUS, '+' },
	{ &KEY_NSPIRE_MINUS, '-' }, { &KEY_NSPIRE_NEGATIVE, '-' },
	{ &KEY_NSPIRE_MULTIPLY, '*' }, { &KEY_NSPIRE_DIVIDE, '/' },
	{ &KEY_NSPIRE_LP, '(' }, { &KEY_NSPIRE_RP, ')' },
	{ &KEY_NSPIRE_SQU, '^' }, { &KEY_NSPIRE_EQU, '=' },
	{ &KEY_NSPIRE_COLON, ':' }
};

/*
 * One key, if one is down. Returns 0 when none is, 1 when *k or *ch was
 * filled. The arrows and the keypad keys keep their meaning; everything else
 * that prints comes through the table above.
 */
static int read_key(ui_key_t *k, char *ch)
{
	if (isKeyPressed(KEY_NSPIRE_ESC)) {
		*k = UI_KEY_BACK;
		return 1;
	}
	if (isKeyPressed(KEY_NSPIRE_ENTER) || isKeyPressed(KEY_NSPIRE_RET)) {
		*k = UI_KEY_ACCEPT;
		return 1;
	}
	if (isKeyPressed(KEY_NSPIRE_UP)) {
		*k = UI_KEY_UP;
		return 1;
	}
	if (isKeyPressed(KEY_NSPIRE_DOWN)) {
		*k = UI_KEY_DOWN;
		return 1;
	}
	if (isKeyPressed(KEY_NSPIRE_LEFT)) {
		*k = UI_KEY_LEFT;
		return 1;
	}
	if (isKeyPressed(KEY_NSPIRE_RIGHT)) {
		*k = UI_KEY_RIGHT;
		return 1;
	}
	if (isKeyPressed(KEY_NSPIRE_DEL)) {
		*k = UI_KEY_DELETE;
		return 1;
	}
	if (isKeyPressed(KEY_NSPIRE_MENU)) {
		*k = UI_KEY_MODE;
		return 1;
	}
	for (unsigned i = 0; i < sizeof char_keys / sizeof char_keys[0]; i++) {
		if (isKeyPressed(*char_keys[i].key)) {
			*ch = char_keys[i].ch;
			return 1;
		}
	}
	return 0;
}

int ui_run(pipeline_t *p, const char *note)
{
	static ui_t u;

	ui_init(&u, p, note);

	if (!lcd_init(SCR_320x240_565)) {
		show_msgbox("STEM helper", "This screen mode is not available.");
		return 0;
	}

	for (;;) {
		ui_key_t k = UI_KEY_NONE;
		ui_key_t act;
		char ch = 0;

		ui_render(&u, fb);
		lcd_blit(fb, SCR_320x240_565);

		do {
			if (read_key(&k, &ch))
				break;
			idle();
		} while (1);
		wait_no_key_pressed();

		if (ch) {
			ui_type(&u, ch);
			continue;
		}
		act = ui_key(&u, k);
		if (act == UI_KEY_BACK)
			break;
		if (act == UI_KEY_GRAPH) {
			/* The plotter owns the LCD mode through its own loop; hand the
			 * screen back afterwards or this loop blits into the wrong one. */
			lcd_init(SCR_TYPE_INVALID);
			graph_run();
			if (!lcd_init(SCR_320x240_565))
				break;
		}
	}

	lcd_init(SCR_TYPE_INVALID);
	return 0;
}
