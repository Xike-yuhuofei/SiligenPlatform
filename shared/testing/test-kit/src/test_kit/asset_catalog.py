from __future__ import annotations

import json
from dataclasses import asdict, dataclass, field
from datetime import date
from pathlib import Path


CANONICAL_SOURCE_ROOTS = (
    "samples",
    "tests/baselines",
    "shared/testing",
    "shared/contracts",
    "docs/validation",
)
EVIDENCE_OUTPUT_ROOTS = ("tests/reports",)
CANONICAL_SHARED_ROOTS = CANONICAL_SOURCE_ROOTS + EVIDENCE_OUTPUT_ROOTS
BASELINE_MANIFEST_PATH = "tests/baselines/baseline-manifest.json"
BASELINE_MANIFEST_SCHEMA_PATH = "tests/baselines/baseline-manifest.schema.json"
BASELINE_FILE_SUFFIXES = (
    ".simulation-baseline.json",
    ".recording-baseline.json",
    ".preview-snapshot-baseline.json",
)
BASELINE_FILE_NAMES = {
    "dxf-preview-profile-thresholds.json",
}
IGNORED_DEPRECATED_ROOT_FILES = {"README.md"}
FAULT_SCENARIO_SCHEMA_VERSION = "fault-scenario.v1"
FAULT_MATRIX_ID = "fault-matrix.simulated-line.v1"
DEFAULT_DETERMINISTIC_SEED = 0
DEFAULT_CLOCK_PROFILE = "deterministic-monotonic"
FAULT_SCENARIO_ALLOWED_HOOKS = (
    "controller.readiness",
    "controller.abort",
    "io.disconnect",
)
PERFORMANCE_SAMPLE_SPECS = (
    ("small", "sample.dxf.rect_diag", "samples/dxf/rect_diag.dxf"),
    ("medium", "sample.dxf.rect_medium_ladder", "samples/dxf/rect_medium_ladder.dxf"),
    ("large", "sample.dxf.rect_large_ladder", "samples/dxf/rect_large_ladder.dxf"),
)


@dataclass(frozen=True)
class TestAsset:
    asset_id: str
    asset_kind: str
    canonical_path: str
    owner_scope: str
    source_of_truth: str
    reuse_policy: str
    lifecycle_state: str

    def to_dict(self) -> dict[str, str]:
        return asdict(self)


@dataclass(frozen=True)
class TestFixture:
    fixture_id: str
    fixture_kind: str
    provider_path: str
    supported_layers: tuple[str, ...]
    dependency_mode: str
    reusable_across_modules: bool
    fault_points: tuple[str, ...] = ()
    notes: str = ""

    def to_dict(self) -> dict[str, object]:
        return asdict(self)


@dataclass(frozen=True)
class FaultScenario:
    fault_id: str
    scenario_kind: str
    injection_point: str
    applicable_layers: tuple[str, ...]
    expected_outcome: str
    required_evidence_fields: tuple[str, ...]
    safety_level: str
    schema_version: str = FAULT_SCENARIO_SCHEMA_VERSION
    owner_scope: str = ""
    source_asset_refs: tuple[str, ...] = ()
    injector_id: str = ""
    default_seed: int = DEFAULT_DETERMINISTIC_SEED
    clock_profile: str = DEFAULT_CLOCK_PROFILE
    supported_hooks: tuple[str, ...] = ()

    def to_dict(self) -> dict[str, object]:
        return asdict(self)


@dataclass(frozen=True)
class DeprecatedAssetRoot:
    root_id: str
    relative_path: str
    replacement_root: str
    owner_scope: str
    guard_policy: str
    notes: str

    def to_dict(self) -> dict[str, str]:
        return asdict(self)


@dataclass(frozen=True)
class BaselineManifestEntry:
    baseline_id: str
    canonical_path: str
    baseline_kind: str
    owner_scope: str
    source_asset_refs: tuple[str, ...]
    consumer_layers: tuple[str, ...]
    update_runner: str
    update_reviewers: tuple[str, ...]
    reviewer_guidance: str
    lifecycle_state: str
    review_by: str
    deprecated_source_paths: tuple[str, ...] = ()

    def is_stale(self, *, today: date | None = None) -> bool:
        if self.lifecycle_state != "active":
            return True
        review_due = date.fromisoformat(self.review_by)
        check_date = today or date.today()
        return review_due < check_date

    def to_dict(self) -> dict[str, object]:
        return asdict(self)


