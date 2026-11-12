#ifndef REPAIR_H
#define REPAIR_H

/*
 * Snapping a decoded number back to the number the question actually wrote.
 *
 * The dominant failure on hand-written wording is not the wrong relation, it is
 * the right relation with a digit wrong: "88 g of CO2 at 44 g/mol" comes back
 * as 84 and 48, "0.1 mol in 0.25 L" as 0.2. The tokens are near-misses of a
 * number that is right there in the sentence.
 *
 * So the repair is deliberately timid. An argument is replaced only when it is
 * not in the text at all, when exactly one number in the text has the same
 * digit length and differs by exactly one character, and when no other
 * candidate is that close. Anything else is left for the caller's refusal to
 * handle, because a plausible number invented here would be indistinguishable
 * from a right one — the exact failure the argument grounding rule exists to
 * prevent.
 *
 * Two further refusals came out of measuring the corpus rather than reasoning
 * about it. A second ungrounded argument in the same call is not a keystroke,
 * it is a decode that went wrong, so two repairs are refused together instead
 * of stacking two guesses. And a repair may not repeat a number the call
 * already carries: "pythagoras 2 7" is one character from "7 7" and from
 * nothing else, which is why the rule needs the second half.
 */

/*
 * Rewrites every ungrounded argument of `call` to the one number in
 * `question` it can only have meant. Returns the number of arguments repaired;
 * `out` always holds the call, repaired or not.
 */
int repair_call(const char *call, const char *question, char *out, int outlen);

/*
 * The single number in `question` that `arg` can only have meant, written into
 * out. Returns 1 on a repair, 0 when nothing is close enough or two candidates
 * are, and -1 on bad input.
 */
int repair_argument(const char *arg, const char *question, char *out, int outlen);

#endif
