# process-core seed

This document defines the first migration slice for `modules/process-core`.

## Primary ownership

`process-core` owns:
- recipe semantics
- dispense process semantics
- process execution planning
- process orchestration state

## First migration slice

Move or wrap first:
- `src/domain/recipes/aggregates/Recipe.h`
- `src/domain/recipes/aggregates/RecipeVersion.h`
- `src/domain/recipes/domain-services/RecipeValidationService.*`
- `src/domain/recipes/domain-services/RecipeActivationService.*`
- `src/domain/dispensing/value-objects/DispensingPlan.h`
- `src/domain/dispensing/value-objects/TriggerPlan.h`
- `src/domain/dispensing/domain-services/DispensingProcessService.*`
- `src/domain/dispensing/domain-services/ValveCoordinationService.*`
- `src/application/usecases/recipes/RecipeCommandUseCase.*`
- `src/application/usecases/recipes/RecipeQueryUseCase.*`
- `src/application/usecases/dispensing/valve/ValveCommandUseCase.*`
- `src/application/usecases/dispensing/valve/coordination/ValveCoordinationUseCase.*`

## Split before moving

Review and split these areas before migration:
- `src/application/services/dxf/DxfPbPreparationService.*`
  - likely runtime / external preprocessing integration
- `src/application/usecases/dispensing/dxf/*`
  - likely mixes file upload, planning, trajectory conversion, and execution control
- `src/domain/dispensing/ports/IValvePort.h`
  - contains both device-facing control details and process-facing concepts

## New seed abstractions

The first seed abstractions added under `modules/process-core/include` are intentionally
small and execution-oriented:
- process execution request / summary
- recipe reference
- motion execution port
- dispenser actuator port
- safety guard port
- recipe repository port
- process execution service

These new headers are not yet wired into every production path. They define the intended landing zone
for future incremental migration.

## First real migrated slice

The following assets now have real implementations under `modules/process-core`:
- `recipes/value_objects/parameter_schema.h`
- `recipes/value_objects/recipe_types.h`
- `recipes/aggregates/recipe.h`
- `recipes/aggregates/recipe_version.h`
- `recipes/ports/recipe_repository_port.h`
- `recipes/ports/audit_repository_port.h`
- `recipes/services/recipe_validation_service.h/.cpp`
- `recipes/services/recipe_activation_service.h/.cpp`

This slice is intentionally recipe-centric and avoids DXF, runtime preprocessing, or valve
hardware concerns.