@dataclass
class AssetCatalog:
    workspace_root: Path
    assets: dict[str, TestAsset] = field(default_factory=dict)
    fixtures: dict[str, TestFixture] = field(default_factory=dict)
    faults: dict[str, FaultScenario] = field(default_factory=dict)
    deprecated_roots: dict[str, DeprecatedAssetRoot] = field(default_factory=dict)

    def add_asset(self, asset: TestAsset) -> None:
        self.assets[asset.asset_id] = asset

    def add_fixture(self, fixture: TestFixture) -> None:
        self.fixtures[fixture.fixture_id] = fixture

    def add_fault(self, fault: FaultScenario) -> None:
        self.faults[fault.fault_id] = fault

    def add_deprecated_root(self, deprecated_root: DeprecatedAssetRoot) -> None:
        self.deprecated_roots[deprecated_root.root_id] = deprecated_root

    def source_roots(self) -> dict[str, str]:
        root = self.workspace_root.resolve()
        return {
            "samples_root": str((root / "samples").resolve()),
            "baselines_root": str((root / "tests" / "baselines").resolve()),
            "shared_testing_root": str((root / "shared" / "testing").resolve()),
            "shared_contracts_root": str((root / "shared" / "contracts").resolve()),
            "validation_docs_root": str((root / "docs" / "validation").resolve()),
        }

    def evidence_roots(self) -> dict[str, str]:
        root = self.workspace_root.resolve()
        return {
            "reports_root": str((root / "tests" / "reports").resolve()),
        }

    def canonical_roots(self) -> dict[str, str]:
        return {
            **self.source_roots(),
            **self.evidence_roots(),
        }

    def to_dict(self) -> dict[str, object]:
        return {
            "workspace_root": str(self.workspace_root.resolve()),
            "canonical_roots": self.canonical_roots(),
            "assets": {key: value.to_dict() for key, value in sorted(self.assets.items())},
            "fixtures": {key: value.to_dict() for key, value in sorted(self.fixtures.items())},
            "faults": {key: value.to_dict() for key, value in sorted(self.faults.items())},
            "deprecated_roots": {
                key: value.to_dict() for key, value in sorted(self.deprecated_roots.items())
            },
            "baseline_manifest_path": str(baseline_manifest_path(self.workspace_root)),
            "baseline_manifest_schema_path": str(baseline_manifest_schema_path(self.workspace_root)),
        }


DEPRECATED_PRIVATE_ROOTS = (
    DeprecatedAssetRoot(
        root_id="deprecated.simulation-baselines",
        relative_path="data/baselines/simulation",
        replacement_root="tests/baselines",
        owner_scope="runtime-execution",
        guard_policy="blocking",
        notes="legacy simulation baseline root must stay empty after migration",
    ),
    DeprecatedAssetRoot(
        root_id="deprecated.engineering-fixtures",
        relative_path="data/baselines/engineering",
        replacement_root="shared/contracts/engineering/fixtures/cases/rect_diag",
        owner_scope="shared/contracts",
        guard_policy="blocking",
        notes="legacy engineering fixture root must stay empty after migration",
    ),
)


def _relative(path: Path, workspace_root: Path) -> str:
    return path.resolve().relative_to(workspace_root.resolve()).as_posix()


def _relative_or_absolute(path: Path, workspace_root: Path) -> str:
    try:
        return _relative(path, workspace_root)
    except ValueError:
        return path.resolve().as_posix()


def _under_canonical_root(relative_path: str) -> bool:
    return any(
        relative_path == root_name or relative_path.startswith(f"{root_name}/")
        for root_name in CANONICAL_SHARED_ROOTS
    )


def _under_source_root(relative_path: str) -> bool:
    return any(
        relative_path == root_name or relative_path.startswith(f"{root_name}/")
        for root_name in CANONICAL_SOURCE_ROOTS
    )


def _asset_root_name(relative_path: str) -> str:
    for root_name in CANONICAL_SOURCE_ROOTS:
        if relative_path == root_name or relative_path.startswith(f"{root_name}/"):
            return root_name
    for root_name in EVIDENCE_OUTPUT_ROOTS:
        if relative_path == root_name or relative_path.startswith(f"{root_name}/"):
            return root_name
    return "unclassified"


def _load_json(path: Path) -> dict[str, object]:
    return json.loads(path.read_text(encoding="utf-8"))


def baseline_manifest_path(workspace_root: Path) -> Path:
    return workspace_root.resolve() / BASELINE_MANIFEST_PATH


def baseline_manifest_schema_path(workspace_root: Path) -> Path:
    return workspace_root.resolve() / BASELINE_MANIFEST_SCHEMA_PATH


def _parse_string_sequence(raw_value: object, *, field_name: str, entry_id: str) -> tuple[str, ...]:
    if not isinstance(raw_value, list) or not raw_value:
        raise ValueError(f"{entry_id}: {field_name} must be a non-empty string array")
    values: list[str] = []
    for item in raw_value:
        if not isinstance(item, str) or not item.strip():
            raise ValueError(f"{entry_id}: {field_name} contains an empty value")
        values.append(item.strip())
    return tuple(values)


def _parse_optional_string_sequence(raw_value: object, *, field_name: str, entry_id: str) -> tuple[str, ...]:
    if raw_value is None:
        return ()
    if not isinstance(raw_value, list):
        raise ValueError(f"{entry_id}: {field_name} must be a string array")
    values: list[str] = []
    for item in raw_value:
        if not isinstance(item, str) or not item.strip():
            raise ValueError(f"{entry_id}: {field_name} contains an empty value")
        values.append(item.strip())
    return tuple(values)


def load_baseline_manifest(workspace_root: Path) -> dict[str, object]:
    manifest_file = baseline_manifest_path(workspace_root)
    if not manifest_file.exists():
        raise FileNotFoundError(f"baseline manifest missing: {manifest_file}")
    return _load_json(manifest_file)


