/*
 * Host tests for the screen model.
 *
 * The calculator cannot be debugged interactively, so every screen is driven
 * here with synthetic keys: the input line is typed into, the pane is scrolled
 * past both ends, all four pipeline outcomes are displayed, and history is
 * walked back and forth. Each case asserts on the model's own state and on the
 * pixels where the change is supposed to land, because a model that is
 * self-consistent but draws in the wrong place looks correct to a state check.
 *
 *   ./test_ui
 */
#include <stdio.h>
#include <string.h>

#include "pipeline.h"
#include "screen.h"
#include "ui.h"
#include "coursetable.h"
#include "diagram.h"
#include "reltable.h"

/* Revised 2026-11-16: solve scientific inputs through the formula picker. */

#define PAD 8
#define CANARY 0xA5A5u

static uint16_t arena[PAD + SCREEN_W * SCREEN_HEIGHT + PAD];
static uint16_t *const fb = arena + PAD;
static ui_t u;
static pipeline_t model;
static int failures;

#define PANE_W SCREEN_W
#define PANE_H (UI_PANE_ROWS * SCREEN_LINE_H)

static void test_diagram_screen(void);
static void test_plot_key(void);
static void test_template(void);
static void test_course_picker(void);
static void check(int cond, const char *what)
{
	if (!cond) {
		printf("FAIL %s\n", what);
		failures++;
	}
}

static int guards_ok(void)
{
	for (int i = 0; i < PAD; i++)
		if (arena[i] != CANARY ||
		    arena[PAD + SCREEN_W * SCREEN_HEIGHT + i] != CANARY)
			return 0;
	return 1;
}

static void render(void)
{
	memset(arena, 0xA5, sizeof arena);
	ui_render(&u, fb);
	check(guards_ok(), "a render stays inside the framebuffer");
}

static int ink(int x, int y, int w, int h)
{
	int n = 0;

	for (int row = y; row < y + h; row++)
		for (int col = x; col < x + w; col++)
			if (fb[row * SCREEN_W + col] != SCREEN_BLACK)
				n++;
	return n;
}

static int ink_of(int x, int y, int w, int h, uint16_t colour)
{
	int n = 0;

	for (int row = y; row < y + h; row++)
		for (int col = x; col < x + w; col++)
			if (fb[row * SCREEN_W + col] == colour)
				n++;
	return n;
}

static unsigned long hash(int x, int y, int w, int h)
{
	unsigned long acc = 5381;

	for (int row = y; row < y + h; row++)
		for (int col = x; col < x + w; col++)
			acc = acc * 33 + fb[row * SCREEN_W + col];
	return acc;
}

static void put_ch(char **at, const char *end, char c)
{
	if (*at < end)
		*(*at)++ = c;
	**at = '\0';
}

static void put(char **at, const char *end, const char *s)
{
	while (*s)
		put_ch(at, end, *s++);
}

/*
 * Whether one line of the frame is exactly this text, in this colour, on the
 * background the screens clear to. The expected frame is drawn by the same
 * renderer into a scratch buffer, so the assertion is about the string and the
 * colour at that row rather than about "something was drawn here".
 */
static int band_is_text(int y, const char *text, uint16_t colour)
{
	static uint16_t scratch[SCREEN_W * SCREEN_HEIGHT];

	if (y < 0 || y + SCREEN_LINE_H > SCREEN_HEIGHT)
		return 0;
	screen_clear(scratch, SCREEN_BLACK);
	screen_text(scratch, UI_PANE_X, y, text, colour);
	for (int r = y; r < y + SCREEN_LINE_H; r++)
		for (int c = 0; c < SCREEN_W; c++)
			if (fb[r * SCREEN_W + c] != scratch[r * SCREEN_W + c])
				return 0;
	return 1;
}

static void type_text(const char *s)
{
	while (*s)
		ui_type(&u, *s++);
}

/*
 * How many catalogued names contain `q` verbatim. The finder's own matcher also
 * folds case and treats a space as an underscore, and those are asserted as
 * behaviour below rather than folded in here: the count has to come from the
 * catalogue, and the promises from the finder.
 */
static int names_containing(const char *q)
{
	int n = 0;

	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (strstr(rel_rows[i].name, q))
			n++;
	return n;
}

/* Whether the finder's current list holds this name at all. */
static int listed(const char *name)
{
	for (int i = 0; i < u.tmpl_count; i++)
		if (strcmp(rel_rows[u.tmpl_rels[i]].name, name) == 0)
			return 1;
	return 0;
}

static int page_has(const char *needle)
{
	for (int i = 0; i < u.nrows; i++)
		if (strstr(ui_row(&u, i), needle))
			return 1;
	return 0;
}

static int rows_have(const char *needle)
{
	return page_has(needle);
}

static void ask(const char *text)
{
	type_text(text);
	ui_key(&u, UI_KEY_ACCEPT);
}

/* ---- menu and modes ---- */

static void test_menu(void)
{
	ui_init(&u, NULL, NULL);
	check(u.mode == UI_MODE_MENU, "the screen opens on the menu");
	render();
	check(ink(4, 0, 120, 12) > 0, "the title is drawn");
	check(ink(280, 0, 40, 12) > 0, "the mode label is drawn on the right");
	check(ink(4, 199, 300, 20) > 0, "the key hints are drawn");

	ui_key(&u, UI_KEY_UP);
	check(u.menu_sel == 0, "up at the top stays at the top");
	ui_key(&u, UI_KEY_DOWN);
	check(u.menu_sel == 1, "down moves the selection");
	for (int i = 0; i < 10; i++)
		ui_key(&u, UI_KEY_DOWN);
	check(u.menu_sel == 6, "down at the bottom stops at the bottom");

	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_MODE);
	check(u.mode == UI_MODE_ASK, "a mode change reaches the input line");
	ui_key(&u, UI_KEY_MODE);
	check(u.mode == UI_MODE_MENU, "and a second one comes back to the menu");

	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_DOWN);
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_NONE,
	      "the menu opens the graph screen without leaving");
	check(u.mode == UI_MODE_GRAPH, "the graph screen is its own mode");

	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 6; i++)
		ui_key(&u, UI_KEY_DOWN);
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_BACK, "the menu can leave the app");
	check(ui_key(&u, UI_KEY_BACK) == UI_KEY_BACK, "back from the menu leaves the app");
}

/* ---- keypad help ---- */

