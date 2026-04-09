from __future__ import annotations

import unittest
from pathlib import Path


WORKSPACE_ROOT = Path(__file__).resolve().parents[2]


def _read(path: Path) -> str:
    return path.read_text(encoding="utf-8-sig", errors="ignore")


def _fixed_string_hits(pattern: str, roots: tuple[str, ...] = ("modules", "apps", "tests")) -> list[str]:
    hits: list[str] = []
    valid_suffixes = {".h", ".hpp", ".cpp", ".cc", ".cxx", ".cmake", ".ps1", ".py"}
    current_test_file = Path(__file__).resolve()
    for root in roots:
        for path in (WORKSPACE_ROOT / root).rglob("*"):
            if not path.is_file():
                continue
            if path.resolve() == current_test_file:
                continue
            if path.parts[-3:-1] == ("tests", "contracts") and path.suffix == ".py":
                continue
            if path.name != "CMakeLists.txt" and path.suffix not in valid_suffixes:
                continue
            if pattern in _read(path):
                hits.append(path.relative_to(WORKSPACE_ROOT).as_posix())
    return hits


class MachineResidualExitContractTest(unittest.TestCase):
    def test_runtime_host_machine_execution_state_is_runtime_owned(self) -> None:
        host_cmake = _read(WORKSPACE_ROOT / "modules" / "runtime-execution" / "runtime" / "host" / "CMakeLists.txt")
        host_tests_cmake = _read(
            WORKSPACE_ROOT / "modules" / "runtime-execution" / "runtime" / "host" / "tests" / "CMakeLists.txt"
        )
        app_container = _read(WORKSPACE_ROOT / "apps" / "runtime-service" / "container" / "ApplicationContainer.System.cpp")

        self.assertIn("MachineExecutionStateStore.cpp", host_cmake)
        self.assertIn("MachineExecutionStateBackend.cpp", host_cmake)
        self.assertNotIn("DispenserModelMachineExecutionStateBackend.cpp", host_cmake)
        self.assertIn("MachineExecutionStateBackendTest.cpp", host_tests_cmake)
        self.assertIn("MachineExecutionStateStoreTest.cpp", host_tests_cmake)
        self.assertNotIn("DispenserModelMachineExecutionStateBackendTest.cpp", host_tests_cmake)
        self.assertIn("CreateMachineExecutionStatePort()", app_container)

        runtime_backend = (
            WORKSPACE_ROOT
            / "modules"
            / "runtime-execution"
            / "runtime"
            / "host"
            / "runtime"
            / "system"
            / "MachineExecutionStateBackend.h"
        )
        runtime_store = runtime_backend.with_name("MachineExecutionStateStore.h")
        legacy_backend = runtime_backend.with_name("DispenserModelMachineExecutionStateBackend.h")
        self.assertTrue(runtime_backend.exists(), msg="runtime-host must expose MachineExecutionStateBackend.h")
        self.assertTrue(runtime_store.exists(), msg="runtime-host must expose MachineExecutionStateStore.h")
        self.assertFalse(legacy_backend.exists(), msg="legacy dispenser-model backend header should be deleted")

        for relative in (
            "modules/runtime-execution/runtime/host/tests/integration/HostBootstrapSmokeTest.cpp",
            "modules/runtime-execution/runtime/host/tests/regression/RuntimeExecutionStateMachineRegressionTest.cpp",
            "modules/runtime-execution/runtime/host/tests/unit/runtime/system/MachineExecutionStateBackendTest.cpp",
        ):
            self.assertIn(
                '#include "runtime/system/MachineExecutionStateBackend.h"',
                _read(WORKSPACE_ROOT / relative),
                msg=f"{relative} must include the runtime-owned machine execution backend",
            )

        for pattern in (
            '#include "runtime/system/DispenserModelMachineExecutionStateBackend.h"',
            '#include "domain/machine/aggregates/DispenserModel.h"',
        ):
            hits = _fixed_string_hits(pattern, roots=("modules", "apps", "tests"))
            self.assertFalse(hits, msg=f"legacy runtime-host machine-state reference still present for {pattern}: {hits}")

    def test_coordinate_alignment_machine_residuals_exit_live_surface(self) -> None:
        for relative in (
            "modules/coordinate-alignment/application/CMakeLists.txt",
            "modules/coordinate-alignment/application/include/coordinate_alignment/application/CoordinateAlignmentApplicationSurface.h",
            "modules/coordinate-alignment/domain/machine/CMakeLists.txt",
            "modules/coordinate-alignment/domain/machine/aggregates/DispenserModel.h",
            "modules/coordinate-alignment/domain/machine/domain-services/CalibrationProcess.h",
            "modules/coordinate-alignment/domain/machine/ports/ICalibrationDevicePort.h",
            "modules/coordinate-alignment/domain/machine/ports/ICalibrationResultPort.h",
            "modules/coordinate-alignment/domain/machine/ports/IHardwareConnectionPort.h",
            "modules/coordinate-alignment/domain/machine/ports/IHardwareTestPort.h",
            "modules/coordinate-alignment/domain/machine/value-objects/CalibrationTypes.h",
            "modules/coordinate-alignment/tests/unit/CalibrationProcessTest.cpp",
            "modules/workflow/domain/include/domain/machine/aggregates/DispenserModel.h",
            "modules/workflow/domain/include/domain/machine/domain-services/CalibrationProcess.h",
            "modules/workflow/domain/include/domain/machine/ports/IHardwareConnectionPort.h",
            "modules/workflow/domain/include/domain/machine/ports/IHardwareTestPort.h",
            "modules/runtime-execution/adapters/device/include/siligen/device/adapters/hardware/HardwareConnectionAdapter.h",
            "modules/runtime-execution/adapters/device/src/adapters/motion/controller/connection/HardwareConnectionAdapter.h",
            "modules/runtime-execution/adapters/device/src/adapters/motion/controller/connection/HardwareConnectionAdapter.cpp",
            "modules/runtime-execution/adapters/device/src/adapters/diagnostics/health/testing/HardwareTestAdapter.h",
        ):
            self.assertFalse((WORKSPACE_ROOT / relative).exists(), msg=f"legacy machine residual should be deleted: {relative}")

        module_cmake = _read(WORKSPACE_ROOT / "modules" / "coordinate-alignment" / "CMakeLists.txt")
        tests_cmake = _read(WORKSPACE_ROOT / "modules" / "coordinate-alignment" / "tests" / "CMakeLists.txt")
        hardware_test_header = _read(
            WORKSPACE_ROOT
            / "modules"
            / "runtime-execution"
            / "adapters"
            / "device"
            / "include"
            / "siligen"
            / "device"
            / "adapters"
            / "hardware"
            / "HardwareTestAdapter.h"
        )
        trigger_header = _read(
            WORKSPACE_ROOT
            / "modules"
            / "runtime-execution"
            / "adapters"
            / "device"
            / "include"
            / "siligen"
            / "device"
            / "adapters"
            / "dispensing"
            / "TriggerControllerAdapter.h"
        )
        motion_connection_header = _read(
            WORKSPACE_ROOT
            / "modules"
            / "runtime-execution"
            / "adapters"
            / "device"
            / "include"
            / "siligen"
            / "device"
            / "adapters"
            / "motion"
            / "MotionRuntimeConnectionAdapter.h"
        )
        motion_runtime_header = _read(
            WORKSPACE_ROOT
            / "modules"
            / "runtime-execution"
            / "adapters"
            / "device"
            / "include"
            / "siligen"
            / "device"
            / "adapters"
            / "motion"
            / "MotionRuntimeFacade.h"
        )

        self.assertNotIn("siligen_coordinate_alignment_domain_machine", module_cmake)
        self.assertNotIn("CalibrationProcessTest.cpp", tests_cmake)
        self.assertNotIn("IHardwareTestPort", hardware_test_header)
        self.assertNotIn("IHardwareTestPort", trigger_header)
        self.assertNotIn("IHardwareConnectionPort", motion_connection_header)
        self.assertNotIn("IHardwareConnectionPort", motion_runtime_header)

        for pattern in (
            '#include "../../../../../../coordinate-alignment/domain/machine/ports/IHardwareConnectionPort.h"',
            '#include "../../../../../../coordinate-alignment/domain/machine/ports/IHardwareTestPort.h"',
            '#include "domain/machine/ports/IHardwareConnectionPort.h"',
            '#include "domain/machine/ports/IHardwareTestPort.h"',
            '#include "siligen/device/adapters/hardware/HardwareConnectionAdapter.h"',
            "siligen_coordinate_alignment_domain_machine",
        ):
            hits = _fixed_string_hits(pattern, roots=("modules", "apps", "tests"))
            self.assertFalse(hits, msg=f"legacy machine residual reference still present for {pattern}: {hits}")


if __name__ == "__main__":
    unittest.main()
