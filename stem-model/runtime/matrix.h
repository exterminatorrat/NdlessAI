#ifndef MATRIX_H
#define MATRIX_H

#include <stddef.h>

/*
 * Typed linear algebra: determinant, inverse, rank and solving a square
 * system, answered exactly from the text the student types.
 *
 * This is the typed path, so it needs no weights, no vocabulary and no
 * inference: it costs microseconds and works on a calculator with no model
 * file installed. Everything is exact rational arithmetic, so a determinant
 * that is 1/3 prints as 1/3 rather than 0.3333, and a singular matrix is
 * reported as singular instead of being inverted to garbage.
 *
 * The forms are:
 *
 *   det([1,2;3,4])          determinant of a square matrix
 *   inv([2,1;1,3])          inverse, or a refusal when there is none
 *   rank([1,2;2,4])         rank
 *   solve([2,1;1,3],[5,10]) solution vector of a square system
 *
 * Rows are separated by ';', entries by ','. The cap is 4x4: every extra
 * row doubles the sheet of intermediate rationals, and the systems that show
 * up in this material are 2x2, 3x3 and the occasional 4x4. Anything larger
 * is refused by name rather than silently truncated.
 */
int matrix_typed_answer(const char *text, char *out, size_t outlen);

#endif