static void test_help(void)
{
	static const char *const want[] = {
		"integrate(x^2,0,3)",
		"d/dx(x^3)",
		"series(sin(x),0,5)",
		"ode(2y,0,1,1,64)",
		"desolve(2y,0,1)",
		"desolve(x*y,0,2)",
		"weights.bin.tns",
		"sqrt(50)+ln(2)"
	};
	int found[sizeof want / sizeof want[0]] = { 0 };

	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_DOWN);
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_NONE, "help opens without leaving");
	check(u.mode == UI_MODE_HELP && u.help_page == 0, "help starts on the first page");

	for (int p = 0; p < UI_HELP_PAGES; p++) {
		check(u.help_page == p, "right pages forward through the help");
		render();
		for (unsigned i = 0; i < sizeof want / sizeof want[0]; i++)
			if (page_has(want[i]))
				found[i] = 1;
		ui_key(&u, UI_KEY_RIGHT);
	}
	check(u.help_page == UI_HELP_PAGES - 1, "right stops on the last page");
	for (unsigned i = 0; i < sizeof want / sizeof want[0]; i++)
		if (!found[i]) {
			printf("FAIL keypad help never shows %s\n", want[i]);
			failures++;
		}

	ui_key(&u, UI_KEY_LEFT);
	check(u.help_page == UI_HELP_PAGES - 2, "left pages back");
	ui_key(&u, UI_KEY_BACK);
	check(u.mode == UI_MODE_MENU, "back from help returns to the menu");

	/* Every example the pages print must answer on the typed path, or the
	 * help promises a form the solver refuses. */
	{
		static const char *const examples[] = {
			"integrate(x^2,0,3)", "integrate(sin(x),0,pi)",
			"int(x^2,0,3)", "d/dx(x^3)", "diff(x^3)", "d/dx(5)",
			"series(sin(x),0,5)", "ode(2y,0,1,1,64)", "ode(x,0,0,2,32)",
			"desolve(2y,0,1)", "desolve(x*y,0,2)", "sqrt(50)+ln(2)",
			"3/4 + 2*5", "2^10", "abs(-7)", "det([1,2;3,4])",
			"inv([2,1;1,3])"
		};
		char answer[64];

		for (unsigned i = 0; i < sizeof examples / sizeof examples[0]; i++)
			if (pipeline_typed_answer(examples[i], answer, (int)sizeof answer) != PIPELINE_OK) {
				printf("FAIL keypad help advertises %s, which does not answer\n",
				       examples[i]);
				failures++;
			}
	}

	/* The pages go through the same pane as the answers, so the long one
	 * scrolls with the same keys. */
	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	render();
	check(ink(0, UI_PANE_Y, PANE_W, PANE_H) > 0, "the help page reaches the pane");
	check(u.nrows > UI_PANE_ROWS, "the calculus page is longer than the window");
	check(strcmp(ui_row(&u, 0), "Calculus and series") == 0,
	      "the page starts where the pane does");
	ui_key(&u, UI_KEY_DOWN);
	check(u.pane_top == 1, "help scrolls one row per key");
	check(ui_render(&u, fb) == u.nrows - UI_PANE_ROWS - 1,
	      "help reports what is left below");

	/* Help borrows the answer pane; leaving it puts the answer back. */
	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_MODE);
	ui_result(&u, "q", PIPELINE_OK, "42", "");
	ui_key(&u, UI_KEY_MODE);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	check(strcmp(ui_row(&u, 0), "Calculus and series") == 0,
	      "help takes over the pane while it is open");
	ui_key(&u, UI_KEY_BACK);
	ui_key(&u, UI_KEY_UP);
	ui_key(&u, UI_KEY_UP);
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.mode == UI_MODE_ASK, "the menu returns to the input line");
	check(strcmp(ui_row(&u, 0), "42") == 0,
	      "the answer is back where the help left it");
}

/* ---- the input line ---- */

static void test_input(void)
{
	unsigned long pane;

	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_MODE);
	ui_result(&u, "q", PIPELINE_OK, "42", "");
	render();
	pane = hash(0, UI_PANE_Y, PANE_W, PANE_H);

	type_text("12+3");
	check(strcmp(u.input, "12+3") == 0, "typing fills the input line");
	check(u.input_len == 4 && u.cursor == 4, "the cursor follows the text");
	render();
	check(ink_of(UI_INPUT_X, UI_INPUT_Y, 120, 9, SCREEN_WHITE) > 0,
	      "the typed text is drawn in the input line");
	check(ink(0, UI_PANE_Y, PANE_W, PANE_H) > 0, "the answer is still drawn");

	ui_key(&u, UI_KEY_LEFT);
	ui_key(&u, UI_KEY_LEFT);
	check(u.cursor == 2, "left moves the cursor");
	ui_key(&u, UI_KEY_DELETE);
	check(strcmp(u.input, "1+3") == 0, "delete removes the character before the cursor");
	check(u.cursor == 1, "delete moves the cursor back");
	ui_type(&u, 'x');
	check(strcmp(u.input, "1x+3") == 0, "typing inserts at the cursor");
	check(u.cursor == 2 && u.input_len == 4, "insert grows the line");

	while (u.cursor < u.input_len)
		ui_key(&u, UI_KEY_RIGHT);
	check(u.cursor == u.input_len, "right stops at the end of the line");
	ui_key(&u, UI_KEY_RIGHT);
	check(u.cursor == u.input_len, "right at the end does nothing");

	while (u.input_len > 0)
		ui_key(&u, UI_KEY_DELETE);
	check(u.input_len == 0, "delete empties the line");

	render();
	check(hash(0, UI_PANE_Y, PANE_W, PANE_H) == pane,
	      "editing leaves the answer pane untouched");
	check(ink_of(UI_INPUT_X, UI_INPUT_Y, 200, 9, SCREEN_WHITE) == 0,
	      "the empty line has no text in it");

	ui_key(&u, UI_KEY_BACK);
	check(u.mode == UI_MODE_MENU, "back from an empty line returns to the menu");
	ui_key(&u, UI_KEY_MODE);
	type_text("9");
	ui_key(&u, UI_KEY_BACK);
	check(u.input_len == 0 && u.mode == UI_MODE_ASK,
	      "back clears a typed line before it leaves");
}

/* ---- answering and history ---- */

static void test_answer_and_history(void)
{
	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_MODE);

	ask("1+1");
	check(strcmp(ui_row(&u, 0), "2") == 0,
	      "typed arithmetic is answered with no model file");
	check(u.input_len == 0, "asking clears the input line");
	render();
	check(ink(0, UI_PANE_Y, PANE_W, PANE_H) > 0, "the answer reaches the pane");
	check(ink_of(4, 3, 120, 7, SCREEN_WHITE) > 0, "the title still has ink");

	ask("2*3");
	check(strcmp(ui_row(&u, 0), "6") == 0, "a second question replaces the pane");
	ask("10/4");
	check(strcmp(ui_row(&u, 0), "2.5") == 0, "a third question replaces the pane");

	/*
	 * The pane already shows the newest pair, so the first recall asks for
	 * the one before it: the second question, then the first.
	 */
	ui_key(&u, UI_KEY_LEFT);
	check(strcmp(u.input, "2*3") == 0, "the first recall comes back second");
	check(strcmp(ui_row(&u, 0), "6") == 0, "a recall brings its answer back");
	ui_key(&u, UI_KEY_LEFT);
	check(strcmp(u.input, "1+1") == 0, "the second recall comes back first");
	ui_key(&u, UI_KEY_LEFT);
	check(strcmp(u.input, "1+1") == 0, "recall stops at the oldest question");

	ui_key(&u, UI_KEY_RIGHT);
	check(strcmp(u.input, "2*3") == 0, "right walks forward again");
	ui_key(&u, UI_KEY_RIGHT);
	check(strcmp(u.input, "10/4") == 0, "right reaches the newest question");
	ui_key(&u, UI_KEY_RIGHT);
	check(strcmp(u.input, "10/4") == 0, "recall stops at the newest question");

	/* Editing a recalled question detaches from the ring, so the next recall
	 * starts from the newest rather than from where the edit happened. */
	ui_type(&u, '7');
	check(u.recall < 0, "typing leaves the history ring");
	ui_key(&u, UI_KEY_BACK);
	ui_key(&u, UI_KEY_LEFT);
	check(strcmp(u.input, "10/4") == 0, "after an edit the newest comes back");

	/* A fixed ring: the ninth question pushes the first one out. */
	ui_key(&u, UI_KEY_BACK);
	ask("2+2");
	ask("3+3");
	ask("4+4");
	ask("5+5");
	ask("6+6");
	ask("7+7");
	check(u.hist_n == UI_HISTORY, "the ring saturates at eight pairs");
	ui_key(&u, UI_KEY_LEFT);
	check(strcmp(u.input, "6+6") == 0, "the oldest question has been dropped");
	for (int i = 0; i < 6; i++)
		ui_key(&u, UI_KEY_LEFT);
	check(strcmp(u.input, "2*3") == 0, "the ring holds the eight newest");
	ui_key(&u, UI_KEY_LEFT);
	check(strcmp(u.input, "2*3") == 0, "and stops at its oldest entry");
}

