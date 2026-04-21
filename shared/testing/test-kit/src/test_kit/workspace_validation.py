from __future__ import annotations

import argparse
import os
from dataclasses import replace
from datetime import datetime, timezone
from pathlib import Path

from .asset_catalog import (
    baseline_governance_summary,
    build_asset_catalog,
    build_asset_inventory,
    default_engineering_regression_asset_ids,
    default_performance_sample_asset_ids,
    default_simulated_line_asset_ids,
    shared_asset_ids_for_smoke,
)
from .control_apps_build import (
    ControlAppsBuildReadiness,
    control_apps_build_root_probes,
    preferred_control_apps_build_root,
    probe_control_apps_build_readiness,
)
from .evidence_bundle import (
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)
from .fault_injection import build_fault_matrix
from .runner import (
    KNOWN_FAILURE_EXIT_CODE,
    SKIPPED_EXIT_CODE,
    ValidationCase,
    ValidationReport,
    ValidationResult,
    python_command,
    report_to_json,
    report_to_markdown,
    run_case,
)
from .validation_layers import (
    EXECUTION_LANES,
    SIZE_LABELS,
    SUITE_TAXONOMY,
    VALIDATION_LAYERS,
    build_request,
    lane_policy_metadata,
    route_validation_request,
    suite_label_refs,
)
from .workspace_layout import load_workspace_layout


WORKSPACE_ROOT = Path(__file__).resolve().parents[5]
DEFAULT_SUITES = ("apps", "contracts", "protocol-compatibility", "integration", "e2e")
WORKSPACE_LAYOUT = load_workspace_layout(WORKSPACE_ROOT)
CONTROL_APPS_BUILD_ROOT_PROBES = control_apps_build_root_probes(
    WORKSPACE_ROOT,
    explicit_build_root=os.getenv("SILIGEN_CONTROL_APPS_BUILD_ROOT"),
)
CONTROL_APPS_BUILD_ROOT = preferred_control_apps_build_root(
    WORKSPACE_ROOT,
    explicit_build_root=os.getenv("SILIGEN_CONTROL_APPS_BUILD_ROOT"),
).root
PERFORMANCE_THRESHOLD_CONFIG = WORKSPACE_ROOT / "tests" / "baselines" / "performance" / "dxf-preview-profile-thresholds.json"
CANONICAL_RECIPE_ID = "recipe-7d1b00f4-6a99"
CANONICAL_VERSION_ID = "version-fea9ce29-f963"
CONTROL_APPS_CASE_ARTIFACTS: dict[str, str] = {
    "runtime-service-dry-run": "siligen_runtime_service.exe",
    "runtime-gateway-dry-run": "siligen_runtime_gateway.exe",
    "planner-cli-dry-run": "siligen_planner_cli.exe",
    "shared-kernel-unit": "siligen_shared_kernel_tests.exe",
    "runtime-host-unit": "siligen_runtime_host_unit_tests.exe",
    "dxf-geometry-unit": "siligen_dxf_geometry_unit_tests.exe",
    "job-ingest-unit": "siligen_job_ingest_unit_tests.exe",
    "workflow-unit": "siligen_unit_tests.exe",
}
CONTROL_APPS_PREREQUISITE_CASE_ID = "control-apps-build-readiness"


def _default_report_dir() -> Path:
    return WORKSPACE_ROOT / "tests" / "reports"


def _canonical_recipe_context_args() -> list[str]:
    return [
        "--recipe-id",
        CANONICAL_RECIPE_ID,
        "--version-id",
        CANONICAL_VERSION_ID,
    ]


def _layout_absolute_path(key: str) -> Path:
    return WORKSPACE_LAYOUT[key].absolute


def _control_apps_executable(name: str) -> Path:
    candidates = [
        CONTROL_APPS_BUILD_ROOT / "bin" / name,
        CONTROL_APPS_BUILD_ROOT / "bin" / "Debug" / name,
        CONTROL_APPS_BUILD_ROOT / "bin" / "Release" / name,
        CONTROL_APPS_BUILD_ROOT / "bin" / "RelWithDebInfo" / name,
    ]
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[1]


def _powershell_file_command(script_path: Path, *script_args: str) -> list[str]:
    return [
        "powershell",
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        str(script_path),
        *script_args,
    ]


def _resolve_freeze_report_dir(base_report_dir: Path) -> Path:
    configured = os.getenv("SILIGEN_FREEZE_DOCSET_REPORT_DIR", "").strip()
    if configured:
        return Path(configured).resolve()
    resolved_report_dir = base_report_dir.resolve()
    return (
        resolved_report_dir
        if resolved_report_dir.name == "dsp-e2e-spec-docset"
        else resolved_report_dir / "dsp-e2e-spec-docset"
    )


def _resolve_legacy_exit_report_dir(base_report_dir: Path) -> Path:
    configured = os.getenv("SILIGEN_LEGACY_EXIT_REPORT_DIR", "").strip()
    if configured:
        return Path(configured).resolve()
    resolved_report_dir = base_report_dir.resolve()
    return resolved_report_dir if resolved_report_dir.name == "legacy-exit" else resolved_report_dir / "legacy-exit"


def _freeze_evidence_cases() -> tuple[str, ...]:
    configured = os.getenv("SILIGEN_FREEZE_EVIDENCE_CASES", "").strip()
    if not configured:
        return ("success", "block", "rollback", "recovery", "archive")
    requested = tuple(item.strip() for item in configured.split(",") if item.strip())
    supported = ("success", "block", "rollback", "recovery", "archive")
    return tuple(item for item in requested if item in supported) or supported


def _freeze_validator_command(
    *,
    report_dir: Path,
    report_stem: str,
    scenario_classes: tuple[str, ...] = (),
) -> list[str]:
    command = [
        *python_command(WORKSPACE_ROOT / "scripts" / "migration" / "validate_dsp_e2e_spec_docset.py"),
        "--report-dir",
        str(report_dir),
        "--report-stem",
        report_stem,
    ]
    for scenario_class in scenario_classes:
        command.extend(["--scenario-class", scenario_class])
    return command


