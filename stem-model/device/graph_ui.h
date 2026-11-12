#ifndef GRAPH_UI_H
#define GRAPH_UI_H

/*
 * Runs the plotting screen until the student presses ESC, then gives the
 * display back. Both entries read the same way from the caller: the graph
 * screen hands over the function it typed, an answer hands over the call it
 * came from. Neither prompts for anything, so no message box ever appears over
 * a screenshot the student was looking at.
 */
int graph_run_text(const char *text);
int graph_run_call(const char *call);

#endif
