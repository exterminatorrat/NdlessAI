Measures the int8 dot-product cost that dominates on-device transformer decoding
on a CX II (ARM926EJ-S, 396 MHz, no FPU, no NEON).

Timing is manual because `clock()` is stubbed in this SDK: `_times()` in
`libsyscalls/stdlib.cpp` returns 0, so every `clock()` call reads the same
value. Start a stopwatch at `Enter:`, stop it at `PHASE n DONE`, then

    tokens/s = 3 / seconds

because each rep is one token's worth of MACs (2 * 11000000).

Codegen, from `arm-none-eabi-gcc -mcpu=arm926ej-s -marm -O3`:

* `dot_i8_ref`: 5 instructions per MAC (`ldrsb`, `ldrsb`, `cmp`, `smlabb`, `bne`)
* `dot_i8x4_i16`: 21 instructions per 4 MACs (5.25 per MAC)

So packing int8 into words does not pay: ARMv5TE has no SIMD, and GCC already
contracts the multiply-accumulate to `SMLABB`. Expect about 5 cycles per MAC,
i.e. ~110M cycles for a 22M-MAC token: 0.28 s/token (3.6 tok/s) at 396 MHz on
battery, 0.38 s/token (2.6 tok/s) at 288 MHz while USB is attached. Speed has
to come from issuing fewer tokens and using fewer parameters, not from the
inner loop.

The kernels agree bit-exactly with the reference; that check runs first and
prints on screen.
