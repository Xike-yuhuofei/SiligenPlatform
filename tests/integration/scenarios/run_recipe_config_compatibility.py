from __future__ import annotations

import argparse
import configparser
import json
import sys
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, cast


ROOT = Path(__file__).resolve().parents[3]
HMI_SRC = ROOT / "apps" / "hmi-app" / "src"
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
CONTRACTS_ROOT = ROOT / "shared" / "contracts" / "application"
MACHINE_CONFIG = ROOT / "config" / "machine" / "machine_config.ini"

if str(HMI_SRC) not in sys.path:
    sys.path.insert(0, str(HMI_SRC))
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from hmi_client.client.protocol import CommandProtocol  # noqa: E402
from test_kit.evidence_bundle import (  # noqa: E402
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    default_deterministic_replay,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)


@dataclass
class CaseResult:
    case_id: str
    title: str
    status: str
    sample_id: str
    detail: dict[str, Any]
    note: str = ""


class _FakeClient:
    def __init__(self, fixture_map: dict[str, dict[str, Any]]) -> None:
        self._fixture_map = fixture_map
        self.calls: list[dict[str, Any]] = []

    def send_request(self, method: str, params: object = None, timeout: float = 5.0) -> dict[str, Any]:
        self.calls.append(
            {
                "method": method,
                "params": cast(dict[str, Any] | None, params) or {},
                "timeout": timeout,
            }
        )
        fixture = self._fixture_map.get(method)
        if fixture is None:
            raise AssertionError(f"missing fixture for method: {method}")
        return json.loads(json.dumps(fixture))


def _load_json(path: Path) -> dict[str, Any]:
    return cast(dict[str, Any], json.loads(path.read_text(encoding="utf-8")))


def _load_operations() -> dict[str, dict[str, Any]]:
    operations: dict[str, dict[str, Any]] = {}
    for folder in ("commands", "queries"):
        for path in sorted((CONTRACTS_ROOT / folder).glob("*.json")):
            payload = _load_json(path)
            for operation in payload["operations"]:
                operations[str(operation["method"])] = cast(dict[str, Any], operation)
    return operations


class _CaseSensitiveConfigParser(configparser.ConfigParser):
    def optionxform(self, optionstr: str) -> str:
        return optionstr


def _assert_machine_config_defaults() -> CaseResult:
    parser = _CaseSensitiveConfigParser()
    parser.read(MACHINE_CONFIG, encoding="utf-8")

    required_sections = ("Dispensing", "Machine", "DXF", "DXFPreprocess", "DXFTrajectory", "Hardware")
    missing_sections = [name for name in required_sections if not parser.has_section(name)]
    assert not missing_sections, f"missing machine config sections: {missing_sections}"

    hardware_mode = parser.get("Hardware", "mode")
    dxf_path = parser.get("DXF", "dxf_file_path")
    trajectory_script = parser.get("DXFTrajectory", "script")
    normalize_units = parser.getboolean("DXFPreprocess", "normalize_units")
    strict_r12 = parser.getboolean("DXFPreprocess", "strict_r12")
    subsegment_count = parser.getint("Dispensing", "subsegment_count")
    pulse_per_mm = parser.getint("Machine", "pulse_per_mm")

    assert hardware_mode in {"Real", "Mock"}
    assert dxf_path.endswith("samples\\dxf\\rect_diag.dxf")
    assert trajectory_script.endswith("scripts\\engineering-data\\path_to_trajectory.py")
    assert normalize_units is True
    assert strict_r12 is False
    assert subsegment_count >= 1
    assert pulse_per_mm == 200

    return CaseResult(
        case_id="config-machine-defaults",
        title="machine config defaults",
        status="passed",
        sample_id="config.machine.machine_config.ini",
        detail={
            "hardware_mode": hardware_mode,
            "dxf_file_path": dxf_path,
            "trajectory_script": trajectory_script,
            "normalize_units": normalize_units,
            "strict_r12": strict_r12,
            "subsegment_count": subsegment_count,
            "pulse_per_mm": pulse_per_mm,
        },
    )