/* ---- wrapping and scrolling ---- */

static void test_wrap_and_scroll(void)
{
	static char big[2048];
	int n = 0, words = 0, hidden;
	unsigned long header, footer;
	char top[UI_TEXT_COLS + 1];
	int max;

	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_MODE);
	for (int i = 0; i < 660; i++) {
		memcpy(big + n, "ab ", 3);
		n += 3;
	}
	big[n] = '\0';
	ui_result(&u, "a very long answer", PIPELINE_OK, big, "");

	check(u.nrows > UI_PANE_ROWS, "a long answer needs more rows than fit");
	for (int i = 0; i < u.nrows; i++) {
		const char *row = ui_row(&u, i);
		const char *p = row;

		check((int)strlen(row) <= UI_TEXT_COLS, "every row fits the pane width");
		check(row[0] != ' ', "no row starts with the space it wrapped on");
		while (*p) {
			while (*p == ' ')
				p++;
			if (*p)
				words++;
			while (*p && *p != ' ')
				p++;
		}
	}
	check(words == 660, "wrapping keeps every word");

	render();
	hidden = ui_render(&u, fb);
	check(hidden == u.nrows - UI_PANE_ROWS, "the render reports the hidden rows");

	/* Text stops inside the pane's 51 columns; the scroll bar lives past it. */
	check(ink(4 + UI_TEXT_COLS * SCREEN_ADVANCE, UI_PANE_Y, 5, PANE_H) == 0,
	      "no text is drawn beside the scroll bar");

	/* Numbered lines, so "the window moved" is a fact about the text rather
	 * than a hash of two rows that happen to look alike. */
	{
		char lines[512];
		char *at = lines;
		const char *endp = lines + sizeof lines - 1;

		for (int i = 1; i <= 30; i++) {
			if (i > 1)
				put_ch(&at, endp, '\n');
			put(&at, endp, "line ");
			if (i >= 10)
				put_ch(&at, endp, (char)('0' + i / 10));
			put_ch(&at, endp, (char)('0' + i % 10));
		}
		ui_result(&u, "numbered", PIPELINE_OK, lines, "");
	}
	check(u.nrows == 30, "one row per line");

	render();
	max = u.nrows - UI_PANE_ROWS;
	check(ui_render(&u, fb) == max, "the render reports the rows below");
	header = hash(0, 0, SCREEN_W, 13);
	footer = hash(0, UI_FOOTER_Y, SCREEN_W, 20);
	strcpy(top, ui_row(&u, 0));
	check(strcmp(top, "line 1") == 0, "the top row is the first line");

	ui_key(&u, UI_KEY_DOWN);
	check(u.pane_top == 1, "down scrolls one row");
	render();
	check(strcmp(ui_row(&u, u.pane_top), top) != 0,
	      "the first visible row changed");
	check(strcmp(ui_row(&u, u.pane_top), "line 2") == 0,
	      "the next line is the first visible one");
	check(hash(0, 0, SCREEN_W, 13) == header, "scrolling leaves the header alone");
	check(hash(0, UI_FOOTER_Y, SCREEN_W, 20) == footer,
	      "scrolling leaves the footer alone");

	for (int i = 0; i < 4; i++)
		ui_key(&u, UI_KEY_DOWN);
	check(u.pane_top == 5, "each key moves exactly one row");

	for (int i = 0; i < 200; i++)
		ui_key(&u, UI_KEY_DOWN);
	check(u.pane_top == max, "down stops at the last page");
	check(ui_render(&u, fb) == 0, "nothing is hidden at the last page");

	ui_key(&u, UI_KEY_UP);
	check(u.pane_top == max - 1, "up scrolls one row back");
	for (int i = 0; i < 200; i++)
		ui_key(&u, UI_KEY_UP);
	check(u.pane_top == 0, "up stops at the first page");

	ui_key(&u, UI_KEY_DOWN);
	render();
	check(ink_of(SCREEN_W - 5, UI_PANE_Y, 2, PANE_H, SCREEN_ACCENT) > 0,
	      "the scroll bar shows where the window sits");
}

/* ---- the pipeline's four outcomes ---- */

static void test_errors(void)
{
	memset(&model, 0, sizeof model);
	model.ready = 1;

	ui_init(&u, NULL, "weights.bin.tns is not beside this program.");
	ui_key(&u, UI_KEY_MODE);
	ask("a car travels 10 m");
	check(strcmp(ui_row(&u, 0), "No model file: word problems are off.") == 0,
	      "no model produces its documented first line");
	check(rows_have("weights.bin.tns"), "the no-model note says which file is missing");
	check(rows_have("sqrt(50)+ln(2)"), "the no-model note says what still works");
	check(ui_row_dim(&u, 0) == 0, "the headline is not dimmed");

	ui_init(&u, &model, NULL);
	ui_key(&u, UI_KEY_MODE);
	ui_result(&u, "q", PIPELINE_UNCERTAIN, "", "CALL latent_heat 25 3");
	check(strcmp(ui_row(&u, 0), "Some numbers here are not in the question.") == 0,
	      "uncertain produces its documented first line");
	check(rows_have("CALL latent_heat 25 3"), "uncertain shows the call it tried");
	check(ui_row_dim(&u, 0) == 0 && ui_row_dim(&u, 1) != 0,
	      "the explanation is dimmed under the headline");

	ui_result(&u, "q", PIPELINE_ERR, "", "CALL friction 3 4");
	check(strcmp(ui_row(&u, 0), "I could not read that as a problem.") == 0,
	      "an error produces its documented first line");
	check(rows_have("CALL friction 3 4"), "an error shows the call the model produced");
	check(rows_have("tokenizer"), "an error names the usual cause");

	ui_result(&u, "q", PIPELINE_ERR, "", "");
	check(rows_have("(nothing)"), "an error with no call shows that there was none");
}

/* ---- the working above the answer ---- */

