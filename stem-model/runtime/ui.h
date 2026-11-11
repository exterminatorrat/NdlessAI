#ifndef UI_H
#define UI_H

#include <stdint.h>

#include "pipeline.h"
#include "screen.h"

/*
 * The calculator's screen: one menu, one input line, and one pane that either
 * shows an answer with its working or a page of keypad help.
 *
 * The model is a state machine over key events and draws into a 320x240 565
 * framebuffer, so every screen here can be driven from a host test with
 * synthetic keys and inspected pixel by pixel. Nothing in this file knows what
 * Ndless is; device/ui_ndless.c reads the keypad and blits the result.
 *
 * Two things are deliberate rather than incidental:
 *
 *  - The pane is the only place text is shown, and it scrolls by whole rows.
 *    A worked answer is three lines of prose and numbers; a student should
 *    never have to guess which part of it is cut off, so the scroll offset is
 *    clamped at both ends and a bar on the right says where the window sits.
 *  - History keeps the question, not just the answer. The question is what a
 *    student wants back in the input line after a typo, and eight pairs fit in
 *    a fixed array, so there is no allocation anywhere on this path.
 */

/* Shown in the title bar so it is obvious which build is on the calculator. A
 * stale .tns is otherwise indistinguishable from a broken fix, so bump this
 * whenever the screen's behaviour changes. */
#define UI_TITLE "STEM HELPER"
#define UI_STAMP "V9"

/* The pane's text box: 51 six-pixel columns, with a scroll bar to the right. */
#define UI_PANE_X     4
#define UI_PANE_Y     16
#define UI_TEXT_COLS  51
#define UI_PANE_ROWS  18

#define UI_INPUT_Y    183
#define UI_INPUT_X    16
#define UI_INPUT_COLS 50

#define UI_FOOTER_Y   199

#define UI_INPUT_MAX  64
#define UI_ROWS_MAX   64
#define UI_HISTORY    8
#define UI_HIST_PANE  2048
#define UI_HELP_PAGES 3
#define UI_CALL_MAX   128

typedef enum {
	UI_KEY_NONE, UI_KEY_ACCEPT, UI_KEY_BACK, UI_KEY_UP, UI_KEY_DOWN,
	UI_KEY_LEFT, UI_KEY_RIGHT, UI_KEY_DELETE, UI_KEY_MODE,
	/*
	 * Returned by ui_key for the menu's Graph entry, never fed into it: the
	 * plotter owns the LCD through its own loop on the Ndless side, so the
	 * host has to step out of this model to run it.
	 */
	UI_KEY_GRAPH,
	/*
	 * Toggles the diagram over the answer pane. A relation with no diagram
	 * ignores it rather than showing an empty screen, so the key is safe to
	 * press on every answer.
	 */
	UI_KEY_DIAGRAM
} ui_key_t;

typedef enum {
	UI_MODE_MENU,
	UI_MODE_ASK,
	UI_MODE_HELP
} ui_mode_t;

typedef struct {
	char q[UI_INPUT_MAX];
	char pane[UI_HIST_PANE];
	/* Source offsets of the dimmed section: rows that start at or after
	 * `dim_from` and before `dim_to` are the working, not the answer. */
	int16_t dim_from;
	int16_t dim_to;
} ui_hist_t;

typedef struct {
	pipeline_t *p;          /* NULL when no model file was found */
	const char *note;       /* why there is no model, or NULL */

	ui_mode_t mode;
	int menu_sel;
	int help_page;

	char input[UI_INPUT_MAX];
	int input_len;
	int cursor;

	char rows[UI_ROWS_MAX][UI_TEXT_COLS + 1];
	uint8_t row_dim[UI_ROWS_MAX];
	int nrows;
	int pane_top;
	/* The text the answer pane currently holds. Help borrows the pane, so it
	 * has to be possible to put the answer back when the student returns. */
	char pane_src[UI_HIST_PANE];
	int16_t pane_dim_from, pane_dim_to;

	/* The call the last answer came from: a diagram is drawn from it, and an
	 * answer that did not come from a relation has none. */
	char last_call[UI_CALL_MAX];
	int diagram;

	ui_hist_t hist[UI_HISTORY];
	int hist_n;
	int hist_head;
	int recall;
} ui_t;

void ui_init(ui_t *u, pipeline_t *p, const char *missing_model_note);

/*
 * Feeds one key. Returns UI_KEY_BACK when the student asked to leave the
 * screen, or UI_KEY_GRAPH when the menu asked for the plotter; every other
 * value means the model changed and the caller should redraw.
 */
ui_key_t ui_key(ui_t *u, ui_key_t k);

/* Inserts a printable character at the input cursor. Returns 1 if it went in. */
int ui_type(ui_t *u, char c);

/* Draws the whole screen. Returns the number of rows hidden below the window. */
int ui_render(ui_t *u, uint16_t *fb);

/*
 * Displays one pipeline outcome. `rc` is a PIPELINE_* code, `answer` and
 * `call` are what pipeline_answer filled in; `question` is what was asked and
 * is what history keeps. Split out of the key handler so the host tests can
 * exercise all four messages without a weights file.
 */
void ui_result(ui_t *u, const char *question, int rc,
               const char *answer, const char *call);

/* The wrapped pane row the last render would draw, and whether it is dimmed. */
const char *ui_row(const ui_t *u, int i);
int ui_row_dim(const ui_t *u, int i);

/* Implemented in device/ui_ndless.c: runs the loop until the student leaves. */
int ui_run(pipeline_t *p, const char *missing_model_note);

#endif /* UI_H */
