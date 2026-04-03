from __future__ import annotations

from dataclasses import asdict, dataclass, field
from datetime import datetime, timezone
from pathlib import Path


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


@dataclass(frozen=True)
class ValidationLayerDefinition:
    layer_id: str
    layer_kind: str
    owner_surface: str
    default_entry_refs: tuple[str, ...]
    allowed_environments: tuple[str, ...]
    upgrade_prerequisites: tuple[str, ...]
    required_evidence_profile: str
    default_enabled: bool = True


@dataclass(frozen=True)
class ExecutionLaneDefinition:
    lane_id: str
    lane_kind: str
    entrypoint_refs: tuple[str, ...]
    default_suite_set: tuple[str, ...]
    allowed_layer_refs: tuple[str, ...]
    report_root: str
    gate_decision: str
    default_fail_policy: str
    timeout_budget_seconds: int
    retry_budget: int
    fail_fast_case_limit: int
    human_approval_required: bool


@dataclass(frozen=True)
class ValidationCaseMetadata:
    case_id: str
    owner_scope: str
    primary_layer: str
    suite_ref: str
    risk_tags: tuple[str, ...] = ()
    required_assets: tuple[str, ...] = ()
    required_fixtures: tuple[str, ...] = ()
    evidence_profile: str = "workspace-validation"
    stability_state: str = "stable"
    size_label: str = ""
    label_refs: tuple[str, ...] = ()

    def to_dict(self) -> dict[str, object]:
        return asdict(self)


@dataclass(frozen=True)
class SuiteTaxonomyDefinition:
    suite_id: str
    primary_layer_refs: tuple[str, ...]
    default_size_label: str
    owner_surface: str
    allowed_lane_refs: tuple[str, ...]
    label_refs: tuple[str, ...]


@dataclass(frozen=True)
class ValidationRequest:
    request_id: str
    trigger_source: str
    changed_scopes: tuple[str, ...]
    risk_profile: str
    desired_depth: str
    requested_suites: tuple[str, ...]
    include_hardware_smoke: bool = False
    include_hil_closed_loop: bool = False
    include_hil_case_matrix: bool = False
    explicit_lane: str = "auto"
    skip_layers: tuple[str, ...] = ()
    skip_justification: str = ""
    requested_at: str = field(default_factory=utc_now)

    def to_dict(self) -> dict[str, object]:
        return asdict(self)


@dataclass(frozen=True)
class RoutedValidationRequest:
    request: ValidationRequest
    selected_lane_ref: str
    selected_layer_refs: tuple[str, ...]
    skipped_layer_refs: tuple[str, ...]
    deferred_layer_refs: tuple[str, ...]
    upgrade_recommendation: str
    upgrade_reason: str
    lane_was_auto_selected: bool

    def to_metadata(self) -> dict[str, object]:
        lane_definition = EXECUTION_LANES[self.selected_lane_ref]
        requested_suite_label_refs: list[str] = []
        for suite_id in self.request.requested_suites:
            for label_ref in suite_label_refs(suite_id):
                if label_ref not in requested_suite_label_refs:
                    requested_suite_label_refs.append(label_ref)
        return {
            "request_id": self.request.request_id,
            "trigger_source": self.request.trigger_source,
            "changed_scopes": list(self.request.changed_scopes),
            "risk_profile": self.request.risk_profile,
            "desired_depth": self.request.desired_depth,
            "requested_suites": list(self.request.requested_suites),
            "selected_lane_ref": self.selected_lane_ref,
            "selected_layer_refs": list(self.selected_layer_refs),
            "skipped_layer_refs": list(self.skipped_layer_refs),
            "deferred_layer_refs": list(self.deferred_layer_refs),
            "skip_justification": self.request.skip_justification,
            "upgrade_recommendation": self.upgrade_recommendation,
            "upgrade_reason": self.upgrade_reason,
            "lane_was_auto_selected": self.lane_was_auto_selected,
            "include_hardware_smoke": self.request.include_hardware_smoke,
            "include_hil_closed_loop": self.request.include_hil_closed_loop,
            "include_hil_case_matrix": self.request.include_hil_case_matrix,
            "requested_at": self.request.requested_at,
            "requested_suite_label_refs": requested_suite_label_refs,
            "selected_lane_gate_decision": lane_definition.gate_decision,
            "selected_lane_default_fail_policy": lane_definition.default_fail_policy,
            "selected_lane_timeout_budget_seconds": lane_definition.timeout_budget_seconds,
            "selected_lane_retry_budget": lane_definition.retry_budget,
            "selected_lane_fail_fast_case_limit": lane_definition.fail_fast_case_limit,
            "selected_lane_default_suite_set": list(lane_definition.default_suite_set),
            "selected_lane_allowed_layer_refs": list(lane_definition.allowed_layer_refs),
        }