def _normalize_suites(requested: list[str] | None) -> list[str]:
    if not requested or "all" in requested:
        return list(DEFAULT_SUITES)

    normalized: list[str] = []
    for suite in requested:
        if suite not in normalized:
            normalized.append(suite)
    return normalized


def _workspace_validation_metadata(control_apps_readiness: ControlAppsBuildReadiness) -> dict[str, object]:
    selected_probe = control_apps_readiness.selected_probe
    metadata = {
        "workspace_layout_file": str((WORKSPACE_ROOT / "cmake" / "workspace-layout.env").resolve()),
        "workspace_root": str(WORKSPACE_ROOT.resolve()),
        "tests_root": str((WORKSPACE_ROOT / "tests").resolve()),
        "scripts_root": str((WORKSPACE_ROOT / "scripts").resolve()),
        "samples_root": str((WORKSPACE_ROOT / "samples").resolve()),
        "control_apps_build_root": (
            str(selected_probe.root)
            if selected_probe
            else ""
        ),
        "control_apps_cmake_home_directory": (
            selected_probe.cmake_home_directory
            if selected_probe
            else ""
        ),
        "control_apps_build_root_source": (
            selected_probe.source
            if selected_probe
            else ""
        ),
        "control_apps_ready": control_apps_readiness.ready,
        "control_apps_required_artifacts": list(control_apps_readiness.required_artifacts),
        "control_apps_missing_artifacts": list(control_apps_readiness.missing_artifacts),
        "control_apps_readiness_reason": control_apps_readiness.reason,
        "control_apps_search_roots": [str(probe.root) for probe in CONTROL_APPS_BUILD_ROOT_PROBES],
        "control_apps_rejected_roots": [
            {"root": str(probe.root), "reason": probe.reason}
            for probe in CONTROL_APPS_BUILD_ROOT_PROBES
            if not probe.accepted
        ],
        "canonical_validation_layers": list(VALIDATION_LAYERS.keys()),
        "canonical_execution_lanes": list(EXECUTION_LANES.keys()),
        "canonical_suite_taxonomy": list(SUITE_TAXONOMY.keys()),
        "canonical_size_labels": list(SIZE_LABELS),
    }

    for key in ("SILIGEN_FORMAL_FREEZE_DOCSET_DIR", "SILIGEN_FORMAL_FREEZE_INDEX_FILE"):
        if key in WORKSPACE_LAYOUT:
            metadata[key.lower()] = str(_layout_absolute_path(key))
    return metadata


def _required_control_apps_artifacts(cases: list[ValidationCase]) -> tuple[str, ...]:
    required_artifacts: list[str] = []
    for case in cases:
        artifact_name = CONTROL_APPS_CASE_ARTIFACTS.get(case.name, "")
        if artifact_name and artifact_name not in required_artifacts:
            required_artifacts.append(artifact_name)
    return tuple(required_artifacts)


def _control_apps_readiness_result(readiness: ControlAppsBuildReadiness) -> ValidationResult:
    selected_probe = readiness.selected_probe
    stdout_lines = [
        f"control_apps_ready={str(readiness.ready).lower()}",
        f"required_artifacts={','.join(readiness.required_artifacts)}",
        f"missing_artifacts={','.join(readiness.missing_artifacts)}",
        f"reason={readiness.reason}",
        f"selected_root={selected_probe.root if selected_probe else ''}",
        f"selected_source={selected_probe.source if selected_probe else ''}",
    ]
    if selected_probe:
        stdout_lines.append(f"selected_cmake_home={selected_probe.cmake_home_directory}")

    return ValidationResult(
        name=CONTROL_APPS_PREREQUISITE_CASE_ID,
        case_id=CONTROL_APPS_PREREQUISITE_CASE_ID,
        layer="static",
        description="control-apps current-workspace build root readiness",
        status="passed" if readiness.ready else "known_failure",
        return_code=0 if readiness.ready else KNOWN_FAILURE_EXIT_CODE,
        duration_seconds=0.0,
        command=[],
        cwd=str(WORKSPACE_ROOT),
        stdout="\n".join(stdout_lines),
        note="" if readiness.ready else f"{readiness.reason}: {', '.join(readiness.missing_artifacts)}",
        owner_scope="root-validation",
        primary_layer="L0",
        suite_ref="apps",
        evidence_profile="gate-summary",
        stability_state="stable",
        size_label="small",
        label_refs=("suite:apps", "kind:entrypoint", "size:small", "layer:L0", "owner:apps", "stability:stable"),
        risk_tags=("entrypoint",),
        required_fixtures=("fixture.workspace-validation-runner",),
    )


def _control_apps_blocked_skip_result(case: ValidationCase) -> ValidationResult:
    return ValidationResult(
        name=case.name,
        case_id=case.case_id,
        layer=case.layer,
        description=case.description,
        status="skipped",
        return_code=None,
        duration_seconds=0.0,
        command=case.command,
        cwd=str(case.cwd),
        note=f"blocked by {CONTROL_APPS_PREREQUISITE_CASE_ID}",
        owner_scope=case.owner_scope,
        primary_layer=case.primary_layer,
        suite_ref=case.suite_ref,
        risk_tags=case.risk_tags,
        required_assets=case.required_assets,
        required_fixtures=case.required_fixtures,
        evidence_profile=case.evidence_profile,
        stability_state=case.stability_state,
        size_label=case.size_label,
        label_refs=case.label_refs,
    )


def _primary_layer_for_case(case: ValidationCase) -> str:
    if case.primary_layer:
        return case.primary_layer
    if case.name in {"hardware-smoke", "hil-closed-loop", "hil-case-matrix"}:
        return "L5"
    if case.layer == "static":
        return "L0"
    if case.layer == "unit":
        return "L1"
    if case.layer in {"contracts", "protocol-compatibility", "freeze"}:
        return "L2"
    if case.layer == "integration":
        return "L3"
    if case.layer == "e2e":
        return "L4"
    if case.layer == "performance":
        return "L6"
    return "L0"


