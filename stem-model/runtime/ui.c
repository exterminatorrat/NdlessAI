/*
 * The calculator's screen model: a menu, an input line and a scrolling pane.
 *
 * Everything a student sees is decided here, in code the host tests can drive,
 * because the calculator cannot be debugged interactively. device/ui_ndless.c
 * reads keys and blits this file's framebuffer; it makes no choices.
 */
#include <stdio.h>
#include <string.h>

#include "pipeline.h"
#include "screen.h"
#include "ui.h"
#include "diagram.h"
#include "expr.h"
#include "graph.h"
#include "reltable.h"
#include "coursetable.h"
#include "solvers.h"

#define UI_BG      SCREEN_BLACK
#define UI_FG      SCREEN_WHITE
#define UI_DIM     SCREEN_DIM
#define UI_ACCENT  SCREEN_ACCENT
#define UI_SCROLL  0x2104

#define UI_MENU_N   7
#define UI_MENU_Y   24

static const char *const menu_items[UI_MENU_N] = {
	"Ask a question",
	"Solve by formula",
	"Keypad help",
	"Graph",
	"By course",
	"Find a relation",
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
	"Next page: chains.",

	"Chain calls with a semicolon\n"
	"\n"
	"<1> is the number the first step\n"
	"answered with, <2> the second, and\n"
	"so on, up to five steps.\n"
	"\n"
	"newton2 2 9.8 ; friction_force 0.2 <1>\n"
	"  the weight, then the friction\n"
	"  it causes: 3.92 N\n"
	"\n"
	"kinematic_v 0 3 4 ; kinetic_energy 5 <1>\n"
	"  the speed, then the energy it\n"
	"  carries: 360 J\n"
	"\n"
	"A step that needs two numbers of\n"
	"its own cannot be chained into."
};

/*
 * What the graph screen puts in the pane on the way in. The plot screen is the
 * only screen with no text on it to explain itself, and an empty input line
 * says nothing, so the page a student lands on names the forms the parser takes
 * and the keys the plot listens to. It fits the window exactly: 18 rows in 18.
 */
static const char *const graph_page =
	"Graphing\n"
	"\n"
	"Type the right-hand side:\n"
	"a function of x, nothing else.\n"
	"\n"
	"x^2-3x+1   a parabola\n"
	"sin(x)\n"
	"1/x\n"
	"abs(x)\n"
	"sqrt(x)\n"
	"\n"
	"On the plot:\n"
	"ARROWS pan the window\n"
	"+/- zoom in and out\n"
	"T traces a point as X and Y\n"
	"R returns the first window\n"
	"\n"
	"VAR on an answer graphs its relation.";

static int has_model(const ui_t *u)
{
	return u->p && u->p->ready;
}

/* The screens with an input line: the graph screen is the same one in another
 * mode, so it draws and edits through the same rows, and the name finder is the
 * relation list with the line live above it. */
static int find_active(const ui_t *u)
{
	return u->mode == UI_MODE_TMPL && u->tmpl_find &&
	       u->tmpl_step == UI_TMPL_STEP_RELATION;
}