LAYER_SEQUENCE = ("L0", "L1", "L2", "L3", "L4", "L5", "L6")

SIZE_LABELS = ("small", "medium", "large")


VALIDATION_LAYERS: dict[str, ValidationLayerDefinition] = {
    "L0": ValidationLayerDefinition(
        layer_id="L0",
        layer_kind="static-and-build-gate",
        owner_surface="root build/test/ci + scripts/validation + static reports",
        default_entry_refs=(
            "build.ps1",
            "test.ps1",
            "ci.ps1",
            "scripts/validation/run-local-validation-gate.ps1",
            "pyrightconfig.json",
            "tools/testing/check_no_loose_mock.py",
        ),
        allowed_environments=("offline", "ci"),
        upgrade_prerequisites=(),
        required_evidence_profile="gate-summary",
    ),
    "L1": ValidationLayerDefinition(
        layer_id="L1",
        layer_kind="unit-tests",
        owner_surface="apps/*/tests/unit + modules/*/tests/unit",
        default_entry_refs=(
            "apps/hmi-app/tests/unit/",
            "modules/hmi-application/tests/unit/",
        ),
        allowed_environments=("offline", "ci"),
        upgrade_prerequisites=("L0",),
        required_evidence_profile="unit-report",
    ),
    "L2": ValidationLayerDefinition(
        layer_id="L2",
        layer_kind="module-contract-tests",
        owner_surface="tests/contracts/ + tests/integration/protocol-compatibility/",
        default_entry_refs=(
            "tests/contracts/",
            "tests/integration/protocol-compatibility/",
        ),
        allowed_environments=("offline", "ci"),
        upgrade_prerequisites=("L0", "L1"),
        required_evidence_profile="contract-report",
    ),
    "L3": ValidationLayerDefinition(
        layer_id="L3",
        layer_kind="integration-tests",
        owner_surface="tests/integration/scenarios/",
        default_entry_refs=(
            "tests/integration/scenarios/run_engineering_regression.py",
        ),
        allowed_environments=("offline", "simulated", "ci"),
        upgrade_prerequisites=("L0", "L1", "L2"),
        required_evidence_profile="integration-report",
    ),
    "L4": ValidationLayerDefinition(
        layer_id="L4",
        layer_kind="simulated-e2e",
        owner_surface="tests/e2e/simulated-line/",
        default_entry_refs=("tests/e2e/simulated-line/run_simulated_line.py",),
        allowed_environments=("simulated", "ci"),
        upgrade_prerequisites=("L0", "L1", "L2", "L3"),
        required_evidence_profile="simulated-e2e-report",
    ),
    "L5": ValidationLayerDefinition(
        layer_id="L5",
        layer_kind="hil-closed-loop",
        owner_surface="tests/e2e/hardware-in-loop/",
        default_entry_refs=(
            "tests/e2e/hardware-in-loop/run_hil_closed_loop.py",
            "tests/e2e/hardware-in-loop/run_case_matrix.py",
        ),
        allowed_environments=("hardware-limited",),
        upgrade_prerequisites=("L0", "L1", "L2", "L3", "L4"),
        required_evidence_profile="hil-report",
    ),
    "L6": ValidationLayerDefinition(
        layer_id="L6",
        layer_kind="performance-and-stability",
        owner_surface="tests/performance/",
        default_entry_refs=("tests/performance/collect_dxf_preview_profiles.py",),
        allowed_environments=("offline", "simulated", "ci", "hardware-limited"),
        upgrade_prerequisites=("L0", "L1", "L2", "L3", "L4"),
        required_evidence_profile="performance-report",
    ),
}