def baseline_manifest_entries(workspace_root: Path) -> dict[str, BaselineManifestEntry]:
    raw_manifest = load_baseline_manifest(workspace_root)
    if raw_manifest.get("schema_version") != "baseline-manifest.v1":
        raise ValueError("baseline manifest schema_version must be baseline-manifest.v1")

    workflow = raw_manifest.get("reviewer_workflow")
    if not isinstance(workflow, dict):
        raise ValueError("baseline manifest reviewer_workflow must be an object")
    if not isinstance(workflow.get("required_reviewers"), list) or not workflow.get("required_reviewers"):
        raise ValueError("baseline manifest reviewer_workflow.required_reviewers must be non-empty")
    if not isinstance(workflow.get("steps"), list) or not workflow.get("steps"):
        raise ValueError("baseline manifest reviewer_workflow.steps must be non-empty")

    raw_entries = raw_manifest.get("baselines")
    if not isinstance(raw_entries, list) or not raw_entries:
        raise ValueError("baseline manifest baselines must be a non-empty array")

    entries: dict[str, BaselineManifestEntry] = {}
    for raw_entry in raw_entries:
        if not isinstance(raw_entry, dict):
            raise ValueError("baseline manifest entry must be an object")

        baseline_id = raw_entry.get("baseline_id")
        if not isinstance(baseline_id, str) or not baseline_id.strip():
            raise ValueError("baseline manifest entry baseline_id must be a non-empty string")
        if baseline_id in entries:
            raise ValueError(f"baseline manifest contains duplicate baseline_id: {baseline_id}")

        canonical_path = raw_entry.get("canonical_path")
        if not isinstance(canonical_path, str) or not canonical_path.strip():
            raise ValueError(f"{baseline_id}: canonical_path must be a non-empty string")
        canonical_path = canonical_path.strip()
        if not _under_source_root(canonical_path) or not canonical_path.startswith("tests/baselines/"):
            raise ValueError(f"{baseline_id}: canonical_path must stay under tests/baselines")

        baseline_kind = raw_entry.get("baseline_kind")
        owner_scope = raw_entry.get("owner_scope")
        update_runner = raw_entry.get("update_runner")
        reviewer_guidance = raw_entry.get("reviewer_guidance")
        lifecycle = raw_entry.get("lifecycle")

        for field_name, raw_value in (
            ("baseline_kind", baseline_kind),
            ("owner_scope", owner_scope),
            ("update_runner", update_runner),
            ("reviewer_guidance", reviewer_guidance),
        ):
            if not isinstance(raw_value, str) or not raw_value.strip():
                raise ValueError(f"{baseline_id}: {field_name} must be a non-empty string")

        if not isinstance(lifecycle, dict):
            raise ValueError(f"{baseline_id}: lifecycle must be an object")
        lifecycle_state = lifecycle.get("state")
        review_by = lifecycle.get("review_by")
        if not isinstance(lifecycle_state, str) or not lifecycle_state.strip():
            raise ValueError(f"{baseline_id}: lifecycle.state must be a non-empty string")
        if not isinstance(review_by, str) or not review_by.strip():
            raise ValueError(f"{baseline_id}: lifecycle.review_by must be a non-empty string")
        date.fromisoformat(review_by.strip())

        entries[baseline_id] = BaselineManifestEntry(
            baseline_id=baseline_id.strip(),
            canonical_path=canonical_path,
            baseline_kind=baseline_kind.strip(),
            owner_scope=owner_scope.strip(),
            source_asset_refs=_parse_string_sequence(
                raw_entry.get("source_asset_refs"),
                field_name="source_asset_refs",
                entry_id=baseline_id,
            ),
            consumer_layers=_parse_string_sequence(
                raw_entry.get("consumer_layers"),
                field_name="consumer_layers",
                entry_id=baseline_id,
            ),
            update_runner=update_runner.strip(),
            update_reviewers=_parse_string_sequence(
                raw_entry.get("update_reviewers"),
                field_name="update_reviewers",
                entry_id=baseline_id,
            ),
            reviewer_guidance=reviewer_guidance.strip(),
            lifecycle_state=lifecycle_state.strip(),
            review_by=review_by.strip(),
            deprecated_source_paths=_parse_optional_string_sequence(
                raw_entry.get("deprecated_source_paths"),
                field_name="deprecated_source_paths",
                entry_id=baseline_id,
            ),
        )

    return entries


def _asset(
    workspace_root: Path,
    *,
    asset_id: str,
    asset_kind: str,
    relative_path: str,
    owner_scope: str,
    source_of_truth: str,
    reuse_policy: str = "shared",
    lifecycle_state: str = "active",
) -> TestAsset | None:
    absolute = workspace_root / Path(relative_path)
    if not absolute.exists():
        return None
    canonical_path = _relative(absolute, workspace_root)
    if not _under_source_root(canonical_path):
        raise ValueError(f"asset path must stay under canonical source roots: {canonical_path}")
    return TestAsset(
        asset_id=asset_id,
        asset_kind=asset_kind,
        canonical_path=canonical_path,
        owner_scope=owner_scope,
        source_of_truth=source_of_truth,
        reuse_policy=reuse_policy,
        lifecycle_state=lifecycle_state,
    )


