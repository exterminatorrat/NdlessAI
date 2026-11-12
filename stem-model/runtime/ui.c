/*
 * The calculator's screen model: a menu, an input line and a scrolling pane.
 *
 * Everything a student sees is decided here, in code the host tests can drive,
 * because the calculator cannot be debugged interactively. device/ui_ndless.c
 * reads keys and blits this file's framebuffer; it makes no choices.
 */
#include <string.h>

#include "pipeline.h"
#include "screen.h"
#include "ui.h"
#include "diagram.h"
#include "graph.h"

#define UI_BG      SCREEN_BLACK
#define UI_FG      SCREEN_WHITE
#define UI_DIM     SCREEN_DIM
#define UI_ACCENT  SCREEN_ACCENT
#define UI_SCROLL  0x2104

#define UI_MENU_N   4
#define UI_MENU_Y   24

static const char *const menu_items[UI_MENU_N] = {
	"Ask a question",
	"Keypad help",
	"Graph",
	"Quit"
};

/*
 * The typed forms the solver understands without the model. Kept as whole
 * pages of text rather than a list to assemble, so what the test finds in the
 * rows is exactly what the student reads.
 */
static const char *const help_pages[UI_HELP_PAGES] = {
	"Calculus and series\n"
	"\n"
	"integrate(x^2,0,3)\n"
	"  definite integral, x from 0 to 3\n"
	"integrate(sin(x),0,pi)\n"
	"  a bound may be pi\n"
	"int(x^2,0,3)\n"
	"  the same call, shorter name\n"
	"d/dx(x^3)\n"
	"  derivative of x^3\n"
	"diff(x^3)\n"
	"  the same, shorter name\n"
	"d/dx(5)\n"
	"  the derivative of a constant\n"
	"series(sin(x),0,5)\n"
	"  five terms about x = 0\n"
	"\n"
	"Type one call as shown, one per line,\n"
	"with numbers where the bounds go.",

	"Differential equations\n"
	"\n"
	"ode(2y,0,1,1,64)\n"
	"  y' = 2y from x=0 to x=1, 64 steps\n"
	"ode(x,0,0,2,32)\n"
	"  y' = x, from y=0 at x=0 to x=2\n"
	"desolve(2y,0,1)\n"
	"  y' = 2y, y(0) = 1\n"
	"desolve(x*y,0,2)\n"
	"  y' = x*y, y(0) = 2\n"
	"\n"
	"ode follows the solution numerically.\n"
	"desolve gives the formula when one\n"
	"exists.",

	"Arithmetic needs no model file\n"
	"\n"
	"sqrt(50)+ln(2)\n"
	"3/4 + 2*5\n"
	"2^10\n"
	"abs(-7)\n"
	"\n"
	"Matrices are written [row;row]:\n"
	"det([1,2;3,4])\n"
	"inv([2,1;1,3])\n"
	"\n"
	"These work with no weights.bin.tns.\n"
	"Word problems need it: put the file\n"
	"beside this program.",

	"Name a relation, then its numbers\n"
	"\n"
	"fri 0.4 500\n"
	"  friction force, mu 0.4, N 500\n"
	"kv 9 2 8\n"
	"  velocity after 8 s from 9 m/s\n"
	"ov 5 5\n"
	"  voltage for 5 A through 5 ohm\n"
	"newton2 5 2\n"
	"  the whole name works too\n"
	"\n"
	"Leave out spaces: every call above\n"
	"answers with no model and no words.\n"
	"Next page: the same, longer."
};

static int has_model(const ui_t *u)
{
	return u->p && u->p->ready;
}

/* ---- text building ---- */

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
 * Copies src, marking the cut with "..." when it does not fit. History keeps
 * a truncated copy of a pane that was already shown in full; the marker makes
 * that visible on recall instead of passing off half an answer as the whole.
 */
static void copy_trunc(char *dst, size_t cap, const char *src)
{
	size_t n;

	if (!src || cap < 1) {
		if (cap)
			dst[0] = '\0';
		return;
	}
	n = strlen(src);
	if (n < cap) {
		memcpy(dst, src, n + 1);
		return;
	}
	if (cap < 5) {
		dst[0] = '\0';
		return;
	}
	memcpy(dst, src, cap - 4);
	memcpy(dst + cap - 4, "...", 4);
}

/* ---- the pane ---- */