def _assert_recipe_fixture_roundtrip() -> list[CaseResult]:
    fixture_map = {
        "recipe.get": _load_json(CONTRACTS_ROOT / "fixtures" / "responses" / "recipe.get.success.json"),
        "recipe.import": _load_json(CONTRACTS_ROOT / "fixtures" / "responses" / "recipe.import.conflicts.success.json"),
    }
    request_get = _load_json(CONTRACTS_ROOT / "fixtures" / "requests" / "recipe.get.request.json")
    request_import = _load_json(CONTRACTS_ROOT / "fixtures" / "requests" / "recipe.import.request.json")

    client = _FakeClient(fixture_map)
    protocol = CommandProtocol(cast(Any, client))

    ok_get, recipe_payload, get_error = protocol.recipe_get("recipe-001")
    assert ok_get is True
    assert get_error == ""
    assert recipe_payload["id"] == "recipe-001"
    assert recipe_payload["activeVersionId"] == "ver-003"
    assert client.calls[0]["method"] == "recipe.get"
    assert client.calls[0]["params"] == request_get["params"]

    ok_import, import_payload, import_error = protocol.recipe_import(
        bundle_json='{"recipes":[]}',
        resolution="rename",
        dry_run=True,
        actor="tester",
    )
    assert ok_import is True
    assert import_error == ""
    assert import_payload["status"] == "conflicts"
    assert import_payload["importedCount"] == 0
    assert import_payload["conflicts"][0]["suggestedResolution"] == "rename"
    assert client.calls[1]["method"] == "recipe.import"
    assert client.calls[1]["params"] == request_import["params"]

    return [
        CaseResult(
            case_id="recipe-get-fixture-roundtrip",
            title="recipe.get fixture roundtrip",
            status="passed",
            sample_id="protocol.fixture.recipe_get_request",
            detail={
                "request": client.calls[0],
                "response_recipe_id": recipe_payload["id"],
                "response_active_version_id": recipe_payload["activeVersionId"],
            },
        ),
        CaseResult(
            case_id="recipe-import-fixture-roundtrip",
            title="recipe.import fixture roundtrip",
            status="passed",
            sample_id="protocol.fixture.recipe_import_request",
            detail={
                "request": client.calls[1],
                "response_status": import_payload["status"],
                "conflict_count": len(import_payload["conflicts"]),
            },
        ),
    ]


def _assert_recipe_command_family() -> CaseResult:
    client = _FakeClient(
        {
            "recipe.update": {"id": "1", "version": "1.0", "success": True, "result": {"recipe": {"id": "recipe-001"}}},
            "recipe.versions": {"id": "2", "version": "1.0", "success": True, "result": {"versions": [{"id": "ver-001"}]}},
            "recipe.version.create": {"id": "3", "version": "1.0", "success": True, "result": {"version": {"id": "ver-002"}}},
            "recipe.version.compare": {"id": "4", "version": "1.0", "success": True, "result": {"changes": [{"field": "pressure"}]}},
            "recipe.version.activate": {"id": "5", "version": "1.0", "success": True, "result": {"activated": True}},
        }
    )
    protocol = CommandProtocol(cast(Any, client))

    ok_update, _, _ = protocol.recipe_update("recipe-001", name="Sealant-A", description="", tags=["prod"], actor="tester")
    ok_versions, _, _ = protocol.recipe_versions("recipe-001")
    ok_version_create, _, _ = protocol.recipe_version_create(
        "recipe-001",
        base_version_id="ver-001",
        version_label="v2",
        change_note="compat refresh",
        actor="tester",
    )
    ok_compare, _, _ = protocol.recipe_compare("recipe-001", "ver-001", "ver-002")
    ok_activate, _, _ = protocol.recipe_activate("recipe-001", "ver-002", actor="tester")

    assert all((ok_update, ok_versions, ok_version_create, ok_compare, ok_activate))

    expected_calls = [
        {
            "method": "recipe.update",
            "params": {
                "recipeId": "recipe-001",
                "name": "Sealant-A",
                "description": "",
                "tags": ["prod"],
                "actor": "tester",
            },
        },
        {"method": "recipe.versions", "params": {"recipeId": "recipe-001"}},
        {
            "method": "recipe.version.create",
            "params": {
                "recipeId": "recipe-001",
                "baseVersionId": "ver-001",
                "versionLabel": "v2",
                "changeNote": "compat refresh",
                "actor": "tester",
            },
        },
        {"method": "recipe.version.compare", "params": {"recipeId": "recipe-001", "baseVersionId": "ver-001", "versionId": "ver-002"}},
        {"method": "recipe.version.activate", "params": {"recipeId": "recipe-001", "versionId": "ver-002", "actor": "tester"}},
    ]

    actual_calls = [{"method": item["method"], "params": item["params"]} for item in client.calls]
    assert actual_calls == expected_calls

    return CaseResult(
        case_id="recipe-command-family-canonical-params",
        title="recipe command family canonical params",
        status="passed",
        sample_id="recipe.command.family",
        detail={"calls": actual_calls},
    )


def _assert_alias_overrides_declared() -> CaseResult:
    operations = _load_operations()
    overrides = _load_json(CONTRACTS_ROOT / "mappings" / "compatibility-overrides.json")
    override_methods = set(overrides["overrides"][0]["methods"])
    expected_methods = {
        "recipe.get",
        "recipe.update",
        "recipe.archive",
        "recipe.draft.create",
        "recipe.draft.update",
        "recipe.publish",
        "recipe.versions",
        "recipe.version.create",
        "recipe.version.compare",
        "recipe.version.activate",
        "recipe.audit",
        "recipe.export",
        "recipe.import",
    }
    assert expected_methods.issubset(override_methods)

    checked: dict[str, list[str]] = {}
    for method in sorted(expected_methods):
        compatibility = cast(dict[str, Any], operations[method].get("compatibility", {}))
        aliases = compatibility.get("requestAliases", [])
        assert aliases, f"missing compatibility.requestAliases for {method}"
        checked[method] = [str(entry["canonical"]) for entry in aliases]

    return CaseResult(
        case_id="recipe-alias-overrides-declared",
        title="recipe alias overrides declared",
        status="passed",
        sample_id="protocol.fixture.recipe_alias_overrides",
        detail={"methods": checked},
    )