def build_asset_catalog(workspace_root: Path) -> AssetCatalog:
    root = workspace_root.resolve()
    catalog = AssetCatalog(workspace_root=root)

    for deprecated_root in DEPRECATED_PRIVATE_ROOTS:
        catalog.add_deprecated_root(deprecated_root)

    for asset in (
        _asset(
            root,
            asset_id="sample.dxf.rect_diag",
            asset_kind="sample-input",
            relative_path="samples/dxf/rect_diag.dxf",
            owner_scope="runtime-execution",
            source_of_truth="tracked DXF regression sample",
        ),
        _asset(
            root,
            asset_id="sample.dxf.rect_medium_ladder",
            asset_kind="sample-input",
            relative_path="samples/dxf/rect_medium_ladder.dxf",
            owner_scope="tests/performance",
            source_of_truth="tracked canonical medium nightly-performance DXF sample",
        ),
        _asset(
            root,
            asset_id="sample.dxf.rect_large_ladder",
            asset_kind="sample-input",
            relative_path="samples/dxf/rect_large_ladder.dxf",
            owner_scope="tests/performance",
            source_of_truth="tracked canonical large nightly-performance DXF sample",
        ),
        _asset(
            root,
            asset_id="sample.simulation.rect_diag",
            asset_kind="sample-input",
            relative_path="samples/simulation/rect_diag.simulation-input.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked simulated-line rect_diag input",
        ),
        _asset(
            root,
            asset_id="sample.simulation.sample_trajectory",
            asset_kind="sample-input",
            relative_path="samples/simulation/sample_trajectory.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked simulated-line trajectory sample",
        ),
        _asset(
            root,
            asset_id="sample.simulation.invalid_empty_segments",
            asset_kind="sample-input",
            relative_path="samples/simulation/invalid_empty_segments.simulation-input.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked simulated-line invalid-empty-segments fault input",
        ),
        _asset(
            root,
            asset_id="sample.simulation.following_error_quantized",
            asset_kind="sample-input",
            relative_path="samples/simulation/following_error_quantized.simulation-input.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked simulated-line following-error input",
        ),
        _asset(
            root,
            asset_id="sample.replay.rect_diag_recording",
            asset_kind="sample-input",
            relative_path="samples/replay-data/rect_diag.scheme_c.recording.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked scheme C replay data for rect_diag",
        ),
        _asset(
            root,
            asset_id="sample.replay.sample_trajectory_recording",
            asset_kind="sample-input",
            relative_path="samples/replay-data/sample_trajectory.scheme_c.recording.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked scheme C replay data for sample_trajectory",
        ),
        _asset(
            root,
            asset_id="baseline.simulation.compat_rect_diag",
            asset_kind="golden-baseline",
            relative_path="tests/baselines/rect_diag.simulation-baseline.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked simulated-line compat baseline",
        ),
        _asset(
            root,
            asset_id="baseline.simulation.scheme_c_rect_diag",
            asset_kind="golden-baseline",
            relative_path="tests/baselines/rect_diag.scheme_c.recording-baseline.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked scheme C baseline",
        ),
        _asset(
            root,
            asset_id="baseline.simulation.sample_trajectory",
            asset_kind="golden-baseline",
            relative_path="tests/baselines/sample_trajectory.scheme_c.recording-baseline.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked scheme C sample_trajectory baseline",
        ),
        _asset(
            root,
            asset_id="baseline.simulation.invalid_empty_segments",
            asset_kind="golden-baseline",
            relative_path="tests/baselines/invalid_empty_segments.scheme_c.recording-baseline.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked scheme C invalid-empty-segments baseline",
        ),
        _asset(
            root,
            asset_id="baseline.simulation.following_error_quantized",
            asset_kind="golden-baseline",
            relative_path="tests/baselines/following_error_quantized.scheme_c.recording-baseline.json",
            owner_scope="runtime-execution",
            source_of_truth="tracked scheme C following-error baseline",
        ),
        _asset(
            root,
            asset_id="baseline.preview.rect_diag_snapshot",
            asset_kind="golden-baseline",
            relative_path="tests/baselines/preview/rect_diag.preview-snapshot-baseline.json",
            owner_scope="hmi-application",
            source_of_truth="tracked real-preview snapshot baseline",
        ),
        _asset(
            root,
            asset_id="baseline.performance.dxf_preview_thresholds",
            asset_kind="golden-baseline",
            relative_path="tests/baselines/performance/dxf-preview-profile-thresholds.json",
            owner_scope="tests/performance",
            source_of_truth="tracked nightly-performance threshold baseline",
        ),
        _asset(
            root,
            asset_id="protocol.fixture.rect_diag_dxf",
            asset_kind="protocol-fixture",
            relative_path="shared/contracts/engineering/fixtures/cases/rect_diag/rect_diag.dxf",
            owner_scope="shared/contracts",
            source_of_truth="engineering contract fixture DXF",
        ),
        _asset(
            root,
            asset_id="protocol.fixture.rect_diag_pb",
            asset_kind="protocol-fixture",
            relative_path="shared/contracts/engineering/fixtures/cases/rect_diag/rect_diag.pb",
            owner_scope="shared/contracts",
            source_of_truth="engineering contract fixture PB",
        ),
        _asset(
            root,
            asset_id="protocol.fixture.rect_diag_preview_artifact",
            asset_kind="protocol-fixture",
            relative_path="shared/contracts/engineering/fixtures/cases/rect_diag/preview-artifact.json",
            owner_scope="shared/contracts",
            source_of_truth="engineering contract fixture preview artifact",
        ),
        _asset(
            root,
            asset_id="protocol.fixture.rect_diag_engineering",
            asset_kind="protocol-fixture",
            relative_path="shared/contracts/engineering/fixtures/cases/rect_diag/simulation-input.json",
            owner_scope="shared/contracts",
            source_of_truth="engineering contract fixture simulation input",
        ),
        _asset(
            root,
            asset_id="checklist.online_test_matrix",
            asset_kind="manual-checklist",
            relative_path="docs/validation/online-test-matrix-v1.md",
            owner_scope="validation",
            source_of_truth="tracked online validation checklist",
        ),
    ):
        if asset is not None:
            catalog.add_asset(asset)

    catalog.add_fixture(
        TestFixture(
            fixture_id="fixture.workspace-validation-runner",
            fixture_kind="report-emitter",
            provider_path="shared/testing/test-kit/src/test_kit/workspace_validation.py",
            supported_layers=("L0-structure-gate", "L1-module-contract", "L2-offline-integration"),
            dependency_mode="offline-only",
            reusable_across_modules=True,
            notes="root validation routing and report enrichment",
        )
    )
    catalog.add_fixture(
        TestFixture(
            fixture_id="fixture.validation-evidence-bundle",
            fixture_kind="report-emitter",
            provider_path="shared/testing/test-kit/src/test_kit/evidence_bundle.py",
            supported_layers=(
                "L0-structure-gate",
                "L1-module-contract",
                "L2-offline-integration",
                "L3-simulated-e2e",
                "L4-performance",
                "L5-limited-hil",
            ),
            dependency_mode="none",
            reusable_across_modules=True,
            notes="shared machine-readable evidence bundle writer",
        )
    )
    catalog.add_fixture(
        TestFixture(
            fixture_id="fixture.layered-routing-smoke",
            fixture_kind="assertion-kit",
            provider_path="tests/integration/scenarios/run_layered_validation_smoke.py",
            supported_layers=("L2-offline-integration",),
            dependency_mode="offline-only",
            reusable_across_modules=True,
            notes="layer/lane routing smoke",
        )
    )
    catalog.add_fixture(
        TestFixture(
            fixture_id="fixture.shared-asset-reuse-smoke",
            fixture_kind="assertion-kit",
            provider_path="tests/integration/scenarios/run_shared_asset_reuse_smoke.py",
            supported_layers=("L2-offline-integration",),
            dependency_mode="offline-only",
            reusable_across_modules=True,
            fault_points=("fault.simulated.invalid-empty-segments",),
            notes="shared asset reuse and fault mapping smoke",
        )
    )
    catalog.add_fixture(
        TestFixture(
            fixture_id="fixture.baseline-governance-smoke",
            fixture_kind="assertion-kit",
            provider_path="tests/integration/scenarios/run_baseline_governance_smoke.py",
            supported_layers=("L2-offline-integration",),
            dependency_mode="offline-only",
            reusable_across_modules=True,
            notes="baseline manifest, inventory, and second-source governance smoke",
        )
    )
    catalog.add_fixture(
        TestFixture(
            fixture_id="fixture.fault-matrix-smoke",
            fixture_kind="assertion-kit",
            provider_path="tests/integration/scenarios/run_fault_matrix_smoke.py",
            supported_layers=("L2-offline-integration", "L3-simulated-e2e"),
            dependency_mode="offline-only",
            reusable_across_modules=True,
            fault_points=(
                "fault.simulated.invalid-empty-segments",
                "fault.simulated.following_error_quantized",
            ),
            notes="cross-owner simulated fault matrix smoke",
        )
    )
    catalog.add_fixture(
        TestFixture(
            fixture_id="fixture.simulated-line-regression",
            fixture_kind="simulator",
            provider_path="tests/e2e/simulated-line/run_simulated_line.py",
            supported_layers=("L3-simulated-e2e",),
            dependency_mode="simulated",
            reusable_across_modules=True,
            fault_points=(
                "fault.simulated.invalid-empty-segments",
                "fault.simulated.following_error_quantized",
            ),
        )
    )
    catalog.add_fixture(
        TestFixture(
            fixture_id="fixture.hil-closed-loop",
            fixture_kind="fake-controller",
            provider_path="tests/e2e/hardware-in-loop/run_hil_closed_loop.py",
            supported_layers=("L5-limited-hil",),
            dependency_mode="hardware-limited",
            reusable_across_modules=True,
            fault_points=("fault.hil.tcp-disconnect", "fault.hil.dispenser-state-timeout"),
            notes="bounded HIL closed-loop evidence producer",
        )
    )
    catalog.add_fixture(
        TestFixture(
            fixture_id="fixture.dxf-preview-profiler",
            fixture_kind="report-emitter",
            provider_path="tests/performance/collect_dxf_preview_profiles.py",
            supported_layers=("L4-performance",),
            dependency_mode="simulated",
            reusable_across_modules=True,
            notes="preview/execution/single-flight performance evidence",
        )
    )

    catalog.add_fault(
        FaultScenario(
            fault_id="fault.simulated.invalid-empty-segments",
            scenario_kind="resource-missing",
            injection_point="samples/simulation/invalid_empty_segments.simulation-input.json",
            applicable_layers=("L2-offline-integration", "L3-simulated-e2e"),
            expected_outcome="failed",
            required_evidence_fields=("stage_id", "artifact_id", "failure_code", "evidence_path"),
            safety_level="simulated-safe",
            owner_scope="runtime-execution",
            source_asset_refs=(
                "sample.simulation.invalid_empty_segments",
                "baseline.simulation.invalid_empty_segments",
            ),
            injector_id="simulated-line.input-asset",
            default_seed=DEFAULT_DETERMINISTIC_SEED,
            clock_profile=DEFAULT_CLOCK_PROFILE,
            supported_hooks=("controller.readiness",),
        )
    )
    catalog.add_fault(
        FaultScenario(
            fault_id="fault.simulated.following_error_quantized",
            scenario_kind="timeout",
            injection_point="samples/simulation/following_error_quantized.simulation-input.json",
            applicable_layers=("L3-simulated-e2e", "L4-performance"),
            expected_outcome="deferred",
            required_evidence_fields=("stage_id", "artifact_id", "event_name", "evidence_path"),
            safety_level="simulated-safe",
            owner_scope="runtime-execution",
            source_asset_refs=(
                "sample.simulation.following_error_quantized",
                "baseline.simulation.following_error_quantized",
            ),
            injector_id="simulated-line.input-asset",
            default_seed=DEFAULT_DETERMINISTIC_SEED,
            clock_profile=DEFAULT_CLOCK_PROFILE,
            supported_hooks=("controller.abort", "io.disconnect"),
        )
    )
    catalog.add_fault(
        FaultScenario(
            fault_id="fault.hil.tcp-disconnect",
            scenario_kind="disconnect",
            injection_point="tests/e2e/hardware-in-loop/run_hil_closed_loop.py::TcpJsonClient",
            applicable_layers=("L5-limited-hil",),
            expected_outcome="failed",
            required_evidence_fields=("stage_id", "artifact_id", "failure_code", "evidence_path"),
            safety_level="hardware-bounded",
            owner_scope="runtime-execution",
            source_asset_refs=("sample.dxf.rect_diag", "checklist.online_test_matrix"),
            injector_id="hil.transport-client",
            default_seed=DEFAULT_DETERMINISTIC_SEED,
            clock_profile=DEFAULT_CLOCK_PROFILE,
            supported_hooks=("io.disconnect",),
        )
    )
    catalog.add_fault(
        FaultScenario(
            fault_id="fault.hil.dispenser-state-timeout",
            scenario_kind="timeout",
            injection_point="tests/e2e/hardware-in-loop/run_hil_closed_loop.py::_wait_for_dispenser_state",
            applicable_layers=("L5-limited-hil",),
            expected_outcome="blocked",
            required_evidence_fields=("stage_id", "artifact_id", "failure_code", "evidence_path"),
            safety_level="hardware-bounded",
            owner_scope="runtime-execution",
            source_asset_refs=("sample.dxf.rect_diag", "checklist.online_test_matrix"),
            injector_id="hil.state-wait",
            default_seed=DEFAULT_DETERMINISTIC_SEED,
            clock_profile=DEFAULT_CLOCK_PROFILE,
            supported_hooks=("controller.readiness", "controller.abort"),
        )
    )

    return catalog


