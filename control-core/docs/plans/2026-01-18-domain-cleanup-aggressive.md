# Domain Cleanup Aggressive Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Remove redundant/obsolete Domain files and shrink migration scaffolding while keeping the build green.

**Architecture:** Eliminate forwarding headers and unused domain services, prefer direct subdomain port includes, and keep only compiled/used domain services. Update docs to match reality.

**Tech Stack:** C++17, CMake, PowerShell, ripgrep

---

### Task 1: Create isolated worktree and capture baseline build

**Files:**
- Modify: none
- Test: build only

**Step 1: Create a worktree (recommended)**

Run:
```powershell
git worktree add D:\Projects\SiligenSuite\control-core_worktrees\domain_cleanup_2026-01-18
```
Expected: new worktree directory created.

**Step 2: Configure build directory**

Run:
```powershell
cmake -S . -B build\stage4-all-modules-check -DSILIGEN_BUILD_TESTS=ON
```
Expected: CMake configure succeeds.

**Step 3: Baseline build**

Run:
```powershell
cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_application
```
Expected: build succeeds.

**Step 4: Commit baseline info (optional)**

Run:
```powershell
git status -sb
```
Expected: clean status.

---

### Task 2: Remove obsolete diagnostics domain services

**Files:**
- Delete: `src/domain/diagnostics/domain-services/DiagnosticSystem.h`
- Delete: `src/domain/diagnostics/domain-services/DiagnosticSystem.cpp`
- Delete: `src/domain/diagnostics/domain-services/StatusMonitor.h`
- Delete: `src/domain/diagnostics/domain-services/StatusMonitor.cpp`
- Modify: `src/README.md`
- Modify: `src/domain/diagnostics/README.md`

**Step 1: Remove files**

Run:
```powershell
Remove-Item src/domain/diagnostics/domain-services/DiagnosticSystem.h
Remove-Item src/domain/diagnostics/domain-services/DiagnosticSystem.cpp
Remove-Item src/domain/diagnostics/domain-services/StatusMonitor.h
Remove-Item src/domain/diagnostics/domain-services/StatusMonitor.cpp
```
Expected: files removed.

**Step 2: Update docs to drop references**

Edit the above README files to remove mentions of DiagnosticSystem/StatusMonitor.

**Step 3: Build to verify**

Run:
```powershell
cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_application
```
Expected: build succeeds.

**Step 4: Commit**

Run:
```powershell
git add src/README.md src/domain/diagnostics/README.md
git add -u
git commit -m "cleanup: remove obsolete diagnostics services"
```
Expected: commit created.

---

### Task 3: Remove duplicate ArcTriggerPointCalculator in domain

**Files:**
- Delete: `src/domain/dispensing/domain-services/ArcTriggerPointCalculator.h`
- Delete: `src/domain/dispensing/domain-services/ArcTriggerPointCalculator.cpp`

**Step 1: Remove files**

Run:
```powershell
Remove-Item src/domain/dispensing/domain-services/ArcTriggerPointCalculator.h
Remove-Item src/domain/dispensing/domain-services/ArcTriggerPointCalculator.cpp
```
Expected: files removed.

**Step 2: Build to verify**

Run:
```powershell
cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_application
```
Expected: build succeeds.

**Step 3: Commit**

Run:
```powershell
git add -u
git commit -m "cleanup: drop duplicate arc trigger calculator in domain"
```
Expected: commit created.

---

### Task 4: Remove unused domain headers (aggressive)

**Files:**
- Delete: `src/domain/diagnostics/value-objects/FingerprintStrategy.h`
- Delete: `src/domain/diagnostics/value-objects/RedactionPolicy.h`
- Delete: `src/domain/diagnostics/value-objects/TestTypes.h`
- Delete: `src/domain/machine/value-objects/SystemTypes.h`
- Delete: `src/domain/motion/domain-services/MotionConfigService.h`
- Delete: `src/domain/motion/domain-services/MotionInitializationService.h`
- Delete: `src/domain/motion/state/StateMachine.h`
- Delete: `src/domain/motion/value-objects/TrajectoryDataTypes.h`
- Delete: `src/domain/<subdomain>/ports/IClockPort.h`
- Delete: `src/domain/<subdomain>/ports/IDataQueryPort.h`
- Delete: `src/domain/<subdomain>/ports/IDataSerializerPort.h`
- Delete: `src/domain/<subdomain>/ports/IEnvironmentInfoProviderPort.h`
- Delete: `src/domain/<subdomain>/ports/IStacktraceProviderPort.h`
- Delete: `src/domain/_shared/value-objects/IOTypes.h`