/*
 * Wraps one text into display rows. Rows whose source starts at or after
 * `dim_from` and before `dim_to` draw dim: they carry the working, and the
 * rows outside that range carry the answer or the headline.
 *
 * A break happens at the last space that fits, so a word is never split while
 * there is a space to break on; a single word wider than the pane is split at
 * the column, because the alternative is drawing past the edge.
 */
static void pane_set(ui_t *u, const char *text, int dim_from, int dim_to)
{
	size_t len, off = 0;
	int r = 0;

	u->nrows = 0;
	u->pane_top = 0;
	if (!text || !text[0])
		return;
	len = strlen(text);

	while (off <= len && r < UI_ROWS_MAX) {
		size_t end = off;
		int n, at = 0;

		while (end < len && text[end] != '\n')
			end++;
		n = (int)(end - off);
		do {
			int adv = n - at;
			int copy;

			if (adv > UI_TEXT_COLS) {
				int cut = UI_TEXT_COLS;

				while (cut > 0 && text[off + (size_t)at + (size_t)cut] != ' ')
					cut--;
				adv = cut > 0 ? cut : UI_TEXT_COLS;
			}
			copy = adv;
			while (copy > 0 && text[off + (size_t)at + (size_t)copy - 1] == ' ')
				copy--;
			memcpy(u->rows[r], text + off + (size_t)at, (size_t)copy);
			u->rows[r][copy] = '\0';
			u->row_dim[r] = (uint8_t)((int)(off + (size_t)at) >= dim_from &&
			                          (int)(off + (size_t)at) < dim_to);
			r++;
			at += adv;
			while (at < n && text[off + (size_t)at] == ' ')
				at++;
		} while (at < n && r < UI_ROWS_MAX);
		if (end >= len)
			break;
		off = end + 1;
	}
	u->nrows = r;
}

/* Shows an answer and remembers it, so a help page can borrow the pane. */
static void pane_show(ui_t *u, const char *text, int dim_from, int dim_to)
{
	copy_trunc(u->pane_src, sizeof u->pane_src, text);
	u->pane_dim_from = (int16_t)dim_from;
	u->pane_dim_to = (int16_t)dim_to;
	pane_set(u, u->pane_src, dim_from, dim_to);
}

const char *ui_row(const ui_t *u, int i)
{
	return (i >= 0 && i < u->nrows) ? u->rows[i] : "";
}

int ui_row_dim(const ui_t *u, int i)
{
	return (i >= 0 && i < u->nrows) ? u->row_dim[i] : 0;
}

/* ---- history ---- */

static int hist_oldest(const ui_t *u)
{
	return (u->hist_head + UI_HISTORY - (u->hist_n - 1)) % UI_HISTORY;
}

static int hist_step(const ui_t *u, int i, int delta)
{
	if (u->hist_n <= 0)
		return -1;
	if (delta < 0)
		return i == hist_oldest(u) ? i : (i + UI_HISTORY - 1) % UI_HISTORY;
	return i == u->hist_head ? i : (i + 1) % UI_HISTORY;
}

static void history_push(ui_t *u, const char *question, const char *pane,
                         int dim_from, int dim_to)
{
	ui_hist_t *h;

	u->hist_head = (u->hist_head + 1) % UI_HISTORY;
	if (u->hist_n < UI_HISTORY)
		u->hist_n++;
	h = &u->hist[u->hist_head];
	copy_trunc(h->q, sizeof h->q, question ? question : "");
	copy_trunc(h->pane, sizeof h->pane, pane);
	h->dim_from = (int16_t)dim_from;
	h->dim_to = (int16_t)dim_to;
	u->recall = u->hist_head;
}

static void recall_apply(ui_t *u, int idx)
{
	const ui_hist_t *h = &u->hist[idx];
	int len = (int)strlen(h->pane);
	int from = h->dim_from;
	int to = h->dim_to;

	memcpy(u->input, h->q, sizeof u->input);
	u->input_len = (int)strlen(u->input);
	u->cursor = u->input_len;
	u->recall = idx;
	/* A pane that had to be truncated may have lost the end of its working;
	 * clamping keeps the stored offsets inside the text that is left. */
	if (from > len)
		from = len;
	if (to > len)
		to = len;
	pane_show(u, h->pane, from, to);
}

static void recall_step(ui_t *u, int delta)
{
	int idx;

	if (u->hist_n <= 0)
		return;
	if (u->recall < 0)
		idx = delta < 0 ? u->hist_head : -1;
	else
		idx = hist_step(u, u->recall, delta);
	if (idx >= 0)
		recall_apply(u, idx);
}

