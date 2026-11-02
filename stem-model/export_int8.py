import argparse

import numpy as np

GROUP = 32
SCALE_BITS = 16


def quantize(w, group=GROUP):
    rows, cols = w.shape
    assert cols % group == 0, f"cols {cols} not divisible by group {group}"
    q = np.zeros_like(w, dtype=np.int8)
    scales_q16 = np.zeros((rows, cols // group), dtype=np.int32)
    for c0 in range(0, cols, group):
        block = w[:, c0:c0 + group]
        scale = np.abs(block).max(axis=1) / 127.0
        scale[scale == 0] = 1e-8
        q[:, c0:c0 + group] = np.clip(np.round(block / scale[:, None]), -127, 127)
        scales_q16[:, c0 // group] = np.round(scale * (1 << SCALE_BITS))
    return q, scales_q16


def dequantize(q, scales_q16, group=GROUP):
    rows, cols = q.shape
    out = np.zeros((rows, cols), dtype=np.float64)
    for c0 in range(0, cols, group):
        scale = scales_q16[:, c0 // group].astype(np.float64) / (1 << SCALE_BITS)
        out[:, c0:c0 + group] = q[:, c0:c0 + group] * scale[:, None]
    return out


def pack_words(q):
    flat = q.reshape(-1)
    pad = (-len(flat)) % 4
    flat = np.concatenate([flat, np.zeros(pad, dtype=np.int8)])
    return np.frombuffer(flat.tobytes(), dtype="<u4")


def emit(path, q, scales_q16, cols):
    words = pack_words(q)

    def arr(name, ctype, values):
        body = ",".join(str(int(v)) for v in values)
        return f"static const {ctype} {name}[] = {{{body}}};\n"

    with open(path, "w") as f:
        f.write(f"#define W_COLS {cols}\n")
        f.write(f"#define W_GROUP {GROUP}\n")
        f.write(f"#define W_SCALE_BITS {SCALE_BITS}\n")
        f.write(arr("w_words", "uint32_t", words))
        f.write(arr("w_scales_q16", "int32_t", scales_q16.reshape(-1)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rows", type=int, default=64)
    ap.add_argument("--cols", type=int, default=352)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--out", default="weights.h")
    args = ap.parse_args()

    rng = np.random.default_rng(args.seed)
    w = rng.normal(0, 0.02, size=(args.rows, args.cols))

    q, scales_q16 = quantize(w)
    w_hat = dequantize(q, scales_q16)
    err = np.abs(w_hat - w).max()
    print(f"{args.rows}x{args.cols} int8, group {GROUP}: max abs err {err:.2e}")

    a = rng.normal(0, 1, size=(args.cols,))
    ref = w @ a
    got = w_hat @ a
    rel = np.linalg.norm(got - ref) / np.linalg.norm(ref)
    print(f"dot-product relative error over {args.rows} rows: {rel:.3%}")

    emit(args.out, q, scales_q16, args.cols)
    print(f"wrote {args.out} ({len(pack_words(q)) * 4 + scales_q16.size * 4} bytes)")


if __name__ == "__main__":
    main()
