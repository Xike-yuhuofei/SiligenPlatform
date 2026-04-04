# ARCH-201 Stage E Mainline Reconcile

## Goal

Rescue the remaining owner-boundary deltas from the stale `refactor/motion/ARCH-201-stage-b-reconcile` worktree onto current `origin/main` without replaying the full Stage B/C/D stack.

## Scope

- Remove the last `workflow` planning compat build target from the live graph.
- Keep workflow planning public headers as deprecated compatibility shims only.
- Restore workflow `TimePlanningConfig` to a thin shim over motion-planning contracts.
- Update owner-boundary docs and tests to match the rescued seam.

## Non-Goals

- Do not replay unrelated stale worktree changes.
- Do not modify `apps/hmi-app` gates.
- Do not rewrite the archived Stage B task set.
