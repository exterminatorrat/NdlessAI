#include <stdio.h>
#include <string.h>

#include "matrix.h"
#include "solvers_subject.h"

#define MAT_MAX 4		/* largest matrix the student can type */
#define MAT_WIDE (2 * MAT_MAX)	/* room for [A|I] and [A|b] */
#define ENTRY_MAX 24

typedef struct {
	int rows, cols;
	rat v[MAT_MAX][MAT_WIDE];
} mat_t;

static const rat ZERO = { 0, 1 };
static const rat ONE = { 1, 1 };

static int is_space(char c)
{
	return c == ' ' || c == '\t';
}

static char lower(char c)
{
	return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* "[1,2;3,4]" at *p, advancing past the closing bracket. */
static int parse_matrix(const char **p, mat_t *m)
{
	const char *s = *p;
	int rows = 0, cols = 0;

	if (*s != '[')
		return -1;
	s++;

	for (;;) {
		int col = 0;

		for (;;) {
			char buf[ENTRY_MAX];
			int n = 0, flt = 0;
			rat r;

			while (is_space(*s))
				s++;
			while (*s && *s != ',' && *s != ';' && *s != ']' &&
			       !is_space(*s)) {
				if (n >= ENTRY_MAX - 1)
					return -1;
				buf[n++] = *s++;
			}
			buf[n] = '\0';
			if (n == 0)
				return -1;
			if (rows >= MAT_MAX || col >= MAT_MAX)
				return -2;	/* too big, a nameable refusal */
			/* parse_rat takes integers and decimals, not fractions, so
			 * "1/3" in the input is refused rather than read as "1".
			 * Exactness is preserved on the way out. */
			if (parse_rat(buf, &r, &flt) != 0)
				return -1;
			m->v[rows][col++] = r;
			while (is_space(*s))
				s++;
			if (*s == ',') {
				s++;
				continue;
			}
			break;
		}

		if (rows == 0)
			cols = col;
		else if (col != cols)
			return -1;
		rows++;
		if (*s == ';') {
			s++;
			continue;
		}
		break;
	}

	if (*s != ']')
		return -1;
	m->rows = rows;
	m->cols = cols;
	*p = s + 1;
	return 0;
}

/*
 * Reduced row echelon form in place. Returns the rank, or -1 when an
 * intermediate rational leaves the exact domain: a caller must report that as
 * a refusal, because a wrapped value is indistinguishable from an answer.
 */
static int rref(mat_t *m, int *pivots)
{
	int rank = 0;

	for (int c = 0; c < m->cols && rank < m->rows; c++) {
		int pivot = -1;
		rat inv;

		for (int i = rank; i < m->rows; i++)
			if (m->v[i][c].n != 0) {
				pivot = i;
				break;
			}
		if (pivot < 0)
			continue;

		if (pivot != rank)
			for (int j = 0; j < m->cols; j++) {
				rat t = m->v[rank][j];

				m->v[rank][j] = m->v[pivot][j];
				m->v[pivot][j] = t;
			}

		if (rat_div(&inv, &ONE, &m->v[rank][c]) != 0)
			return -1;
		for (int j = 0; j < m->cols; j++)
			if (rat_mul(&m->v[rank][j], &m->v[rank][j], &inv) != 0)
				return -1;

		for (int i = 0; i < m->rows; i++) {
			rat f, t;

			if (i == rank || m->v[i][c].n == 0)
				continue;
			f = m->v[i][c];
			for (int j = 0; j < m->cols; j++) {
				if (rat_mul(&t, &f, &m->v[rank][j]) != 0)
					return -1;
				if (rat_sub(&m->v[i][j], &m->v[i][j], &t) != 0)
					return -1;
			}
		}

		pivots[rank++] = c;
	}

	return rank;
}

/*
 * Determinant by elimination on a copy: the product of the pivots, with the
 * sign flipped once per row swap. The rows are not normalised, so the pivots
 * stay the factors the determinant is built from.
 */
static int determinant(const mat_t *src, rat *out)
{
	mat_t m = *src;
	int n = m.rows;
	rat det = ONE;

	for (int c = 0; c < n; c++) {
		int pivot = -1;

		for (int i = c; i < n; i++)
			if (m.v[i][c].n != 0) {
				pivot = i;
				break;
			}
		if (pivot < 0) {
			*out = ZERO;
			return 0;
		}

		if (pivot != c) {
			for (int j = 0; j < n; j++) {
				rat t = m.v[c][j];

				m.v[c][j] = m.v[pivot][j];
				m.v[pivot][j] = t;
			}
			det.n = -det.n;
		}

		if (rat_mul(&det, &det, &m.v[c][c]) != 0)
			return -1;

		for (int i = c + 1; i < n; i++) {
			rat f, t;

			if (m.v[i][c].n == 0)
				continue;
			if (rat_div(&f, &m.v[i][c], &m.v[c][c]) != 0)
				return -1;
			for (int j = c; j < n; j++) {
				if (rat_mul(&t, &f, &m.v[c][j]) != 0)
					return -1;
				if (rat_sub(&m.v[i][j], &m.v[i][j], &t) != 0)
					return -1;
			}
		}
	}

	*out = det;
	return 0;
}

static int render_matrix(const mat_t *m, char *out, size_t outlen)
{
	size_t used = 0;

	if (outlen < 4)
		return -1;
	out[used++] = '[';

	for (int i = 0; i < m->rows; i++) {
		for (int j = 0; j < m->cols; j++) {
			char cell[48];
			size_t n;

			if (j > 0) {
				if (used + 2 >= outlen)
					return -1;
				out[used++] = ',';
				out[used++] = ' ';
			}
			fmt_rat(&m->v[i][j], cell, sizeof cell);
			n = strlen(cell);
			if (used + n >= outlen)
				return -1;
			memcpy(out + used, cell, n);
			used += n;
		}
		if (i + 1 < m->rows) {
			if (used + 2 >= outlen)
				return -1;
			out[used++] = ';';
			out[used++] = ' ';
		}
	}

	if (used + 2 > outlen)
		return -1;
	out[used++] = ']';
	out[used] = '\0';
	return 0;
}

static void say(char *out, size_t outlen, const char *text)
{
	out_set(out, outlen, text);
}

static void answer_det(const mat_t *m, char *out, size_t outlen)
{
	rat d;

	if (m->rows != m->cols) {
		say(out, outlen, "determinant needs a square matrix");
		return;
	}
	if (determinant(m, &d) != 0) {
		say(out, outlen, "value out of range for exact arithmetic");
		return;
	}
	fmt_rat(&d, out, outlen);
}

static void answer_rank(const mat_t *m, char *out, size_t outlen)
{
	mat_t w = *m;
	int pivots[MAT_MAX];
	int r = rref(&w, pivots);
	char buf[16];

	if (r < 0) {
		say(out, outlen, "value out of range for exact arithmetic");
		return;
	}
	snprintf(buf, sizeof buf, "%d", r);
	out_set(out, outlen, buf);
}

static void answer_inv(const mat_t *m, char *out, size_t outlen)
{
	mat_t w, inv_mat, probe;
	int pivots[MAT_MAX];
	int n = m->rows;
	int r;

	if (m->rows != m->cols) {
		say(out, outlen, "inverse needs a square matrix");
		return;
	}

	/* The rank has to come from the matrix, not from [A|I]: the identity
	 * block supplies a pivot in every column A fails to, so a singular
	 * matrix reduced as an augmented pair looks full rank and gets divided
	 * by zero into a matrix of garbage. */
	probe = *m;
	probe.rows = n;
	probe.cols = n;
	r = rref(&probe, pivots);
	if (r < 0) {
		say(out, outlen, "value out of range for exact arithmetic");
		return;
	}
	if (r < n) {
		say(out, outlen, "no inverse: the matrix is singular");
		return;
	}

	w.rows = n;
	w.cols = 2 * n;
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++) {
			w.v[i][j] = m->v[i][j];
			w.v[i][j + n] = (i == j) ? ONE : ZERO;
		}

	if (rref(&w, pivots) < 0) {
		say(out, outlen, "value out of range for exact arithmetic");
		return;
	}

	inv_mat.rows = n;
	inv_mat.cols = n;
	for (int i = 0; i < n; i++)
		for (int j = 0; j < n; j++)
			inv_mat.v[i][j] = w.v[i][j + n];
	if (render_matrix(&inv_mat, out, outlen) != 0)
		say(out, outlen, "value out of range for exact arithmetic");
}