static void test_steps(void)
{
	memset(&model, 0, sizeof model);
	model.ready = 1;
	ui_init(&u, &model, NULL);
	ui_key(&u, UI_KEY_MODE);

	ui_result(&u, "how much heat melts 25 g", PIPELINE_OK, "8350 J",
	          "CALL latent_heat 25 334");
	check(u.nrows == 3, "the working is three rows");
	check(strcmp(ui_row(&u, 0), "Q = m * L") == 0, "the formula comes first");
	check(strcmp(ui_row(&u, 1), "= 25 * 334") == 0, "the substitution comes next");
	check(strcmp(ui_row(&u, 2), "= 8350 J") == 0, "the answer comes last");
	check(ui_row_dim(&u, 0) && ui_row_dim(&u, 1), "the working is dimmed");
	check(!ui_row_dim(&u, 2), "the answer is not dimmed");

	render();
	check(ink_of(UI_PANE_X, UI_PANE_Y, UI_TEXT_COLS * SCREEN_ADVANCE, 9,
	             SCREEN_DIM) > 0, "the working is drawn in the dim colour");
	check(ink_of(UI_PANE_X, UI_PANE_Y + 2 * SCREEN_LINE_H,
	             UI_TEXT_COLS * SCREEN_ADVANCE, 9, SCREEN_WHITE) > 0,
	      "the answer is drawn in the bright colour");
	check(ink_of(UI_PANE_X, UI_PANE_Y + 2 * SCREEN_LINE_H,
	             UI_TEXT_COLS * SCREEN_ADVANCE, 9, SCREEN_DIM) == 0,
	      "the answer is not dimmed on screen");

	ui_result(&u, "solve x^2-5x+6", PIPELINE_OK, "x=2, x=3",
	          "CALL quad_solve 1 -5 6");
	check(u.nrows == 1, "a relation with no template shows one row");
	check(strcmp(ui_row(&u, 0), "x=2, x=3") == 0,
	      "a relation with no template shows the answer alone");
	check(!ui_row_dim(&u, 0), "that answer is not dimmed");
}

/* ---- the graph screen ---- */

/*
 * The graph screen is the answer screen in another mode: same title bar, same
 * input line, same hint rows. Its one difference is what ENTER does, and what
 * this test is for is that the plotter is handed the text the screen shows, and
 * that a line the plotter cannot draw is refused here rather than in a message
 * box after the plot has taken the display.
 */
static void test_graph_mode(void)
{
	ui_init(&u, NULL, NULL);
	/* Third entry: Ask, Solve by formula, Keypad help, Graph, Quit. */
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	render();

	check(ink(280, 0, 40, 12) > 0, "the graph screen labels itself");
	check(ink(4, UI_FOOTER_Y, 300, 20) > 0, "the graph screen has key hints");
	check(ink(0, UI_PANE_Y, PANE_W, PANE_H) > 0,
	      "the pane says which functions the parser takes");
	check(page_has("function of x"), "the page says what to type");
	check(page_has("ARROWS pan"), "the page names the plot's keys");
	check(u.nrows <= UI_PANE_ROWS, "the page fits the window without scrolling");

	type_text("x^2-3x+1");
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_GRAPH,
	      "ENTER hands the function to the plotter");
	check(strcmp(ui_graph_text(&u), "x^2-3x+1") == 0,
	      "the plotter is handed what was typed");
	check(u.mode == UI_MODE_GRAPH, "plotting leaves the screen where it was");
	check(u.input_len == 8, "the line keeps the function, so it can be nudged");

	/* "y =" is what the old prompt asked for and what a student types out of
	 * habit; the line is rewritten to what the plotter reads. */
	ui_key(&u, UI_KEY_BACK);
	check(u.input_len == 0, "back clears the line before it leaves");
	type_text("y = sin(x)");
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_GRAPH, "a leading y = is accepted");
	check(strcmp(ui_graph_text(&u), "sin(x)") == 0, "y = is dropped before plotting");
	check(strcmp(u.input, "sin(x)") == 0, "the line shows what will be plotted");

	/* Neither refusal leaves the screen, and both say so in the pane. */
	ui_key(&u, UI_KEY_BACK);
	type_text("x^^");
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_NONE, "a parse error stays on the screen");
	check(u.mode == UI_MODE_GRAPH, "and does not leave it");
	check(page_has("not an expression"), "the pane says what went wrong");
	check(u.input_len == 3, "the line keeps the text to fix");

	ui_key(&u, UI_KEY_BACK);
	type_text("x+y");
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_NONE, "an expression in y is refused");
	check(page_has("Use x only"), "the pane says to use x");

	/* History is the same ring, so a function plotted once comes back. Only
	 * the two that plotted are in it: a refused line was never a question. */
	ui_key(&u, UI_KEY_BACK);
	ui_key(&u, UI_KEY_LEFT);
	check(strcmp(u.input, "sin(x)") == 0,
	      "the newest plotted function comes back first");
	ui_key(&u, UI_KEY_LEFT);
	check(strcmp(u.input, "x^2-3x+1") == 0,
	      "and the one before it comes back next");
}

/* Leaving puts the answer back: the graph page borrowed the pane. */
static void test_graph_returns_to_answer(void)
{
	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_MODE);
	ui_result(&u, "q", PIPELINE_OK, "42", "");
	ui_key(&u, UI_KEY_MODE);

	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	check(strcmp(ui_row(&u, 0), "Graphing") == 0,
	      "the graph page takes over the pane while it is open");

	ui_key(&u, UI_KEY_BACK);
	check(u.mode == UI_MODE_MENU, "back from an empty line returns to the menu");

	ui_key(&u, UI_KEY_UP);
	ui_key(&u, UI_KEY_UP);
	ui_key(&u, UI_KEY_UP);
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.mode == UI_MODE_ASK, "the menu returns to the input line");
	check(strcmp(ui_row(&u, 0), "42") == 0,
	      "the answer is back where the graph screen left it");
}

/* ---- the frame as a whole ---- */

static void test_render_frame(void)
{
	int bg = 0;

	int off_palette = 0;

	ui_init(&u, NULL, NULL);
	render();
	for (int i = 0; i < SCREEN_W * SCREEN_HEIGHT; i++) {
		uint16_t c = fb[i];

		bg += c == SCREEN_BLACK;
		off_palette += !(c == SCREEN_BLACK || c == SCREEN_WHITE ||
		                 c == SCREEN_DIM || c == SCREEN_ACCENT || c == 0x2104);
	}
	check(off_palette == 0, "every pixel is one of the palette's colours");
	check(bg > SCREEN_W * SCREEN_HEIGHT / 2, "the screen is mostly background");

	/* A full screen of text still leaves the frame intact, and a redraw is
	 * idempotent: nothing from the previous frame survives. */
	ui_key(&u, UI_KEY_MODE);
	ui_result(&u, "q", PIPELINE_OK, "42", "");
	render();
	{
		unsigned long once = hash(0, 0, SCREEN_W, SCREEN_HEIGHT);

		render();
		check(hash(0, 0, SCREEN_W, SCREEN_HEIGHT) == once,
		      "rendering twice draws the same screen");
	}
}


/*
 * 4.6e reached from the answer screen: a relation with a diagram takes over
 * the pane on the diagram key, and a relation without one ignores the key
 * rather than showing a blank screen. Leaving the diagram puts the answer
 * back, which is what stops the key from eating the answer a student asked
 * for.
 */
static void test_diagram_screen(void)
{
	unsigned after_answer;

	ui_init(&u, NULL, "no model");
	/* Ask mode is where an answer lives; ui_result is called from there in the
	 * real flow, and the diagram key is only handled on that screen. */
	u.mode = UI_MODE_ASK;
	ui_result(&u, "friction force", PIPELINE_OK,
	          "6 N", "CALL friction_force 0.3 20");
	check(u.diagram == 0, "diagram starts hidden");
	ui_key(&u, UI_KEY_DIAGRAM);
	check(u.diagram == 1, "diagram key shows it");
	render();
	check(ink_of(0, 0, SCREEN_W, SCREEN_HEIGHT, DIAGRAM_HEAD) > 0,
	      "the diagram drew its arrows");
	after_answer = (unsigned)ink(0, 0, SCREEN_W, SCREEN_HEIGHT);

	/* Any other key returns to the answer. */
	ui_key(&u, UI_KEY_DOWN);
	check(u.diagram == 0, "a key returns to the answer");
	render();
	check((unsigned)ink(0, 0, SCREEN_W, SCREEN_HEIGHT) != after_answer,
	      "the pane came back");

	/* A relation with no diagram keeps the answer on screen. */
	ui_result(&u, "molarity", PIPELINE_OK, "2 M", "CALL molarity 4 2");
	ui_key(&u, UI_KEY_DIAGRAM);
	check(u.diagram == 0, "no diagram for a relation without one");

	/* And a typed answer, which has no call at all. */
	ui_result(&u, "2+2", PIPELINE_OK, "4", "");
	ui_key(&u, UI_KEY_DIAGRAM);
	check(u.diagram == 0, "no diagram for arithmetic");
}