def _owner_scope_for_case(case: ValidationCase) -> str:
    if case.owner_scope and case.owner_scope != "shared/testing":
        return case.owner_scope
    if case.layer == "static":
        return "root-validation"
    if case.layer in {"apps", "unit"}:
        return "apps"
    if case.layer in {"contracts", "freeze"}:
        return "shared/contracts"
    if case.layer == "protocol-compatibility":
        return "tests/integration"
    if case.layer == "integration":
        return "tests/integration"
    if case.name == "simulated-line":
        return "runtime-execution"
    if case.name in {"hardware-smoke", "hil-closed-loop", "hil-case-matrix"}:
        return "runtime-execution"
    if case.layer == "performance":
        return "tests/performance"
    return "tests"


def _suite_ref_for_case(case: ValidationCase) -> str:
    if case.suite_ref:
        return case.suite_ref
    if case.layer == "static":
        return "static"
    if case.layer in {"freeze", "contracts"}:
        return "contracts"
    if case.layer == "unit":
        return "contracts"
    if case.layer == "protocol-compatibility":
        return "protocol-compatibility"
    if case.layer == "integration":
        return "integration"
    if case.layer == "performance":
        return "performance"
    if case.layer == "apps":
        return "apps"
    return "e2e"


def _size_label_for_case(case: ValidationCase) -> str:
    if case.size_label:
        return case.size_label
    suite_ref = _suite_ref_for_case(case)
    suite_definition = SUITE_TAXONOMY.get(suite_ref)
    if suite_definition:
        return suite_definition.default_size_label
    return "medium"


def _label_refs_for_case(case: ValidationCase) -> tuple[str, ...]:
    if case.label_refs:
        return case.label_refs
    suite_ref = _suite_ref_for_case(case)
    labels: list[str] = []
    for label_ref in suite_label_refs(suite_ref):
        if label_ref not in labels:
            labels.append(label_ref)
    primary_layer = _primary_layer_for_case(case)
    size_label = _size_label_for_case(case)
    owner_scope = _owner_scope_for_case(case)
    stability_state = case.stability_state or "stable"
    for derived_label in (
        f"layer:{primary_layer}",
        f"size:{size_label}",
        f"owner:{owner_scope}",
        f"stability:{stability_state}",
    ):
        if derived_label not in labels:
            labels.append(derived_label)
    return tuple(labels)


def _risk_tags_for_case(case: ValidationCase) -> tuple[str, ...]:
    if case.risk_tags:
        return case.risk_tags
    if case.name in {"hardware-smoke", "hil-closed-loop", "hil-case-matrix"}:
        return ("hardware", "state-machine")
    if case.name == "simulated-line":
        return ("acceptance", "regression")
    if case.layer == "performance":
        return ("performance", "single-flight")
    if case.layer == "integration":
        return ("integration", "workflow")
    if case.layer == "protocol-compatibility":
        return ("contract", "protocol")
    if case.layer in {"contracts", "unit", "freeze"}:
        return ("contract",)
    if case.layer == "apps":
        return ("entrypoint",)
    return ("validation",)


def _required_assets_for_case(case: ValidationCase) -> tuple[str, ...]:
    if case.required_assets:
        return case.required_assets
    if case.name == "simulated-line":
        return default_simulated_line_asset_ids(WORKSPACE_ROOT)
    if case.name == "fault-matrix-smoke":
        return (
            "sample.simulation.invalid_empty_segments",
            "baseline.simulation.invalid_empty_segments",
            "sample.simulation.following_error_quantized",
            "baseline.simulation.following_error_quantized",
        )
    if case.name in {"hardware-smoke", "hil-closed-loop", "hil-case-matrix"}:
        return ("sample.dxf.rect_diag",)
    if case.name == "performance-baseline-collection":
        return default_performance_sample_asset_ids(WORKSPACE_ROOT)
    if case.name == "shared-asset-reuse-smoke":
        return shared_asset_ids_for_smoke(WORKSPACE_ROOT)
    if case.name == "baseline-governance-smoke":
        return (
            "baseline.simulation.compat_rect_diag",
            "baseline.simulation.scheme_c_rect_diag",
            "baseline.preview.rect_diag_snapshot",
        )
    return ()


def _required_fixtures_for_case(case: ValidationCase) -> tuple[str, ...]:
    if case.required_fixtures:
        return case.required_fixtures
    if case.name == "layered-validation-routing-smoke":
        return ("fixture.layered-routing-smoke", "fixture.validation-evidence-bundle")
    if case.name == "shared-asset-reuse-smoke":
        return ("fixture.shared-asset-reuse-smoke", "fixture.validation-evidence-bundle")
    if case.name == "baseline-governance-smoke":
        return ("fixture.baseline-governance-smoke", "fixture.validation-evidence-bundle")
    if case.name == "fault-matrix-smoke":
        return ("fixture.fault-matrix-smoke", "fixture.validation-evidence-bundle")
    if case.name == "simulated-line":
        return ("fixture.simulated-line-regression", "fixture.validation-evidence-bundle")
    if case.name in {"hardware-smoke", "hil-closed-loop", "hil-case-matrix"}:
        return ("fixture.hil-closed-loop", "fixture.validation-evidence-bundle")
    if case.layer == "performance":
        return ("fixture.dxf-preview-profiler", "fixture.validation-evidence-bundle")
    return ("fixture.workspace-validation-runner",)


def _evidence_profile_for_case(case: ValidationCase) -> str:
    if case.evidence_profile != "workspace-validation":
        return case.evidence_profile
    primary_layer = _primary_layer_for_case(case)
    if primary_layer == "L5":
        return "hil-report"
    if primary_layer == "L6":
        return "performance-report"
    if primary_layer == "L4":
        return "simulated-e2e-report"
    if primary_layer == "L3":
        return "integration-report"
    if primary_layer == "L2":
        return "contract-report"
    if primary_layer == "L1":
        return "unit-report"
    return "gate-summary"