/* ---- the pipeline's outcomes ---- */

void ui_result(ui_t *u, const char *question, int rc,
               const char *answer, const char *call)
{
	char src[UI_HIST_PANE];
	char block[256];
	char *at = src;
	const char *end = src + sizeof src - 1;
	int dim_from = 0, dim_to = 0;

	/* A new answer replaces whatever diagram the last one was showing. */
	u->diagram = 0;
	copy_trunc(u->last_call, sizeof u->last_call, call ? call : "");
	if (rc == PIPELINE_OK) {
		if (pipeline_steps(call, answer ? answer : "", block, (int)sizeof block) == 1) {
			char *last;

			copy_trunc(src, sizeof src, block);
			last = strrchr(src, '\n');
			/* Everything before the answer line is the working. */
			dim_to = last ? (int)(last - src) + 1 : 0;
		} else {
			copy_trunc(src, sizeof src, answer ? answer : "");
		}
	} else if (rc == PIPELINE_UNCERTAIN) {
		/*
		 * The model found a relation but used numbers the question does not
		 * contain. Saying so beats a plausible wrong answer, and showing the
		 * call tells the student which numbers it invented.
		 */
		put(&at, end, "Some numbers here are not in the question.\n");
		put(&at, end, "I matched a relation, but it used numbers\n"
		              "you did not write. A wrong number is\n"
		              "worse than no answer, so I stopped.\n");
		if (call && call[0]) {
			put(&at, end, "It tried: ");
			put(&at, end, call);
			put(&at, end, "\n");
		}
	} else if (!has_model(u)) {
		/*
		 * No weights file: the typed path was the only one that could have
		 * answered, and it did not. Say what is missing and what still works.
		 */
		put(&at, end, "No model file: word problems are off.\n");
		put(&at, end, "Arithmetic still works, for example\n"
		              "sqrt(50)+ln(2).\n");
		if (u->note && u->note[0]) {
			put(&at, end, u->note);
			put(&at, end, "\n");
		} else {
			put(&at, end, "Put weights.bin.tns beside this program.\n");
		}
	} else {
		/*
		 * The generated call is the only visible symptom of the failure a
		 * tokenizer mismatch produces: a call the solver cannot read. Show it
		 * rather than a bare apology.
		 */
		put(&at, end, "I could not read that as a problem.\n");
		put(&at, end, "It produced:\n");
		if (call && call[0]) {
			put(&at, end, call);
			put(&at, end, "\n");
		} else {
			put(&at, end, "(nothing)\n");
		}
		put(&at, end, "If that text looks garbled or empty,\n"
		              "this build's tokenizer does not match\n"
		              "the model the weights came from.\n");
	}
	if (rc != PIPELINE_OK) {
		char *nl = strchr(src, '\n');

		/* The headline is bright; the explanation under it is not. */
		dim_from = nl ? (int)(nl - src) + 1 : (int)strlen(src);
		dim_to = (int)strlen(src) + 1;
	}
	pane_show(u, src, dim_from, dim_to);
	history_push(u, question, src, dim_from, dim_to);
}

static void ask(ui_t *u)
{
	char answer[256];
	char call[256];
	int rc;

	if (u->input_len == 0)
		return;
	answer[0] = '\0';
	call[0] = '\0';
	if (u->p) {
		rc = pipeline_answer(u->p, u->input, answer, (int)sizeof answer,
		                     call, (int)sizeof call);
	} else if (pipeline_answer_offline(u->input, answer, (int)sizeof answer,
	                                   call, (int)sizeof call) == 0) {
		rc = PIPELINE_OK;
	} else {
		rc = PIPELINE_ERR;
	}
	ui_result(u, u->input, rc, answer, call);
	u->input[0] = '\0';
	u->input_len = 0;
	u->cursor = 0;
}

/* Whether the relation a call named has a diagram. The call is "CALL name
 * args...", so the name is the second token; a call that is not that shape has
 * no diagram either. */
static int call_has_diagram(const char *call)
{
	char name[48];
	int i = 0;

	if (!call || strncmp(call, "CALL ", 5) != 0)
		return 0;
	call += 5;
	while (call[i] && call[i] != ' ' && i < (int)sizeof name - 1) {
		name[i] = call[i];
		i++;
	}
	name[i] = '\0';
	return i > 0 && diagram_supports(name);
}

