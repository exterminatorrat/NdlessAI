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
 *   "CALL"                -> the relations, plus the one space that separates
 *                            the header from the relation name (run_call_text
 *                            splits fields on it, so "CALLohm_i" would be
 *                            rejected as an unknown word)
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

#endif
