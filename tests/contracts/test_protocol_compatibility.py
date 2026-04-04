import json
import re
import sys
from pathlib import Path
from typing import Any, cast

import pytest


ROOT = Path(__file__).resolve().parents[2]
HMI_APP_SRC = ROOT / "apps" / "hmi-app" / "src"
if str(HMI_APP_SRC) not in sys.path:
    sys.path.insert(0, str(HMI_APP_SRC))

CONTRACTS = ROOT / "shared" / "contracts" / "application"
HMI_PROTOCOL = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "client" / "protocol.py"
HMI_MAIN_WINDOW = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "ui" / "main_window.py"
TCP_DISPATCHER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "tcp" / "TcpCommandDispatcher.cpp"
RUNTIME_STATUS_EXPORT_PORT = ROOT / "apps" / "runtime-service" / "runtime" / "status" / "WorkflowRuntimeStatusExportPort.cpp"

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
        "dxf.load",
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
    assert "dxf.preview.snapshot" in operations
    assert "dxf.preview.confirm" in operations
    assert "dxf.artifact.create" in operations
    assert "dxf.plan.prepare" in operations
    assert "dxf.job.start" in operations
    assert "dxf.job.status" in operations

    preview = operations["dxf.preview.snapshot"]
    preview_params = preview["paramsSchema"]
    assert "plan_id" in preview_params["required"]
    preview_notes = " ".join(preview.get("compatibility", {}).get("notes", []))
    assert "显式 plan_id" in preview_notes
    assert "缺少 plan_id 时必须直接失败" in preview_notes
    assert "snapshot_hash" in preview["resultSchema"]["required"]
    assert "plan_id" in preview["resultSchema"]["required"]
    assert "preview_state" in preview["resultSchema"]["required"]
    assert "preview_source" in preview["resultSchema"]["required"]
    preview_result_properties = preview["resultSchema"]["properties"]
    assert "preview_source" in preview_result_properties
    assert "motion_preview" in preview_result_properties

    preview_confirm = operations["dxf.preview.confirm"]
    assert {"plan_id", "snapshot_hash"}.issubset(set(preview_confirm["paramsSchema"]["required"]))
    assert {"confirmed", "plan_id", "snapshot_hash", "preview_state", "confirmed_at"}.issubset(
        set(preview_confirm["resultSchema"]["required"])
    )

    legacy_methods = {"dxf.execute", "dxf.pause", "dxf.resume", "dxf.stop", "dxf.progress"}
    assert legacy_methods.isdisjoint(operations)

    plan_prepare = operations["dxf.plan.prepare"]
    assert "preview_request_signature" not in plan_prepare["resultSchema"]["required"]

    job_start = operations["dxf.job.start"]
    assert {"started", "job_id", "plan_id", "plan_fingerprint", "target_count"}.issubset(
        set(job_start["resultSchema"]["required"])
    )
    assert "task_id" not in job_start["resultSchema"]["required"]
    assert "task_id" not in job_start["resultSchema"]["properties"]

    job_status = operations["dxf.job.status"]
    assert job_status["resultRef"].endswith("#/definitions/dxfJobStatus")
    states = load_json(CONTRACTS / "models" / "states.json")
    dxf_job_status = states["definitions"]["dxfJobStatus"]
    assert "active_task_id" not in dxf_job_status["required"]
    assert "active_task_id" not in dxf_job_status["properties"]


def test_core_estop_reset_contract_and_disconnect_semantics():
    operations = load_operations()
    estop_reset = operations["estop.reset"]
    disconnect = operations["disconnect"]
    mock_io_set = operations["mock.io.set"]
    protocol_source = HMI_PROTOCOL.read_text(encoding="utf-8")

    assert {"reset", "message"}.issubset(set(estop_reset["resultSchema"]["required"]))
    assert {2602, 2603}.issubset(set(estop_reset["errorCodes"]))
    assert "hmi-client CommandProtocol.estop_reset" in estop_reset["consumers"]
    assert "def estop_reset" in protocol_source
    assert 'send_request("estop.reset")' in protocol_source

    notes = " ".join(disconnect.get("compatibility", {}).get("notes", []))
    assert "幂等断开确认" in notes
    assert {"estop", "door", "limit_x_neg", "limit_y_neg"}.issubset(set(mock_io_set["resultSchema"]["required"]))
    assert {2050, 2051, 2052}.issubset(set(mock_io_set["errorCodes"]))
    assert 'def mock_io_set' in protocol_source
    assert '_call("mock.io.set"' in protocol_source


def test_manual_precondition_error_codes_are_contracted():
    operations = load_operations()
    assert {2421, 2423}.issubset(set(operations["home.auto"]["errorCodes"]))
    assert {2411, 2417}.issubset(set(operations["home.go"]["errorCodes"]))
    assert {2304, 2305}.issubset(set(operations["jog"]["errorCodes"]))
    assert {2403, 2404}.issubset(set(operations["move"]["errorCodes"]))
    assert {2703}.issubset(set(operations["dispenser.start"]["errorCodes"]))
    assert {2842}.issubset(set(operations["supply.open"]["errorCodes"]))