def build_asset_inventory(workspace_root: Path, *, catalog: AssetCatalog | None = None) -> dict[str, object]:
    active_catalog = catalog or build_asset_catalog(workspace_root)
    root = workspace_root.resolve()
    source_roots = {
        root_name: {
            "absolute_path": str((root / root_name).resolve()),
            "tracked_asset_ids": [],
        }
        for root_name in CANONICAL_SOURCE_ROOTS
    }
    evidence_roots = {
        root_name: {
            "absolute_path": str((root / root_name).resolve()),
            "source_of_truth": False,
        }
        for root_name in EVIDENCE_OUTPUT_ROOTS
    }
    for asset in active_catalog.assets.values():
        root_name = _asset_root_name(asset.canonical_path)
        if root_name in source_roots:
            source_roots[root_name]["tracked_asset_ids"].append(asset.asset_id)

    fixture_roots: dict[str, list[str]] = {}
    for fixture in active_catalog.fixtures.values():
        provider_root = Path(fixture.provider_path).parent.as_posix()
        fixture_roots.setdefault(provider_root, []).append(fixture.fixture_id)

    fault_roots: dict[str, list[str]] = {}
    for fault in active_catalog.faults.values():
        injection_root = fault.injection_point.split("::", 1)[0]
        fault_roots.setdefault(injection_root, []).append(fault.fault_id)

    manifest_entry_ids: list[str] = []
    try:
        manifest_entry_ids = sorted(baseline_manifest_entries(workspace_root))
    except (FileNotFoundError, ValueError):
        manifest_entry_ids = []

    return {
        "source_roots": {
            root_name: {
                **payload,
                "tracked_asset_ids": sorted(payload["tracked_asset_ids"]),
            }
            for root_name, payload in sorted(source_roots.items())
        },
        "evidence_output_roots": evidence_roots,
        "fixture_roots": {key: sorted(value) for key, value in sorted(fixture_roots.items())},
        "fault_roots": {key: sorted(value) for key, value in sorted(fault_roots.items())},
        "deprecated_roots": {
            key: value.to_dict() for key, value in sorted(active_catalog.deprecated_roots.items())
        },
        "baseline_manifest_path": str(baseline_manifest_path(workspace_root).resolve()),
        "baseline_manifest_schema_path": str(baseline_manifest_schema_path(workspace_root).resolve()),
        "baseline_manifest_entry_ids": manifest_entry_ids,
    }