EXECUTION_LANES: dict[str, ExecutionLaneDefinition] = {
    "quick-gate": ExecutionLaneDefinition(
        lane_id="quick-gate",
        lane_kind="quick-gate",
        entrypoint_refs=("build.ps1", "test.ps1", "scripts/validation/run-local-validation-gate.ps1"),
        default_suite_set=("apps", "contracts", "protocol-compatibility"),
        allowed_layer_refs=("L0", "L1", "L2"),
        report_root="tests/reports/",
        gate_decision="blocking",
        default_fail_policy="fail-fast",
        timeout_budget_seconds=900,
        retry_budget=0,
        fail_fast_case_limit=1,
        human_approval_required=False,
    ),
    "full-offline-gate": ExecutionLaneDefinition(
        lane_id="full-offline-gate",
        lane_kind="full-offline-gate",
        entrypoint_refs=("test.ps1", "ci.ps1"),
        default_suite_set=("apps", "contracts", "protocol-compatibility", "integration", "e2e"),
        allowed_layer_refs=("L0", "L1", "L2", "L3", "L4"),
        report_root="tests/reports/",
        gate_decision="blocking",
        default_fail_policy="collect-and-report",
        timeout_budget_seconds=2700,
        retry_budget=1,
        fail_fast_case_limit=0,
        human_approval_required=False,
    ),
    "nightly-performance": ExecutionLaneDefinition(
        lane_id="nightly-performance",
        lane_kind="nightly-performance",
        entrypoint_refs=("ci.ps1", "tests/performance/collect_dxf_preview_profiles.py"),
        default_suite_set=("contracts", "protocol-compatibility", "integration", "e2e", "performance"),
        allowed_layer_refs=("L0", "L1", "L2", "L3", "L4", "L6"),
        report_root="tests/reports/performance/",
        gate_decision="blocking",
        default_fail_policy="collect-and-report",
        timeout_budget_seconds=3600,
        retry_budget=1,
        fail_fast_case_limit=0,
        human_approval_required=False,
    ),
    "limited-hil": ExecutionLaneDefinition(
        lane_id="limited-hil",
        lane_kind="limited-hil",
        entrypoint_refs=(
            "test.ps1",
            "tests/e2e/hardware-in-loop/run_hil_closed_loop.py",
            "tests/e2e/hardware-in-loop/run_case_matrix.py",
        ),
        default_suite_set=("apps", "contracts", "protocol-compatibility", "integration", "e2e"),
        allowed_layer_refs=("L0", "L1", "L2", "L3", "L4", "L5", "L6"),
        report_root="tests/reports/",
        gate_decision="blocking",
        default_fail_policy="manual-signoff-required",
        timeout_budget_seconds=1800,
        retry_budget=0,
        fail_fast_case_limit=1,
        human_approval_required=True,
    ),
}