/*
 * 4.6a from the answer: the plot key has to leave the model, because the
 * plotter runs its own loop on the device. So the only thing worth asserting
 * here is whether the model asks for it, which it may do only when the call
 * the answer came from has a curve.
 */
static void test_plot_key(void)
{
	ui_init(&u, NULL, "no model");
	u.mode = UI_MODE_ASK;
	ui_result(&u, "friction force", PIPELINE_OK,
	          "6 N", "CALL friction_force 0.3 20");
	check(strcmp(ui_last_call(&u), "CALL friction_force 0.3 20") == 0,
	      "the model kept the call");
	check(ui_key(&u, UI_KEY_GRAPH) == UI_KEY_GRAPH,
	      "an answer from a relation offers its plot");
	/* The key is not on the keypad's own label, so the screen has to say it. */
	render();
	check(ink(0, UI_FOOTER_Y + 22, SCREEN_W, 8) > 0,
	      "the answer advertises the keys it offers");

	/* A call whose arguments are not numbers has no curve. */
	ui_result(&u, "integrate", PIPELINE_OK, "x^3/3", "CALL integrate(x^2)");
	check(ui_key(&u, UI_KEY_GRAPH) == UI_KEY_NONE,
	      "a call that does not plot asks for nothing");

	/* Arithmetic never had a call, so the key is inert rather than an error. */
	ui_result(&u, "2+2", PIPELINE_OK, "4", "");
	check(ui_last_call(&u)[0] == '\0', "arithmetic leaves no call behind");
	check(ui_key(&u, UI_KEY_GRAPH) == UI_KEY_NONE, "no plot for arithmetic");
}


/*
 * Finding a relation by name: the catalogue with a filter line above it,
 * reached from the menu without knowing the subject or the course. What this
 * proves is that a partial name narrows the list as it is typed, that the
 * query survives a trip into the argument prompts, and that a query with no
 * match says so in the pane while the key hints stay exactly where they were.
 */
static void test_find(void)
{
	unsigned long footer;
	int found;

	ui_init(&u, NULL, NULL);
	render();
	/* Seven rows of menu: the last one's text ends at y = 174 + 7, so the band
	 * above the key hints has to stay empty. */
	check(ink(0, UI_FOOTER_Y - 14, SCREEN_W, 14) == 0,
	      "the seventh menu row clears the key hints");
	for (int i = 0; i < 5; i++)
		ui_key(&u, UI_KEY_DOWN);        /* Find a relation */
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_NONE, "the finder opens in place");
	check(u.mode == UI_MODE_TMPL, "the finder is the picker, not a new mode");
	check(u.tmpl_find == 1, "on the relation list with a filter line");
	check(u.tmpl_step == UI_TMPL_STEP_RELATION, "with no subject step above it");
	check(u.tmpl_count == REL_NUM_RELATIONS,
	      "an empty line is the whole catalogue");
	render();
	check(ink(0, UI_PANE_Y, PANE_W, PANE_H) > 0, "the catalogue is drawn");
	check(ink_of(UI_INPUT_X, UI_INPUT_Y, 200, 9, SCREEN_WHITE) == 0,
	      "the empty filter line has no text in it");
	check(ink(4, UI_FOOTER_Y, 300, 20) > 0, "the finder has key hints");

	type_text("perc");
	/*
	 * The count is derived, not written down. It read 5 until the catalogue
	 * grew two more names containing "perc" and broke a gate that was
	 * really about arithmetic on a stale constant; a batch of relations
	 * must not be able to fail a test of the finder.
	 */
	check(u.tmpl_count == names_containing("perc"),
	      "the list is every name that contains the query");
	check(u.tmpl_count > 0 &&
	      strncmp(rel_rows[u.tmpl_rels[0]].name, "perc", 4) == 0,
	      "a name the query starts is offered before one that contains it");
	check(listed("percent_error"), "percent_error is still found by perc");
	check(listed("mass_percent"),
	      "mass_percent still matches, after the ones it starts");
	{
		/* The ranking itself: all the names the query starts, then all the
		 * ones that only contain it. Asserted as an order over whatever
		 * the catalogue holds rather than over three fixed rows. */
		int inside_seen = 0, ordered = 1;

		for (int i = 0; i < u.tmpl_count; i++) {
			int head = strncmp(rel_rows[u.tmpl_rels[i]].name, "perc", 4) == 0;

			if (!head)
				inside_seen = 1;
			else if (inside_seen)
				ordered = 0;
		}
		check(ordered, "every prefix match ranks before a mid-name match");
	}
	{
		/* Case is folded: the same query in capitals is the same list in
		 * the same order, which no count of the table can establish. */
		unsigned char first = u.tmpl_rels[0];
		unsigned char last = u.tmpl_rels[u.tmpl_count - 1];
		int want = u.tmpl_count;

		while (u.input_len > 0)
			ui_key(&u, UI_KEY_DELETE);
		type_text("PERC");
		check(u.tmpl_count == want && u.tmpl_rels[0] == first &&
		      u.tmpl_rels[want - 1] == last,
		      "the filter folds case and keeps the order");
	}
	/* A space stands for the underscore a name is written with. */
	while (u.input_len > 0)
		ui_key(&u, UI_KEY_DELETE);
	type_text("percent error");
	check(u.tmpl_count == 1 &&
	      strcmp(rel_rows[u.tmpl_rels[0]].name, "percent_error") == 0,
	      "a typed space matches the underscore in a name");
	render();
	check(ink_of(UI_INPUT_X, UI_INPUT_Y, 120, 9, SCREEN_WHITE) > 0,
	      "the query is drawn in the input line");
	check(ink(14, UI_PANE_Y, 200, 9) > 0, "the match is drawn where the list is");
	check(ink(4 + UI_TEXT_COLS * SCREEN_ADVANCE, UI_PANE_Y + SCREEN_LINE_H,
	          5, SCREEN_LINE_H - 2) == 0,
	      "a list row stops short of the scroll bar");

	/* No match: the pane says so, and the hints do not move to make room. */
	render();
	footer = hash(0, UI_FOOTER_Y, SCREEN_W, SCREEN_HEIGHT - UI_FOOTER_Y);
	while (u.input_len > 0)
		ui_key(&u, UI_KEY_DELETE);
	type_text("zz");
	check(u.tmpl_count == 0, "a query with no match empties the list");
	check(u.tmpl_note[0] != '\0', "and records why");
	render();
	check(ink_of(UI_PANE_X, UI_PANE_Y, 200, 9, SCREEN_DIM) > 0,
	      "the notice is drawn in the pane, dimmed");
	check(hash(0, UI_FOOTER_Y, SCREEN_W, SCREEN_HEIGHT - UI_FOOTER_Y) == footer,
	      "a notice leaves the key hints exactly as they were");
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_NONE,
	      "accepting an empty match list stays put");
	check(u.tmpl_step == UI_TMPL_STEP_RELATION, "and does not open the prompts");

	/* Back widens the list before it leaves the screen. */
	ui_key(&u, UI_KEY_BACK);
	check(u.input_len == 0, "back clears the filter");
	check(u.tmpl_count == REL_NUM_RELATIONS, "and the whole catalogue is back");
	check(u.mode == UI_MODE_TMPL && u.tmpl_find == 1, "still in the finder");
	ui_key(&u, UI_KEY_BACK);
	check(u.mode == UI_MODE_MENU, "a second back leaves for the menu");
	check(u.tmpl_find == 0, "and the finder is no longer armed");

	/* The separators a name is written with are not something to remember. */
	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 5; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	type_text("friction force");
	check(u.tmpl_count == 1 &&
	      strcmp(rel_rows[u.tmpl_rels[0]].name, "friction_force") == 0,
	      "a space stands in for the underscore");
	for (int i = 0; i < 14; i++)
		ui_key(&u, UI_KEY_DELETE);
	type_text("FRICTION");
	check(u.tmpl_count == 2, "the filter ignores case");
	check(strcmp(rel_rows[u.tmpl_rels[0]].name, "friction_force") == 0,
	      "and the name it starts still comes first");
	for (int i = 0; i < 8; i++)
		ui_key(&u, UI_KEY_DELETE);
	type_text("force");
	found = 0;
	for (int i = 0; i < u.tmpl_count; i++)
		if (strcmp(rel_rows[u.tmpl_rels[i]].name, "friction_force") == 0)
			found = 1;
	check(found, "a match need not start at the first character");

	/* The name typed exactly as it is printed matches too, underscores and
	 * all: that is what a student copies off a help page. */
	for (int i = 0; i < 5; i++)
		ui_key(&u, UI_KEY_DELETE);
	type_text("friction_force");
	check(u.tmpl_count == 1 &&
	      strcmp(rel_rows[u.tmpl_rels[0]].name, "friction_force") == 0,
	      "the name typed exactly as printed matches");

	/* Choosing one opens its prompts, and back returns to the narrowed list
	 * rather than to the catalogue: the query is still what was typed. */
	for (int i = 0; i < 14; i++)
		ui_key(&u, UI_KEY_DELETE);
	type_text("friction");
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.tmpl_step == UI_TMPL_STEP_ARGS, "choosing a match opens its prompts");
	check(strcmp(rel_rows[u.tmpl_rels[u.tmpl_rel]].name, "friction_force") == 0,
	      "and it is the relation the row named");
	check(u.input_len == 0, "the filter line becomes the number field");
	ui_key(&u, UI_KEY_BACK);
	check(u.tmpl_step == UI_TMPL_STEP_RELATION, "back returns to the list");
	check(strcmp(u.input, "friction") == 0, "with the query it was narrowed by");
	check(u.tmpl_count == 2, "and the same matches");
	check(u.tmpl_sel == u.tmpl_rel, "with the chosen row still selected");

	/* The flat catalogue scrolls like every other list. */
	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 5; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	for (int i = 0; i < 200; i++)
		ui_key(&u, UI_KEY_DOWN);
	check(u.tmpl_sel == REL_NUM_RELATIONS - 1, "down stops at the last relation");
	check(u.tmpl_top == REL_NUM_RELATIONS - UI_PANE_ROWS,
	      "and the window follows it to the end");
	render();
	check(ink_of(SCREEN_W - 5, UI_PANE_Y, 2, PANE_H, SCREEN_ACCENT) > 0,
	      "the finder shows where the window sits");
	for (int i = 0; i < 200; i++)
		ui_key(&u, UI_KEY_UP);
	check(u.tmpl_sel == 0 && u.tmpl_top == 0, "up stops at the first relation");

	/* Every name has to fit the column the list draws it in, or a row would
	 * run under the scroll bar on the one screen that shows all of them. */
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		check(screen_text_w(rel_rows[i].name) <=
		      (UI_TEXT_COLS - 2) * SCREEN_ADVANCE,
		      "every relation name fits the list column");
}