def _deprecated_root_files(
    workspace_root: Path,
    deprecated_root: DeprecatedAssetRoot,
) -> list[Path]:
    absolute_root = workspace_root.resolve() / deprecated_root.relative_path
    if not absolute_root.exists():
        return []
    files = []
    for candidate in sorted(absolute_root.rglob("*")):
        if not candidate.is_file():
            continue
        if candidate.name in IGNORED_DEPRECATED_ROOT_FILES:
            continue
        files.append(candidate)
    return files


def detect_deprecated_root_issues(
    workspace_root: Path,
    *,
    catalog: AssetCatalog | None = None,
) -> list[dict[str, str]]:
    active_catalog = catalog or build_asset_catalog(workspace_root)
    issues: list[dict[str, str]] = []
    for deprecated_root in active_catalog.deprecated_roots.values():
        for file_path in _deprecated_root_files(workspace_root, deprecated_root):
            issues.append(
                {
                    "root_id": deprecated_root.root_id,
                    "relative_path": _relative_or_absolute(file_path, workspace_root),
                    "replacement_root": deprecated_root.replacement_root,
                    "guard_policy": deprecated_root.guard_policy,
                }
            )
    return issues


def detect_duplicate_sources(
    workspace_root: Path,
    *,
    catalog: AssetCatalog | None = None,
) -> list[dict[str, str]]:
    active_catalog = catalog or build_asset_catalog(workspace_root)
    asset_name_index: dict[str, list[TestAsset]] = {}
    for asset in active_catalog.assets.values():
        asset_name_index.setdefault(Path(asset.canonical_path).name, []).append(asset)

    findings: list[dict[str, str]] = []
    for deprecated_root in active_catalog.deprecated_roots.values():
        for file_path in _deprecated_root_files(workspace_root, deprecated_root):
            for asset in asset_name_index.get(file_path.name, ()):
                findings.append(
                    {
                        "root_id": deprecated_root.root_id,
                        "asset_id": asset.asset_id,
                        "deprecated_path": _relative_or_absolute(file_path, workspace_root),
                        "canonical_path": asset.canonical_path,
                        "owner_scope": asset.owner_scope,
                        "guard_policy": deprecated_root.guard_policy,
                    }
                )
    return findings


