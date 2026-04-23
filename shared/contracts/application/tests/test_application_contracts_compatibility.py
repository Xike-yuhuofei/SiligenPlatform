import json
import re
import sys
from pathlib import Path
from typing import Any, cast

import pytest


ROOT = Path(__file__).resolve().parents[4]
HMI_APP_SRC = ROOT / "apps" / "hmi-app" / "src"
if str(HMI_APP_SRC) not in sys.path:
    sys.path.insert(0, str(HMI_APP_SRC))

CONTRACTS = ROOT / "shared" / "contracts" / "application"
HMI_PROTOCOL = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "client" / "protocol.py"
HMI_MAIN_WINDOW = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "ui" / "main_window.py"
TCP_DISPATCHER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "tcp" / "TcpCommandDispatcher.cpp"
RUNTIME_STATUS_EXPORT_PORT = ROOT / "apps" / "runtime-service" / "runtime" / "status" / "RuntimeStatusExportPort.cpp"
RUNTIME_SUPERVISION_ADAPTER = ROOT / "modules" / "runtime-execution" / "runtime" / "host" / "runtime" / "supervision" / "RuntimeSupervisionPortAdapter.cpp"

from hmi_client.client.protocol import CommandProtocol


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"))


def load_operations():
    operations = {}
    for folder in ("commands", "queries"):
        for path in sorted((CONTRACTS / folder).glob("*.json")):
            payload = load_json(path)
            for op in payload["operations"]:
                method = op["method"]
                if method in operations:
                    raise AssertionError(f"duplicate contract method: {method}")
                operations[method] = op
    return operations


@pytest.fixture
def operations():
    return load_operations()


def extract_hmi_methods():
    source = HMI_PROTOCOL.read_text(encoding="utf-8")
    direct = set(re.findall(r'send_request\("([^"]+)"', source))
    indirect = set(re.findall(r'_call\("([^"]+)"', source))
    return direct | indirect


def extract_tcp_methods():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    return set(re.findall(r'RegisterCommand\("([^"]+)"', source))


def assert_success_envelope(payload):
    assert payload["version"] == "1.0"
    assert payload["success"] is True
    assert "id" in payload
    assert "result" in payload


def assert_error_envelope(payload):
    assert payload["version"] == "1.0"
    assert payload["success"] is False
    assert "id" in payload
    assert set(payload["error"].keys()) == {"code", "message"}


def assert_event_envelope(payload):
    assert payload["version"] == "1.0"
    assert set(payload.keys()) == {"version", "event", "data"}


def test_hmi_methods_are_contracted(operations):
    hmi_methods = extract_hmi_methods()
    missing = sorted(hmi_methods - operations.keys())
    assert not missing, f"HMI methods missing from contracts: {missing}"


def test_tcp_methods_match_contracts(operations):
    tcp_methods = extract_tcp_methods()
    missing = sorted(tcp_methods - operations.keys())
    extra = sorted(operations.keys() - tcp_methods)
    assert not missing, f"TCP dispatcher methods missing from contracts: {missing}"
    assert not extra, f"Contract methods not registered by TCP dispatcher: {extra}"


def test_fixtures():
    request_dir = CONTRACTS / "fixtures" / "requests"
    response_dir = CONTRACTS / "fixtures" / "responses"
    event_dir = CONTRACTS / "fixtures" / "events"

    for path in sorted(request_dir.glob("*.json")):
        payload = load_json(path)
        assert "id" in payload and "method" in payload, f"bad request fixture: {path.name}"

    for path in sorted(response_dir.glob("*.json")):
        payload = load_json(path)
        if path.name.startswith("error."):
            assert_error_envelope(payload)
        else:
            assert_success_envelope(payload)

    for path in sorted(event_dir.glob("*.json")):
        assert_event_envelope(load_json(path))