SUITE_TAXONOMY: dict[str, SuiteTaxonomyDefinition] = {
    "static": SuiteTaxonomyDefinition(
        suite_id="static",
        primary_layer_refs=("L0",),
        default_size_label="small",
        owner_surface="root build/test/ci + scripts/validation",
        allowed_lane_refs=("quick-gate", "full-offline-gate", "nightly-performance", "limited-hil"),
        label_refs=("suite:static", "kind:static", "size:small"),
    ),
    "apps": SuiteTaxonomyDefinition(
        suite_id="apps",
        primary_layer_refs=("L0", "L1"),
        default_size_label="small",
        owner_surface="apps/",
        allowed_lane_refs=("quick-gate", "full-offline-gate", "limited-hil"),
        label_refs=("suite:apps", "kind:entrypoint", "size:small"),
    ),
    "contracts": SuiteTaxonomyDefinition(
        suite_id="contracts",
        primary_layer_refs=("L2",),
        default_size_label="small",
        owner_surface="tests/contracts + module */tests",
        allowed_lane_refs=("quick-gate", "full-offline-gate", "nightly-performance", "limited-hil"),
        label_refs=("suite:contracts", "kind:contract", "size:small"),
    ),
    "protocol-compatibility": SuiteTaxonomyDefinition(
        suite_id="protocol-compatibility",
        primary_layer_refs=("L2",),
        default_size_label="medium",
        owner_surface="tests/integration/protocol-compatibility/",
        allowed_lane_refs=("quick-gate", "full-offline-gate", "nightly-performance", "limited-hil"),
        label_refs=("suite:protocol-compatibility", "kind:contract", "size:medium"),
    ),
    "integration": SuiteTaxonomyDefinition(
        suite_id="integration",
        primary_layer_refs=("L3",),
        default_size_label="medium",
        owner_surface="tests/integration/scenarios/",
        allowed_lane_refs=("full-offline-gate", "nightly-performance", "limited-hil"),
        label_refs=("suite:integration", "kind:integration", "size:medium"),
    ),
    "e2e": SuiteTaxonomyDefinition(
        suite_id="e2e",
        primary_layer_refs=("L4",),
        default_size_label="large",
        owner_surface="tests/e2e/",
        allowed_lane_refs=("full-offline-gate", "nightly-performance", "limited-hil"),
        label_refs=("suite:e2e", "kind:e2e", "size:large"),
    ),
    "performance": SuiteTaxonomyDefinition(
        suite_id="performance",
        primary_layer_refs=("L6",),
        default_size_label="large",
        owner_surface="tests/performance/",
        allowed_lane_refs=("nightly-performance", "limited-hil"),
        label_refs=("suite:performance", "kind:performance", "size:large"),
    ),
}


SUITE_TO_PRIMARY_LAYERS: dict[str, tuple[str, ...]] = {
    suite_id: definition.primary_layer_refs
    for suite_id, definition in SUITE_TAXONOMY.items()
}


def ensure_known_layer_ids(layer_ids: tuple[str, ...]) -> tuple[str, ...]:
    unknown = sorted({layer_id for layer_id in layer_ids if layer_id not in VALIDATION_LAYERS})
    if unknown:
        raise ValueError(f"unknown validation layers: {unknown}")
    ordered = [layer_id for layer_id in LAYER_SEQUENCE if layer_id in set(layer_ids)]
    return tuple(ordered)


def lane_covering_layers(layer_ids: tuple[str, ...]) -> str:
    normalized_layers = ensure_known_layer_ids(layer_ids)
    if not normalized_layers:
        return "quick-gate"

    for lane_id in ("quick-gate", "full-offline-gate", "nightly-performance", "limited-hil"):
        allowed = set(EXECUTION_LANES[lane_id].allowed_layer_refs)
        if set(normalized_layers).issubset(allowed):
            return lane_id
    return "limited-hil"


def suites_to_layers(requested_suites: tuple[str, ...]) -> tuple[str, ...]:
    layers: list[str] = []
    for suite_name in requested_suites:
        for layer_id in SUITE_TO_PRIMARY_LAYERS.get(suite_name, ()):
            if layer_id not in layers:
                layers.append(layer_id)
    if not layers:
        layers.append("L0")
    return ensure_known_layer_ids(tuple(layers))


def _base_lane_for_request(request: ValidationRequest) -> str:
    if request.explicit_lane and request.explicit_lane != "auto":
        return request.explicit_lane
    if request.include_hardware_smoke or request.include_hil_closed_loop or request.include_hil_case_matrix:
        return "limited-hil"
    if request.desired_depth == "hil":
        return "limited-hil"
    if request.desired_depth == "nightly" or "performance" in request.requested_suites:
        return "nightly-performance"
    if request.desired_depth == "full-offline":
        return "full-offline-gate"
    return "quick-gate"


