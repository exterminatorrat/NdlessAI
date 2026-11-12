#ifndef PLAN_H
#define PLAN_H

/*
 * A plan: several calls, each able to use the number the step before produced.
 *
 *     CALL newton2 2 9.8 ; CALL friction_force 0.2 <1>
 *
 * The first step answers with the weight, 19.6 N; <1> is that number, so the
 * second answers with the friction force, 3.92 N. That is the shape a hard
 * question has — "a 2 kg block on a 30 degree ramp with mu = 0.2" — and it is
 * what one call per question could not express.
 *
 * The rules are deliberately narrow, because a plan that half-runs is worse
 * than a plan that is refused:
 *
 *  - At most PLAN_MAX_STEPS steps, separated by ';', each written exactly as
 *    the answer pane already prints a call: "CALL name arg arg ...".
 *  - An argument is a number or <n>, which is the magnitude of step n's
 *    answer. Only earlier steps can be referenced, so a plan cannot loop.
 *  - Every step must answer with a single value. "x=0, x=3" is two numbers, so
 *    it cannot feed the next step and the plan is refused at that step.
 *  - Units ride along with the magnitude for display, but only the magnitude
 *    is passed: unit algebra is a later slice, and pretending otherwise would
 *    let 5 N flow into a slot meaning 5 kg.
 */

#define PLAN_MAX_STEPS 5

typedef struct {
	/* The step's own copies: a plan is executed once and rendered after, so
	 * nothing in here may point into a scratch buffer. */
	char name[32];
	char args[8][24];
	int nargs;
	char answer[64];
	char magnitude[24];
} plan_step_t;

typedef struct {
	int n;
	plan_step_t step[PLAN_MAX_STEPS];
	char err[64];
} plan_t;

/*
 * Runs `text` as a plan. Returns 0 on success, -1 when the text is not a plan
 * (so the caller can treat it as something else), and -2 when it is a plan
 * that failed, with the reason in p->err and p->n set to the step that failed.
 */
int plan_execute(const char *text, plan_t *p);

/*
 * The working for a plan that ran: each step's formula with its numbers, then
 * its answer, headed by the step number. Returns 1 when something was written,
 * 0 when a step carries no template, -1 when it did not fit.
 */
int plan_render(const plan_t *p, char *out, int outlen);

/* Whether `text` looks like a plan at all, cheaply: it has a step separator. */
int plan_is(const char *text);

/* The call for step n (1-based) of a plan in text form, or NULL. */
const char *plan_step_text(const char *text, int n, char *out, int outlen);

#endif