def test_known_compatibility_gaps_are_recorded():
    overrides = load_json(CONTRACTS / "mappings" / "compatibility-overrides.json")
    ids = {item["id"] for item in overrides["overrides"]}
    assert "dxf-info-total-segments-gap" not in ids
    assert "recipe-request-aliases" in ids

    hmi_ui = HMI_MAIN_WINDOW.read_text(encoding="utf-8")
    tcp_source = TCP_DISPATCHER.read_text(encoding="utf-8")
    assert 'total_segments = info.get("total_segments")' in hmi_ui
    assert "if total_segments is None:" in hmi_ui
    assert 'total_segments = getattr(self, "_dxf_segment_count_cache", 0)' in hmi_ui
    assert 'self._dxf_segment_count_cache = int(total_segments or 0)' in hmi_ui
    info_label_match = re.search(
        r"def _update_info_label\(self\):(?P<body>.*?)(?:\n    def |\Z)",
        hmi_ui,
        re.S,
    )
    assert info_label_match, "cannot locate _update_info_label body"
    info_label_body = info_label_match.group("body")
    assert "self._sync_preview_session_fields()" in info_label_body
    assert "self._dxf_info_label.setText(self._preview_session.info_label_text())" in info_label_body

    dxf_info_match = re.search(
        r"std::string TcpCommandDispatcher::HandleDxfInfo.*?return GatewayJsonProtocol::MakeSuccessResponse",
        tcp_source,
        re.S,
    )
    assert dxf_info_match, "cannot locate HandleDxfInfo body"
    dxf_info_body = dxf_info_match.group(0)
    assert '"total_length"' in dxf_info_body
    assert '"bounds"' in dxf_info_body
    assert '"total_segments"' in dxf_info_body