/* ---- keys ---- */

static void scroll_pane(ui_t *u, int delta)
{
	int max = u->nrows - UI_PANE_ROWS;

	if (max < 0)
		max = 0;
	u->pane_top += delta;
	if (u->pane_top < 0)
		u->pane_top = 0;
	if (u->pane_top > max)
		u->pane_top = max;
}

static void help_open(ui_t *u, int page)
{
	if (page < 0)
		page = 0;
	if (page >= UI_HELP_PAGES)
		page = UI_HELP_PAGES - 1;
	u->help_page = page;
	pane_set(u, help_pages[page], 0, 0);
}

static ui_key_t menu_key(ui_t *u, ui_key_t k)
{
	switch (k) {
	case UI_KEY_UP:
		if (u->menu_sel > 0)
			u->menu_sel--;
		break;
	case UI_KEY_DOWN:
		if (u->menu_sel < UI_MENU_N - 1)
			u->menu_sel++;
		break;
	case UI_KEY_ACCEPT:
		switch (u->menu_sel) {
		case 0:
			u->mode = UI_MODE_ASK;
			break;
		case 1:
			u->mode = UI_MODE_HELP;
			help_open(u, 0);
			break;
		case 2:
			return UI_KEY_GRAPH;
		default:
			return UI_KEY_BACK;
		}
		break;
	case UI_KEY_BACK:
		return UI_KEY_BACK;
	case UI_KEY_MODE:
		u->mode = UI_MODE_ASK;
		break;
	default:
		break;
	}
	return UI_KEY_NONE;
}

static ui_key_t help_key(ui_t *u, ui_key_t k)
{
	switch (k) {
	case UI_KEY_UP:
		scroll_pane(u, -1);
		break;
	case UI_KEY_DOWN:
		scroll_pane(u, 1);
		break;
	case UI_KEY_LEFT:
		help_open(u, u->help_page - 1);
		break;
	case UI_KEY_RIGHT:
	case UI_KEY_ACCEPT:
		help_open(u, u->help_page + 1);
		break;
	case UI_KEY_BACK:
	case UI_KEY_MODE:
		/* Put the answer back: the help borrowed the pane, and returning to
		 * the menu with a page of syntax sitting where the answer was would
		 * read as if that page were the result. */
		pane_set(u, u->pane_src, u->pane_dim_from, u->pane_dim_to);
		u->mode = UI_MODE_MENU;
		break;
	default:
		break;
	}
	return UI_KEY_NONE;
}

static ui_key_t ask_key(ui_t *u, ui_key_t k)
{
	switch (k) {
	case UI_KEY_ACCEPT:
		ask(u);
		break;
	case UI_KEY_BACK:
		if (u->input_len > 0) {
			u->input[0] = '\0';
			u->input_len = 0;
			u->cursor = 0;
			u->recall = -1;
		} else {
			u->mode = UI_MODE_MENU;
		}
		break;
	case UI_KEY_MODE:
		u->mode = UI_MODE_MENU;
		break;
	case UI_KEY_DIAGRAM:
		u->diagram = !u->diagram && call_has_diagram(u->last_call);
		break;
	case UI_KEY_GRAPH:
		/* The plotter is the device's loop, so the model can only ask for
		 * it. An answer that came from arithmetic has no curve to show. */
		return graph_call_plottable(u->last_call) ? UI_KEY_GRAPH
		                                        : UI_KEY_NONE;
	case UI_KEY_UP:
		scroll_pane(u, -1);
		break;
	case UI_KEY_DOWN:
		scroll_pane(u, 1);
		break;
	/*
	 * History paging and cursor movement share the arrows: while the line is
	 * empty or is a recalled entry, left and right walk the ring; once the
	 * student has typed or deleted, they move the cursor instead. Typing and
	 * deleting clear `recall`, so an edited entry is never silently replaced
	 * by the next arrow press.
	 */
	case UI_KEY_LEFT:
		if (u->recall >= 0 || u->input_len == 0)
			recall_step(u, -1);
		else if (u->cursor > 0)
			u->cursor--;
		break;
	case UI_KEY_RIGHT:
		if (u->recall >= 0 || u->input_len == 0)
			recall_step(u, 1);
		else if (u->cursor < u->input_len)
			u->cursor++;
		break;
	case UI_KEY_DELETE:
		if (u->cursor > 0) {
			memmove(u->input + u->cursor - 1, u->input + u->cursor,
			        (size_t)(u->input_len - u->cursor + 1));
			u->cursor--;
			u->input_len--;
			u->recall = -1;
		}
		break;
	default:
		break;
	}
	return UI_KEY_NONE;
}