/* The form's rows, as the drawing puts them: name, formula, blank, then one
 * row per argument, then the hint and the note. */
#define FORM_SLOT(i)  (UI_PANE_Y + SCREEN_LINE_H * (3 + (i)))
#define FORM_HINT     (UI_PANE_Y + SCREEN_LINE_H * 6)
#define FORM_NOTE     (UI_PANE_Y + SCREEN_LINE_H * 7)

/*
 * The argument prompts say what they want. The names and their order come from
 * the relation table's own parameter list, and the formula above them comes from
 * the step table — the only place the tables explain a symbol like "fa". Each
 * band is compared against a frame drawn from the expected string, so this
 * asserts the exact text and colour at the exact row.
 */
static void test_slots(void)
{
	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 5; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	type_text("friction_force");
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.tmpl_step == UI_TMPL_STEP_ARGS, "the finder opens the prompts");
	render();
	check(band_is_text(UI_PANE_Y, "friction_force", SCREEN_WHITE),
	      "the relation's name heads the prompts");
	check(band_is_text(UI_PANE_Y + SCREEN_LINE_H, "f = mu*N", SCREEN_DIM),
	      "its formula from the step table sits under it, dimmed");
	check(band_is_text(FORM_SLOT(1), "2. N = -", SCREEN_WHITE),
	      "each slot is named from the relation's own parameter list");
	check(band_is_text(FORM_HINT, "type the number, ENTER next", SCREEN_DIM),
	      "and the line under them says what ENTER does");

	/* The case this exists for: "fa" and "fb" are unreadable on their own, and
	 * the formula is what says they are f(a) and f(b). */
	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 5; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	type_text("average_rate_of_change");
	ui_key(&u, UI_KEY_ACCEPT);
	render();
	check(band_is_text(UI_PANE_Y + SCREEN_LINE_H,
	                   "rate = (f(b) - f(a)) / (b - a)", SCREEN_DIM),
	      "a four-slot relation shows the formula that explains its slots");
	check(band_is_text(FORM_SLOT(3), "4. fb = -", SCREEN_WHITE),
	      "and all four of its slots in order");

	/* Six arguments, and no step row: the formula band stays empty rather than
	 * borrowing the name's row, and the sixth slot still lands where the layout
	 * says it does. */
	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 5; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	type_text("linear2");
	ui_key(&u, UI_KEY_ACCEPT);
	render();
	check(band_is_text(UI_PANE_Y + SCREEN_LINE_H, "", SCREEN_DIM),
	      "a relation with no step row draws no formula");
	check(band_is_text(FORM_SLOT(5), "6. c2 = -", SCREEN_WHITE),
	      "the sixth argument is the sixth parameter");
	check(ui_type(&u, 'a') == 0, "a letter is still not an argument");
	check(ui_type(&u, '7') == 1, "a digit still is");

	/* The footer advertises DEL on this screen, so a mistyped number has to be
	 * fixable without clearing the slot. */
	type_text("25");
	check(strcmp(u.input, "725") == 0, "the slot line takes what is typed");
	ui_key(&u, UI_KEY_DELETE);
	check(strcmp(u.input, "72") == 0, "DEL erases the digit behind the cursor");
	ui_key(&u, UI_KEY_LEFT);
	ui_key(&u, UI_KEY_DELETE);
	check(strcmp(u.input, "2") == 0, "the arrow moves the cursor and DEL follows it");
	check(u.cursor == 0, "and the cursor stays where the delete left it");
	ui_type(&u, '4');
	check(strcmp(u.input, "42") == 0, "typing inserts where the cursor is");
	check(u.recall < 0, "the slot line never pulls the answer history in");
}

