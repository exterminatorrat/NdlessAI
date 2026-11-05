# Progress ledger

The single source of truth for where Tier 1 stands. Any agent resuming this
work reads this file first, then runs `sh stem-model/gate.sh` to confirm the
recorded state is real.

Update this file in the same commit that completes a section. Never mark a
section done without the gate passing.

| Section | Status | Commit | Notes |
|---|---|---|---|
| S0 infrastructure | done | | ledger, gate script, batch template |
| A1 sin_eval | done | | 200/200 C cases exact; cos/tan split out as A1b/A1c |
| A2 cos_eval | done | | 200/200 exact |
| A3 tan_eval | done | | 200/200 exact; 90 excluded (undefined) |
| A4 pythagoras | done | | 200/200 exact; uses sqrt12 |
| A5 law_sines | done | | 200/200 exact |
| A6 law_cosines | done | | 200/200 exact; sqrt at 1e6 scale |
| A7 arc_length | done | | 200/200 exact |
| A8 right_triangle | blocked | | needs atan; no helper exists |
| A9 sinusoid | pending | | multi-value output, deferred |
| A2 trig_inverse | pending | | |
| A3 pythagoras | pending | | |
| A4 right_triangle | pending | | |
| A5 law_sines | pending | | |
| A6 law_cosines | pending | | |
| A7 sinusoid | pending | | |
| A8 arc_length | pending | | |
| B1 log_eval | pending | | |
| B2 log_expand | pending | | |
| B3 change_base | pending | | |
| B4 compound_interest | pending | | |
| B5 continuous_growth | pending | | |
| B6 half_life | pending | | |
| B7 doubling_time | pending | | |
| C1 cubic_solve | pending | | |
| C2 factor_quadratic | pending | | |
| C3 complete_square | pending | | |
| C4 arith_seq | pending | | |
| C5 geom_seq | pending | | |
| C6 system3 | pending | | |
| C7 abs_value_eq | pending | | |
| C8 linear_ineq | pending | | |
| D1 kinetic_energy | pending | | |
| D2 potential_energy | pending | | |
| D3 work | pending | | |
| D4 power | pending | | |
| D5 momentum | pending | | |
| D6 newton2 | pending | | |
| D7 circular_accel | pending | | |
| D8 wave_speed | pending | | |
| D9 specific_heat | pending | | |
| D10 ohm_v | pending | | |
| D11 ohm_power | pending | | |
| D12 free_fall | pending | | |
| E1 molarity | pending | | |
| E2 ph | pending | | |
| E3 poh | pending | | |
| E4 ph_from_poh | pending | | |
| E5 percent_yield | pending | | |
| E6 boyles | pending | | |
| E7 charles | pending | | |
| E8 combined_gas | pending | | |
| E9 density | pending | | |
| E10 stoichiometry | pending | | |
| F1 unit_convert | pending | | |
| F2 sig_figs | pending | | |
| F3 percent_error | pending | | |
| F4 scientific_notation | pending | | |
| F5 average | pending | | |
| G1 retrain | pending | | after all batches |
| G2 hardware test | pending | | one question per batch |

Status values: `pending`, `in-progress`, `blocked`, `done`.

## Resuming after context loss

1. Read this file.
2. Run `sh stem-model/gate.sh`. It prints PASS or FAIL per gate.
3. If a section is marked `done` but its gate fails, the record is wrong:
   set it back to `pending` and fix it before continuing.
4. Take the first `pending` section and follow its entry in
   `TIER1_SECTIONS.md`.

## Why a ledger

This project lost time three times to artifacts that were assumed current but
were not: a checkpoint paired with the wrong tokenizer, a `bpe.h` that `make`
never rebuilt, and a `.tns` that was never replaced on the calculator. A
written record plus a gate command makes "assumed done" impossible to confuse
with "verified done".
