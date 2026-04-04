# US4 Traceability Matrix

| Requirement | Owner Surface | Primary Files | Validation |
| --- | --- | --- | --- |
| O1 | M8 owns CMP service and Trigger/CMP planning implementations | `modules/dispense-packaging/domain/dispensing/CMakeLists.txt`, `modules/dispense-packaging/domain/dispensing/domain-services/CMPTriggerService.cpp`, `modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp` | targeted build of `siligen_unit_tests` |
| O2 | Workflow Trigger/CMP headers are compatibility shims only | `modules/workflow/domain/include/domain/dispensing/domain-services/CMPTriggerService.h`, `modules/workflow/domain/include/domain/dispensing/planning/domain-services/DispensingPlannerService.h`, `modules/workflow/domain/domain/dispensing/domain-services/TriggerPlanner.h` | `siligen_motion_planning_unit_tests` owner-boundary assertions |
| O3 | Workflow domain no longer compiles M8 Trigger/CMP owner implementations | `modules/workflow/domain/domain/CMakeLists.txt`, `modules/workflow/tests/process-runtime-core/CMakeLists.txt` | bridge assertion script |
| O4 | US4 regressions are blocked by boundary checks | `scripts/validation/assert-module-boundary-bridges.ps1` | bridge assertion script |