/*
 * The last answer as an argument. This is the chain the keypad-help page prints
 * as "<1>", done by picking a relation instead of typing its name: the speed a
 * first step answered with becomes the speed a second step needs.
 */
static void test_reuse(void)
{
	unsigned long footer;

	/* Nothing to reuse yet: the key explains itself, in the pane, and the key
	 * hints keep their own rows while it does. */
	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 5; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	type_text("friction_force");
	ui_key(&u, UI_KEY_ACCEPT);
	render();
	footer = hash(0, UI_FOOTER_Y, SCREEN_W, SCREEN_HEIGHT - UI_FOOTER_Y);
	check(ui_key(&u, UI_KEY_DIAGRAM) == UI_KEY_NONE,
	      "the reuse key stays on the screen");
	check(u.input_len == 0, "with no answer there is nothing to insert");
	check(strcmp(u.tmpl_note, "no answer yet to reuse") == 0, "and it says why");
	render();
	check(band_is_text(FORM_NOTE, "no answer yet to reuse", SCREEN_DIM),
	      "the note is drawn under the prompts");
	check(hash(0, UI_FOOTER_Y, SCREEN_W, SCREEN_HEIGHT - UI_FOOTER_Y) == footer,
	      "and a note leaves the key hints exactly as they were");

	/* Step one, by formula: 0 + 3*4 = 12 m/s. */
	ui_init(&u, NULL, NULL);
	u.mode = UI_MODE_ASK;
	ui_result(&u, "kinematic_v 0 3 4", PIPELINE_OK, "12 m/s",
	          "CALL kinematic_v 0 3 4");
	check(strcmp(u.last_num, "12") == 0, "the answer's number is kept");

	/* Step two: the mass is typed, the speed is taken. */
	ui_key(&u, UI_KEY_MODE);            /* back to the menu */
	for (int i = 0; i < 5; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	type_text("kinetic_energy");
	ui_key(&u, UI_KEY_ACCEPT);
	type_text("5");
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.tmpl_arg == 1, "the first argument is in");
	check(ui_key(&u, UI_KEY_DIAGRAM) == UI_KEY_NONE,
	      "the reuse key fills the second instead of leaving");
	check(strcmp(u.input, "12") == 0, "with the number the first step gave");
	check(u.cursor == u.input_len, "and the cursor after it, so it can be edited");
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.mode == UI_MODE_ASK, "the completed formula answers");
	check(strcmp(ui_last_call(&u), "CALL kinetic_energy 5 12") == 0,
	      "with the reused number as its argument");
	check(strcmp(ui_row(&u, u.nrows - 1), "= 360 J") == 0, "and the right answer");
	check(strcmp(u.last_num, "360") == 0, "which becomes the next answer to reuse");

	/* An answer with no single number at the front cannot be reused, and
	 * saying so beats inserting half of it. */
	ui_result(&u, "quad", PIPELINE_OK, "x=2, y=3", "CALL quad_solve 1 -5 6");
	check(u.last_num[0] == '\0', "a multi-part answer keeps no number");
	ui_result(&u, "heat", PIPELINE_OK, "8350 J", "CALL latent_heat 25 334");
	check(strcmp(u.last_num, "8350") == 0, "an answer with a unit keeps its number");
	ui_result(&u, "drop", PIPELINE_OK, "-4.5 m", "CALL free_fall 0.3");
	check(strcmp(u.last_num, "-4.5") == 0, "a signed decimal keeps its sign");
	ui_result(&u, "q", PIPELINE_ERR, "", "CALL friction 3 4");
	check(u.last_num[0] == '\0', "a failed question clears it rather than keeping a stale one");
}

/*
 * Solve by formula: pick a subject, pick a relation, answer one prompt per
 * argument. The point of the test is that it works with no pipeline at all —
 * the picker never decodes anything — and that the answer arrives on the
 * answer screen with the call the rest of the program keys off.
 */
static void test_template(void)
{
	const char *want_subject = "physics";
	int subject = -1, n = 0, seen = 0;

	/* The subject list is the relation table's own order, first appearance.
	 * Find physics in it so the test names what it picks. */
	for (int i = 0; i < REL_NUM_RELATIONS; i++) {
		int dup = 0;

		for (int j = 0; j < i && !dup; j++)
			if (strcmp(rel_rows[j].subject, rel_rows[i].subject) == 0)
				dup = 1;
		if (dup)
			continue;
		if (strcmp(rel_rows[i].subject, want_subject) == 0) {
			subject = n;
			break;
		}
		n++;
	}
	check(subject > 0, "physics is in the subject list");

	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_DOWN);            /* Solve by formula */
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.mode == UI_MODE_TMPL, "the menu opens the picker");
	check(u.tmpl_step == UI_TMPL_STEP_SUBJECT, "on the subject list");
	render();
	check(ink(0, UI_PANE_Y, PANE_W, PANE_H) > 0, "the subjects are drawn");
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_NONE, "choosing does not leave the screen");
	check(u.tmpl_step == UI_TMPL_STEP_RELATION, "and moves to the relations");

	/* Walk to physics: the list starts at the first subject. */
	ui_key(&u, UI_KEY_BACK);
	check(u.tmpl_step == UI_TMPL_STEP_SUBJECT, "back returns to the subjects");
	for (int i = 0; i < subject; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.tmpl_count > 0, "physics has relations");
	check(strcmp(rel_rows[u.tmpl_rels[0]].subject, "physics") == 0,
	      "every row in the list is in that subject");

	/* Find friction_force in the list and choose it. */
	seen = 0;
	for (int i = 0; i < u.tmpl_count; i++) {
		if (strcmp(rel_rows[u.tmpl_rels[u.tmpl_sel]].name, "friction_force") == 0) {
			seen = 1;
			break;
		}
		ui_key(&u, UI_KEY_DOWN);
	}
	check(seen, "friction_force is under physics");
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.tmpl_step == UI_TMPL_STEP_ARGS, "choosing a relation starts its prompts");
	check(u.tmpl_arg == 0, "at the first argument");
	render();
	check(ink(0, UI_PANE_Y, PANE_W, PANE_H) > 0, "the prompts are drawn");

	/* The prompt takes numbers only. */
	check(ui_type(&u, 'a') == 0, "a letter is not an argument");
	for (const char *p = "4e-1"; *p; p++)
		check(ui_type(&u, *p), "a scientific coefficient can be typed");
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.tmpl_arg == 1, "ENTER moves to the second argument");
	for (const char *p = "5E+2"; *p; p++)
		check(ui_type(&u, *p), "an uppercase exponent and plus can be typed");
	ui_key(&u, UI_KEY_ACCEPT);

	check(u.mode == UI_MODE_ASK, "the last argument answers");
	check(strcmp(ui_last_call(&u), "CALL friction_force 4e-1 5E+2") == 0,
	      "and the answer carries the call");
	check(strcmp(ui_row(&u, u.nrows - 1), "= 200 N") == 0, "with the right number");
	check(strstr(ui_row(&u, 0), "f = mu*N") != NULL, "and the working above it");

	/* Esc inside the prompts walks back out, one level at a time. */
	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	ui_key(&u, UI_KEY_ACCEPT);
	ui_key(&u, UI_KEY_ACCEPT);
	ui_type(&u, '1');
	ui_key(&u, UI_KEY_BACK);
	check(u.tmpl_arg == 0 && u.input_len == 0, "ESC clears a half-typed argument");
	ui_key(&u, UI_KEY_BACK);
	check(u.tmpl_step == UI_TMPL_STEP_RELATION, "and then returns to the relations");
	ui_key(&u, UI_KEY_BACK);
	check(u.tmpl_step == UI_TMPL_STEP_SUBJECT, "and to the subjects");
	ui_key(&u, UI_KEY_BACK);
	check(u.mode == UI_MODE_MENU, "and to the menu");
}