def _augment_layers_for_request(request: ValidationRequest, base_layers: tuple[str, ...]) -> tuple[str, ...]:
    resolved = list(base_layers)
    if "L0" not in resolved:
        resolved.append("L0")
    if request.risk_profile in {"high", "hardware-sensitive"} and "L3" not in resolved:
        resolved.append("L3")
    if "performance" in request.requested_suites and "L6" not in resolved:
        resolved.append("L6")
    if request.include_hardware_smoke or request.include_hil_closed_loop or request.include_hil_case_matrix:
        for layer_id in ("L1", "L2", "L3", "L4", "L5"):
            if layer_id not in resolved:
                resolved.append(layer_id)
    if request.desired_depth == "hil" and "L5" not in resolved:
        for layer_id in ("L1", "L2", "L3", "L4", "L5"):
            if layer_id not in resolved:
                resolved.append(layer_id)
    return ensure_known_layer_ids(tuple(resolved))


def route_validation_request(request: ValidationRequest) -> RoutedValidationRequest:
    if request.skip_layers and not request.skip_justification.strip():
        raise ValueError("skip_justification is required when skip_layers is not empty")

    if request.explicit_lane not in {"auto", *EXECUTION_LANES.keys()}:
        raise ValueError(f"unsupported execution lane: {request.explicit_lane}")

    unknown_suites = sorted({suite_id for suite_id in request.requested_suites if suite_id not in SUITE_TAXONOMY})
    if unknown_suites:
        raise ValueError(f"unsupported suites: {unknown_suites}")

    base_layers = _augment_layers_for_request(request, suites_to_layers(request.requested_suites))
    base_lane = _base_lane_for_request(request)
    if set(base_layers).issubset(set(EXECUTION_LANES[base_lane].allowed_layer_refs)):
        covering_lane = base_lane
    else:
        covering_lane = lane_covering_layers(base_layers)
    lane_was_auto_selected = request.explicit_lane == "auto"
    selected_lane = base_lane
    upgrade_reason = ""

    if base_lane != covering_lane:
        selected_lane = covering_lane
        upgrade_reason = f"requested layers {list(base_layers)} exceed lane {base_lane}; upgraded to {covering_lane}"

    if request.risk_profile == "hardware-sensitive" and selected_lane == "quick-gate":
        selected_lane = "full-offline-gate"
        upgrade_reason = "hardware-sensitive risk cannot stop at quick-gate"

    selected_layers = list(base_layers)
    skipped_layers: list[str] = []
    for layer_id in request.skip_layers:
        if layer_id in selected_layers and layer_id not in skipped_layers:
            skipped_layers.append(layer_id)
            selected_layers.remove(layer_id)

    selected_layers_tuple = ensure_known_layer_ids(tuple(selected_layers))
    skipped_layers_tuple = ensure_known_layer_ids(tuple(skipped_layers))

    lane_definition = EXECUTION_LANES[selected_lane]
    deferred_layers = tuple(
        layer_id
        for layer_id in LAYER_SEQUENCE
        if layer_id not in selected_layers_tuple
        and layer_id not in skipped_layers_tuple
        and layer_id in lane_definition.allowed_layer_refs
    )

    upgrade_recommendation = ""
    if request.include_hardware_smoke or request.include_hil_closed_loop or request.include_hil_case_matrix:
        upgrade_recommendation = "limited-hil"
    elif request.risk_profile in {"high", "hardware-sensitive"} and selected_lane == "quick-gate":
        upgrade_recommendation = "full-offline-gate"
    elif selected_lane == "quick-gate" and any(layer_id in deferred_layers for layer_id in ("L3", "L4")):
        upgrade_recommendation = "full-offline-gate"
    elif selected_lane in {"quick-gate", "full-offline-gate"} and "L6" in deferred_layers:
        upgrade_recommendation = "nightly-performance"

    return RoutedValidationRequest(
        request=request,
        selected_lane_ref=selected_lane,
        selected_layer_refs=selected_layers_tuple,
        skipped_layer_refs=skipped_layers_tuple,
        deferred_layer_refs=deferred_layers,
        upgrade_recommendation=upgrade_recommendation,
        upgrade_reason=upgrade_reason,
        lane_was_auto_selected=lane_was_auto_selected,
    )


