import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[4]
CONTRACTS = ROOT / "shared" / "contracts" / "application"
HMI_PROTOCOL = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "client" / "protocol.py"
HMI_MAIN_WINDOW = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "ui" / "main_window.py"
TCP_DISPATCHER = ROOT / "apps" / "runtime-gateway" / "transport-gateway" / "src" / "tcp" / "TcpCommandDispatcher.cpp"


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
    assert "segments = getattr(self, '_dxf_segment_count_cache', 0)" in hmi_ui

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
    assert "snapshot_hash" in preview["resultSchema"]["required"]
    assert "plan_id" in preview["resultSchema"]["required"]
    assert "preview_state" in preview["resultSchema"]["required"]
    preview_result_properties = preview["resultSchema"]["properties"]
    assert "trajectory_polyline" in preview_result_properties
    assert "polyline_point_count" in preview_result_properties
    assert "polyline_source_point_count" in preview_result_properties

    preview_confirm = operations["dxf.preview.confirm"]
    assert {"plan_id", "snapshot_hash"}.issubset(set(preview_confirm["paramsSchema"]["required"]))
    assert {"confirmed", "plan_id", "snapshot_hash", "preview_state", "confirmed_at"}.issubset(
        set(preview_confirm["resultSchema"]["required"])
    )

    legacy_methods = {"dxf.execute", "dxf.pause", "dxf.resume", "dxf.stop", "dxf.progress"}
    assert legacy_methods.isdisjoint(operations)

    plan_prepare = operations["dxf.plan.prepare"]
    assert "preview_request_signature" not in plan_prepare["resultSchema"]["required"]

    job_status = operations["dxf.job.status"]
    assert job_status["resultRef"].endswith("#/definitions/dxfJobStatus")


def test_status_contract_describes_backend_interlock_authority():
    states = load_json(CONTRACTS / "models" / "states.json")
    io_required = set(states["definitions"]["ioStatus"]["required"])
    io_props = states["definitions"]["ioStatus"]["properties"]
    assert {"estop_known", "door_known"}.issubset(io_required)
    assert "后端权威" in io_props["estop"]["description"]
    assert "运行时互锁端口" in io_props["door"]["description"]


def test_status_contract_exposes_effective_interlocks_and_supervision():
    states = load_json(CONTRACTS / "models" / "states.json")
    fixture = load_json(CONTRACTS / "fixtures" / "responses" / "status.success.json")
    protocol_source = HMI_PROTOCOL.read_text(encoding="utf-8")
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
    assert "effective_interlocks_data = result.get(\"effective_interlocks\", {})" in protocol_source
    assert "supervision_data = result.get(\"supervision\", {})" in protocol_source
    assert "def gate_estop_active" in protocol_source
    assert "def gate_door_active" in protocol_source
    assert "def home_boundary_active" in protocol_source
    assert "{\"supervision\", supervisionJson}" in tcp_source
    assert "{\"effective_interlocks\", effectiveInterlocksJson}" in tcp_source


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