def _write_report(report_dir: Path, results: list[CaseResult], overall_status: str) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "recipe-config-compatibility-summary.json"
    md_path = report_dir / "recipe-config-compatibility-summary.md"

    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "overall_status": overall_status,
        "results": [asdict(item) for item in results],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Recipe Config Compatibility Summary",
        "",
        f"- overall_status: `{overall_status}`",
        "",
        "## Cases",
        "",
    ]
    for item in results:
        lines.append(f"- `{item.status}` `{item.case_id}` sample=`{item.sample_id}`")
        if item.note:
            lines.append(f"  note: {item.note}")
        lines.append("  ```json")
        lines.extend(f"  {line}" for line in json.dumps(item.detail, ensure_ascii=False, indent=2).splitlines())
        lines.append("  ```")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def _write_bundle(report_dir: Path, json_path: Path, md_path: Path, results: list[CaseResult], overall_status: str) -> None:
    asset_map = {
        "config-machine-defaults": (),
        "recipe-get-fixture-roundtrip": (
            "protocol.fixture.recipe_get_request",
            "protocol.fixture.recipe_get_response",
        ),
        "recipe-import-fixture-roundtrip": (
            "protocol.fixture.recipe_import_request",
            "protocol.fixture.recipe_import_response",
        ),
        "recipe-command-family-canonical-params": (),
        "recipe-alias-overrides-declared": ("protocol.fixture.recipe_alias_overrides",),
    }

    bundle = EvidenceBundle(
        bundle_id="recipe-config-compatibility",
        request_ref="recipe-config-compatibility",
        producer_lane_ref="full-offline-gate",
        report_root=str(report_dir.resolve()),
        summary_file=str(md_path.resolve()),
        machine_file=str(json_path.resolve()),
        verdict=infer_verdict([item.status for item in results]),
        linked_asset_refs=tuple(sorted({asset for item in results for asset in asset_map.get(item.case_id, ())})),
        case_records=[
            EvidenceCaseRecord(
                case_id=item.case_id,
                name=item.title,
                suite_ref="integration",
                owner_scope="tests/integration",
                primary_layer="L3",
                producer_lane_ref="full-offline-gate",
                status=item.status,
                evidence_profile="integration-report",
                stability_state="stable",
                size_label="medium",
                label_refs=("suite:integration", "kind:integration", "size:medium", "layer:L3"),
                required_assets=asset_map.get(item.case_id, ()),
                required_fixtures=("fixture.recipe-config-compatibility", "fixture.validation-evidence-bundle"),
                deterministic_replay=default_deterministic_replay(passed=item.status == "passed", seed=0, clock_profile="fixture", repeat_count=1),
                note=item.note,
                trace_fields=trace_fields(
                    stage_id="L3",
                    artifact_id=item.case_id,
                    module_id="tests/integration",
                    workflow_state="executed",
                    execution_state=item.status,
                    event_name=item.title,
                    failure_code="" if item.status == "passed" else f"recipe-config.{item.case_id}",
                    evidence_path=str(json_path.resolve()),
                ),
            )
            for item in results
        ],
        metadata={"overall_status": overall_status},
    )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=json_path,
        summary_md_path=md_path,
        evidence_links=[
            EvidenceLink(label="machine_config.ini", path=str(MACHINE_CONFIG.resolve()), role="config"),
            EvidenceLink(
                label="recipe.get.request.json",
                path=str((CONTRACTS_ROOT / "fixtures" / "requests" / "recipe.get.request.json").resolve()),
                role="fixture",
            ),
            EvidenceLink(
                label="recipe.import.request.json",
                path=str((CONTRACTS_ROOT / "fixtures" / "requests" / "recipe.import.request.json").resolve()),
                role="fixture",
            ),
        ],
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run recipe/config/version compatibility regression and emit evidence.")
    parser.add_argument(
        "--report-dir",
        default=str(ROOT / "tests" / "reports" / "integration" / "recipe-config-compatibility"),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    results: list[CaseResult] = []

    try:
        results.append(_assert_machine_config_defaults())
        results.extend(_assert_recipe_fixture_roundtrip())
        results.append(_assert_recipe_command_family())
        results.append(_assert_alias_overrides_declared())
        overall_status = "passed"
    except Exception as exc:
        overall_status = "failed"
        results.append(
            CaseResult(
                case_id="recipe-config-compatibility",
                title="recipe config compatibility",
                status="failed",
                sample_id="protocol.fixture.recipe_import_request",
                detail={},
                note=str(exc),
            )
        )
        print(f"recipe config compatibility failed: {exc}", file=sys.stderr)

    json_path, md_path = _write_report(report_dir, results, overall_status)
    _write_bundle(report_dir, json_path, md_path, results, overall_status)
    print(f"recipe config compatibility overall_status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
