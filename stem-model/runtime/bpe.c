#define BPE_DATA_IMPL
#include "bpe.h"

#include <string.h>

#define BPE_KEY_PAIR(a, b) \
    ((((uint64_t)(a)) << BPE_KEY_ID_BITS) | (uint64_t)(b))

static int bpe_rank_of(const bpe_tok *tok, uint16_t a, uint16_t b)
{
    uint64_t key = BPE_KEY_PAIR(a, b);
    int lo = 0;
    int hi = BPE_NUM_MERGES - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        uint64_t cur = tok->keys[mid] >> BPE_KEY_ID_BITS;
        if (cur < key) {
            lo = mid + 1;
        } else if (cur > key) {
            hi = mid - 1;
        } else {
            return (int)(tok->keys[mid] & BPE_KEY_ID_MAX);
        }
    }
    return -1;
}

int bpe_init(bpe_tok *tok)
{
    /* The key carries 12 bits per id and 12 for the rank; refuse to build a
     * table the encoding cannot represent rather than pack it silently wrong. */
    if (BPE_NUM_MERGES > (BPE_KEY_ID_MAX + 1) ||
        BPE_VOCAB_SIZE > (BPE_KEY_ID_MAX + 1)) {
        return -1;
    }
    int n = 0;
    for (int i = 0; i < BPE_NUM_MERGES; i++) {
        uint64_t entry = BPE_KEY_PAIR(bpe_merges[i][0], bpe_merges[i][1]) << BPE_KEY_ID_BITS
                         | (uint64_t)i;
        int lo = 0;
        int hi = n;
        while (lo < hi) {
            int mid = lo + (hi - lo) / 2;
            if (tok->keys[mid] < entry) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        memmove(&tok->keys[lo + 1], &tok->keys[lo], (size_t)(n - lo) * sizeof(tok->keys[0]));
        tok->keys[lo] = entry;
        n++;
    }
    return 0;
}

static int bpe_special_at(const char *s)
{
    for (int i = 0; i < BPE_NUM_SPECIALS; i++) {
        if (strncmp(s, bpe_specials[i], strlen(bpe_specials[i])) == 0) {
            return i;
        }
    }
    return -1;
}

int bpe_encode(const bpe_tok *tok, const char *text, uint16_t *out_ids, int max_ids)
{
    int n = 0;
    size_t i = 0;
    while (text[i] != '\0') {
        int sp = bpe_special_at(text + i);
        if (sp >= 0) {
            if (n >= max_ids) {
                return -1;
            }
            out_ids[n++] = bpe_special_ids[sp];
            i += strlen(bpe_specials[sp]);
            continue;
        }
        size_t j = i;
        while (text[j] != '\0' && bpe_special_at(text + j) < 0) {
            j++;
        }
        if (n + (int)(j - i) > max_ids) {
            return -1;
        }
        int start = n;
        for (size_t k = i; k < j; k++) {
            out_ids[n++] = (uint16_t)(unsigned char)text[k];
        }
        int len = (int)(j - i);
        uint16_t *chunk = out_ids + start;
        while (len > 1) {
            int best_rank = -1;
            int best_pos = -1;
            for (int p = 0; p + 1 < len; p++) {
                int r = bpe_rank_of(tok, chunk[p], chunk[p + 1]);
                if (r >= 0 && (best_rank < 0 || r < best_rank)) {
                    best_rank = r;
                    best_pos = p;
                }
            }
            if (best_pos < 0) {
                break;
            }
            chunk[best_pos] = (uint16_t)(BPE_FIRST_MERGE + best_rank);
            if (best_pos + 2 < len) {
                memmove(chunk + best_pos + 1, chunk + best_pos + 2,
                        (size_t)(len - best_pos - 2) * sizeof(uint16_t));
            }
            len--;
        }
        n = start + len;
        i = j;
    }
    return n;
}

int bpe_token_bytes(const bpe_tok *tok, uint16_t id, char *out, int max)
{
    int n = 0;

    (void)tok;
    if (id < 256) {
        if (max < 2)
            return -1;
        out[0] = (char)id;
        out[1] = '\0';
        return 1;
    }
    for (int i = 0; i < BPE_NUM_SPECIALS; i++) {
        if (bpe_special_ids[i] == id) {
            size_t len = strlen(bpe_specials[i]);
            if ((int)len + 1 > max)
                return -1;
            memcpy(out, bpe_specials[i], len + 1);
            return (int)len;
        }
    }
    if (id >= BPE_FIRST_MERGE && id < BPE_FIRST_MERGE + BPE_NUM_MERGES) {
        const uint16_t *pair = bpe_merges[id - BPE_FIRST_MERGE];
        for (int k = 0; k < 2; k++) {
            char part[64];
            int got = bpe_token_bytes(tok, pair[k], part, (int)sizeof part);
            if (got < 0 || n + got + 1 > max)
                return -1;
            memcpy(out + n, part, (size_t)got);
            n += got;
        }
        out[n] = '\0';
        return n;
    }
    return -1;
}