def _enrich_cases(cases: list[ValidationCase]) -> list[ValidationCase]:
    enriched: list[ValidationCase] = []
    for case in cases:
        enriched.append(
            replace(
                case,
                case_id=case.case_id or case.name,
                owner_scope=_owner_scope_for_case(case),
                primary_layer=_primary_layer_for_case(case),
                suite_ref=_suite_ref_for_case(case),
                risk_tags=_risk_tags_for_case(case),
                required_assets=_required_assets_for_case(case),
                required_fixtures=_required_fixtures_for_case(case),
                evidence_profile=_evidence_profile_for_case(case),
                size_label=_size_label_for_case(case),
                label_refs=_label_refs_for_case(case),
            )
        )
    return enriched


def _apply_lane_policy(cases: list[ValidationCase], lane_ref: str) -> list[ValidationCase]:
    lane_definition = EXECUTION_LANES[lane_ref]
    applied: list[ValidationCase] = []
    for case in cases:
        applied.append(
            replace(
                case,
                timeout_seconds=case.timeout_seconds or lane_definition.timeout_budget_seconds,
                retry_budget=max(case.retry_budget, lane_definition.retry_budget),
            )
        )
    return applied


def _validate_case_taxonomy(cases: list[ValidationCase]) -> None:
    missing: list[str] = []
    for case in cases:
        if case.suite_ref not in SUITE_TAXONOMY:
            missing.append(f"{case.name}: unsupported suite_ref={case.suite_ref}")
        if case.size_label not in SIZE_LABELS:
            missing.append(f"{case.name}: invalid size_label={case.size_label}")
        if not case.label_refs:
            missing.append(f"{case.name}: missing label_refs")
        if not case.case_id or not case.owner_scope or not case.primary_layer:
            missing.append(f"{case.name}: missing required case metadata")
    if missing:
        raise ValueError("case taxonomy metadata is incomplete: " + "; ".join(missing))