def test_home_auto_contract_and_hmi_entry():
    operations = load_operations()
    protocol_source = HMI_PROTOCOL.read_text(encoding="utf-8")
    hmi_ui = HMI_MAIN_WINDOW.read_text(encoding="utf-8")

    home_auto = operations["home.auto"]
    required = set(home_auto["resultSchema"]["required"])
    assert {"accepted", "summary_state", "message", "axis_results", "total_time_ms"}.issubset(required)
    assert "hmi-client CommandProtocol.home_auto" in home_auto["consumers"]
    assert "def home_auto" in protocol_source
    assert 'send_request("home.auto"' in protocol_source
    assert "protocol.home_auto(" in hmi_ui
    assert "_resolve_home_action" not in hmi_ui


def test_status_contract_describes_backend_interlock_authority():
    states = load_json(CONTRACTS / "models" / "states.json")
    io_required = set(states["definitions"]["ioStatus"]["required"])
    io_props = states["definitions"]["ioStatus"]["properties"]
    axis_props = states["definitions"]["axisStatus"]["properties"]
    coord_axis_props = states["definitions"]["motionCoordAxisDiagnostics"]["properties"]
    assert {"estop_known", "door_known"}.issubset(io_required)
    assert "后端权威" in io_props["estop"]["description"]
    assert "运行时互锁端口" in io_props["door"]["description"]
    assert 'homing_state == "homed"' in axis_props["homed"]["description"]
    assert 'homing_state == "homed"' in coord_axis_props["homed"]["description"]


def test_status_contract_exposes_effective_interlocks_and_supervision():
    states = load_json(CONTRACTS / "models" / "states.json")
    fixture = load_json(CONTRACTS / "fixtures" / "responses" / "status.success.json")
    tcp_source = TCP_DISPATCHER.read_text(encoding="utf-8")

    machine_required = set(states["definitions"]["machineStatus"]["required"])
    effective_interlocks_required = set(states["definitions"]["effectiveInterlocks"]["required"])
    supervision_required = set(states["definitions"]["supervisionStatus"]["required"])

    assert {"supervision", "effective_interlocks"}.issubset(machine_required)
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
    assert "控制器有效保护，不等同于原始负限位输入" in states["definitions"]["effectiveInterlocks"]["properties"]["home_boundary_x_active"]["description"]
    assert "监督层当前目标状态" in states["definitions"]["supervisionStatus"]["properties"]["requested_state"]["description"]
    assert "监督态切换窗口" in states["definitions"]["supervisionStatus"]["properties"]["state_change_in_process"]["description"]

    fixture_result = fixture["result"]
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
    assert fixture_result["effective_interlocks"]["positive_escape_only_axes"] == []
    assert fixture_result["supervision"]["requested_state"] == "Idle"
    assert fixture_result["supervision"]["state_change_in_process"] is False

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
    assert status.machine_state == "Idle"
    assert status.runtime_state == "Idle"
    assert status.runtime_state_reason == "idle"
    assert status.gate_estop_known() is True
    assert status.gate_estop_active() is False
    assert status.gate_door_known() is True
    assert status.gate_door_active() is False
    assert status.home_boundary_active("X") is False
    assert status.home_boundary_active("Y") is False
    assert status.effective_interlocks.sources["estop"] == "system_interlock"
    assert status.supervision.requested_state == "Idle"
    assert status.supervision.state_change_in_process is False

    legacy_fixture = json.loads(json.dumps(fixture))
    legacy_result = legacy_fixture["result"]
    legacy_result["machine_state"] = "Preparing"
    legacy_result["machine_state_reason"] = "awaiting-supervision"
    legacy_result["supervision"] = ["legacy-payload"]

    legacy_status = CommandProtocol(cast(Any, StubClient(legacy_fixture))).get_status()
    assert legacy_status.supervision.current_state == "Preparing"
    assert legacy_status.supervision.requested_state == "Preparing"
    assert legacy_status.supervision.state_reason == "awaiting-supervision"
    assert legacy_status.supervision.state_change_in_process is False
    assert "runtimeStatusExportPort_->ReadSnapshot()" in tcp_source
    assert "BuildRawIoJson(status_snapshot)" in tcp_source
    assert "BuildEffectiveInterlocksJson(status_snapshot)" in tcp_source
    assert "BuildSupervisionJson(status_snapshot)" in tcp_source
    status_source = RUNTIME_STATUS_EXPORT_PORT.read_text(encoding="utf-8")
    assert "snapshot.machine_state = supervision.supervision.current_state;" in status_source
    assert "snapshot.machine_state_reason = supervision.supervision.state_reason;" in status_source
    assert "snapshot.io = supervision.io;" in status_source
    assert "snapshot.effective_interlocks = supervision.effective_interlocks;" in status_source
    assert "snapshot.supervision = supervision.supervision;" in status_source
    assert "{\"supervision\", supervisionJson}" in tcp_source
    assert "{\"effective_interlocks\", effectiveInterlocksJson}" in tcp_source


def main():
    operations = load_operations()
    tests = [
      lambda: test_hmi_methods_are_contracted(operations),
      lambda: test_tcp_methods_match_contracts(operations),
      test_fixtures,
      test_known_compatibility_gaps_are_recorded,
      test_recipe_aliases_are_explicit,
      test_dxf_preview_and_job_contract,
      test_core_estop_reset_contract_and_disconnect_semantics,
      test_manual_precondition_error_codes_are_contracted,
      test_home_auto_contract_and_hmi_entry,
      test_status_contract_describes_backend_interlock_authority,
      test_status_contract_exposes_effective_interlocks_and_supervision,
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
