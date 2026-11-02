import numpy as np

GROUP = 32
SCALE_BITS = 24


def quantize(w, group=GROUP):
    rows, cols = w.shape
    assert cols % group == 0, f"cols {cols} not divisible by group {group}"
    q = np.zeros_like(w, dtype=np.int8)
    scales_q = np.zeros((rows, cols // group), dtype=np.int32)
    for c0 in range(0, cols, group):
        block = w[:, c0:c0 + group]
        scale = np.abs(block).max(axis=1) / 127.0
        scale[scale == 0] = 1e-8
        q[:, c0:c0 + group] = np.clip(np.round(block / scale[:, None]), -127, 127)
        scales_q[:, c0 // group] = np.round(scale * (1 << SCALE_BITS))
    return q, scales_q


def dequantize(q, scales_q, group=GROUP):
    rows, cols = q.shape
    out = np.zeros((rows, cols), dtype=np.float64)
    for c0 in range(0, cols, group):
        scale = scales_q[:, c0 // group].astype(np.float64) / (1 << SCALE_BITS)
        out[:, c0:c0 + group] = q[:, c0:c0 + group] * scale[:, None]
    return out