def _discover_baseline_files(workspace_root: Path) -> list[str]:
    baseline_root = workspace_root.resolve() / "tests" / "baselines"
    if not baseline_root.exists():
        return []

    baseline_paths: list[str] = []
    for candidate in sorted(baseline_root.rglob("*.json")):
        relative_path = _relative(candidate, workspace_root)
        if relative_path in {
            BASELINE_MANIFEST_PATH,
            BASELINE_MANIFEST_SCHEMA_PATH,
        }:
            continue
        if (
            any(relative_path.endswith(suffix) for suffix in BASELINE_FILE_SUFFIXES)
            or Path(relative_path).name in BASELINE_FILE_NAMES
        ):
            baseline_paths.append(relative_path)
    return baseline_paths


def baseline_governance_summary(
    workspace_root: Path,
    *,
    today: date | None = None,
    catalog: AssetCatalog | None = None,
) -> dict[str, object]:
    active_catalog = catalog or build_asset_catalog(workspace_root)
    summary: dict[str, object] = {
        "schema_path": str(baseline_manifest_schema_path(workspace_root).resolve()),
        "manifest_path": str(baseline_manifest_path(workspace_root).resolve()),
        "schema_present": baseline_manifest_schema_path(workspace_root).exists(),
        "manifest_present": baseline_manifest_path(workspace_root).exists(),
        "errors": [],
        "entry_count": 0,
        "baseline_catalog_asset_ids": sorted(
            asset.asset_id
            for asset in active_catalog.assets.values()
            if asset.asset_kind == "golden-baseline"
        ),
        "manifest_baseline_ids": [],
        "missing_manifest_asset_ids": [],
        "unknown_manifest_baseline_ids": [],
        "missing_source_asset_refs": [],
        "unused_baseline_paths": [],
        "stale_baseline_ids": [],
        "deprecated_root_issues": detect_deprecated_root_issues(workspace_root, catalog=active_catalog),
        "duplicate_source_issues": detect_duplicate_sources(workspace_root, catalog=active_catalog),
    }

    if not summary["schema_present"]:
        summary["errors"].append(f"missing baseline manifest schema: {BASELINE_MANIFEST_SCHEMA_PATH}")
    if not summary["manifest_present"]:
        summary["errors"].append(f"missing baseline manifest: {BASELINE_MANIFEST_PATH}")

    manifest_entries_map: dict[str, BaselineManifestEntry] = {}
    if summary["manifest_present"]:
        try:
            manifest_entries_map = baseline_manifest_entries(workspace_root)
        except (FileNotFoundError, ValueError) as exc:
            summary["errors"].append(f"invalid baseline manifest: {exc}")

    summary["entry_count"] = len(manifest_entries_map)
    manifest_baseline_ids = sorted(manifest_entries_map)
    summary["manifest_baseline_ids"] = manifest_baseline_ids

    baseline_catalog_asset_ids = summary["baseline_catalog_asset_ids"]
    summary["missing_manifest_asset_ids"] = sorted(
        asset_id for asset_id in baseline_catalog_asset_ids if asset_id not in manifest_entries_map
    )
    summary["unknown_manifest_baseline_ids"] = sorted(
        baseline_id for baseline_id in manifest_entries_map if baseline_id not in baseline_catalog_asset_ids
    )

    missing_source_refs: list[dict[str, str]] = []
    for entry in manifest_entries_map.values():
        for source_ref in entry.source_asset_refs:
            if source_ref not in active_catalog.assets:
                missing_source_refs.append({"baseline_id": entry.baseline_id, "source_asset_ref": source_ref})
    summary["missing_source_asset_refs"] = missing_source_refs

    declared_paths = {entry.canonical_path for entry in manifest_entries_map.values()}
    summary["unused_baseline_paths"] = [
        path for path in _discover_baseline_files(workspace_root) if path not in declared_paths
    ]
    summary["stale_baseline_ids"] = [
        entry.baseline_id
        for entry in manifest_entries_map.values()
        if entry.is_stale(today=today)
    ]
    summary["blocking_issue_count"] = len(blocking_baseline_governance_issues(summary))
    summary["advisory_issue_count"] = len(advisory_baseline_governance_issues(summary))
    return summary