static void answer_solve(const mat_t *a, const mat_t *b, char *out,
                         size_t outlen)
{
	mat_t w, probe;
	int pivots[MAT_MAX];
	int n = a->rows;
	int rank_a, rank_w;
	size_t used = 0;

	if (a->rows != a->cols) {
		say(out, outlen, "solve needs a square system");
		return;
	}

	/* A one-row b is the usual "[5,10]"; a one-column b is the same vector
	 * written the way a student writes a column. */
	if (!((b->rows == 1 && b->cols == n) || (b->rows == n && b->cols == 1))) {
		say(out, outlen, "solve: the right-hand side does not fit");
		return;
	}

	probe = *a;
	probe.rows = n;
	probe.cols = n;
	rank_a = rref(&probe, pivots);
	if (rank_a < 0) {
		say(out, outlen, "value out of range for exact arithmetic");
		return;
	}

	w.rows = n;
	w.cols = n + 1;
	for (int i = 0; i < n; i++) {
		for (int j = 0; j < n; j++)
			w.v[i][j] = a->v[i][j];
		w.v[i][n] = (b->rows == 1) ? b->v[0][i] : b->v[i][0];
	}

	rank_w = rref(&w, pivots);
	if (rank_w < 0) {
		say(out, outlen, "value out of range for exact arithmetic");
		return;
	}

	/* The coefficient matrix decides the shape of the answer, and the
	 * augmented one decides whether the equations contradict. Saying "no
	 * solution" for a consistent but underdetermined system is the mistake
	 * this split exists to avoid. */
	if (rank_a < n) {
		say(out, outlen, rank_w > rank_a
		                   ? "no solution: the equations contradict"
		                   : "infinitely many solutions: the system is singular");
		return;
	}

	for (int i = 0; i < n; i++) {
		char cell[48];
		int head = snprintf(cell, sizeof cell, "x%d = ", i + 1);
		size_t len;

		if (head < 0 || head >= (int)sizeof cell) {
			say(out, outlen, "value out of range for exact arithmetic");
			return;
		}
		fmt_rat(&w.v[i][n], cell + head, sizeof cell - (size_t)head);
		len = strlen(cell);
		if (used + len + 3 > outlen) {
			say(out, outlen, "value out of range for exact arithmetic");
			return;
		}
		if (i > 0) {
			memcpy(out + used, ", ", 2);
			used += 2;
		}
		memcpy(out + used, cell, len);
		used += len;
	}
	out[used] = '\0';
}

