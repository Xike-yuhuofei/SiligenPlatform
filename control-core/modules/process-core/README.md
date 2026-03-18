# process-core

Repository-internal modularization seed for the process execution core.

Responsibilities:
- recipe and version semantics
- dispense process semantics
- execution planning and orchestration

Current stage:
- directory and CMake target exist
- seed interfaces now live under `include/siligen/process`
- large-scale code migration is intentionally deferred

Seed headers:
- `execution/process_types.h`
- `ports/motion_executor_port.h`
- `ports/dispenser_actuator_port.h`
- `ports/safety_guard_port.h`
- `ports/recipe_repository_port.h`
- `services/process_execution_service.h`

Real migrated slice:
- recipe parameter schema and recipe metadata types
- recipe aggregates
- recipe repository / audit repository ports
- recipe validation service
- recipe activation service
