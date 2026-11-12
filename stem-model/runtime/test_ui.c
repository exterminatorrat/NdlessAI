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
#include "diagram.h"
#include "reltable.h"

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

static void type_text(const char *s)
{
	while (*s)
		ui_type(&u, *s++);
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
	check(u.menu_sel == 4, "down at the bottom stops at the bottom");

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
	for (int i = 0; i < 4; i++)
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
	ui_type(&u, '0');
	ui_type(&u, '.');
	ui_type(&u, '4');
	ui_key(&u, UI_KEY_ACCEPT);
	check(u.tmpl_arg == 1, "ENTER moves to the second argument");
	ui_type(&u, '5');
	ui_type(&u, '0');
	ui_type(&u, '0');
	ui_key(&u, UI_KEY_ACCEPT);

	check(u.mode == UI_MODE_ASK, "the last argument answers");
	check(strcmp(ui_last_call(&u), "CALL friction_force 0.4 500") == 0,
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
	test_graph_mode();
	test_graph_returns_to_answer();

	if (failures) {
		printf("%d failure(s)\n", failures);
		return 1;
	}
	printf("ui: all checks passed\n");
	return 0;
}
