/*
 * End-to-end pipeline as it will run on the calculator:
 *
 *   question text -> C BPE -> fixed-point forward -> conversational decode
 *                 -> call string -> C solvers -> answer text
 *
 * Reads one question per line on stdin and prints the answer for each.
 * Everything below the file loading is the device path: no float, no libm
 * beyond what the on-device port needs, no dynamic allocation inside a token.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bpe.h"
#include "fixed.h"
#include "infer.h"
#include "solvers.h"

#define PROMPT_SUFFIX " <sep>"
#define MAX_GEN 20

static int argmax32(const int32_t *v, int n)
{
    int best = 0;

    for (int i = 1; i < n; i++)
        if (v[i] > v[best])
            best = i;
    return best;
}

static int ids_to_text(const bpe_tok *tok, const uint16_t *ids, int n,
                       char *out, int max)
{
    int used = 0;

    out[0] = '\0';
    for (int i = 0; i < n; i++) {
        char part[64];
        int got = bpe_token_bytes(tok, ids[i], part, (int)sizeof part);

        if (got < 0 || used + got + 1 > max)
            return -1;
        memcpy(out + used, part, (size_t)got);
        used += got;
    }
    out[used] = '\0';
    return used;
}

static int run_call_text(const char *call, char *answer, int answer_max)
{
    char work[256];
    char *parts[10];
    int n = 0;
    const char *args[8];
    size_t len = strlen(call);

    if (len + 1 > sizeof work)
        return -1;
    memcpy(work, call, len + 1);

    for (size_t i = 0; i < len && n < 10;) {
        while (i < len && work[i] == ' ')
            work[i++] = '\0';
        if (i >= len)
            break;
        parts[n++] = work + i;
        while (i < len && work[i] != ' ')
            i++;
    }
    if (n < 2 || strcmp(parts[0], "CALL") != 0)
        return -1;
    if (n - 2 > 8)
        return -1;
    for (int i = 0; i < n - 2; i++)
        args[i] = parts[i + 2];
    return solvers_solve(parts[1], args, n - 2, answer, answer_max);
}

/*
 * Every argument must be a number that appears in the question. The model's
 * dominant failure on unseen wording is copying a plausible but wrong number,
 * and those show up as arguments with no counterpart in the text. Refusing is
 * better than answering: a wrong number is indistinguishable from a right one.
 */
static int args_are_grounded(const char *call, const char *question)
{
    char work[256];
    size_t len = strlen(call), i = 0;
    int word = 0;

    if (len + 1 > sizeof work)
        return 0;
    memcpy(work, call, len + 1);

    while (i < len) {
        char *start;

        while (i < len && work[i] == ' ')
            work[i++] = '\0';
        if (i >= len)
            break;
        start = work + i;
        while (i < len && work[i] != ' ')
            i++;
        if (word >= 2 && !strstr(question, start))
            return 0;
        word++;
    }
    return 1;
}

int main(int argc, char **argv)
{
    static model_t m;
    static bpe_tok tok;
    static qstate_t qs;
    qwork_t qw;
    int32_t *logits;
    uint16_t prompt[256];
    int rc = 1;
    char line[512];
    char call[256];
    char answer[256];

    if (argc < 3) {
        fprintf(stderr, "usage: %s weights.bin <seq>\n", argv[0]);
        return 2;
    }
    memset(&qw, 0, sizeof qw);
    if (model_load(argv[1], &m) < 0) {
        fprintf(stderr, "cannot load %s\n", argv[1]);
        return 1;
    }
    m.seq = atoi(argv[2]);
    if (m.seq < 8 || m.seq > 512) {
        fprintf(stderr, "seq must be 8..512\n");
        return 1;
    }
    if (bpe_init(&tok) < 0) {
        fprintf(stderr, "bpe_init failed\n");
        return 1;
    }
    if (qstate_init(&qs, &m, m.seq) < 0 || qwork_alloc(&qw, &m) < 0) {
        fprintf(stderr, "fixed-point buffers failed\n");
        return 1;
    }
    logits = malloc((size_t)m.vocab * sizeof *logits);
    if (!logits) {
        fprintf(stderr, "out of memory\n");
        return 1;
    }

    while (fgets(line, sizeof line, stdin)) {
        size_t len = strlen(line);
        int n = 0, pos = 0, gen = 0;

        while (len && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (!len)
            continue;
        if (strlen(line) + strlen(PROMPT_SUFFIX) + 1 > sizeof line - 1) {
            printf("!input_too_long\n");
            continue;
        }
        strcat(line, PROMPT_SUFFIX);
        n = bpe_encode(&tok, line, prompt, (int)(sizeof prompt / sizeof prompt[0]));
        if (n <= 0 || n >= m.seq) {
            printf("!encode_failed\n");
            continue;
        }

        for (int i = 0; i < n; i++) {
            if (forward_q(&m, &qs, &qw, prompt[i], pos++, logits) < 0) {
                printf("!forward_failed\n");
                goto next;
            }
        }
        for (int i = 0; i < MAX_GEN && pos < m.seq; i++) {
            int next = argmax32(logits, m.vocab);

            if (next == 258 || next == 256)
                break;
            if (n + gen + 1 < (int)(sizeof prompt / sizeof prompt[0]))
                prompt[n + gen++] = (uint16_t)next;
            else
                break;
            if (forward_q(&m, &qs, &qw, next, pos++, logits) < 0)
                break;
        }

        if (ids_to_text(&tok, prompt + n, gen, call, (int)sizeof call) < 0) {
            printf("!decode_failed\n");
            continue;
        }
        if (getenv("NSM_IDS")) {
            fprintf(stderr, "ids:");
            for (int i = 0; i < gen; i++)
                fprintf(stderr, " %u", prompt[n + i]);
            fprintf(stderr, "  text: %s\n", call);
        }
        if (run_call_text(call, answer, (int)sizeof answer) < 0) {
            printf("bad\t%s\n", call);
            continue;
        }
        if (!getenv("NSM_NO_GUARD") && !args_are_grounded(call, line)) {
            printf("!uncertain\t%s\n", call);
            continue;
        }
        printf("%s\t%s\n", answer, call);
next:
        continue;
    }

    if (getenv("NSM_IDS"))
        fprintf(stderr, "saturations %lu\n", (unsigned long)fixed_sat_count);
    free(logits);
    qwork_free(&qw);
    qstate_free(&qs);
    model_free(&m);
    return rc;
}
