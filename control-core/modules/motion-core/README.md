# motion-core

Repository-internal modularization seed for the motion semantics core.

Responsibilities:
- axis and coordinate semantics
- motion command semantics
- trajectory, interpolation, and safety rules

Current stage:
- directory and CMake target exist
- seed interfaces now live under `include/siligen/motion`
- legacy implementation remains in `src/domain/motion`, `src/domain/trajectory`, and related usecases

Seed headers:
- `model/motion_types.h`
- `ports/motion_controller_port.h`
- `ports/axis_feedback_port.h`
- `ports/interlock_port.h`
- `ports/emergency_port.h`
- `services/motion_service.h`

Real migrated slice:
- safety interlock value objects
- interlock signal port
- interlock policy evaluation service
- jog / homing / hard-limit / soft-limit safety helper semantics