static void test_template_multi_argument(void)
{
	static const struct {
		const char *name;
		int arity;
		const char *args[UI_TMPL_ARGS];
		const char *call;
		const char *answer;
	} cases[] = {
		{"dilution", 3, {"4", "50", "250"},
		 "CALL dilution 4 50 250", "= 0.8 M"},
		{"stoich_mass", 4, {"44", "44", "18", "2"},
		 "CALL stoich_mass 44 44 18 2", "= 36 g"},
		{"combined_gas", 5, {"1", "2", "300", "2", "300"},
		 "CALL combined_gas 1 2 300 2 300", "= 1 L"},
		{"linear2", 6, {"1", "0", "2", "0", "1", "3"},
		 "CALL linear2 1 0 2 0 1 3", "x=2, y=3"}
	};

	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		check(rel_rows[i].arity <= UI_TMPL_ARGS,
		      "every formula fits the picker's argument capacity");
	for (unsigned c = 0; c < sizeof cases / sizeof cases[0]; c++) {
		int found = 0;

		ui_init(&u, NULL, NULL);
		ui_key(&u, UI_KEY_DOWN);
		ui_key(&u, UI_KEY_ACCEPT);
		/* Visit subjects through the same key events as a student. */
		for (int subject = 0; subject < REL_NUM_RELATIONS && !found; subject++) {
			ui_key(&u, UI_KEY_ACCEPT);
			for (int i = 0; i < u.tmpl_count; i++) {
				if (strcmp(rel_rows[u.tmpl_rels[i]].name, cases[c].name) == 0) {
					for (int j = 0; j < i; j++)
						ui_key(&u, UI_KEY_DOWN);
					found = 1;
					break;
				}
			}
			if (!found) {
				ui_key(&u, UI_KEY_BACK);
				ui_key(&u, UI_KEY_DOWN);
			}
		}
		check(found, cases[c].name);
		if (!found)
			continue;
		ui_key(&u, UI_KEY_ACCEPT);
		for (int i = 0; i < cases[c].arity; i++) {
			check(u.mode == UI_MODE_TMPL && u.tmpl_arg == i,
			      "the picker requests every argument in order");
			for (const char *p = cases[c].args[i]; *p; p++)
				check(ui_type(&u, *p), "the argument can be typed");
			ui_key(&u, UI_KEY_ACCEPT);
		}
		check(u.mode == UI_MODE_ASK, "the completed formula opens its answer");
		check(strcmp(ui_last_call(&u), cases[c].call) == 0, cases[c].call);
		check(strcmp(ui_row(&u, u.nrows - 1), cases[c].answer) == 0,
		      cases[c].answer);
		render();
	}
}


/* True when a course's map lists that relation id. */
static int course_has_rel(int course, int rel_id)
{
	const crs_course_t *row = &crs_courses[course];

	for (int i = 0; i < row->num_units; i++) {
		const crs_unit_t *unit = &crs_units[row->first_unit + i];

		for (int j = 0; j < unit->num_topics; j++)
			if (crs_topics[unit->first_topic + j].rel == rel_id)
				return 1;
	}
	return 0;
}

/*
 * The course route: the same three-step flow entered on a course rather than a
 * subject. What the map records as a gap, or as work the typed engine answers,
 * must not be offered here as something to call, and the flat catalogue has to
 * stay reachable from the menu for a student who does not know their course.
 */
static void test_course_picker(void)
{
	int bc = -1, precalc = -1;

	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 4; i++)
		ui_key(&u, UI_KEY_DOWN);         /* By course */
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.mode == UI_MODE_TMPL, "the course route opens the picker");
	check(u.tmpl_step == UI_TMPL_STEP_SUBJECT, "on the first list");
	check(u.tmpl_by_course == 1, "and it is the course list, not the subjects");
	render();
	check(ink(0, UI_PANE_Y, PANE_W, PANE_H) > 0, "the courses are drawn");

	for (int i = 0; i < CRS_NUM_COURSES; i++) {
		if (strcmp(crs_courses[i].code, "ap-calc-bc") == 0)
			bc = i;
		if (strcmp(crs_courses[i].code, "ap-precalc") == 0)
			precalc = i;
	}
	check(bc >= 0, "AP Calculus BC is in the table");
	check(precalc >= 0, "AP Precalculus is in the table");

	/* AP Calculus BC's topics are all typed-engine work, so the course
	 * offers no call at all, and accepting the empty list must not crash. */
	ui_key(&u, UI_KEY_BACK);
	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 4; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	for (int i = 0; i < bc; i++)
		ui_key(&u, UI_KEY_DOWN);
	check(u.tmpl_sel == bc, "the selection walks to that course");
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_NONE, "choosing a course stays put");
	check(u.tmpl_step == UI_TMPL_STEP_RELATION, "and shows its relations");
	check(u.tmpl_count == 0, "of which AP Calculus BC has none to call");
	check(ui_key(&u, UI_KEY_ACCEPT) == UI_KEY_NONE,
	      "accepting an empty list is not a crash");
	check(u.tmpl_step == UI_TMPL_STEP_RELATION, "an empty list stays on the list");

	ui_key(&u, UI_KEY_BACK);
	check(u.tmpl_step == UI_TMPL_STEP_SUBJECT, "back returns to the courses");
	check(u.tmpl_by_course == 1, "still on the course list");

	/* Now a course that does offer calls. */
	ui_init(&u, NULL, NULL);
	for (int i = 0; i < 4; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	for (int i = 0; i < precalc; i++)
		ui_key(&u, UI_KEY_DOWN);
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.tmpl_count > 0, "AP Precalculus offers relations to call");
	render();
	check(ink(0, UI_PANE_Y, PANE_W, PANE_H) > 0, "its relations are drawn");
	for (int i = 0; i < u.tmpl_count; i++)
		check(course_has_rel(precalc, rel_rows[u.tmpl_rels[i]].id),
		      "every row offered belongs to that course's map");
	check(course_has_rel(precalc, rel_rows[u.tmpl_rels[0]].id),
	      "and the first row is one of them");

	/* The flat catalogue is still one menu press away. */
	ui_init(&u, NULL, NULL);
	ui_key(&u, UI_KEY_DOWN);            /* Solve by formula */
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.tmpl_by_course == 0, "Solve by formula still opens the subject list");
	check(u.mode == UI_MODE_TMPL, "and still reaches the picker");
}

int main(void)
{
	test_menu();
	test_help();
	test_input();
	test_answer_and_history();
	test_wrap_and_scroll();
	test_errors();
	test_steps();
	test_render_frame();
	test_diagram_screen();
	test_plot_key();
	test_template();
	test_find();
	test_slots();
	test_reuse();
	test_course_picker();
	test_template_multi_argument();
	test_graph_mode();
	test_graph_returns_to_answer();

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("ui: all checks passed\n");
	return 0;
}
