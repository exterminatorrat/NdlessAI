Measures the int8 dot-product cost that dominates on-device transformer decoding
on a CX II (ARM926EJ-S, 396 MHz, no FPU, no NEON).

Timing is manual because `clock()` is stubbed in this SDK: `_times()` in
`libsyscalls/stdlib.cpp` returns 0, so every `clock()` call reads the same
value. Start a stopwatch at `Enter:`, stop it at `PHASE n DONE`, then

because each rep is one token's worth of MACs (11000000, one pass over the
weights; a batch-1 decode costs about one MAC per parameter, plus attention
which is negligible at the short contexts this model uses).

Codegen, from `arm-none-eabi-gcc -mcpu=arm926ej-s -marm -O3`:

* `dot_i8_ref`: 5 instructions per MAC (`ldrsb`, `ldrsb`, `cmp`, `smlabb`, `bne`)
* `dot_i8x4_i16`: 21 instructions per 4 MACs (5.25 per MAC)

So packing int8 into words does not pay: ARMv5TE has no SIMD, and GCC already
contracts the multiply-accumulate to `SMLABB`. At about 5 cycles per MAC, a
token costs roughly `5 * params` cycles. For 5.46M parameters that is 27M
cycles: 190 ms/token (5.3 tok/s) at a measured 144 MHz, or 138 ms/token
(7.3 tok/s) at 198 MHz. Speed has to come from issuing fewer tokens and using
fewer parameters, not from the inner loop.

The CPU clock is disputed: TI documentation and CoreMark results say 396 MHz,
while `docs/HARDWARE.md` in the nspire-tlm project reports 198 MHz on a first
run and 144 MHz on later ones. This benchmark exists to settle that on the unit
in hand, so run it before fixing a model size.

The kernels agree bit-exactly with the reference; that check runs first and
prints on screen.
