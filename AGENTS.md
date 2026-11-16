# Repository agent instructions

Read `.agents/skills/pstack/SKILL.md` before starting any engineering task in this repository, and apply it as the default workflow: ground the system, sketch types and boundaries before code, subtract before adding, and verify the real artifact.

The skill covers architecture and engineering rigor only. It does not govern UI aesthetics, visual design, or copy polish.

## Working order

Implement the requested change first. Land it and verify it with the nearest check that covers it before starting any build, packaging, release, toolchain, refactor, or cleanup work the request did not ask for. A full build or an unrelated fix is a separate task, not a prerequisite, unless the change cannot be verified without it.

Keep the change as small as the request: no speculative features, no new dependencies, no drive-by cleanup. Commit it and open one focused pull request so the work stays visible, and stack a real dependency instead of batching unrelated work into one.

## Plan and delivery

`docs/waves.md` is the plan of record: wave order, the workstreams inside each
wave, which files each one owns, and what "done" means. Read it before starting
work and update its status table when a workstream changes state.

- **One pull request per concern, under 1,000 changed lines.** Split or stack
  anything larger. A change that needs three rounds of fix-and-rerun was too
  big, not unlucky.
- **Parallel agents own distinct files.** The ownership matrix in
  `docs/waves.md` exists so two agents never edit one file in the same wave.
  Dependencies stack: the blocked branch names the branch it needs as its base.
- **Verify before claiming.** Every pull request body states the command it was
  checked with and the output. A check that could not run is reported as not
  run, with the reason, never implied to have passed.
- **Report in outcomes.** Close each workstream with what a student can now do
  that they could not before, and which number moved. Module names are not a
  report.

## Backlog

The capability catalogue in `docs/capabilities.md` is the full list, including
the entries recorded as not worth doing; `docs/status.md` records what is already
built. Read both before claiming something is missing.

Every entry that is startable is filed as an issue in the development tracker,
labelled `capability`. `area:model`, `area:math`, `area:interface` and
`area:subjects` say which surface a piece touches; `needs-retrain` marks the ones
that cannot be a source-only pull request, because a relation or vocabulary
change renumbers the tokenizer and forces a rebuild; `good first issue` marks the
ones that fit one pull request. Take the issue before you start so two agents do
not write the same capability. Outside that tracker, name the `docs/capabilities.md`
entry you are taking in the pull request instead.

Do not copy the catalogue into this file or into the tracker: the issues and the
docs are the two copies, and a third one rots silently.