static int has_input(const ui_t *u)
{
	return u->mode == UI_MODE_ASK || u->mode == UI_MODE_GRAPH ||
	       find_active(u);
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

/* A plan's last step is the one that produced the answer, so it is the one a
 * diagram or a plot is about. */
static const char *last_step_of(const char *call)
{
	const char *at;

	if (!call)
		return "";
	at = strrchr(call, ';');
	if (!at)
		return call;
	at++;
	while (*at == ' ')
		at++;
	return at;
}

/* Whether the relation a call named has a diagram. The call is "CALL name
 * args...", so the name is the second token; a call that is not that shape has
 * no diagram either. */
static int call_has_diagram(const char *call)
{
	char name[48];
	int i = 0;

	call = last_step_of(call);
	if (strncmp(call, "CALL ", 5) != 0)
		return 0;
	call += 5;
	while (call[i] && call[i] != ' ' && i < (int)sizeof name - 1) {
		name[i] = call[i];
		i++;
	}
	name[i] = '\0';
	return i > 0 && diagram_supports(name);
}

/* ---- solve by formula ---- */

/*
 * The picker walks the relation table the decoder already masks from, so a
 * relation cannot be answerable in English and missing from the list. The
 * subjects come out of that table too, in the order they first appear, which
 * keeps physics before chemistry without a hand-kept list to drift.
 */
/* Defined with the drawing, used by the key handler. */
static void pick_follow(ui_t *u, int count);
/* Defined with the text screens, used by the finder's filter line. */
static void edit_key(ui_t *u, ui_key_t k, int history);

static int subject_count(void)
{
	int n = 0;

	for (int i = 0; i < REL_NUM_RELATIONS; i++) {
		int seen = 0;

		for (int j = 0; j < i && !seen; j++)
			if (strcmp(rel_rows[j].subject, rel_rows[i].subject) == 0)
				seen = 1;
		if (!seen)
			n++;
	}
	return n;
}

static const char *subject_name(int index)
{
	int n = -1;

	for (int i = 0; i < REL_NUM_RELATIONS; i++) {
		int seen = 0;

		for (int j = 0; j < i && !seen; j++)
			if (strcmp(rel_rows[j].subject, rel_rows[i].subject) == 0)
				seen = 1;
		if (!seen && ++n == index)
			return rel_rows[i].subject;
	}
	return "";
}

/* The relation index for a decoder id, or -1. The picker walks the course
 * table, which stores ids, and the decoder is indexed by position. */
static int rel_index_for_id(int id)
{
	for (int i = 0; i < REL_NUM_RELATIONS; i++)
		if (rel_rows[i].id == id)
			return i;
	return -1;
}

static char lower_ch(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/*
 * Whether the query matches the name starting exactly at its first character.
 * Case is ignored, and so are the separators a name is written with: a student
 * types "friction force" and means friction_force, and asking them to remember
 * where the underscores are is the reason the picker was slow to use. Both
 * strings are walked in place, so a match costs no copy and no allocation.
 */
static int find_at(const char *name, const char *q)
{
	while (*q) {
		/* The separator is optional on both sides, so a student who types
		 * the name exactly as it is printed matches it too. */
		if (*q == ' ' || *q == '_') {
			q++;
			continue;
		}
		while (*name == '_' || *name == ' ')
			name++;
		if (!*name || lower_ch(*name) != lower_ch(*q))
			return 0;
		name++;
		q++;
	}
	return 1;
}

/* Whether the query appears anywhere in the name, not only at its start. */
static int find_inside(const char *name, const char *q)
{
	for (const char *start = name + 1; *start; start++)
		if (find_at(start, q))
			return 1;
	return 0;
}

/*
 * The list the finder shows: every relation whose name matches the input line,
 * the ones it starts with first. An empty line is the whole catalogue, so
 * opening the finder and arrowing through it is a flat list of everything the
 * program can call, which is what the subject route gives for one subject.
 *
 * The order matters because the first row is the one ENTER takes: "perc"
 * should open on percent_change, not on mass_percent, which only contains it.
 */
static void find_reload(ui_t *u)
{
	int n = 0;

	/* The array is as long as the catalogue and no relation is collected
	 * twice, so the write cannot run past its end. */
	for (int pass = 0; pass < 2; pass++)
		for (int i = 0; i < REL_NUM_RELATIONS; i++) {
			int head = find_at(rel_rows[i].name, u->input);

			if ((head || find_inside(rel_rows[i].name, u->input)) &&
			    head == (pass == 0))
				u->tmpl_rels[n++] = (unsigned char)i;
		}
	u->tmpl_count = n;
	u->tmpl_sel = 0;
	u->tmpl_top = 0;
	copy_trunc(u->tmpl_note, sizeof u->tmpl_note,
	           n == 0 ? "no relation matches" : "");
}

/* The finder's own way in: the catalogue, filtered by a line that starts
 * empty. Shared by the menu entry and by backing out of the argument prompts. */
static void find_open(ui_t *u, const char *query)
{
	u->mode = UI_MODE_TMPL;
	u->tmpl_by_course = 0;
	u->tmpl_find = 1;
	u->tmpl_step = UI_TMPL_STEP_RELATION;
	copy_trunc(u->input, sizeof u->input, query ? query : "");
	u->input_len = (int)strlen(u->input);
	u->cursor = u->input_len;
	u->recall = -1;
	find_reload(u);
}

/*
 * Fill the relation list from a course. Only topics the map records as
 * relations are offered: a topic recorded as a gap has no solver behind it, and
 * a typed topic is answered by the expression engine rather than by a call, so
 * neither belongs in a list of things to call.
 */
/* How many rows the current pick list has: courses, subjects, or relations. */
static int list_count(const ui_t *u)
{
	if (u->tmpl_step != UI_TMPL_STEP_SUBJECT)
		return u->tmpl_count;
	return u->tmpl_by_course ? CRS_NUM_COURSES : subject_count();
}

static void tmpl_load_course(ui_t *u, int course)
{
	const crs_course_t *row = &crs_courses[course];

	u->tmpl_subject = -1;
	u->tmpl_count = 0;
	u->tmpl_sel = 0;
	u->tmpl_top = 0;
	for (int i = 0; i < row->num_units; i++) {
		const crs_unit_t *unit = &crs_units[row->first_unit + i];

		for (int j = 0; j < unit->num_topics; j++) {
			const crs_topic_t *topic = &crs_topics[unit->first_topic + j];
			int index;

			if (topic->kind != 1)
				continue;
			index = rel_index_for_id(topic->rel);
			if (index < 0 || u->tmpl_count >= UI_TMPL_ROWS)
				continue;
			u->tmpl_rels[u->tmpl_count++] = (unsigned char)index;
		}
	}
}

static void tmpl_load(ui_t *u, int subject)
{
	const char *name = subject_name(subject);

	u->tmpl_subject = subject;
	u->tmpl_count = 0;
	u->tmpl_sel = 0;
	u->tmpl_top = 0;
	for (int i = 0; i < REL_NUM_RELATIONS && u->tmpl_count < UI_TMPL_ROWS; i++)
		if (strcmp(rel_rows[i].subject, name) == 0)
			u->tmpl_rels[u->tmpl_count++] = (unsigned char)i;
}

/* The i-th argument's name, out of the row's space-separated parameter list. */
static void arg_label(const rel_row_t *row, int i, char *out, int outlen)
{
	const char *p = row->params;
	int n = 0;

	out[0] = '\0';
	while (p && *p) {
		int len = 0;

		while (p[len] && p[len] != ' ')
			len++;
		if (n == i) {
			if (len > outlen - 1)
				len = outlen - 1;
			memcpy(out, p, (size_t)len);
			out[len] = '\0';
			return;
		}
		n++;
		p += len;
		while (*p == ' ')
			p++;
	}
}

/*
 * The last prompt answered: build the call, let the solver answer it, and show
 * the result on the answer screen with its working. ui_result reads the call
 * from the pane's own state, so TAB and VAR work on it like any other answer.
 */
static void tmpl_solve(ui_t *u)
{
	const rel_row_t *row = &rel_rows[u->tmpl_rels[u->tmpl_rel]];
	/* Revised 2026-11-16: size argument pointers for the full formula capacity. */
	const char *args[UI_TMPL_ARGS];
	char call[UI_CALL_MAX];
	char answer[128];
	int used, rc, i;

	for (i = 0; i < row->arity; i++)
		args[i] = u->tmpl_args[i];
	used = snprintf(call, sizeof call, "CALL %s", row->name);
	for (i = 0; i < row->arity && used > 0 && used < (int)sizeof call; i++)
		used += snprintf(call + used, (size_t)((int)sizeof call - used), " %s",
		                 args[i]);
	answer[0] = '\0';
	rc = solvers_solve(row->name, args, row->arity, answer, sizeof answer) < 0
	     ? PIPELINE_ERR : PIPELINE_OK;
	u->mode = UI_MODE_ASK;
	ui_result(u, row->name, rc, answer, call);
}

static ui_key_t tmpl_key(ui_t *u, ui_key_t k)
{
	int arity;

	switch (u->tmpl_step) {
	case UI_TMPL_STEP_SUBJECT:
		if (k == UI_KEY_UP && u->tmpl_sel > 0) {
			u->tmpl_sel--;
			pick_follow(u, list_count(u));
		} else if (k == UI_KEY_DOWN && u->tmpl_sel < list_count(u) - 1) {
			u->tmpl_sel++;
			pick_follow(u, list_count(u));
		} else if (k == UI_KEY_ACCEPT) {
			if (u->tmpl_by_course)
				tmpl_load_course(u, u->tmpl_sel);
			else
				tmpl_load(u, u->tmpl_sel);
			u->tmpl_step = UI_TMPL_STEP_RELATION;
		} else if (k == UI_KEY_BACK)
			u->mode = UI_MODE_MENU;
		break;
	case UI_TMPL_STEP_RELATION:
		if (k == UI_KEY_UP && u->tmpl_sel > 0) {
			u->tmpl_sel--;
			pick_follow(u, u->tmpl_count);
		} else if (k == UI_KEY_DOWN && u->tmpl_sel < u->tmpl_count - 1) {
			u->tmpl_sel++;
			pick_follow(u, u->tmpl_count);
		}
		else if (k == UI_KEY_ACCEPT) {
			if (u->tmpl_count == 0)
				break;
			u->tmpl_rel = u->tmpl_sel;
			u->tmpl_arg = 0;
			/* The line becomes the number field, so the filter it held has
			 * to be put somewhere the way back can find it. */
			if (u->tmpl_find)
				copy_trunc(u->tmpl_find_q, sizeof u->tmpl_find_q,
				           u->input);
			u->input[0] = '\0';
			u->input_len = 0;
			u->cursor = 0;
			u->tmpl_note[0] = '\0';
			u->tmpl_step = UI_TMPL_STEP_ARGS;
		} else if (k == UI_KEY_BACK) {
			/* In the finder, back clears the filter before it leaves: a
			 * narrowed list is still a list the student can widen. */
			if (u->tmpl_find && u->input_len > 0) {
				u->input[0] = '\0';
				u->input_len = 0;
				u->cursor = 0;
				find_reload(u);
			} else if (u->tmpl_find) {
				u->tmpl_find = 0;
				u->mode = UI_MODE_MENU;
			} else {
				u->tmpl_step = UI_TMPL_STEP_SUBJECT;
				u->tmpl_sel = u->tmpl_subject;
			}
		} else if (u->tmpl_find) {
			int had = u->input_len;

			/* The cursor keys edit the filter here; there is no history to
			 * page through on a screen whose line is a search box. */
			edit_key(u, k, 0);
			if (u->input_len != had)
				find_reload(u);
		}
		break;
	case UI_TMPL_STEP_ARGS:
		arity = rel_rows[u->tmpl_rels[u->tmpl_rel]].arity;
		if (k == UI_KEY_ACCEPT) {
			if (u->input_len == 0)
				break;
			copy_trunc(u->tmpl_args[u->tmpl_arg], UI_TMPL_ARG, u->input);
			u->input[0] = '\0';
			u->input_len = 0;
			u->cursor = 0;
			if (++u->tmpl_arg >= arity)
				tmpl_solve(u);
		} else if (k == UI_KEY_BACK) {
			if (u->input_len > 0) {
				u->input[0] = '\0';
				u->input_len = 0;
				u->cursor = 0;
			} else if (u->tmpl_find) {
				/* Back to the list the student narrowed, not to the
				 * catalogue: the query is still what they typed. */
				find_open(u, u->tmpl_find_q);
				u->tmpl_sel = u->tmpl_rel;
				pick_follow(u, u->tmpl_count);
			} else {
				u->tmpl_step = UI_TMPL_STEP_RELATION;
				u->tmpl_sel = u->tmpl_rel;
			}
		}
		break;
	}
	return UI_KEY_NONE;
}

/* ---- the graph screen ---- */

/*
 * The input line as the plotter reads it. The old prompt asked for the whole
 * "y = f(x)", so a student types the "y =" out of habit; dropping it is worth
 * more than the paragraph it would take to talk them out of it.
 */
static const char *graph_expr(const ui_t *u)
{
	const char *s = u->input;

	while (*s == ' ')
		s++;
	if (*s == 'y' || *s == 'Y') {
		const char *p = s + 1;

		while (*p == ' ')
			p++;
		if (*p == '=') {
			s = p + 1;
			while (*s == ' ')
				s++;
		}
	}
	return s;
}

const char *ui_graph_text(const ui_t *u)
{
	return graph_expr(u);
}

/*
 * Refused functions stay on the screen with the reason. The plot screen takes
 * the whole display, so a complaint raised after it opens is a complaint the
 * student has already lost the input line to.
 */
static void graph_note(ui_t *u, const char *headline, const char *detail)
{
	char src[256];
	char *at = src;
	const char *end = src + sizeof src - 1;
	int from;

	put(&at, end, headline);
	put(&at, end, "\n");
	put(&at, end, detail);
	from = (int)strlen(headline) + 1;
	pane_set(u, src, from, (int)strlen(src) + 1);
}

static void graph_enter(ui_t *u)
{
	u->mode = UI_MODE_GRAPH;
	pane_set(u, graph_page, 0, 0);
}

/*
 * The help pages and the graph screen borrow the pane. Leaving puts back
 * whatever was there: returning to the menu with a page of syntax sitting where
 * the answer was would read as if that page were the result.
 */
static void pane_restore(ui_t *u)
{
	pane_set(u, u->pane_src, u->pane_dim_from, u->pane_dim_to);
	u->mode = UI_MODE_MENU;
}

/*
 * The parser the plot screen uses, so what this screen accepts is exactly what
 * that one can draw, and the refusal is worded here instead of arriving as a
 * message box over a half-drawn plot. The tree is a scratch: it is thrown away
 * once the line parses, and the plotter parses again for itself.
 */
static gexpr_t graph_scratch;

static ui_key_t graph_plot(ui_t *u)
{
	const char *src = graph_expr(u);
	char err[64];

	if (!src[0])
		return UI_KEY_NONE;
	if (gexpr_parse(src, &graph_scratch, err, (int)sizeof err) < 0) {
		graph_note(u, "That is not an expression in x.", err);
		return UI_KEY_NONE;
	}
	if (gexpr_has_y(&graph_scratch)) {
		graph_note(u, "Use x only.",
		           "y is the value, so y is what gets\n"
		           "plotted. Try x^2-3x+1.");
		return UI_KEY_NONE;
	}
	/* Rewrite the line as the plotter reads it: pressing ENTER twice then
	 * plots the same curve twice, and the student sees the text that is being
	 * plotted rather than the text they typed around it. */
	memmove(u->input, src, strlen(src) + 1);
	u->input_len = (int)strlen(u->input);
	u->cursor = u->input_len;
	history_push(u, u->input, graph_page, 0, 0);
	return UI_KEY_GRAPH;
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
			/* Solve by formula opens on the subject list. Nothing is
			 * typed but numbers, and the model is never involved. */
			u->mode = UI_MODE_TMPL;
			u->tmpl_by_course = 0;
			u->tmpl_find = 0;
			u->tmpl_step = UI_TMPL_STEP_SUBJECT;
			u->tmpl_sel = 0;
			u->tmpl_top = 0;
			break;
		case 2:
			u->mode = UI_MODE_HELP;
			help_open(u, 0);
			break;
		case 3:
			graph_enter(u);
			break;
		case 4:
			/* The same flow, opened on the course the student is taking.
			 * The subject route stays for anyone who does not know their
			 * course, so nothing becomes unreachable. */
			u->mode = UI_MODE_TMPL;
			u->tmpl_by_course = 1;
			u->tmpl_find = 0;
			u->tmpl_step = UI_TMPL_STEP_SUBJECT;
			u->tmpl_sel = 0;
			u->tmpl_top = 0;
			break;
		case 5:
			/* The third way in: the whole catalogue with a filter line, for
			 * a student who knows the name and not the subject or course. */
			find_open(u, "");
			break;
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
		pane_restore(u);
		break;
	default:
		break;
	}
	return UI_KEY_NONE;
}

/*
 * The keys the two text screens share: history paging and cursor movement on
 * the same arrows, delete behind the cursor, and the pane scrolled by whole
 * rows. While the line is empty or is a recalled entry, left and right walk the
 * ring; once the student has typed or deleted, they move the cursor instead.
 * Typing and deleting clear `recall`, so an edited entry is never silently
 * replaced by the next arrow press.
 *
 * `history` is off on the finder, where the line is a search box: paging the
 * answer history into it would replace the filter with an old question.
 */
static void edit_key(ui_t *u, ui_key_t k, int history)
{
	switch (k) {
	case UI_KEY_UP:
		scroll_pane(u, -1);
		break;
	case UI_KEY_DOWN:
		scroll_pane(u, 1);
		break;
	case UI_KEY_LEFT:
		if (history && (u->recall >= 0 || u->input_len == 0))
			recall_step(u, -1);
		else if (u->cursor > 0)
			u->cursor--;
		break;
	case UI_KEY_RIGHT:
		if (history && (u->recall >= 0 || u->input_len == 0))
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
}

/* Back on a typed line clears it; back on an empty one leaves the screen. */
static void back_key(ui_t *u, void (*leave)(ui_t *))
{
	if (u->input_len > 0) {
		u->input[0] = '\0';
		u->input_len = 0;
		u->cursor = 0;
		u->recall = -1;
	} else {
		leave(u);
	}
}

static void ask_leave(ui_t *u)
{
	u->mode = UI_MODE_MENU;
}

static ui_key_t ask_key(ui_t *u, ui_key_t k)
{
	switch (k) {
	case UI_KEY_ACCEPT:
		ask(u);
		break;
	case UI_KEY_BACK:
		back_key(u, ask_leave);
		break;
	case UI_KEY_MODE:
		ask_leave(u);
		break;
	case UI_KEY_DIAGRAM:
		u->diagram = !u->diagram && call_has_diagram(u->last_call);
		break;
	case UI_KEY_GRAPH:
		/* The plotter is the device's loop, so the model can only ask for
		 * it. An answer that came from arithmetic has no curve to show. */
		return graph_call_plottable(last_step_of(u->last_call))
		       ? UI_KEY_GRAPH : UI_KEY_NONE;
	default:
		edit_key(u, k, 1);
		break;
	}
	return UI_KEY_NONE;
}

/*
 * The graph screen: the same input line, checked against the plotter's own
 * parser, and a key that hands the line to the plot screen. The line is kept
 * after a plot, so a parabola can be nudged into the next one without retyping
 * it; ESC clears it, and ESC again goes back to the menu.
 */
static ui_key_t graph_screen_key(ui_t *u, ui_key_t k)
{
	switch (k) {
	case UI_KEY_ACCEPT:
		return graph_plot(u);
	case UI_KEY_BACK:
		back_key(u, pane_restore);
		break;
	case UI_KEY_MODE:
		pane_restore(u);
		break;
	default:
		edit_key(u, k, 1);
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
	case UI_MODE_TMPL:
		return tmpl_key(u, k);
	case UI_MODE_GRAPH:
		return graph_screen_key(u, k);
	}
	return UI_KEY_NONE;
}

const char *ui_last_call(const ui_t *u)
{
	return u->last_call;
}

int ui_type(ui_t *u, char c)
{
	int args = u->mode == UI_MODE_TMPL && u->tmpl_step == UI_TMPL_STEP_ARGS;

	if (u->mode != UI_MODE_ASK && u->mode != UI_MODE_GRAPH && !args &&
	    !find_active(u))
		return 0;
	if (c < 32 || c > 126)
		return 0;
	/* An argument is a number. A letter here would only reach the solver as a
	 * parse error, so it never reaches the line. */
	/* Revised 2026-11-16: accept scientific notation's exponent and signs. */
	if (args && !((c >= '0' && c <= '9') || c == '.' || c == '-' || c == '+' ||
	              c == 'e' || c == 'E'))
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
	/* The finder narrows as the student types; every other line is a value
	 * that only means something when ENTER is pressed. */
	if (find_active(u))
		find_reload(u);
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

/* Keeps the highlighted row inside the visible window as the arrows move. */
static void pick_follow(ui_t *u, int count)
{
	if (u->tmpl_sel < u->tmpl_top)
		u->tmpl_top = u->tmpl_sel;
	if (u->tmpl_sel >= u->tmpl_top + UI_PANE_ROWS)
		u->tmpl_top = u->tmpl_sel - UI_PANE_ROWS + 1;
	if (u->tmpl_top > count - UI_PANE_ROWS)
		u->tmpl_top = count - UI_PANE_ROWS;
	if (u->tmpl_top < 0)
		u->tmpl_top = 0;
}

static void pick_scrollbar(int count, int top, uint16_t *fb)
{
	int track = UI_PANE_ROWS * SCREEN_LINE_H;
	int thumb, span, y;

	if (count <= UI_PANE_ROWS)
		return;
	thumb = track * UI_PANE_ROWS / count;
	if (thumb < 10)
		thumb = 10;
	span = track - thumb;
	y = UI_PANE_Y + span * top / (count - UI_PANE_ROWS);
	screen_fill(fb, SCREEN_W - 5, UI_PANE_Y, 2, track, UI_SCROLL);
	screen_fill(fb, SCREEN_W - 5, y, 2, thumb, UI_ACCENT);
}

/*
 * The subject list, then the relations of the chosen subject. The selected
 * row is inverted the way the menu's is, so the two screens read as one
 * program.
 *
 * In the finder the same list holds the matches, and a query that matches
 * nothing says so where the rows would have been. That line is drawn here, in
 * the pane, so the key hints at the bottom keep saying which keys do what.
 */
static void draw_pick(const ui_t *u, uint16_t *fb)
{
	int count = list_count(u);
	int y = UI_PANE_Y;

	for (int i = 0; i < UI_PANE_ROWS; i++) {
		int r = u->tmpl_top + i;
		const char *text;
		int on;

		if (r >= count)
			break;
		if (u->tmpl_step == UI_TMPL_STEP_SUBJECT)
			text = u->tmpl_by_course ? crs_courses[r].name
			                         : subject_name(r);
		else
			text = rel_rows[u->tmpl_rels[r]].name;
		on = r == u->tmpl_sel;
		if (on)
			screen_fill(fb, 0, y - 3, SCREEN_W, SCREEN_LINE_H, UI_ACCENT);
		if (on)
			screen_text(fb, 4, y, ">", UI_BG);
		screen_text(fb, 14, y, text, on ? UI_BG : UI_FG);
		y += SCREEN_LINE_H;
	}
	if (count == 0 && find_active(u) && u->tmpl_note[0])
		screen_text(fb, UI_PANE_X, UI_PANE_Y, u->tmpl_note, UI_DIM);
	pick_scrollbar(count, u->tmpl_top, fb);
}

/*
 * The argument prompts: the relation's name, then one line per argument with
 * the value already given. The current one is marked and gets the input line,
 * so the student never has to remember how many arguments are left.
 */
static void draw_tmpl_form(const ui_t *u, uint16_t *fb)
{
	const rel_row_t *row = &rel_rows[u->tmpl_rels[u->tmpl_rel]];
	int y = UI_PANE_Y;

	screen_text(fb, UI_PANE_X, y, row->name, UI_FG);
	y += SCREEN_LINE_H * 2;
	for (int i = 0; i < row->arity && i < UI_TMPL_ARGS; i++) {
		char label[UI_TMPL_ARG];
		char line[UI_TEXT_COLS + 1];
		int on = i == u->tmpl_arg;
		int at = 0;

		arg_label(row, i, label, (int)sizeof label);
		line[at++] = (char)('0' + (i + 1) % 10);
		line[at++] = '.';
		line[at++] = ' ';
		for (int j = 0; label[j] && at < UI_TEXT_COLS - 2; j++)
			line[at++] = label[j];
		line[at++] = ' ';
		line[at++] = '=';
		line[at++] = ' ';
		if (i < u->tmpl_arg)
			for (int j = 0; u->tmpl_args[i][j] && at < UI_TEXT_COLS; j++)
				line[at++] = u->tmpl_args[i][j];
		else if (i > u->tmpl_arg)
			line[at++] = '-';
		line[at] = '\0';
		if (on)
			screen_fill(fb, 0, y - 3, SCREEN_W, SCREEN_LINE_H, UI_ACCENT);
		screen_text(fb, UI_PANE_X, y, line, on ? UI_BG : UI_FG);
		y += SCREEN_LINE_H;
	}
	screen_text(fb, UI_PANE_X, y + SCREEN_LINE_H,
	            "type the number, ENTER next", UI_DIM);
}

/* The input line under an argument prompt, labelled with the argument's name. */
static void draw_tmpl_input(const ui_t *u, uint16_t *fb)
{
	const rel_row_t *row = &rel_rows[u->tmpl_rels[u->tmpl_rel]];
	char label[UI_TMPL_ARG];
	char line[UI_INPUT_COLS + 1];
	int x, cx, n;

	arg_label(row, u->tmpl_arg, label, (int)sizeof label);
	screen_text(fb, 4, UI_INPUT_Y, ">", UI_ACCENT);
	screen_text(fb, UI_INPUT_X, UI_INPUT_Y, label, UI_ACCENT);
	x = UI_INPUT_X + (int)strlen(label) * SCREEN_ADVANCE;
	screen_text(fb, x, UI_INPUT_Y, "=", UI_ACCENT);
	x += 2 * SCREEN_ADVANCE;

	n = u->input_len;
	if (n > UI_INPUT_COLS)
		n = UI_INPUT_COLS;
	memcpy(line, u->input, (size_t)n);
	line[n] = '\0';
	if (n == 0)
		screen_text(fb, x, UI_INPUT_Y, "0", UI_DIM);
	else
		screen_text(fb, x, UI_INPUT_Y, line, UI_FG);
	cx = x + u->cursor * SCREEN_ADVANCE;
	if (cx > SCREEN_W - 5)
		cx = SCREEN_W - 5;
	screen_fill(fb, cx, UI_INPUT_Y + 8, 4, 2, UI_ACCENT);
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
	} else if (find_active(u)) {
		/* Not "SOLVE 2/3": the finder is the relation list without the
		 * subject step above it, and numbering a step the student did not
		 * take would read as a list they are stuck in. */
		put(&at, end, "FIND");
	} else if (u->mode == UI_MODE_TMPL) {
		put(&at, end, "SOLVE ");
		put_ch(&at, end, (char)('1' + u->tmpl_step));
		put(&at, end, "/3");
	} else if (u->mode == UI_MODE_GRAPH) {
		/* The graph screen never needs the model, so it never says whether
		 * one was found. */
		put(&at, end, "GRAPH");
	} else if (has_model(u)) {
		put(&at, end, "ASK");
	} else {
		put(&at, end, "ASK - NO MODEL");
	}
	screen_text(fb, 4, 3, SCREEN_TITLE " " SCREEN_STAMP, UI_FG);
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
		screen_text(fb, UI_INPUT_X, UI_INPUT_Y,
		            u->mode == UI_MODE_GRAPH ? "a function of x"
		            : find_active(u)        ? "type part of a name"
		                                     : "type a problem", UI_DIM);
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
	} else if (u->mode == UI_MODE_TMPL && u->tmpl_step == UI_TMPL_STEP_SUBJECT) {
		first = "ENTER choose   UP/DOWN move";
		second = "ESC menu";
	} else if (find_active(u)) {
		/* The one hint that has to be here: nothing else on this screen says
		 * the list is searchable. */
		first = "type to narrow   ENTER choose";
		second = "UP/DOWN move   ESC menu";
	} else if (u->mode == UI_MODE_TMPL && u->tmpl_step == UI_TMPL_STEP_RELATION) {
		first = "ENTER choose   UP/DOWN move";
		second = "ESC back";
	} else if (u->mode == UI_MODE_TMPL) {
		first = "ENTER next number   DEL erase";
		second = "ESC back";
	} else if (u->mode == UI_MODE_GRAPH) {
		first = "ENTER plot   DEL erase   ESC clear";
		second = "LEFT/RIGHT history   UP/DOWN scroll";
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
	else if (u->mode == UI_MODE_ASK && !has_model(u))
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
	if (u->mode == UI_MODE_TMPL) {
		if (u->tmpl_step == UI_TMPL_STEP_ARGS) {
			draw_tmpl_form(u, fb);
			screen_hline(fb, 0, UI_INPUT_Y - 4, SCREEN_W, UI_DIM);
			draw_tmpl_input(u, fb);
			screen_hline(fb, 0, UI_INPUT_Y + 12, SCREEN_W, UI_DIM);
		} else {
			draw_pick(u, fb);
			/* The finder is the one pick screen with a line of its own. */
			if (has_input(u)) {
				screen_hline(fb, 0, UI_INPUT_Y - 4, SCREEN_W, UI_DIM);
				draw_input(u, fb);
				screen_hline(fb, 0, UI_INPUT_Y + 12, SCREEN_W, UI_DIM);
			}
		}
		draw_footer(u, fb);
		return 0;
	}
	if (u->mode == UI_MODE_MENU) {
		draw_menu(u, fb);
	} else {
		draw_pane(u, fb);
	}
	if (has_input(u)) {
		screen_hline(fb, 0, UI_INPUT_Y - 4, SCREEN_W, UI_DIM);
		draw_input(u, fb);
		screen_hline(fb, 0, UI_INPUT_Y + 12, SCREEN_W, UI_DIM);
	}
	draw_footer(u, fb);
	hidden = u->nrows - (u->pane_top + UI_PANE_ROWS);
	return hidden > 0 ? hidden : 0;
}