**Step 1: Remove files**

Run:
```powershell
Remove-Item src/domain/diagnostics/value-objects/FingerprintStrategy.h
Remove-Item src/domain/diagnostics/value-objects/RedactionPolicy.h
Remove-Item src/domain/diagnostics/value-objects/TestTypes.h
Remove-Item src/domain/machine/value-objects/SystemTypes.h
Remove-Item src/domain/motion/domain-services/MotionConfigService.h
Remove-Item src/domain/motion/domain-services/MotionInitializationService.h
Remove-Item src/domain/motion/state/StateMachine.h
Remove-Item src/domain/motion/value-objects/TrajectoryDataTypes.h
Remove-Item src/domain/<subdomain>/ports/IClockPort.h
Remove-Item src/domain/<subdomain>/ports/IDataQueryPort.h
Remove-Item src/domain/<subdomain>/ports/IDataSerializerPort.h
Remove-Item src/domain/<subdomain>/ports/IEnvironmentInfoProviderPort.h
Remove-Item src/domain/<subdomain>/ports/IStacktraceProviderPort.h
Remove-Item src/domain/_shared/value-objects/IOTypes.h
```
Expected: files removed.

**Step 2: Build to verify**

Run:
```powershell
cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_application
```
Expected: build succeeds.

**Step 3: Commit**

Run:
```powershell
git add -u
git commit -m "cleanup: remove unused domain headers"
```
Expected: commit created.

---

### Task 5: Remove forwarding ports and Ports.h, update includes

**Files:**
- Delete: `src/domain/<subdomain>/ports/IAdvancedMotionPort.h`
- Delete: `src/domain/<subdomain>/ports/IAxisControlPort.h`
- Delete: `src/domain/<subdomain>/ports/IConfigurationPort.h`
- Delete: `src/domain/diagnostics/ports/IDiagnosticsPort.h`
- Delete: `src/domain/<subdomain>/ports/IFileStoragePort.h`
- Delete: `src/domain/<subdomain>/ports/IHardwareConnectionPort.h`
- Delete: `src/domain/<subdomain>/ports/IHardwareTestPort.h`
- Delete: `src/domain/motion/ports/IHomingPort.h`
- Delete: `src/domain/<subdomain>/ports/IInterpolationPort.h`
- Delete: `src/domain/<subdomain>/ports/IJogControlPort.h`
- Delete: `src/domain/<subdomain>/ports/IMotionConnectionPort.h`
- Delete: `src/domain/motion/ports/IMotionStatePort.h`
- Delete: `src/domain/<subdomain>/ports/IPositionControlPort.h`
- Delete: `src/domain/dispensing/ports/ITriggerControllerPort.h`
- Delete: `src/domain/<subdomain>/ports/Ports.h`
- Modify: `src/README.md`
- Modify: `src/domain/<subdomain>/ports/README.md`
- Modify: `src/domain/PhysicalDependencyRules.md`
- Modify: `src/application/2026-01-16-architecture-migration-detailed-analysis.md`
- Modify: `apps/control-runtime/container/ApplicationContainer.h`
- Modify: `src/application/usecases/motion/homing/HomeAxesUseCase.h`
- Modify: `src/application/usecases/motion/trajectory/ExecuteTrajectoryUseCase.h`
- Modify: `src/adapters/cli/CLICompositionRoot.cpp`
- Modify: `src/application/cli/MenuHandler.h`
- Modify: `src/application/container/InfrastructureAdapterFactory.h`
- Modify: `src/application/container/README.md`
- Modify: `src/application/services/HardLimitMonitorService.h`
- Modify: `src/application/services/SoftLimitMonitorService.h`
- Modify: `src/application/usecases/dispensing/dxf/CleanupDXFFilesUseCase.h`
- Modify: `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.h`
- Modify: `src/application/usecases/dispensing/dxf/DXFWebPlanningUseCase.h`
- Modify: `src/application/usecases/dispensing/dxf/UploadDXFFileUseCase.cpp`
- Modify: `src/application/usecases/dispensing/valve/coordination/ValveCoordinationUseCase.h`
- Modify: `src/application/usecases/dispensing/valve/dispenser/GetDispenserStatusUseCase.h`
- Modify: `src/application/usecases/dispensing/valve/dispenser/PauseDispenserUseCase.h`
- Modify: `src/application/usecases/dispensing/valve/dispenser/ResumeDispenserUseCase.h`
- Modify: `src/application/usecases/dispensing/valve/dispenser/StartDispenserUseCase.h`
- Modify: `src/application/usecases/dispensing/valve/dispenser/StopDispenserUseCase.h`
- Modify: `src/application/usecases/dispensing/valve/supply/CloseSupplyValveUseCase.h`
- Modify: `src/application/usecases/dispensing/valve/supply/GetSupplyStatusUseCase.h`
- Modify: `src/application/usecases/dispensing/valve/supply/OpenSupplyValveUseCase.h`
- Modify: `src/application/usecases/dispensing/valve/ValveUseCaseHelpers.h`
- Modify: `src/application/usecases/motion/coordination/MotionCoordinationUseCase.h`
- Modify: `src/application/usecases/motion/initialization/MotionInitializationUseCase.h`
- Modify: `src/application/usecases/motion/manual/ManualMotionControlUseCase.h`
- Modify: `src/application/usecases/motion/monitoring/MotionMonitoringUseCase.h`
- Modify: `src/application/usecases/motion/safety/MotionSafetyUseCase.h`
- Modify: `src/application/usecases/README.md`
- Modify: `src/application/usecases/state_machine/SystemStateMachine.h`
- Modify: `src/application/usecases/system/InitializeSystemUseCase.h`
- Modify: `src/application/usecases/USECASE_ANALYSIS.md`
- Modify: `src/infrastructure/adapters/diagnostics/health/DiagnosticsPortAdapter.h`
- Modify: `src/infrastructure/adapters/diagnostics/health/presets/CMPTestPresetService.h`
- Modify: `src/infrastructure/adapters/diagnostics/health/testing/HardwareTestAdapter.h`
- Modify: `src/infrastructure/adapters/dispensing/dispenser/triggering/TriggerControllerAdapter.h`
- Modify: `src/infrastructure/adapters/dispensing/dispenser/ValveAdapter.h`
- Modify: `src/infrastructure/adapters/motion/controller/connection/HardwareConnectionAdapter.h`
- Modify: `src/infrastructure/adapters/motion/controller/control/MultiCardMotionAdapter.h`
- Modify: `src/infrastructure/adapters/motion/controller/homing/HomingPortAdapter.h`
- Modify: `src/infrastructure/adapters/motion/controller/interpolation/InterpolationAdapter.h`
- Modify: `src/infrastructure/adapters/planning/dxf/DXFFileParsingAdapter.h`
- Modify: `src/infrastructure/adapters/planning/visualization/dxf/DXFVisualizationAdapter.h`
- Modify: `src/infrastructure/adapters/README.md`
- Modify: `src/infrastructure/adapters/system/configuration/ConfigFileAdapter.h`
- Modify: `src/infrastructure/adapters/system/configuration/IniTestConfigManager.h`
- Modify: `src/infrastructure/adapters/system/configuration/validators/ConfigValidator.h`
- Modify: `src/infrastructure/adapters/system/persistence/test-records/MockTestRecordRepository.h`
- Modify: `apps/control-runtime/runtime/events/InMemoryEventPublisherAdapter.h`
- Modify: `src/infrastructure/adapters/system/storage/files/LocalFileStorageAdapter.h`

