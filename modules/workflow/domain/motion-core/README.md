# motion-core

Repository-internal modularization seed for the motion semantics core.

Responsibilities:
- axis and coordinate semantics
- motion command semantics
- trajectory, interpolation, and safety rules

Current stage:
- archive/seed-only residue directory
- no live CMake target remains in the workflow build graph
- previous safety payload and compatibility target have exited; canonical live consumers must use runtime-owned or motion-planning-owned surfaces instead of reviving this tree

Seed headers:
- `model/motion_types.h`
- `ports/motion_controller_port.h`
- `ports/axis_feedback_port.h`
- `ports/interlock_port.h`
- `ports/emergency_port.h`
- `services/motion_service.h`

Archived note:
- historical seed headers may remain only as documentation breadcrumbs for migration history
- if a future slice needs to revive motion semantics, it must land in the canonical owner module instead of reactivating `modules/workflow/domain/motion-core`