def test_recipe_aliases_are_explicit():
    operations = load_operations()
    alias_methods = {
        method
        for method, op in operations.items()
        if "compatibility" in op and op["compatibility"].get("requestAliases")
    }
    expected = {
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
    assert expected.issubset(alias_methods)


def test_dxf_preview_and_job_contract():
    operations = load_operations()
    assert "dxf.load" not in operations
    assert "dxf.preview.snapshot" in operations
    assert "dxf.preview.confirm" in operations
    assert "dxf.artifact.create" in operations
    assert "dxf.plan.prepare" in operations
    assert "dxf.job.start" in operations
    assert "dxf.job.status" in operations

    artifact_create = operations["dxf.artifact.create"]
    assert "formal_compare_gate" in artifact_create["resultSchema"]["required"]

    preview = operations["dxf.preview.snapshot"]
    preview_params = preview["paramsSchema"]
    assert "plan_id" in preview_params["required"]
    assert "snapshot_hash" in preview["resultSchema"]["required"]
    assert "plan_id" in preview["resultSchema"]["required"]
    assert "preview_state" in preview["resultSchema"]["required"]
    assert "preview_source" in preview["resultSchema"]["required"]
    assert "preview_kind" in preview["resultSchema"]["required"]
    preview_result_properties = preview["resultSchema"]["properties"]
    assert "preview_source" in preview_result_properties
    assert "preview_kind" in preview_result_properties
    assert "glue_points" in preview_result_properties
    assert "glue_point_count" in preview_result_properties
    assert "glue_reveal_lengths_mm" in preview_result_properties
    assert "preview_binding" in preview_result_properties
    assert "motion_preview" in preview_result_properties

    preview_fixture = load_json(CONTRACTS / "fixtures" / "responses" / "dxf.preview.snapshot.success.json")
    glue_reveal_lengths = preview_fixture["result"]["glue_reveal_lengths_mm"]
    assert len(glue_reveal_lengths) == len(preview_fixture["result"]["glue_points"])
    assert glue_reveal_lengths == sorted(glue_reveal_lengths)
    preview_binding = preview_fixture["result"]["preview_binding"]
    assert preview_binding["source"] == "runtime_authority_preview_binding"
    assert preview_binding["status"] == "ready"
    assert preview_binding["layout_id"]
    assert preview_binding["glue_point_count"] == len(preview_fixture["result"]["glue_points"])
    assert len(preview_binding["source_trigger_indices"]) == len(preview_fixture["result"]["glue_points"])
    assert len(preview_binding["display_reveal_lengths_mm"]) == len(preview_fixture["result"]["glue_points"])
    assert preview_binding["display_reveal_lengths_mm"] == sorted(preview_binding["display_reveal_lengths_mm"])
    assert preview_binding["display_path_length_mm"] >= preview_binding["display_reveal_lengths_mm"][-1]
    motion_preview = preview_fixture["result"]["motion_preview"]
    assert motion_preview["source"] == "execution_trajectory_snapshot"
    assert motion_preview["kind"] == "polyline"
    assert motion_preview["is_sampled"] is True
    assert motion_preview["sampling_strategy"] == "execution_trajectory_geometry_preserving_clamp"
    assert motion_preview["source_point_count"] >= motion_preview["point_count"]
    assert len(motion_preview["polyline"]) == motion_preview["point_count"]

    preview_confirm = operations["dxf.preview.confirm"]
    assert {"plan_id", "snapshot_hash"}.issubset(set(preview_confirm["paramsSchema"]["required"]))
    assert {"confirmed", "plan_id", "snapshot_hash", "preview_state", "confirmed_at"}.issubset(
        set(preview_confirm["resultSchema"]["required"])
    )

    legacy_methods = {"dxf.execute", "dxf.pause", "dxf.resume", "dxf.stop", "dxf.progress"}
    assert legacy_methods.isdisjoint(operations)

    plan_prepare = operations["dxf.plan.prepare"]
    assert "preview_request_signature" not in plan_prepare["resultSchema"]["required"]
    assert "import_result_classification" in plan_prepare["resultSchema"]["required"]
    assert "import_production_ready" in plan_prepare["resultSchema"]["required"]
    assert "formal_compare_gate" in plan_prepare["resultSchema"]["required"]
    assert "prepared_filepath" in plan_prepare["resultSchema"]["required"]
    assert "execution_nominal_time_s" in plan_prepare["resultSchema"]["required"]
    assert "execution_plan_summary" in plan_prepare["resultSchema"]["required"]
    assert "production_baseline" in plan_prepare["resultSchema"]["required"]
    assert "estimated_time_s" not in plan_prepare["resultSchema"]["required"]
    assert {"artifact_id", "dispensing_speed_mm_s"}.issubset(set(plan_prepare["paramsSchema"]["required"]))
    assert "recipe_id" not in plan_prepare["paramsSchema"]["properties"]
    assert "version_id" not in plan_prepare["paramsSchema"]["properties"]
    assert "speed_mm_s" not in plan_prepare["paramsSchema"]["properties"]
    assert not plan_prepare.get("compatibility", {}).get("requestAliases")
    plan_prepare_notes = "\n".join(plan_prepare.get("compatibility", {}).get("notes", []))
    assert "current production baseline" in plan_prepare_notes
    assert "point_flying_carrier_policy" in plan_prepare_notes
    assert "published recipe version" in plan_prepare_notes
    assert "activeVersionId" in plan_prepare_notes
    job_start_notes = "\n".join(operations["dxf.job.start"].get("compatibility", {}).get("notes", []))
    assert "回退最近一次 prepared plan" not in job_start_notes

    job_start = operations["dxf.job.start"]
    assert "auto_continue" in job_start["paramsSchema"]["properties"]
    assert "auto_continue 省略时默认为 true" in job_start_notes
    assert {"started", "job_id", "plan_id", "plan_fingerprint", "target_count"}.issubset(
        set(job_start["resultSchema"]["required"])
    )
    assert {"execution_budget_s", "execution_budget_breakdown", "production_baseline"}.issubset(
        set(job_start["resultSchema"]["required"])
    )
    assert {"import_result_classification", "import_production_ready", "prepared_filepath"}.issubset(
        set(job_start["resultSchema"]["required"])
    )
    assert "formal_compare_gate" in job_start["resultSchema"]["required"]
    assert "task_id" not in job_start["resultSchema"]["required"]
    assert "task_id" not in job_start["resultSchema"]["properties"]

    job_status = operations["dxf.job.status"]
    assert job_status["resultRef"].endswith("#/definitions/dxfJobStatus")
    states = load_json(CONTRACTS / "models" / "states.json")
    dxf_job_status = states["definitions"]["dxfJobStatus"]
    assert "awaiting_continue" in dxf_job_status["properties"]["state"]["enum"]
    assert {"execution_budget_s", "execution_budget_breakdown"}.issubset(set(dxf_job_status["required"]))
    assert "active_task_id" not in dxf_job_status["required"]
    assert "active_task_id" not in dxf_job_status["properties"]

    job_continue = operations["dxf.job.continue"]
    assert {"continued", "job_id"} == set(job_continue["resultSchema"]["required"])

    prepare_request_fixture = load_json(CONTRACTS / "fixtures" / "requests" / "dxf.plan.prepare.request.json")
    artifact_fixture = load_json(CONTRACTS / "fixtures" / "responses" / "dxf.artifact.create.success.json")
    prepare_fixture = load_json(CONTRACTS / "fixtures" / "responses" / "dxf.plan.prepare.success.json")
    start_fixture = load_json(CONTRACTS / "fixtures" / "responses" / "dxf.job.start.success.json")
    assert "recipe_id" not in prepare_request_fixture["params"]
    assert "version_id" not in prepare_request_fixture["params"]
    assert artifact_fixture["result"]["formal_compare_gate"] is None
    assert prepare_fixture["result"]["formal_compare_gate"] is None
    assert prepare_fixture["result"]["production_baseline"]["baseline_id"]
    assert prepare_fixture["result"]["production_baseline"]["baseline_fingerprint"]
    assert "estimated_time_s" not in prepare_fixture["result"]
    assert "execution_nominal_time_s" in prepare_fixture["result"]
    assert "execution_plan_summary" in prepare_fixture["result"]
    assert start_fixture["result"]["formal_compare_gate"] is None
    assert start_fixture["result"]["production_baseline"]["baseline_id"]
    assert start_fixture["result"]["production_baseline"]["baseline_fingerprint"]
    assert {"execution_budget_s", "execution_budget_breakdown", "production_baseline"}.issubset(set(start_fixture["result"].keys()))


def test_status_contract_describes_backend_interlock_authority():
    states = load_json(CONTRACTS / "models" / "states.json")
    io_required = set(states["definitions"]["ioStatus"]["required"])
    io_props = states["definitions"]["ioStatus"]["properties"]
    assert {"estop_known", "door_known"}.issubset(io_required)
    assert "后端权威" in io_props["estop"]["description"]
    assert "运行时互锁端口" in io_props["door"]["description"]
    assert "断线且无有效采样时必须为 false" in io_props["estop_known"]["description"]


def test_status_contract_exposes_effective_interlocks_and_supervision():
    states = load_json(CONTRACTS / "models" / "states.json")
    fixture = load_json(CONTRACTS / "fixtures" / "responses" / "status.success.json")
    tcp_source = TCP_DISPATCHER.read_text(encoding="utf-8")
    status_source = RUNTIME_STATUS_EXPORT_PORT.read_text(encoding="utf-8")
    supervision_adapter = RUNTIME_SUPERVISION_ADAPTER.read_text(encoding="utf-8")

    machine_required = set(states["definitions"]["machineStatus"]["required"])
    effective_interlocks_required = set(states["definitions"]["effectiveInterlocks"]["required"])
    safety_boundary_required = set(states["definitions"]["safetyBoundaryStatus"]["required"])
    action_capabilities_required = set(states["definitions"]["actionCapabilitiesStatus"]["required"])
    supervision_required = set(states["definitions"]["supervisionStatus"]["required"])
    runtime_identity_required = set(states["definitions"]["runtimeIdentityStatus"]["required"])

    assert {"supervision", "runtime_identity", "effective_interlocks", "safety_boundary", "action_capabilities", "job_execution", "device_mode"}.issubset(machine_required)
    assert {"active_job_id", "active_job_state"}.isdisjoint(machine_required)
    assert {"machine_state", "machine_state_reason"}.issubset(machine_required)
    assert {
        "executable_path",
        "working_directory",
        "protocol_version",
        "preview_snapshot_contract",
    }.issubset(runtime_identity_required)
    assert {
        "estop_active",
        "estop_known",
        "door_open_active",
        "door_open_known",
        "home_boundary_x_active",
        "home_boundary_y_active",
        "positive_escape_only_axes",
        "sources",
    }.issubset(effective_interlocks_required)
    assert {
        "current_state",
        "requested_state",
        "state_change_in_process",
        "state_reason",
        "failure_code",
        "failure_stage",
        "recoverable",
        "updated_at",
    }.issubset(supervision_required)
    assert {
        "state",
        "motion_permitted",
        "process_output_permitted",
        "estop_active",
        "estop_known",
        "door_open_active",
        "door_open_known",
        "interlock_latched",
        "blocking_reasons",
    }.issubset(safety_boundary_required)
    assert {
        "motion_commands_permitted",
        "manual_output_commands_permitted",
        "manual_dispenser_pause_permitted",
        "manual_dispenser_resume_permitted",
        "active_job_present",
        "estop_reset_permitted",
    }.issubset(action_capabilities_required)
    assert "控制器有效保护，不等同于原始负限位输入" in states["definitions"]["effectiveInterlocks"]["properties"]["home_boundary_x_active"]["description"]
    assert "断线且无权威急停来源时必须为 false" in states["definitions"]["effectiveInterlocks"]["properties"]["estop_known"]["description"]
    assert "监督层当前目标状态" in states["definitions"]["supervisionStatus"]["properties"]["requested_state"]["description"]
    assert "软件动作准入总览" in states["definitions"]["safetyBoundaryStatus"]["properties"]["state"]["description"]
    assert "dry_run 或 device_mode=test 时必须为 false" in states["definitions"]["safetyBoundaryStatus"]["properties"]["process_output_permitted"]["description"]
    assert "后端粗粒度摘要" in states["definitions"]["actionCapabilitiesStatus"]["properties"]["motion_commands_permitted"]["description"]
    assert "不包含 HMI 本地 pending/worker/session 门禁" in states["definitions"]["actionCapabilitiesStatus"]["properties"]["manual_output_commands_permitted"]["description"]
    assert "点胶阀状态为 Running" in states["definitions"]["actionCapabilitiesStatus"]["properties"]["manual_dispenser_pause_permitted"]["description"]
    assert "点胶阀状态为 Paused" in states["definitions"]["actionCapabilitiesStatus"]["properties"]["manual_dispenser_resume_permitted"]["description"]
    assert "supervision.current_state 为 Estop" in states["definitions"]["actionCapabilitiesStatus"]["properties"]["estop_reset_permitted"]["description"]
    assert "HMI online startup gate" in states["definitions"]["runtimeIdentityStatus"]["properties"]["executable_path"]["description"]
    assert "fail-closed" in states["definitions"]["runtimeIdentityStatus"]["properties"]["preview_snapshot_contract"]["description"]
    assert states["definitions"]["machineStatus"]["properties"]["device_mode"]["enum"] == ["production", "test"]
    assert "运行时执行上下文单向派生" in states["definitions"]["machineStatus"]["properties"]["device_mode"]["description"]
    assert "device_mode" in states["definitions"]["machineStatus"]["properties"]
    assert "machine_state" in states["definitions"]["machineStatus"]["properties"]
    assert "machine_state_reason" in states["definitions"]["machineStatus"]["properties"]
    assert "safety_boundary" in states["definitions"]["machineStatus"]["properties"]
    assert "action_capabilities" in states["definitions"]["machineStatus"]["properties"]

    fixture_result = fixture["result"]
    assert fixture_result["device_mode"] == "production"
    assert fixture_result["machine_state"] == "Idle"
    assert fixture_result["machine_state_reason"] == "idle"
    assert set(fixture_result["runtime_identity"].keys()) == {
        "executable_path",
        "working_directory",
        "protocol_version",
        "preview_snapshot_contract",
    }
    assert fixture_result["runtime_identity"]["executable_path"].endswith("siligen_runtime_gateway.exe")
    assert fixture_result["runtime_identity"]["working_directory"].endswith("build\\ca\\bin")
    assert fixture_result["runtime_identity"]["protocol_version"] == "siligen.application/1.0"
    assert (
        fixture_result["runtime_identity"]["preview_snapshot_contract"]
        == "planned_glue_snapshot.glue_points+execution_trajectory_snapshot.polyline"
    )
    assert set(fixture_result["safety_boundary"].keys()) == {
        "state",
        "motion_permitted",
        "process_output_permitted",
        "estop_active",
        "estop_known",
        "door_open_active",
        "door_open_known",
        "interlock_latched",
        "blocking_reasons",
    }
    assert fixture_result["safety_boundary"]["state"] == "safe"
    assert fixture_result["safety_boundary"]["motion_permitted"] is True
    assert fixture_result["safety_boundary"]["process_output_permitted"] is True
    assert fixture_result["safety_boundary"]["blocking_reasons"] == []
    assert set(fixture_result["action_capabilities"].keys()) == {
        "motion_commands_permitted",
        "manual_output_commands_permitted",
        "manual_dispenser_pause_permitted",
        "manual_dispenser_resume_permitted",
        "active_job_present",
        "estop_reset_permitted",
    }
    assert fixture_result["action_capabilities"]["motion_commands_permitted"] is True
    assert fixture_result["action_capabilities"]["manual_output_commands_permitted"] is True
    assert fixture_result["action_capabilities"]["manual_dispenser_pause_permitted"] is False
    assert fixture_result["action_capabilities"]["manual_dispenser_resume_permitted"] is False
    assert fixture_result["action_capabilities"]["active_job_present"] is False
    assert fixture_result["action_capabilities"]["estop_reset_permitted"] is False
    assert set(fixture_result["job_execution"].keys()) == {
        "job_id",
        "plan_id",
        "plan_fingerprint",
        "state",
        "target_count",
        "completed_count",
        "current_cycle",
        "current_segment",
        "total_segments",
        "cycle_progress_percent",
        "overall_progress_percent",
        "elapsed_seconds",
        "execution_budget_s",
        "execution_budget_breakdown",
        "error_message",
        "dry_run",
    }
    assert fixture_result["job_execution"]["state"] == "idle"
    assert set(fixture_result["effective_interlocks"].keys()) == {
        "estop_active",
        "estop_known",
        "door_open_active",
        "door_open_known",
        "home_boundary_x_active",
        "home_boundary_y_active",
        "positive_escape_only_axes",
        "sources",
    }
    assert set(fixture_result["supervision"].keys()) == {
        "current_state",
        "requested_state",
        "state_change_in_process",
        "state_reason",
        "failure_code",
        "failure_stage",
        "recoverable",
        "updated_at",
    }
    assert set(fixture_result["dispenser"].keys()) == {
        "valve_open",
        "supply_open",
        "completedCount",
        "totalCount",
        "progress",
    }
    assert {"completedCount", "totalCount", "progress"}.issubset(
        states["definitions"]["machineStatus"]["properties"]["dispenser"]["required"]
    )
    assert "点胶阀权威已完成触发计数" in states["definitions"]["machineStatus"]["properties"]["dispenser"]["properties"]["completedCount"]["description"]
    assert "点胶阀权威目标触发总数" in states["definitions"]["machineStatus"]["properties"]["dispenser"]["properties"]["totalCount"]["description"]
    assert "点胶阀权威进度百分比" in states["definitions"]["machineStatus"]["properties"]["dispenser"]["properties"]["progress"]["description"]

    class StubClient:
        def __init__(self, response: dict[str, object]) -> None:
            self._response = response
            self.calls: list[tuple[str, object | None, float]] = []

        def send_request(self, method: str, params: object = None, timeout: float = 5.0) -> dict[str, object]:
            self.calls.append((method, params, timeout))
            return self._response

    client = StubClient(fixture)
    status = CommandProtocol(cast(Any, client)).get_status()
    assert client.calls == [("status", None, 5.0)]
    assert status.connected is True
    assert status.connection_state == "connected"
    assert status.device_mode == "production"
    assert status.runtime_state == "Idle"
    assert status.runtime_state_reason == "idle"
    assert not hasattr(status, "machine_state")
    assert status.gate_estop_known() is True
    assert status.gate_estop_active() is False
    assert status.gate_door_known() is True
    assert status.gate_door_active() is False
    assert status.home_boundary_active("X") is False
    assert status.home_boundary_active("Y") is False
    assert status.job_execution.state == "idle"
    assert status.safety_boundary.state == "safe"
    assert status.safety_boundary.motion_permitted is True
    assert status.safety_boundary.process_output_permitted is True
    assert status.safety_boundary.blocking_reasons == []
    assert status.action_capabilities.motion_commands_permitted is True
    assert status.action_capabilities.manual_output_commands_permitted is True
    assert status.action_capabilities.manual_dispenser_pause_permitted is False
    assert status.action_capabilities.manual_dispenser_resume_permitted is False
    assert status.action_capabilities.active_job_present is False
    assert status.action_capabilities.estop_reset_permitted is False
    assert status.effective_interlocks.sources["estop"] == "system_interlock"
    assert status.supervision.requested_state == "Idle"
    assert status.supervision.state_change_in_process is False

    detailed_result = CommandProtocol(cast(Any, StubClient(fixture))).get_status_detailed()
    assert detailed_result.ok is True
    assert detailed_result.error_message == ""
    assert detailed_result.error_code is None
    assert detailed_result.runtime_identity is not None
    assert detailed_result.status.runtime_identity is not None
    assert detailed_result.status.runtime_identity.executable_path.endswith("siligen_runtime_gateway.exe")
    assert detailed_result.status.runtime_identity.working_directory.endswith("build\\ca\\bin")
    assert detailed_result.status.runtime_identity.protocol_version == "siligen.application/1.0"
    assert (
        detailed_result.status.runtime_identity.preview_snapshot_contract
        == "planned_glue_snapshot.glue_points+execution_trajectory_snapshot.polyline"
    )

    missing_identity_fixture = json.loads(json.dumps(fixture))
    missing_identity_fixture["result"].pop("runtime_identity", None)
    missing_identity_result = CommandProtocol(cast(Any, StubClient(missing_identity_fixture))).get_status_detailed()
    assert missing_identity_result.ok is True
    assert missing_identity_result.error_message == ""
    assert missing_identity_result.error_code is None
    assert missing_identity_result.status.runtime_identity is None

    rpc_error_result = CommandProtocol(
        cast(
            Any,
            StubClient(
                {
                    "id": "10",
                    "version": "1.0",
                    "success": False,
                    "error": {"code": 2101, "message": "status unavailable"},
                }
            ),
        )
    ).get_status_detailed()
    assert rpc_error_result.ok is False
    assert rpc_error_result.error_code == 2101
    assert rpc_error_result.error_message == "status unavailable"
    assert rpc_error_result.status.runtime_identity is None

    legacy_fixture = json.loads(json.dumps(fixture))
    legacy_result = legacy_fixture["result"]
    legacy_result.pop("device_mode", None)
    legacy_result.pop("safety_boundary", None)
    legacy_result.pop("action_capabilities", None)
    legacy_result["supervision"] = ["legacy-payload"]

    legacy_status = CommandProtocol(cast(Any, StubClient(legacy_fixture))).get_status()
    assert legacy_status.device_mode == "production"
    assert legacy_status.supervision.current_state == "Unknown"
    assert legacy_status.supervision.requested_state == "Unknown"
    assert legacy_status.supervision.state_reason == "unknown"
    assert legacy_status.supervision.state_change_in_process is False
    assert legacy_status.safety_boundary.state == "safe"
    assert legacy_status.safety_boundary.motion_permitted is True
    assert legacy_status.safety_boundary.process_output_permitted is True
    assert legacy_status.action_capabilities.motion_commands_permitted is True
    assert legacy_status.action_capabilities.manual_output_commands_permitted is True
    assert legacy_status.action_capabilities.manual_dispenser_pause_permitted is True
    assert legacy_status.action_capabilities.manual_dispenser_resume_permitted is True
    assert legacy_status.action_capabilities.active_job_present is False
    assert legacy_status.action_capabilities.estop_reset_permitted is False
    assert "BuildRuntimeIdentityJson(status_snapshot)" in tcp_source
    assert "BuildJobExecutionJson(status_snapshot)" in tcp_source
    assert "runtimeStatusExportPort_->ReadSnapshot()" in tcp_source
    assert "BuildRawIoJson(status_snapshot)" in tcp_source
    assert "BuildEffectiveInterlocksJson(status_snapshot)" in tcp_source
    assert "BuildSupervisionJson(status_snapshot)" in tcp_source
    assert "BuildSafetyBoundaryJson(status_snapshot)" in tcp_source
    assert "BuildActionCapabilitiesJson(status_snapshot)" in tcp_source
    assert "BuildCompatMachineState(" not in tcp_source
    assert "{\"device_mode\", status_snapshot.device_mode}" in tcp_source
    assert "{\"machine_state\", status_snapshot.machine_state}" in tcp_source
    assert "{\"machine_state_reason\", status_snapshot.machine_state_reason}" in tcp_source
    assert "{\"supervision\", supervisionJson}" in tcp_source
    assert "{\"runtime_identity\", runtimeIdentityJson}" in tcp_source
    assert "{\"safety_boundary\", safetyBoundaryJson}" in tcp_source
    assert "{\"action_capabilities\", actionCapabilitiesJson}" in tcp_source
    assert "{\"effective_interlocks\", effectiveInterlocksJson}" in tcp_source
    assert "{\"job_execution\", jobExecutionJson}" in tcp_source
    assert "{\"active_job_id\"" not in tcp_source
    assert "{\"active_job_state\"" not in tcp_source
    assert "RuntimeIdentityExportSnapshot BuildRuntimeIdentitySnapshot()" in status_source
    assert "snapshot.runtime_identity = BuildRuntimeIdentitySnapshot();" in status_source
    assert 'snapshot.device_mode = snapshot.job_execution.dry_run ? "test" : "production";' in status_source
    assert "snapshot.machine_state = supervision.supervision.current_state;" in status_source
    assert "snapshot.machine_state_reason = supervision.supervision.state_reason;" in status_source
    assert "snapshot.io = supervision.io;" in status_source
    assert "snapshot.effective_interlocks = supervision.effective_interlocks;" in status_source
    assert "snapshot.supervision = supervision.supervision;" in status_source
    assert "snapshot.job_execution = BuildIdleJobExecutionSnapshot();" in status_source
    assert "snapshot.safety_boundary = BuildSafetyBoundarySnapshot(snapshot);" in status_source
    assert "BuildActionCapabilitiesSnapshot(snapshot, dispenser_status_for_action_capabilities);" in status_source
    assert 'snapshot.requested_state = "Idle";' in supervision_adapter
    assert 'snapshot.requested_state = "Estop";' in supervision_adapter
    assert 'snapshot.requested_state = "Fault";' in supervision_adapter
    assert 'snapshot.state_change_in_process = true;' in supervision_adapter
    assert 'snapshot.failure_stage = snapshot.failure_code.empty() ? "" : "runtime_status";' in supervision_adapter


def test_manual_dispenser_pause_resume_contracts_forbid_active_dxf_fallback():
    operations = load_operations()
    pause_notes = " ".join(operations["dispenser.pause"].get("compatibility", {}).get("notes", []))
    resume_notes = " ".join(operations["dispenser.resume"].get("compatibility", {}).get("notes", []))

    assert "直接失败" in pause_notes
    assert "不得隐式停止 DXF 作业" in pause_notes
    assert "直接失败" in resume_notes
    assert "隐式" in resume_notes


def test_mock_io_set_contract_and_hmi_helper():
    operations = load_operations()
    mock_io_set = operations["mock.io.set"]
    protocol_source = HMI_PROTOCOL.read_text(encoding="utf-8")

    assert {"estop", "door", "limit_x_neg", "limit_y_neg"}.issubset(set(mock_io_set["resultSchema"]["required"]))
    assert {2050, 2051, 2052}.issubset(set(mock_io_set["errorCodes"]))
    assert 'def mock_io_set' in protocol_source
    assert '_call("mock.io.set"' in protocol_source


def main():
    operations = load_operations()
    tests = [
      lambda: test_hmi_methods_are_contracted(operations),
      lambda: test_tcp_methods_match_contracts(operations),
      test_fixtures,
      test_known_compatibility_gaps_are_recorded,
      test_recipe_aliases_are_explicit,
      test_dxf_preview_and_job_contract,
      test_status_contract_describes_backend_interlock_authority,
      test_status_contract_exposes_effective_interlocks_and_supervision,
      test_manual_dispenser_pause_resume_contracts_forbid_active_dxf_fallback,
      test_mock_io_set_contract_and_hmi_helper,
    ]
    for test in tests:
        test()
    print("application-contracts compatibility checks passed")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
