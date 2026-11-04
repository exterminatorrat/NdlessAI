---
name: pstack
description: "Apply pstack's architecture-first engineering discipline to every task: ground the system, model the domain, sketch types and boundaries before code, exhaust the design space, subtract before adding, and verify the real artifact. Use for architecture, refactors, module or API design, migrations, debugging, and any non-trivial change. UI aesthetics and visual design are out of scope."
---

# pstack for architecture

Adapted from [pstack](https://github.com/cursor/plugins/tree/main/pstack) by Lauren Tan (MIT; see `LICENSE`). This repository adopts the architecture and engineering-rigor parts. UI aesthetics, visual parity, copy polish, and bot-UI work are out of scope.

## Apply this to every task

Every engineering task enters through **Ground** and leaves through **Verify**. Non-trivial work also passes through **Sketch**; skip it only when the shape is obvious and local.

1. **Ground** — understand the systems the change touches.
2. **Sketch** — write usage and types before implementation.
3. **Agree** — optional human checkpoint; default is to proceed.
4. **Implement** — fill in the sketch without silently changing its contract.
5. **Scrap** — replace the design when friction exposes a wrong shape.
6. **Verify** — prove the real artifact works.

Open a todo list with one entry per phase before starting non-trivial work.

## Phase 1: Ground

Build a real mental model of every system the change touches. Naming a file is not grounding. Read the callers, the types at the boundary, and the invariants the new code must preserve.

- For runtime behavior, ownership, and layering, follow [`references/how.md`](references/how.md).
- For why the existing shape exists, follow [`references/why.md`](references/why.md). Treat historical rationale as a constraint, not a guess.
- Skip grounding only for genuinely greenfield work with no surrounding system.

## Phase 2: Sketch

Write the caller's usage first, then derive the types from it. The usage is the spec; reconcile the sketch to the usage, not the reverse.

1. Show the import, the call sites, and the returned data as a consumer would write them.
2. Define the core types and data structures before logic, per [`references/principles/foundational-thinking.md`](references/principles/foundational-thinking.md).
3. Draw the module boundaries and function signatures with `not implemented` bodies. State which invariants live in types, where validation happens, and what the design deliberately does not do.
4. Produce at least two structurally distinct candidate shapes for novel or cross-cutting work. Compare them on interface depth, not implementation convenience.
5. Screen every candidate against [`references/design-red-flags.md`](references/design-red-flags.md) before choosing.
6. Record the decision with [`references/rationale-template.md`](references/rationale-template.md).

Prefer the design where a change that looks right from one file is right for the whole repository. Assume the next contributor is an agent that reads only the files it opens and copies the nearest example.

When parallel candidate generation is available, follow the arena workflow in [`references/arena.md`](references/arena.md). Otherwise write the candidates yourself and compare them explicitly.

## Phase 3: Agree

Proceed directly to implementation by default. Pause for sign-off only when the invoker asks for a checkpoint or the change is irreversible, migration-heavy, or redefines ownership.

## Phase 4: Implement

Replace `not implemented` bodies with code and pseudocode with logic. The sketch is the contract. Surface every deviation instead of absorbing it silently. If a signature needs something the sketch did not anticipate, ask whether the sketch was wrong, the requirement was missed, or the implementation is overreaching.

## Phase 5: Scrap when the architecture is wrong

If implementation keeps producing friction the sketch cannot absorb, throw the sketch out. The signal is a pattern, not a single edge case:

- the same workaround appears across unrelated code;
- unrelated edge cases all need special-case branches;
- types need `any`, casts, or always-set optional fields to compile;
- callers must know the abstraction's internal rules;
- two or more deviations share the same shape.

Re-ground, redesign as if the new constraints had been day-one assumptions, and make the new sketch smaller than the old one before it grows. Follow [`references/principles/redesign-from-first-principles.md`](references/principles/redesign-from-first-principles.md) and [`references/principles/subtract-before-you-add.md`](references/principles/subtract-before-you-add.md).

## Phase 6: Verify

Verify the real artifact directly, preferably with a deterministic, rerunnable check. Do not substitute reading the diff for running the thing. Follow [`references/principles/prove-it-works.md`](references/principles/prove-it-works.md). If verification is impossible, say exactly what blocked it.

For adversarial review of a design or change, follow [`references/interrogate.md`](references/interrogate.md).

## Principles

Load the principle that matches the situation; do not treat the list as a checklist to recite.

**Architecture**

- [Model the domain](references/principles/model-the-domain.md) — encode the real domain instead of scattering it across conditionals.
- [Boundary discipline](references/principles/boundary-discipline.md) — validate at system boundaries; keep interiors typed and pure.
- [Type-system discipline](references/principles/type-system-discipline.md) — make invalid states unrepresentable.
- [Make operations idempotent](references/principles/make-operations-idempotent.md) — decide what happens on retry or partial failure.
- [Migrate callers, then delete legacy APIs](references/principles/migrate-callers-then-delete-legacy-apis.md) — leave one way to do each task.
- [Separate before serializing shared state](references/principles/separate-before-serializing-shared-state.md) — give each piece of state one owner.

**Foundations**

- [Foundational thinking](references/principles/foundational-thinking.md) — types and data structures before logic.
- [Subtract before you add](references/principles/subtract-before-you-add.md) — remove complexity before building.
- [Redesign from first principles](references/principles/redesign-from-first-principles.md) — propagate new constraints through the design.
- [Exhaust the design space](references/principles/exhaust-the-design-space.md) — compare whole-shape alternatives.
- [Attack the premise](references/principles/attack-the-premise.md) — when repeated fixes fail, test the shared assumption.
- [Outcome-oriented execution](references/principles/outcome-oriented-execution.md) — optimize for the verified end state.
- [Experience first](references/principles/experience-first.md) — design from the consumer's experience.
- [Build the lever](references/principles/build-the-lever.md) — invest in the capability that makes the rest cheap.
- [Laziness protocol](references/principles/laziness-protocol.md) — write less code where less code is correct.
- [Minimize reader load](references/principles/minimize-reader-load.md) — make the next reader's job small.
- [Encode lessons in structure](references/principles/encode-lessons-in-structure.md) — make the codebase prevent repeated mistakes.

**Verification**

- [Prove it works](references/principles/prove-it-works.md) — verify the actual artifact.
- [Fix root causes](references/principles/fix-root-causes.md) — fix the first wrong decision, not the symptom.
- [Sequence verifiable units](references/principles/sequence-verifiable-units.md) — land independently checkable increments.
- [Test behavior, not implementation](references/principles/test-behavior-not-implementation.md) — assert contracts, not internals.
- [Explain the number](references/principles/explain-the-number.md) — justify every performance or budget claim.

## Source and adaptation notes

- Upstream: <https://github.com/cursor/plugins/tree/main/pstack>, MIT license, copyright © 2026 Lauren Tan.
- Upstream references to `pstack-models.mdc` and named runner models describe pstack's Cursor/Claude harness. Use the models and subagents available in this environment instead.
- This repository includes the architecture, foundation, and verification principles. UI and presentation skills from pstack are intentionally excluded.
