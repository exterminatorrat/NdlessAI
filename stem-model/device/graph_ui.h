#ifndef GRAPH_UI_H
#define GRAPH_UI_H

/*
 * Runs the graphing screen until the user presses ESC. Prompts for the
 * function, so it needs no model and no weights file.
 */
int graph_run(void);

/*
 * Plots the curve behind an answer: the same screen, but opened on the call
 * the answer came from instead of on a typed function, so 4.6a is reachable
 * from the answer the student is looking at.
 */
int graph_run_call(const char *call);

#endif
