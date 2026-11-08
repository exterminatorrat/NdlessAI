# Repository agent instructions

Read `.agents/skills/pstack/SKILL.md` before starting any engineering task in this repository, and apply it as the default workflow: ground the system, sketch types and boundaries before code, subtract before adding, and verify the real artifact.

The skill covers architecture and engineering rigor only. It does not govern UI aesthetics, visual design, or copy polish.

## Working order

Implement the requested change first. Land it and verify it with the nearest check that covers it before starting any build, packaging, release, toolchain, refactor, or cleanup work the request did not ask for. A full build or an unrelated fix is a separate task, not a prerequisite, unless the change cannot be verified without it.

Keep the change as small as the request: no speculative features, no new dependencies, no drive-by cleanup. Commit it and open one focused pull request so the work stays visible, and stack a real dependency instead of batching unrelated work into one.