ui_key_t ui_key(ui_t *u, ui_key_t k)
{
	switch (u->mode) {
	case UI_MODE_MENU:
		return menu_key(u, k);
	case UI_MODE_HELP:
		return help_key(u, k);
	case UI_MODE_ASK:
		/* A diagram covers the answer, so any other key has to bring the
		 * answer back rather than act on a screen the student cannot see. */
		if (u->diagram && k != UI_KEY_DIAGRAM) {
			u->diagram = 0;
			return UI_KEY_NONE;
		}
		return ask_key(u, k);
	}
	return UI_KEY_NONE;
}

const char *ui_last_call(const ui_t *u)
{
	return u->last_call;
}

int ui_type(ui_t *u, char c)
{
	if (u->mode != UI_MODE_ASK)
		return 0;
	if (c < 32 || c > 126)
		return 0;
	if (u->input_len >= UI_INPUT_MAX - 1)
		return 0;
	if (u->cursor < 0)
		u->cursor = 0;
	if (u->cursor > u->input_len)
		u->cursor = u->input_len;
	memmove(u->input + u->cursor + 1, u->input + u->cursor,
	        (size_t)(u->input_len - u->cursor + 1));
	u->input[u->cursor] = c;
	u->cursor++;
	u->input_len++;
	u->recall = -1;
	return 1;
}

void ui_init(ui_t *u, pipeline_t *p, const char *missing_model_note)
{
	memset(u, 0, sizeof *u);
	u->p = p;
	u->note = missing_model_note;
	u->mode = UI_MODE_MENU;
	u->recall = -1;
}

/* ---- drawing ---- */

static void draw_header(const ui_t *u, uint16_t *fb)
{
	char right[24];
	char *at = right;
	const char *end = right + sizeof right - 1;
	int w;

	*at = '\0';
	if (u->mode == UI_MODE_HELP) {
		put(&at, end, "HELP ");
		put_ch(&at, end, (char)('1' + u->help_page));
		put_ch(&at, end, '/');
		put_ch(&at, end, (char)('0' + UI_HELP_PAGES));
	} else if (u->mode == UI_MODE_MENU) {
		put(&at, end, "MENU");
	} else if (has_model(u)) {
		put(&at, end, "ASK");
	} else {
		put(&at, end, "ASK - NO MODEL");
	}
	screen_text(fb, 4, 3, UI_TITLE " " UI_STAMP, UI_FG);
	w = screen_text_w(right);
	screen_text(fb, SCREEN_W - 4 - w, 3, right, UI_ACCENT);
	screen_hline(fb, 0, 12, SCREEN_W, UI_ACCENT);
}

static void draw_menu(const ui_t *u, uint16_t *fb)
{
	int y = UI_MENU_Y;

	for (int i = 0; i < UI_MENU_N; i++) {
		int on = (i == u->menu_sel);

		if (on)
			screen_fill(fb, 0, y - 4, SCREEN_W, 16, UI_ACCENT);
		if (on)
			screen_text(fb, 4, y, ">", UI_BG);
		screen_text(fb, 14, y, menu_items[i], on ? UI_BG : UI_FG);
		y += 18;
	}
	screen_text(fb, 4, y + 12, "Word problems need the model.", UI_DIM);
	screen_text(fb, 4, y + 24, "Arithmetic works without it.", UI_DIM);
}

static void draw_scrollbar(const ui_t *u, uint16_t *fb)
{
	int track = UI_PANE_ROWS * SCREEN_LINE_H;
	int max = u->nrows - UI_PANE_ROWS;
	int thumb, span, y;

	if (max <= 0)
		return;
	thumb = track * UI_PANE_ROWS / u->nrows;
	if (thumb < 10)
		thumb = 10;
	span = track - thumb;
	y = UI_PANE_Y + span * u->pane_top / max;
	screen_fill(fb, SCREEN_W - 5, UI_PANE_Y, 2, track, UI_SCROLL);
	screen_fill(fb, SCREEN_W - 5, y, 2, thumb, UI_ACCENT);
}