**Step 1: Update includes to subdomain ports**

Replace includes from:
```
#include "domain/<subdomain>/ports/IAdvancedMotionPort.h"
```
to:
```
#include "domain/motion/ports/IAdvancedMotionPort.h"
```
Apply the same mapping:
- `IAdvancedMotionPort` -> `domain/motion/ports/IAdvancedMotionPort.h`
- `IAxisControlPort` -> `domain/motion/ports/IAxisControlPort.h`
- `IConfigurationPort` -> `domain/configuration/ports/IConfigurationPort.h`
- `IDiagnosticsPort` -> `domain/diagnostics/ports/IDiagnosticsPort.h`
- `IFileStoragePort` -> `domain/configuration/ports/IFileStoragePort.h`
- `IHardwareConnectionPort` -> `domain/machine/ports/IHardwareConnectionPort.h`
- `IHardwareTestPort` -> `domain/machine/ports/IHardwareTestPort.h`
- `IHomingPort` -> `domain/motion/ports/IHomingPort.h`
- `IInterpolationPort` -> `domain/motion/ports/IInterpolationPort.h`
- `IJogControlPort` -> `domain/motion/ports/IJogControlPort.h`
- `IMotionConnectionPort` -> `domain/motion/ports/IMotionConnectionPort.h`
- `IMotionStatePort` -> `domain/motion/ports/IMotionStatePort.h`
- `IPositionControlPort` -> `domain/motion/ports/IPositionControlPort.h`
- `ITriggerControllerPort` -> `domain/dispensing/ports/ITriggerControllerPort.h`