def build_request(
    *,
    requested_suites: list[str],
    changed_scopes: list[str] | None = None,
    trigger_source: str = "local",
    risk_profile: str = "medium",
    desired_depth: str = "quick",
    request_id: str = "",
    explicit_lane: str = "auto",
    include_hardware_smoke: bool = False,
    include_hil_closed_loop: bool = False,
    include_hil_case_matrix: bool = False,
    skip_layers: list[str] | None = None,
    skip_justification: str = "",
) -> ValidationRequest:
    normalized_request_id = request_id.strip() or f"validation-{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')}"
    normalized_scopes = tuple(item.strip() for item in (changed_scopes or []) if item.strip())
    normalized_suites = tuple(item.strip() for item in requested_suites if item.strip())
    normalized_skips = tuple(item.strip() for item in (skip_layers or []) if item.strip())
    return ValidationRequest(
        request_id=normalized_request_id,
        trigger_source=trigger_source,
        changed_scopes=normalized_scopes,
        risk_profile=risk_profile,
        desired_depth=desired_depth,
        requested_suites=normalized_suites,
        include_hardware_smoke=include_hardware_smoke,
        include_hil_closed_loop=include_hil_closed_loop,
        include_hil_case_matrix=include_hil_case_matrix,
        explicit_lane=explicit_lane,
        skip_layers=normalized_skips,
        skip_justification=skip_justification.strip(),
    )


def layer_display_name(layer_id: str) -> str:
    layer = VALIDATION_LAYERS[layer_id]
    return f"{layer.layer_id} ({layer.layer_kind})"


def lane_display_name(lane_id: str) -> str:
    lane = EXECUTION_LANES[lane_id]
    return f"{lane.lane_id} ({lane.lane_kind})"


def canonical_entry_refs_for_lane(lane_id: str) -> tuple[str, ...]:
    return EXECUTION_LANES[lane_id].entrypoint_refs


def canonical_report_root(workspace_root: Path, lane_id: str) -> Path:
    return (workspace_root / EXECUTION_LANES[lane_id].report_root).resolve()


def lane_policy_metadata(lane_id: str) -> dict[str, object]:
    lane_definition = EXECUTION_LANES[lane_id]
    return {
        "lane_id": lane_definition.lane_id,
        "gate_decision": lane_definition.gate_decision,
        "default_fail_policy": lane_definition.default_fail_policy,
        "timeout_budget_seconds": lane_definition.timeout_budget_seconds,
        "retry_budget": lane_definition.retry_budget,
        "fail_fast_case_limit": lane_definition.fail_fast_case_limit,
        "human_approval_required": lane_definition.human_approval_required,
        "default_suite_set": list(lane_definition.default_suite_set),
        "allowed_layer_refs": list(lane_definition.allowed_layer_refs),
    }


def suite_taxonomy_metadata(suite_id: str) -> dict[str, object]:
    suite_definition = SUITE_TAXONOMY[suite_id]
    return {
        "suite_id": suite_definition.suite_id,
        "primary_layer_refs": list(suite_definition.primary_layer_refs),
        "default_size_label": suite_definition.default_size_label,
        "owner_surface": suite_definition.owner_surface,
        "allowed_lane_refs": list(suite_definition.allowed_lane_refs),
        "label_refs": list(suite_definition.label_refs),
    }


def suite_label_refs(suite_id: str) -> tuple[str, ...]:
    return SUITE_TAXONOMY[suite_id].label_refs
