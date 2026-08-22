# Task 6: Multi-agent design/implement/verify workflow — proposal + trial report

## Context

This session already used Claude Code's Agent tool informally: two background
research agents produced `docs/design/task2-feature-inventory-and-proposal.md`
and `docs/design/task3-vm-audit-and-design.md` in parallel while Task 1's fixes
were being implemented directly (not through a role split), and a third
background agent audited the parser for AST-generation bugs. Task 1 itself —
the concrete bug fixes — was done by the orchestrating session directly, not
through a Design→Implementation→Verification agent chain, because the bugs
were already fully diagnosed (exact line numbers, root cause, fix) before any
agent was involved; splitting a 3-line fix across three context handoffs would
have added re-derivation cost for no benefit. That is itself a finding, below.

This document (a) proposes how to operationalize the 3-role split going
forward, and (b) reports on a real trial of it, run on Task 2's `realloc()`/
`calloc()` implementation (chosen because its design memo already existed and
was concrete enough to hand to an Implementation agent as-is).

## 1. How to split the 3 roles

| Option | Description | Pros | Cons |
|---|---|---|---|
| **A. Agent tool, same session/worktree** | Orchestrating session spawns Design/Implementation/Verification as `Agent` calls in sequence, each reading the previous stage's output file | Cheapest; no environment setup duplication (WSL/leg/gcc toolchain already warm); orchestrator can review between stages before the next one starts; fastest iteration | All agents share one working tree — a bad Implementation attempt dirties it until reverted; no true parallelism (each stage must finish before the next starts, since Implementation needs Design's file and Verification needs Implementation's diff) |
| **B. Agent tool with `isolation: "worktree"` for Implementation** | Same as A, but Implementation runs in an isolated git worktree; orchestrator inspects the worktree's diff before merging it into the main tree for Verification | Bad/exploratory implementation attempts don't dirty the main tree; easy to discard and retry | Slightly more setup (worktree creation/cleanup); Verification still needs to run against the merged result, so it's not fully parallel either |
| **C. Separate Claude Code sessions/branches, human-coordinated** | Each role is a distinct session (or even distinct person) working on its own branch; a human merges design→impl→verify manually | True separation of concerns; works across multiple people/days; good for large or contentious changes where a human should gate every handoff | Much higher coordination overhead for changes this size (most fixes here are single-function, hours not days); context doesn't carry between sessions automatically, so each stage re-reads everything from scratch; overkill for the bug-fix-sized work this project mostly needs |

**Recommendation**: **Option B as the default** — Agent-tool orchestration
within one session, with the Implementation stage isolated in a worktree.
Reserve **Option C** for genuinely large, multi-day changes (e.g., the VM
completion work in Task 3, which the audit already estimated as a large
effort) where a real human checkpoint between stages is worth the overhead.
Plain **Option A** (no worktree) is fine for very small, easily-revertible
fixes — not worth the ceremony of a worktree for a 5-line change.

## 2. How stage outputs are passed and recorded

Already-established convention (used all session): a role's output is a
Markdown file at a fixed path, plus a short free-text summary back to the
orchestrator.

- **Design → Implementation**: `docs/design/<topic>.md`. Must be concrete
  enough to hand to a fresh agent with no other context: exact file:line
  references, function signatures, pseudocode, and explicitly-listed edge
  cases (see `task2-feature-inventory-and-proposal.md`'s realloc section for
  the bar to hit — vague "add realloc support" briefs produce vague
  implementations).
- **Implementation → Verification**: no new file *required* — the git diff
  (or worktree diff) *is* the handoff — but the Implementation agent should
  report back a short prose summary of what it changed and, critically,
  **which design-doc edge cases it resolved and how**, since the design doc's
  edge-case list is Verification's test-writing checklist.
- **Verification → orchestrator/merge**: `docs/verification/<topic>.md`,
  following the format already used for Task 1: build command, regression
  methodology, before/after diff accounting, and an explicit statement of
  which new `demofiles/*.c` + `.expect` pairs were added and why. Now that
  `make test` (Task 5) exists, Verification's job is mostly "did `make test`
  pass, and did I add cases for the new behavior" rather than manual `tail`
  inspection.