**Step 2: Replace Ports.h includes**

Replace `#include "domain/<subdomain>/ports/Ports.h"` with direct includes required by the file. Update:
- `apps/control-runtime/container/ApplicationContainer.h`
- `src/application/usecases/motion/homing/HomeAxesUseCase.h`
- `src/application/usecases/motion/trajectory/ExecuteTrajectoryUseCase.h`

**Step 3: Remove forwarding headers and Ports.h**

Run:
```powershell
Remove-Item src/domain/<subdomain>/ports/IAdvancedMotionPort.h
Remove-Item src/domain/<subdomain>/ports/IAxisControlPort.h
Remove-Item src/domain/<subdomain>/ports/IConfigurationPort.h
Remove-Item src/domain/diagnostics/ports/IDiagnosticsPort.h
Remove-Item src/domain/<subdomain>/ports/IFileStoragePort.h
Remove-Item src/domain/<subdomain>/ports/IHardwareConnectionPort.h
Remove-Item src/domain/<subdomain>/ports/IHardwareTestPort.h
Remove-Item src/domain/motion/ports/IHomingPort.h
Remove-Item src/domain/<subdomain>/ports/IInterpolationPort.h
Remove-Item src/domain/<subdomain>/ports/IJogControlPort.h
Remove-Item src/domain/<subdomain>/ports/IMotionConnectionPort.h
Remove-Item src/domain/motion/ports/IMotionStatePort.h
Remove-Item src/domain/<subdomain>/ports/IPositionControlPort.h
Remove-Item src/domain/dispensing/ports/ITriggerControllerPort.h
Remove-Item src/domain/<subdomain>/ports/Ports.h
```
Expected: files removed.

**Step 4: Update docs to remove migration/Ports.h references**

Edit the listed docs to remove references to forwarding ports and Ports.h.

**Step 5: Build to verify**

Run:
```powershell
cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_application
```
Expected: build succeeds.

**Step 6: Commit**

Run:
```powershell
git add -u
git commit -m "cleanup: remove forwarding ports and Ports.h"
```
Expected: commit created.

---

### Task 6: Remove DispensingMachine aggregate (unused)

**Files:**
- Delete: `src/domain/machine/aggregates/DispensingMachine.h`
- Delete: `src/domain/machine/aggregates/DispensingMachine.cpp`
- Modify: `src/domain/machine/README.md`

**Step 1: Remove files**

Run:
```powershell
Remove-Item src/domain/machine/aggregates/DispensingMachine.h
Remove-Item src/domain/machine/aggregates/DispensingMachine.cpp
```
Expected: files removed.

**Step 2: Update README**

Remove references to DispensingMachine in `src/domain/machine/README.md`.

**Step 3: Build to verify**

Run:
```powershell
cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_application
```
Expected: build succeeds.

**Step 4: Commit**

Run:
```powershell
git add -u
git commit -m "cleanup: remove unused DispensingMachine aggregate"
```
Expected: commit created.

---

### Task 7: Remove empty TrajectoryTypes.cpp

**Files:**
- Delete: `src/domain/motion/TrajectoryTypes.cpp`

**Step 1: Remove file**

Run:
```powershell
Remove-Item src/domain/motion/TrajectoryTypes.cpp
```
Expected: file removed.

**Step 2: Build to verify**

Run:
```powershell
cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_application
```
Expected: build succeeds.

**Step 3: Commit**

Run:
```powershell
git add -u
git commit -m "cleanup: remove empty trajectory types cpp"
```
Expected: commit created.

---

### Task 8: Ensure PathOptimizationStrategy is compiled

**Files:**
- Modify: `src/domain/dispensing/CMakeLists.txt`

**Step 1: Add PathOptimizationStrategy.cpp to domain_dispensing**

Edit `src/domain/dispensing/CMakeLists.txt` to add:
```
domain-services/PathOptimizationStrategy.cpp
```
to the `add_library(domain_dispensing STATIC ...)` source list.

**Step 2: Build to verify**

Run:
```powershell
cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_application
```
Expected: build succeeds.

**Step 3: Commit**

Run:
```powershell
git add src/domain/dispensing/CMakeLists.txt
git commit -m "build: compile PathOptimizationStrategy in domain_dispensing"
```
Expected: commit created.

---

### Task 9: Final verification

**Files:**
- Test: build only

**Step 1: Full build**

Run:
```powershell
cmake --build build\stage4-all-modules-check --config Debug
```
Expected: build succeeds.

**Step 2: Report git status**

Run:
```powershell
git status -sb
```
Expected: clean status.