def blocking_baseline_governance_issues(summary: dict[str, object]) -> list[str]:
    issues = [str(item) for item in summary.get("errors", [])]
    issues.extend(
        f"baseline asset missing manifest entry: {item}"
        for item in summary.get("missing_manifest_asset_ids", [])
    )
    issues.extend(
        f"manifest baseline missing catalog asset: {item}"
        for item in summary.get("unknown_manifest_baseline_ids", [])
    )
    issues.extend(
        "baseline source ref missing catalog asset: "
        f"{item['baseline_id']} -> {item['source_asset_ref']}"
        for item in summary.get("missing_source_asset_refs", [])
    )
    issues.extend(
        f"deprecated private root still contains tracked file: {item['relative_path']} -> {item['replacement_root']}"
        for item in summary.get("deprecated_root_issues", [])
    )
    issues.extend(
        "duplicate source detected: "
        f"{item['deprecated_path']} -> {item['canonical_path']} ({item['asset_id']})"
        for item in summary.get("duplicate_source_issues", [])
    )
    return issues


def advisory_baseline_governance_issues(summary: dict[str, object]) -> list[str]:
    issues: list[str] = []
    issues.extend(
        f"unused baseline file missing manifest entry: {item}"
        for item in summary.get("unused_baseline_paths", [])
    )
    issues.extend(
        f"baseline review overdue: {item}"
        for item in summary.get("stale_baseline_ids", [])
    )
    return issues


def default_performance_samples(workspace_root: Path) -> dict[str, Path]:
    root = workspace_root.resolve()
    samples = {
        label: root / Path(relative_path)
        for label, _, relative_path in PERFORMANCE_SAMPLE_SPECS
    }
    return {label: path for label, path in samples.items() if path.exists()}


def default_performance_sample_asset_ids(workspace_root: Path) -> tuple[str, ...]:
    active_labels = set(default_performance_samples(workspace_root))
    return tuple(
        asset_id
        for label, asset_id, _ in PERFORMANCE_SAMPLE_SPECS
        if label in active_labels
    )


def performance_sample_asset_refs(workspace_root: Path, sample_path: Path | str) -> tuple[str, ...]:
    resolved_path = Path(sample_path).resolve()
    refs: list[str] = []
    for _, asset_id, relative_path in PERFORMANCE_SAMPLE_SPECS:
        canonical_path = (workspace_root.resolve() / relative_path).resolve()
        if canonical_path == resolved_path:
            refs.append(asset_id)
    return tuple(refs)


def shared_asset_ids_for_smoke(workspace_root: Path) -> tuple[str, ...]:
    catalog = build_asset_catalog(workspace_root)
    preferred = (
        "sample.dxf.rect_diag",
        "sample.simulation.rect_diag",
        "baseline.simulation.scheme_c_rect_diag",
        "baseline.simulation.following_error_quantized",
        "baseline.preview.rect_diag_snapshot",
    )
    return tuple(asset_id for asset_id in preferred if asset_id in catalog.assets)