int matrix_typed_answer(const char *text, char *out, size_t outlen)
{
	const char *s = text;
	char name[8];
	int n = 0, rc;
	mat_t a, b;

	if (!text || !out || outlen < 2)
		return -1;

	while (is_space(*s))
		s++;
	while (*s && lower(*s) >= 'a' && lower(*s) <= 'z') {
		if (n >= (int)sizeof name - 1)
			return -1;
		name[n++] = lower(*s++);
	}
	name[n] = '\0';

	if (strcmp(name, "det") != 0 && strcmp(name, "inv") != 0 &&
	    strcmp(name, "rank") != 0 && strcmp(name, "solve") != 0)
		return -1;

	while (is_space(*s))
		s++;
	if (*s != '(')
		return -1;
	s++;
	while (is_space(*s))
		s++;

	rc = parse_matrix(&s, &a);
	if (rc != 0) {
		say(out, outlen, rc == -2 ? "matrices are supported up to 4x4"
		                          : "malformed matrix: use [1,2;3,4]");
		return 0;
	}

	if (!strcmp(name, "solve")) {
		while (is_space(*s))
			s++;
		if (*s != ',')
			return -1;
		s++;
		while (is_space(*s))
			s++;
		rc = parse_matrix(&s, &b);
		if (rc != 0) {
			say(out, outlen, rc == -2
			                     ? "matrices are supported up to 4x4"
			                     : "malformed right-hand side: use [5,10]");
			return 0;
		}
	}

	while (is_space(*s))
		s++;
	if (*s != ')')
		return -1;
	s++;
	while (is_space(*s))
		s++;
	if (*s != '\0')
		return -1;

	if (!strcmp(name, "det"))
		answer_det(&a, out, outlen);
	else if (!strcmp(name, "rank"))
		answer_rank(&a, out, outlen);
	else if (!strcmp(name, "inv"))
		answer_inv(&a, out, outlen);
	else
		answer_solve(&a, &b, out, outlen);

	return 0;
}
