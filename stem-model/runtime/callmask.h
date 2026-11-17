#ifndef CALLMASK_H
#define CALLMASK_H

/*
 * Constrained decoding for the call header.
 *
 * Without this the model may emit any token at any step and the runtime only
 * discovers a malformed call afterwards, when it tries to parse it. The mask
 * makes the malformed header impossible instead of merely rejected: it fills
 * one allow byte per vocabulary entry for the token that comes next, given
 * the call text decoded so far.
 *
 *   "" or spaces only     -> CALL alone
 *   "CALL"                -> the separator byte alone, so the header cannot
 *                            fuse onto the relation name: run_call_text splits
 *                            a call on spaces and would read "CALLquad_solve"
 *                            as one unknown field
 *   "CALL" plus spaces    -> the relations
 *   anything longer       -> everything except CALL and the relations, so the
 *                            model cannot restart the header or swap in a
 *                            second relation mid-call
 *
 * A whitelist for the first two steps, a blacklist afterwards. Bytes, digits
 * and separators are never removed by the later steps.
 *
 * Deliberately not attempted: limiting the argument count, and deciding when
 * a number is finished. A number's end is not a token boundary the runtime
 * can see, so forcing that would truncate valid values.
 */
void callmask_next(const char *sofar, unsigned char *allow, int vocab);

#include "bpe.h"

/*
 * Constrained decoding for the arguments: a numeric slot may only be filled
 * from a number the question writes, and a number may be spent once.
 *
 * The first decode is left alone. This mask runs on the retry that a call
 * failing the grounding check earns, so it can only ever replace a refusal with
 * an answer, never take an answer away.
 *
 * Reading order is a consumption rule, not a sequence rule. A span is spent
 * left to right when the same value appears twice in the text, but the slots
 * are not required to follow the sentence: on the hand-written split 693 of the
 * 2,150 expected calls put the earlier number in the later slot ("44 g/mol ...
 * 88 g" is moles 88 44), so forcing the slot order onto the reading order would
 * forbid the correct answer for a third of the corpus. The measurement is in
 * repair.h's neighbour, test_callmask.c.
 *
 * 0 and 1 are always allowed and never spend a span: polynomial questions write
 * coefficients implicitly, which is the same allowance grounding makes.
 */
void callmask_args(const char *sofar, const char *question, const bpe_tok *tok,
                   unsigned char *allow, int vocab);

#endif