def build_cases(
    profile: str,
    suites: list[str],
    *,
    lane_ref: str,
    include_hardware_smoke: bool = False,
    include_hil_closed_loop: bool = False,
    include_hil_case_matrix: bool = False,
    report_dir: Path,
) -> list[ValidationCase]:
    local_profile = profile == "local"
    resolved_report_dir = report_dir.resolve()
    freeze_report_dir = _resolve_freeze_report_dir(resolved_report_dir)
    legacy_exit_report_dir = _resolve_legacy_exit_report_dir(resolved_report_dir)
    freeze_evidence_cases = _freeze_evidence_cases()
    hil_offline_prereq_report = os.getenv("SILIGEN_HIL_OFFLINE_PREREQ_REPORT", "").strip()
    hil_operator_override_reason = os.getenv("SILIGEN_HIL_OPERATOR_OVERRIDE_REASON", "").strip()
    performance_gate_mode = "nightly-performance" if lane_ref in {"nightly-performance", "full-offline-gate"} else "adhoc"

    cases: list[ValidationCase] = []

    static_report_dir = (WORKSPACE_ROOT / "tests" / "reports" / "static").resolve()
    cases.extend(
        [
            ValidationCase(
                name="pyright-static",
                layer="static",
                description="Python static type gate",
                command=_powershell_file_command(
                    WORKSPACE_ROOT / "scripts" / "validation" / "run-pyright-gate.ps1",
                    "-ReportDir",
                    str(static_report_dir),
                ),
                cwd=WORKSPACE_ROOT,
                suite_ref="static",
                owner_scope="root-validation",
                size_label="small",
            ),
            ValidationCase(
                name="no-loose-mock",
                layer="static",
                description="core boundary mock strictness gate",
                command=[
                    *python_command(WORKSPACE_ROOT / "scripts" / "testing" / "check_no_loose_mock.py"),
                    "--report-dir",
                    str(static_report_dir),
                ],
                cwd=WORKSPACE_ROOT,
                suite_ref="static",
                owner_scope="root-validation",
                size_label="small",
            ),
        ]
    )

    if "apps" in suites:
        cases.extend(
            [
                ValidationCase(
                    name="runtime-service-dry-run",
                    layer="apps",
                    description="runtime-service dry-run",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "runtime-service" / "run.ps1",
                        "-DryRun",
                        "-SkipPreflight",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="runtime-gateway-dry-run",
                    layer="apps",
                    description="runtime-gateway dry-run",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "runtime-gateway" / "run.ps1",
                        "-DryRun",
                        "-SkipPreflight",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="planner-cli-dry-run",
                    layer="apps",
                    description="planner-cli dry-run",
                    command=_powershell_file_command(WORKSPACE_ROOT / "apps" / "planner-cli" / "run.ps1", "-DryRun"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="hmi-app-offline-smoke",
                    layer="apps",
                    description="hmi-app 离线 smoke",
                    command=_powershell_file_command(WORKSPACE_ROOT / "apps" / "hmi-app" / "scripts" / "offline-smoke.ps1"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="hmi-app-unit",
                    layer="unit",
                    description="hmi-app unit tests",
                    command=_powershell_file_command(WORKSPACE_ROOT / "apps" / "hmi-app" / "scripts" / "test.ps1"),
                    cwd=WORKSPACE_ROOT,
                    suite_ref="apps",
                ),
                ValidationCase(
                    name="hmi-application-preview-session-unit",
                    layer="unit",
                    description="hmi-application preview session unit tests",
                    command=[
                        *python_command(
                            WORKSPACE_ROOT / "modules" / "hmi-application" / "tests" / "unit" / "test_preview_session.py"
                        ),
                        "-q",
                    ],
                    cwd=WORKSPACE_ROOT,
                    suite_ref="apps",
                ),
            ]
        )

    if "contracts" in suites:
        cases.extend(
            [
                ValidationCase(
                    name="application-contracts",
                    layer="contracts",
                    description="application contracts compatibility",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_protocol_compatibility.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="engineering-contracts",
                    layer="contracts",
                    description="engineering contracts compatibility",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_engineering_contracts.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="engineering-data",
                    layer="contracts",
                    description="engineering data compatibility",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_engineering_data_compatibility.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="control-apps-build-contract",
                    layer="contracts",
                    description="control-apps build root readiness contract",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_control_apps_build_contract.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="freeze-docset-contract",
                    layer="contracts",
                    description="formal freeze docset contract",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_freeze_docset_contract.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="migration-alignment-matrix",
                    layer="contracts",
                    description="migration alignment matrix contract",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_migration_alignment_matrix.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="bridge-exit-contract",
                    layer="contracts",
                    description="bridge exit contract",
                    command=python_command(WORKSPACE_ROOT / "tests" / "contracts" / "test_bridge_exit_contract.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="layered-validation-routing-contract",
                    layer="contracts",
                    description="layered validation routing contract",
                    command=python_command(
                        WORKSPACE_ROOT / "tests" / "contracts" / "test_layered_validation_contract.py"
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="shared-test-assets-contract",
                    layer="contracts",
                    description="shared test assets and fixtures contract",
                    command=python_command(
                        WORKSPACE_ROOT / "tests" / "contracts" / "test_shared_test_assets_contract.py"
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="validation-evidence-bundle-contract",
                    layer="contracts",
                    description="validation evidence bundle contract",
                    command=python_command(
                        WORKSPACE_ROOT / "tests" / "contracts" / "test_validation_evidence_bundle_contract.py"
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="performance-threshold-gate-contract",
                    layer="contracts",
                    description="performance threshold gate contract",
                    command=python_command(
                        WORKSPACE_ROOT / "tests" / "contracts" / "test_performance_threshold_gate_contract.py"
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="fault-scenario-contract",
                    layer="contracts",
                    description="fault scenario schema contract",
                    command=python_command(
                        WORKSPACE_ROOT / "tests" / "contracts" / "test_fault_scenario_contract.py"
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="legacy-exit",
                    layer="contracts",
                    description="legacy and bridge exit checks",
                    command=[
                        *python_command(WORKSPACE_ROOT / "scripts" / "migration" / "legacy-exit-checks.py"),
                        "--profile",
                        profile,
                        "--report-dir",
                        str(legacy_exit_report_dir),
                    ],
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="workspace-layout-boundary",
                    layer="contracts",
                    description="workspace layout gate",
                    command=python_command(WORKSPACE_ROOT / "scripts" / "migration" / "validate_workspace_layout.py"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="dsp-e2e-spec-docset",
                    layer="freeze",
                    description="dsp-e2e-spec docset gate",
                    command=_freeze_validator_command(report_dir=freeze_report_dir, report_stem="dsp-e2e-spec-docset"),
                    cwd=WORKSPACE_ROOT,
                ),
                ValidationCase(
                    name="shared-freeze-boundary",
                    layer="contracts",
                    description="shared freeze boundary gate",
                    command=python_command(WORKSPACE_ROOT / "scripts" / "migration" / "validate_shared_freeze.py"),
                    cwd=WORKSPACE_ROOT,
                ),
            ]
        )

        for scenario_class in freeze_evidence_cases:
            cases.append(
                ValidationCase(
                    name=f"dsp-e2e-spec-{scenario_class}",
                    layer="freeze",
                    description=f"dsp-e2e-spec {scenario_class} evidence case",
                    command=_freeze_validator_command(
                        report_dir=freeze_report_dir,
                        report_stem=f"dsp-e2e-spec-{scenario_class}",
                        scenario_classes=(scenario_class,),
                    ),
                    cwd=WORKSPACE_ROOT,
                )
            )

        if local_profile:
            cases.extend(
                [
                    ValidationCase(
                        name="shared-kernel-unit",
                        layer="unit",
                        description="shared-kernel unit tests",
                        command=[str(_control_apps_executable("siligen_shared_kernel_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                    ValidationCase(
                        name="runtime-host-unit",
                        layer="unit",
                        description="runtime-host unit tests",
                        command=[str(_control_apps_executable("siligen_runtime_host_unit_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                    ValidationCase(
                        name="dxf-geometry-unit",
                        layer="unit",
                        description="dxf-geometry unit tests",
                        command=[str(_control_apps_executable("siligen_dxf_geometry_unit_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                    ValidationCase(
                        name="job-ingest-unit",
                        layer="unit",
                        description="job-ingest unit tests",
                        command=[str(_control_apps_executable("siligen_job_ingest_unit_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                    ValidationCase(
                        name="workflow-unit",
                        layer="unit",
                        description="workflow core unit tests",
                        command=[str(_control_apps_executable("siligen_unit_tests.exe"))],
                        cwd=WORKSPACE_ROOT,
                        allow_missing=True,
                    ),
                ]
            )

    if "integration" in suites:
        cases.extend(
            [
                ValidationCase(
                    name="engineering-regression",
                    layer="integration",
                    description="engineering regression scenarios",
                    command=[
                        *python_command(
                            WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "run_engineering_regression.py"
                        ),
                        "--report-dir",
                        str(resolved_report_dir / "integration" / "engineering-regression"),
                    ],
                    cwd=WORKSPACE_ROOT,
                    required_assets=default_engineering_regression_asset_ids(WORKSPACE_ROOT),
                    required_fixtures=("fixture.validation-evidence-bundle",),
                ),
                ValidationCase(
                    name="preview-flow-integration",
                    layer="integration",
                    description="DXF import -> preview snapshot -> app state update integration",
                    command=[
                        *python_command(
                            WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "run_preview_flow_regression.py"
                        ),
                        "--report-dir",
                        str(resolved_report_dir / "integration" / "preview-flow-integration"),
                    ],
                    cwd=WORKSPACE_ROOT,
                    required_assets=("sample.dxf.rect_diag",),
                    required_fixtures=("fixture.validation-evidence-bundle",),
                ),
                ValidationCase(
                    name="tcp-precondition-matrix",
                    layer="integration",
                    description="preview confirm / execution preflight blocking matrix",
                    command=[
                        *python_command(
                            WORKSPACE_ROOT
                            / "tests"
                            / "integration"
                            / "scenarios"
                            / "first-layer"
                            / "run_tcp_precondition_matrix.py"
                        ),
                        "--report-dir",
                        str(resolved_report_dir / "integration" / "tcp-precondition-matrix"),
                        *_canonical_recipe_context_args(),
                        "--allow-skip-on-missing-gateway",
                    ],
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                    skipped_exit_codes=(SKIPPED_EXIT_CODE,),
                    required_assets=("sample.dxf.rect_diag",),
                    required_fixtures=("fixture.tcp-precondition-matrix", "fixture.validation-evidence-bundle"),
                ),
                ValidationCase(
                    name="hmi-app-online-smoke",
                    layer="integration",
                    description="gateway startup / online connection / runtime actions smoke",
                    command=_powershell_file_command(
                        WORKSPACE_ROOT / "apps" / "hmi-app" / "scripts" / "online-smoke.ps1",
                        "-TimeoutMs",
                        "45000",
                        "-ExerciseRuntimeActions",
                    ),
                    cwd=WORKSPACE_ROOT,
                ),
            ]
        )

    if "e2e" in suites:
        cases.extend(
            [
                ValidationCase(
                    name="simulated-line",
                    layer="e2e",
                    description="simulated-line regression scenarios",
                    command=[
                        *python_command(WORKSPACE_ROOT / "tests" / "e2e" / "simulated-line" / "run_simulated_line.py"),
                        "--report-dir",
                        str(resolved_report_dir / "e2e" / "simulated-line"),
                    ],
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                    skipped_exit_codes=(SKIPPED_EXIT_CODE,),
                ),
                ValidationCase(
                    name="simulated-line-matrix",
                    layer="e2e",
                    description="simulated-line abnormal path matrix",
                    command=[
                        *python_command(
                            WORKSPACE_ROOT / "tests" / "e2e" / "simulated-line" / "run_simulated_line_matrix.py"
                        ),
                        "--report-dir",
                        str(resolved_report_dir / "e2e" / "simulated-line-matrix"),
                    ],
                    cwd=WORKSPACE_ROOT,
                    required_assets=(
                        "sample.simulation.invalid_empty_segments",
                        "baseline.simulation.invalid_empty_segments",
                        "sample.simulation.following_error_quantized",
                        "baseline.simulation.following_error_quantized",
                    ),
                    required_fixtures=(
                        "fixture.simulated-line-matrix",
                        "fixture.simulated-line-regression",
                        "fixture.validation-evidence-bundle",
                    ),
                ),
            ]
        )

        if include_hardware_smoke:
            cases.append(
                ValidationCase(
                    name="hardware-smoke",
                    layer="e2e",
                    description="hardware smoke",
                    command=[
                        *python_command(WORKSPACE_ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hardware_smoke.py"),
                        "--report-dir",
                        str(resolved_report_dir / "hardware-smoke"),
                    ],
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                    skipped_exit_codes=(SKIPPED_EXIT_CODE,),
                )
            )

        if include_hil_closed_loop:
            cases.append(
                ValidationCase(
                    name="hil-closed-loop",
                    layer="e2e",
                    description="HIL 闭环动作与长稳探针",
                    command=[
                        *python_command(WORKSPACE_ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_hil_closed_loop.py"),
                        "--report-dir",
                        str(resolved_report_dir),
                    ],
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                    skipped_exit_codes=(SKIPPED_EXIT_CODE,),
                )
            )
            if hil_offline_prereq_report:
                cases[-1].command.extend(["--offline-prereq-report", hil_offline_prereq_report])
            if hil_operator_override_reason:
                cases[-1].command.extend(["--operator-override-reason", hil_operator_override_reason])

        if include_hil_case_matrix:
            cases.append(
                ValidationCase(
                    name="hil-case-matrix",
                    layer="e2e",
                    description="HIL home/closed_loop online case matrix",
                    command=[
                        *python_command(WORKSPACE_ROOT / "tests" / "e2e" / "hardware-in-loop" / "run_case_matrix.py"),
                        "--report-dir",
                        str(resolved_report_dir / "hil-case-matrix"),
                    ],
                    cwd=WORKSPACE_ROOT,
                    known_failure_exit_codes=(KNOWN_FAILURE_EXIT_CODE,),
                    skipped_exit_codes=(SKIPPED_EXIT_CODE,),
                )
            )
            if hil_offline_prereq_report:
                cases[-1].command.extend(["--offline-prereq-report", hil_offline_prereq_report])
            if hil_operator_override_reason:
                cases[-1].command.extend(["--operator-override-reason", hil_operator_override_reason])

    if "protocol-compatibility" in suites:
        cases.append(
            ValidationCase(
                name="protocol-compatibility",
                layer="protocol-compatibility",
                description="application/engineering protocol compatibility",
                command=python_command(
                    WORKSPACE_ROOT / "tests" / "integration" / "protocol-compatibility" / "run_protocol_compatibility.py"
                ),
                cwd=WORKSPACE_ROOT,
            )
        )
        cases.append(
            ValidationCase(
                name="lane-policy-smoke",
                layer="protocol-compatibility",
                description="lane matrix and gate policy smoke",
                command=python_command(
                    WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "run_lane_policy_smoke.py"
                ),
                cwd=WORKSPACE_ROOT,
            )
        )
        cases.append(
            ValidationCase(
                name="layered-validation-routing-smoke",
                layer="protocol-compatibility",
                description="layered validation routing smoke",
                command=python_command(
                    WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "run_layered_validation_smoke.py"
                ),
                cwd=WORKSPACE_ROOT,
            )
        )
        cases.append(
            ValidationCase(
                name="shared-asset-reuse-smoke",
                layer="protocol-compatibility",
                description="shared asset reuse smoke",
                command=python_command(
                    WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "run_shared_asset_reuse_smoke.py"
                ),
                cwd=WORKSPACE_ROOT,
            )
        )
        cases.append(
            ValidationCase(
                name="baseline-governance-smoke",
                layer="protocol-compatibility",
                description="baseline manifest and second-source governance smoke",
                command=python_command(
                    WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "run_baseline_governance_smoke.py"
                ),
                cwd=WORKSPACE_ROOT,
            )
        )
        cases.append(
            ValidationCase(
                name="fault-matrix-smoke",
                layer="protocol-compatibility",
                description="cross-owner simulated fault matrix smoke",
                command=python_command(
                    WORKSPACE_ROOT / "tests" / "integration" / "scenarios" / "run_fault_matrix_smoke.py"
                ),
                cwd=WORKSPACE_ROOT,
            )
        )

    if "performance" in suites:
        performance_command = [
            *python_command(WORKSPACE_ROOT / "tests" / "performance" / "collect_dxf_preview_profiles.py"),
            "--report-dir",
            str(resolved_report_dir / "performance" / "dxf-preview-profiles"),
            *_canonical_recipe_context_args(),
            "--gate-mode",
            performance_gate_mode,
            "--threshold-config",
            str(PERFORMANCE_THRESHOLD_CONFIG),
        ]
        if lane_ref == "nightly-performance":
            performance_command.extend(
                [
                    "--include-start-job",
                    "--include-control-cycles",
                    "--pause-resume-cycles",
                    "3",
                    "--stop-reset-rounds",
                    "3",
                    "--long-run-minutes",
                    "5",
                ]
            )
        cases.append(
            ValidationCase(
                name="performance-baseline-collection",
                layer="performance",
                description="collect performance baselines",
                command=performance_command,
                cwd=WORKSPACE_ROOT,
            )
        )

    return _enrich_cases(cases)


def _build_case_records(
    report: ValidationReport,
    *,
    request_ref: str,
    lane_ref: str,
    summary_json_path: Path,
) -> list[EvidenceCaseRecord]:
    records: list[EvidenceCaseRecord] = []
    for result in report.results:
        records.append(
            EvidenceCaseRecord(
                case_id=result.case_id or result.name,
                name=result.name,
                suite_ref=result.suite_ref or result.layer,
                owner_scope=result.owner_scope,
                primary_layer=result.primary_layer or result.layer,
                producer_lane_ref=lane_ref,
                status=result.status,
                evidence_profile=result.evidence_profile,
                stability_state=result.stability_state,
                size_label=result.size_label,
                label_refs=result.label_refs,
                risk_tags=result.risk_tags,
                required_assets=result.required_assets,
                required_fixtures=result.required_fixtures,
                note=result.note,
                trace_fields=trace_fields(
                    stage_id=result.primary_layer or result.layer,
                    artifact_id=result.case_id or result.name,
                    module_id=result.owner_scope,
                    workflow_state="executed",
                    execution_state=result.status,
                    event_name=result.name,
                    failure_code=f"validation.{result.status}" if result.status != "passed" else "",
                    evidence_path=str(summary_json_path.resolve()),
                ),
            )
        )
    return records


def _write_workspace_evidence_bundle(
    *,
    report: ValidationReport,
    routed_request,
    report_dir: Path,
    summary_json_path: Path,
    summary_md_path: Path,
) -> dict[str, str]:
    bundle = EvidenceBundle(
        bundle_id=f"workspace-validation-{routed_request.request.request_id}",
        request_ref=routed_request.request.request_id,
        producer_lane_ref=routed_request.selected_lane_ref,
        report_root=str(report_dir.resolve()),
        summary_file=str(summary_md_path.resolve()),
        machine_file=str(summary_json_path.resolve()),
        verdict=infer_verdict([result.status for result in report.results]),
        linked_asset_refs=tuple(
            sorted(
                {
                    asset_id
                    for result in report.results
                    for asset_id in result.required_assets
                }
            )
        ),
        skipped_layer_refs=routed_request.skipped_layer_refs,
        skip_justification=routed_request.request.skip_justification,
        offline_prerequisites=tuple(
            layer_id
            for layer_id in routed_request.selected_layer_refs
            if layer_id in {"L0", "L1", "L2", "L3", "L4"}
        ),
        metadata={
            "workspace_validation_metadata": report.metadata,
        },
        case_records=_build_case_records(
            report,
            request_ref=routed_request.request.request_id,
            lane_ref=routed_request.selected_lane_ref,
            summary_json_path=summary_json_path,
        ),
    )
    links = [
        EvidenceLink(label="workspace-validation.json", path=str(summary_json_path.resolve()), role="machine-summary"),
        EvidenceLink(label="workspace-validation.md", path=str(summary_md_path.resolve()), role="human-summary"),
        EvidenceLink(
            label="docs/validation/layered-test-matrix.md",
            path=str((WORKSPACE_ROOT / "docs" / "validation" / "layered-test-matrix.md").resolve()),
            role="matrix",
        ),
        EvidenceLink(
            label="tests/reports/README.md",
            path=str((WORKSPACE_ROOT / "tests" / "reports" / "README.md").resolve()),
            role="report-root-guide",
        ),
    ]
    return write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=summary_json_path,
        summary_md_path=summary_md_path,
        evidence_links=links,
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run workspace validation suites and write a report.")
    parser.add_argument("--report-dir", default=str(_default_report_dir()))
    parser.add_argument("--profile", choices=("local", "ci"), default="local")
    parser.add_argument(
        "--suite",
        action="append",
        choices=("all", "apps", "contracts", "protocol-compatibility", "integration", "e2e", "performance"),
        help="One or more suites to run. Default is all.",
    )
    parser.add_argument("--request-id", default="")
    parser.add_argument("--changed-scope", action="append", default=[])
    parser.add_argument("--lane", choices=("auto", "quick-gate", "full-offline-gate", "nightly-performance", "limited-hil"), default="auto")
    parser.add_argument("--risk-profile", choices=("low", "medium", "high", "hardware-sensitive"), default="medium")
    parser.add_argument("--desired-depth", choices=("auto", "quick", "full-offline", "nightly", "hil"), default="auto")
    parser.add_argument("--skip-layer", action="append", default=[])
    parser.add_argument("--skip-justification", default="")
    parser.add_argument("--include-hardware-smoke", action="store_true")
    parser.add_argument("--include-hil-closed-loop", action="store_true")
    parser.add_argument("--include-hil-case-matrix", action="store_true")
    parser.add_argument("--fail-on-known-failure", action="store_true")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    suites = _normalize_suites(args.suite)
    desired_depth = args.desired_depth
    if desired_depth == "auto":
        if args.include_hardware_smoke or args.include_hil_closed_loop or args.include_hil_case_matrix:
            desired_depth = "hil"
        elif "performance" in suites:
            desired_depth = "nightly"
        elif args.profile == "ci" or "integration" in suites or "e2e" in suites:
            desired_depth = "full-offline"
        else:
            desired_depth = "quick"

    request = build_request(
        requested_suites=suites,
        changed_scopes=args.changed_scope,
        trigger_source="ci" if args.profile == "ci" else "local",
        risk_profile=args.risk_profile,
        desired_depth=desired_depth,
        request_id=args.request_id,
        explicit_lane=args.lane,
        include_hardware_smoke=bool(args.include_hardware_smoke),
        include_hil_closed_loop=bool(args.include_hil_closed_loop),
        include_hil_case_matrix=bool(args.include_hil_case_matrix),
        skip_layers=args.skip_layer,
        skip_justification=args.skip_justification,
    )
    routed_request = route_validation_request(request)
    lane_definition = EXECUTION_LANES[routed_request.selected_lane_ref]
    cases = _apply_lane_policy(
        build_cases(
            args.profile,
            suites,
            lane_ref=routed_request.selected_lane_ref,
            include_hardware_smoke=bool(args.include_hardware_smoke),
            include_hil_closed_loop=bool(args.include_hil_closed_loop),
            include_hil_case_matrix=bool(args.include_hil_case_matrix),
            report_dir=report_dir,
        ),
        routed_request.selected_lane_ref,
    )
    _validate_case_taxonomy(cases)
    control_apps_readiness = probe_control_apps_build_readiness(
        WORKSPACE_ROOT,
        required_artifacts=_required_control_apps_artifacts(cases),
        explicit_build_root=os.getenv("SILIGEN_CONTROL_APPS_BUILD_ROOT"),
    )
    metadata = _workspace_validation_metadata(control_apps_readiness)
    asset_catalog = build_asset_catalog(WORKSPACE_ROOT)
    asset_inventory = build_asset_inventory(WORKSPACE_ROOT, catalog=asset_catalog)
    baseline_governance = baseline_governance_summary(WORKSPACE_ROOT, catalog=asset_catalog)
    fault_matrix = build_fault_matrix(WORKSPACE_ROOT, consumer_scope="runtime-execution")
    metadata.update(asset_catalog.canonical_roots())
    metadata["canonical_asset_inventory"] = asset_inventory
    metadata["baseline_governance_summary"] = baseline_governance
    metadata["canonical_fault_matrix_id"] = fault_matrix["matrix_id"]
    metadata["fault_matrix_owner_scopes"] = fault_matrix["owner_scopes"]
    metadata["fault_matrix_fault_ids"] = sorted(fault_matrix["faults"])
    metadata["baseline_manifest_entry_count"] = baseline_governance["entry_count"]
    metadata["baseline_blocking_issue_count"] = baseline_governance["blocking_issue_count"]
    metadata["baseline_advisory_issue_count"] = baseline_governance["advisory_issue_count"]
    metadata["baseline_duplicate_source_issue_count"] = len(baseline_governance["duplicate_source_issues"])
    metadata["baseline_deprecated_root_issue_count"] = len(baseline_governance["deprecated_root_issues"])
    metadata["baseline_unused_issue_count"] = len(baseline_governance["unused_baseline_paths"])
    metadata["baseline_stale_issue_count"] = len(baseline_governance["stale_baseline_ids"])
    metadata.update(routed_request.to_metadata())
    report = ValidationReport(
        generated_at=datetime.now(timezone.utc).isoformat(),
        workspace_root=str(WORKSPACE_ROOT),
        metadata=metadata,
    )

    fail_fast_triggered = False
    skipped_due_to_fail_fast = 0
    blocking_failure_count = 0
    if control_apps_readiness.required_artifacts:
        report.results.append(_control_apps_readiness_result(control_apps_readiness))
    for case in cases:
        if (not control_apps_readiness.ready) and case.name in CONTROL_APPS_CASE_ARTIFACTS:
            report.results.append(_control_apps_blocked_skip_result(case))
        else:
            report.results.append(run_case(case))
        current_result = report.results[-1]
        if current_result.status == "failed":
            blocking_failure_count += 1
        if (
            lane_definition.default_fail_policy == "fail-fast"
            and lane_definition.fail_fast_case_limit > 0
            and blocking_failure_count >= lane_definition.fail_fast_case_limit
        ):
            fail_fast_triggered = True
            skipped_due_to_fail_fast = max(0, len(cases) - len(report.results))
            break

    report.metadata["selected_lane_gate_decision"] = lane_definition.gate_decision
    report.metadata["selected_lane_default_fail_policy"] = lane_definition.default_fail_policy
    report.metadata["selected_lane_timeout_budget_seconds"] = lane_definition.timeout_budget_seconds
    report.metadata["selected_lane_retry_budget"] = lane_definition.retry_budget
    report.metadata["selected_lane_fail_fast_case_limit"] = lane_definition.fail_fast_case_limit
    report.metadata["selected_lane_fail_fast_triggered"] = fail_fast_triggered
    report.metadata["selected_lane_skipped_case_count_due_to_fail_fast"] = skipped_due_to_fail_fast
    report.metadata["selected_lane_policy_snapshot"] = lane_policy_metadata(routed_request.selected_lane_ref)

    json_path = report_dir / "workspace-validation.json"
    md_path = report_dir / "workspace-validation.md"
    report_to_json(report, json_path)
    report_to_markdown(report, md_path)
    bundle_paths = _write_workspace_evidence_bundle(
        report=report,
        routed_request=routed_request,
        report_dir=report_dir,
        summary_json_path=json_path,
        summary_md_path=md_path,
    )

    counts = report.counts()
    print(
        "workspace validation complete: "
        f"profile={args.profile} "
        f"lane={routed_request.selected_lane_ref} "
        f"suites={','.join(suites)} "
        f"passed={counts.get('passed', 0)} "
        f"failed={counts.get('failed', 0)} "
        f"known_failure={counts.get('known_failure', 0)} "
        f"skipped={counts.get('skipped', 0)}"
    )
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    print(f"evidence bundle: {bundle_paths['bundle_json']}")
    return report.exit_code(fail_on_known_failure=bool(args.fail_on_known_failure))


if __name__ == "__main__":
    raise SystemExit(main())