static void draw_pane(const ui_t *u, uint16_t *fb)
{
	int y = UI_PANE_Y;

	for (int i = 0; i < UI_PANE_ROWS; i++) {
		int r = u->pane_top + i;

		if (r >= u->nrows)
			break;
		screen_text(fb, UI_PANE_X, y, u->rows[r], u->row_dim[r] ? UI_DIM : UI_FG);
		y += SCREEN_LINE_H;
	}
	draw_scrollbar(u, fb);
}

static void draw_input(const ui_t *u, uint16_t *fb)
{
	char line[UI_INPUT_COLS + 1];
	int off = 0;
	int n = u->input_len;
	int cx;

	if (u->cursor > UI_INPUT_COLS - 1)
		off = u->cursor - (UI_INPUT_COLS - 1);
	if (off > n)
		off = n;
	n -= off;
	if (n > UI_INPUT_COLS)
		n = UI_INPUT_COLS;

	screen_text(fb, 4, UI_INPUT_Y, ">", UI_ACCENT);
	if (u->input_len == 0) {
		screen_text(fb, UI_INPUT_X, UI_INPUT_Y, "type a problem", UI_DIM);
	} else {
		memcpy(line, u->input + off, (size_t)n);
		line[n] = '\0';
		screen_text(fb, UI_INPUT_X, UI_INPUT_Y, line, UI_FG);
	}
	cx = UI_INPUT_X + (u->cursor - off) * SCREEN_ADVANCE;
	if (cx > SCREEN_W - 5)
		cx = SCREEN_W - 5;
	screen_fill(fb, cx, UI_INPUT_Y + 8, 4, 2, UI_ACCENT);
}

static void draw_footer(const ui_t *u, uint16_t *fb)
{
	const char *first, *second;

	if (u->mode == UI_MODE_MENU) {
		first = "ENTER choose   UP/DOWN move";
		second = "ESC quit";
	} else if (u->mode == UI_MODE_HELP) {
		first = "UP/DOWN scroll   LEFT/RIGHT page";
		second = "ESC menu";
	} else {
		first = "ENTER ask   DEL erase   ESC clear";
		second = "LEFT/RIGHT history   UP/DOWN scroll";
	}
	screen_text(fb, 4, UI_FOOTER_Y, first, UI_DIM);
	screen_text(fb, 4, UI_FOOTER_Y + 11, second, UI_DIM);
	/* The answer screen has two pictures on offer, and neither is guessable
	 * from the keypad, so the line that advertises them is the last text row
	 * on the screen. A build without a model gets the warning there instead. */
	if (u->mode == UI_MODE_ASK && call_has_diagram(u->last_call) &&
	    graph_call_plottable(u->last_call))
		screen_text(fb, 4, UI_FOOTER_Y + 22, "TAB diagram   VAR plot",
		            UI_DIM);
	else if (u->mode == UI_MODE_ASK && call_has_diagram(u->last_call))
		screen_text(fb, 4, UI_FOOTER_Y + 22, "TAB diagram", UI_DIM);
	else if (u->mode == UI_MODE_ASK && graph_call_plottable(u->last_call))
		screen_text(fb, 4, UI_FOOTER_Y + 22, "VAR plot", UI_DIM);
	else if (u->mode != UI_MODE_MENU && !has_model(u))
		screen_text(fb, 4, UI_FOOTER_Y + 22, "NO MODEL: ARITHMETIC ONLY",
		            UI_ACCENT);
}

int ui_render(ui_t *u, uint16_t *fb)
{
	int hidden;

	screen_clear(fb, UI_BG);
	draw_header(u, fb);
	if (u->mode == UI_MODE_ASK && u->diagram &&
	    diagram_render(u->last_call, fb, NULL, 0) == 0) {
		screen_text(fb, 4, UI_FOOTER_Y - 6,
		            "any key returns to the answer", UI_DIM);
		return 0;
	}
	if (u->mode == UI_MODE_MENU) {
		draw_menu(u, fb);
	} else {
		draw_pane(u, fb);
	}
	if (u->mode == UI_MODE_ASK) {
		screen_hline(fb, 0, UI_INPUT_Y - 4, SCREEN_W, UI_DIM);
		draw_input(u, fb);
		screen_hline(fb, 0, UI_INPUT_Y + 12, SCREEN_W, UI_DIM);
	}
	draw_footer(u, fb);
	hidden = u->nrows - (u->pane_top + UI_PANE_ROWS);
	return hidden > 0 ? hidden : 0;
}
