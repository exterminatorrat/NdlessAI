#include "bpe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_CASES 16384
#define MAX_TEXT 2048
#define MAX_IDS 512
#define LINE_BYTES 16384
#define TIMING_REPS 200

static char case_text[MAX_CASES][MAX_TEXT];
static char case_type[MAX_CASES][32];
static uint16_t case_expected[MAX_CASES][MAX_IDS];
static int case_len[MAX_CASES];
static int case_count;
static uint16_t got[MAX_IDS];

static int unescape(const char *src, char *dst, size_t dst_size)
{
    size_t n = 0;
    while (*src) {
        char c;
        if (src[0] == '\\' && src[1] != '\0') {
            if (src[1] == 'n') {
                c = '\n';
            } else if (src[1] == 't') {
                c = '\t';
            } else if (src[1] == '\\') {
                c = '\\';
            } else {
                return -1;
            }
            src += 2;
        } else {
            c = *src++;
        }
        if (n + 1 >= dst_size) {
            return -1;
        }
        dst[n++] = c;
    }
    dst[n] = '\0';
    return 0;
}

static int parse_ids(const char *s, uint16_t *ids, int max)
{
    int n = 0;
    while (*s) {
        while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') {
            s++;
        }
        if (*s == '\0') {
            break;
        }
        char *end;
        long v = strtol(s, &end, 10);
        if (end == s) {
            return -1;
        }
        if (n >= max) {
            return -1;
        }
        ids[n++] = (uint16_t)v;
        s = end;
    }
    return n;
}

static void trim_eol(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
}

int main(int argc, char **argv)
{
    const char *path = argc > 1 ? argv[1] : "fixtures/bpe_cases.txt";
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return 2;
    }

    static char line[LINE_BYTES];
    int have_text = 0;
    while (fgets(line, sizeof line, f)) {
        if (strlen(line) + 1 >= sizeof line) {
            fprintf(stderr, "fixture line too long\n");
            return 2;
        }
        trim_eol(line);
        if (line[0] == '#') {
            if (have_text || case_count >= MAX_CASES) {
                fprintf(stderr, "fixture type tag out of place at case %d\n", case_count);
                return 2;
            }
            char *t = line + 1;
            while (*t == ' ') {
                t++;
            }
            size_t tl = strlen(t);
            if (tl >= sizeof case_type[0]) {
                tl = sizeof case_type[0] - 1;
            }
            memcpy(case_type[case_count], t, tl);
            case_type[case_count][tl] = '\0';
        } else if (strncmp(line, "TEXT\t", 5) == 0) {
            if (have_text || case_count >= MAX_CASES) {
                fprintf(stderr, "fixture TEXT out of place at case %d\n", case_count);
                return 2;
            }
            if (unescape(line + 5, case_text[case_count], MAX_TEXT) != 0) {
                fprintf(stderr, "bad escape or text too long at case %d\n", case_count);
                return 2;
            }
            have_text = 1;
        } else if (strncmp(line, "IDS\t", 4) == 0) {
            if (!have_text || case_count >= MAX_CASES) {
                fprintf(stderr, "fixture IDS out of place at case %d\n", case_count);
                return 2;
            }
            int n = parse_ids(line + 4, case_expected[case_count], MAX_IDS);
            if (n < 0) {
                fprintf(stderr, "bad IDS line at case %d\n", case_count);
                return 2;
            }
            case_len[case_count] = n;
            case_count++;
            have_text = 0;
        } else if (line[0] != '\0') {
            fprintf(stderr, "unrecognized fixture line: %.20s\n", line);
            return 2;
        }
    }
    fclose(f);

    bpe_tok tok;
    if (bpe_init(&tok) != 0) {
        fprintf(stderr, "bpe_init failed\n");
        return 2;
    }

    int failures = 0;
    for (int c = 0; c < case_count; c++) {
        int n = bpe_encode(&tok, case_text[c], got, MAX_IDS);
        if (n != case_len[c]
            || (n > 0 && memcmp(got, case_expected[c], (size_t)n * sizeof(uint16_t)) != 0)) {
            failures++;
            if (failures <= 10) {
                fprintf(stderr, "FAIL case %d (%s) text \"%s\": got %d ids, want %d\n",
                        c, case_type[c], case_text[c], n, case_len[c]);
                int lim = n < case_len[c] ? n : case_len[c];
                for (int k = 0; k < lim; k++) {
                    if (got[k] != case_expected[c][k]) {
                        fprintf(stderr, "  first diff at %d: got %u want %u\n",
                                k, (unsigned)got[k], (unsigned)case_expected[c][k]);
                        break;
                    }
                }
            }
        }
    }
    printf("cases: %d, matched exactly: %d, failures: %d\n",
           case_count, case_count - failures, failures);

    int nq = 0;
    for (int c = 0; c < case_count; c++) {
        if (strcmp(case_type[c], "question") == 0) {
            nq++;
        }
    }
    for (int c = 0; c < case_count; c++) {
        bpe_encode(&tok, case_text[c], got, MAX_IDS);
    }
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int r = 0; r < TIMING_REPS; r++) {
        for (int c = 0; c < case_count; c++) {
            bpe_encode(&tok, case_text[c], got, MAX_IDS);
        }
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ns = (double)(t1.tv_sec - t0.tv_sec) * 1e9 + (double)(t1.tv_nsec - t0.tv_nsec);
    if (nq > 0) {
        printf("encode speed: %.3f us/question (%d questions, %d reps)\n",
               ns / ((double)nq * TIMING_REPS) / 1000.0, nq, TIMING_REPS);
    }
    printf("encode speed: %.3f us/case (all %d cases, %d reps)\n",
           ns / ((double)case_count * TIMING_REPS) / 1000.0, case_count, TIMING_REPS);

    return failures != 0 ? 1 : 0;
}