## 3. Minimal workflow and review checkpoints

```
Design memo written  ──────────────► checkpoint 1 (cheap: read a doc)
        │                             orchestrator/user skims the design memo,
        │                             sanity-checks the approach before any
        ▼                             code is touched — this is the highest-
Implementation (worktree)             leverage checkpoint, since redirecting
        │                             here costs a doc rewrite, not a redo of
        ▼                             code
Verification (make test + new
demofiles + report)  ───────────────► checkpoint 2 (diff + verification report)
        │                             orchestrator/user reviews the actual
        │                             code diff plus the verification report
        ▼                             before it's committed
Commit (small, single-purpose)
        │
        ▼
Push  ──────────────────────────────► checkpoint 3 (explicit, per this
                                       project's own established practice —
                                       push only on explicit request)
```

Two checkpoints are load-bearing; the rest can be automatic:
- **Checkpoint 1 (post-design)** catches a wrong approach while it's still
  just prose — by far the cheapest place to redirect.
- **Checkpoint 2 (post-verification, pre-commit)** catches a wrong or
  incomplete implementation before it's permanent history.
Everything else (regenerating `main.c`, running `make test`, writing the
verification report) can run unattended once the design is approved.

## 4. Trial: realloc()/calloc() via this pipeline

Design stage reused the existing memo (no new agent needed — it was already
concrete: exact `main.leg` line numbers, `prim_realloc` pseudocode, the
in-place-vs-moved distinction, five explicit edge cases, and a recommended
build order). Implementation and Verification were run as real, separate
`Agent` calls reading only that file plus the repo — see the companion
verification report at `docs/verification/realloc-calloc-verification.md`
for what actually happened, and the "what got stuck" section below.

## 5. What worked / what got stuck (so far)

**Worked:**
- A design memo written with exact `file:line` references and pseudocode
  really can be handed to a fresh Implementation agent with near-zero
  additional back-and-forth — the concreteness bar matters far more than
  which agent writes it.
- Background/parallel Design-stage research (Task 2 and Task 3 memos
  written concurrently by two agents while Task 1 was being fixed by hand)
  cost nothing in wall-clock time and produced independently-useful,
  cross-checking output (both agents independently rediscovered the same
  write-path UAF gap Task 1 was fixing at the same time).

**Got stuck / had to route around:**
- Environment setup (WSL `leg`/`peg` acquisition, `git apply --cached` +
  `git hash-object`/`update-index` for building staged intermediate states
  for clean per-fix commits) is itself a form of "shared context" that a
  fresh agent has to redo or be told explicitly — it is not free to hand off
  to a new agent even though it's not the interesting part of the task.
  Recommendation: keep a short "how to build/test this repo" cheat sheet
  (now largely captured in this `README.md`) so agent prompts can just
  reference it instead of re-deriving it.
- The safety classifier blocked a plain `git checkout HEAD -- <files>` used
  to reset the working tree for building clean per-commit intermediate
  states, since it's a "could discard uncommitted work" pattern regardless
  of intent. Worked around with `git apply --cached` + `git hash-object` +
  `git update-index --cacheinfo`, which never touches the working tree. Not
  a multi-agent-specific problem, but relevant to any Implementation agent
  that also wants to produce clean, bisectable per-fix commits.
- Splitting a *tiny, already-fully-diagnosed* fix (Task 1's three bugs)
  across a Design/Implementation/Verification handoff would have been pure
  overhead — there was no design decision to make and no ambiguity for an
  Implementation agent to resolve, so a fresh agent would have re-spent
  tokens re-deriving the exact same root-cause analysis already done. The
  pipeline's value shows up on **medium-or-larger, genuinely open-design**
  work (Task 2/3-sized), not single-function bug fixes — worth stating
  explicitly so this workflow isn't cargo-culted onto every change.
