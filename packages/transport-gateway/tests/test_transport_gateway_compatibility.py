import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CONTRACTS = ROOT / "packages" / "application-contracts"
TCP_DISPATCHER = ROOT / "packages" / "transport-gateway" / "src" / "tcp" / "TcpCommandDispatcher.cpp"
APP_MAIN = ROOT / "apps" / "control-tcp-server" / "main.cpp"
APP_CMAKE = ROOT / "apps" / "control-tcp-server" / "CMakeLists.txt"
TRANSPORT_GATEWAY_CMAKE = ROOT / "packages" / "transport-gateway" / "CMakeLists.txt"
ROOT_CMAKE = ROOT / "CMakeLists.txt"


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


def extract_tcp_methods():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    return set(re.findall(r'RegisterCommand\("([^"]+)"', source))


def test_dispatcher_matches_contracts():
    operations = load_operations()
    tcp_methods = extract_tcp_methods()
    missing = sorted(tcp_methods - operations.keys())
    extra = sorted(operations.keys() - tcp_methods)
    assert not missing, f"transport-gateway methods missing from contracts: {missing}"
    assert not extra, f"contract methods not registered by transport-gateway: {extra}"


def test_app_entry_is_thin():
    source = APP_MAIN.read_text(encoding="utf-8")
    assert '#include "siligen/gateway/tcp/tcp_facade_builder.h"' in source
    assert '#include "siligen/gateway/tcp/tcp_server_host.h"' in source
    assert 'BuildTcpFacadeBundle(*container)' in source
    assert 'TcpServerHost server_host(' in source
    assert 'TcpCommandDispatcher' not in source
    assert 'modules/control-gateway/src/' not in source


def test_canonical_targets_are_exported_without_legacy_aliases():
    app_cmake = APP_CMAKE.read_text(encoding="utf-8")
    transport_gateway_cmake = TRANSPORT_GATEWAY_CMAKE.read_text(encoding="utf-8")
    root_cmake = ROOT_CMAKE.read_text(encoding="utf-8")

    assert "siligen_transport_gateway" in app_cmake
    assert "siligen_runtime_host" in app_cmake
    assert "siligen_transport_gateway" in transport_gateway_cmake
    assert "siligen_transport_gateway_protocol" in transport_gateway_cmake
    assert "siligen_control_gateway_tcp_adapter" in transport_gateway_cmake
    assert "siligen_tcp_adapter" in transport_gateway_cmake
    assert "siligen_control_gateway" in transport_gateway_cmake
    assert 'add_subdirectory("${SILIGEN_TRANSPORT_GATEWAY_DIR}"' in root_cmake
    assert "packages/transport-gateway" in root_cmake
    assert 'add_subdirectory(apps)' in root_cmake


def test_dxf_preview_gate_contract_is_wired():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    assert 'RegisterCommand("dxf.preview.snapshot"' in source
    assert 'RegisterCommand("dxf.artifact.create"' in source
    assert 'RegisterCommand("dxf.plan.prepare"' in source
    assert 'RegisterCommand("dxf.job.start"' in source
    assert 'RegisterCommand("dxf.job.status"' in source
    assert 'return HandleDxfPlanPrepare(id, params);' in source
    assert "Missing 'snapshot_hash'" in source
    assert "Preview snapshot not prepared" in source
    assert "Preview request signature mismatch" in source
    assert "Preview snapshot hash mismatch" in source
    assert "HandleDxfPreviewSnapshot" in source


def test_status_reads_backend_interlock_signals():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    assert "ReadInterlockSignals()" in source
    assert 'ioJson["door"] = signals.safety_door_open' in source
    assert 'ioJson["estop"] = estop_active || signals.emergency_stop_triggered' in source


def test_motion_coord_status_exposes_feedback_diagnostics():
    source = TCP_DISPATCHER.read_text(encoding="utf-8")
    assert 'RegisterCommand("motion.coord.status"' in source
    assert '{"selected_feedback_source", x_status_result.Value().selected_feedback_source}' in source
    assert '{"prf_position_mm", x_status_result.Value().profile_position_mm}' in source
    assert '{"enc_position_mm", x_status_result.Value().encoder_position_mm}' in source
    assert '{"prf_velocity_mm_s", x_status_result.Value().profile_velocity_mm_s}' in source
    assert '{"enc_velocity_mm_s", x_status_result.Value().encoder_velocity_mm_s}' in source
    assert '{"prf_position_ret", x_status_result.Value().profile_position_ret}' in source
    assert '{"enc_position_ret", x_status_result.Value().encoder_position_ret}' in source
    assert '{"prf_velocity_ret", x_status_result.Value().profile_velocity_ret}' in source
    assert '{"enc_velocity_ret", x_status_result.Value().encoder_velocity_ret}' in source
    assert '{"selected_feedback_source", y_status_result.Value().selected_feedback_source}' in source
    assert '{"prf_position_mm", y_status_result.Value().profile_position_mm}' in source
    assert '{"enc_position_mm", y_status_result.Value().encoder_position_mm}' in source
    assert '{"prf_velocity_mm_s", y_status_result.Value().profile_velocity_mm_s}' in source
    assert '{"enc_velocity_mm_s", y_status_result.Value().encoder_velocity_mm_s}' in source
    assert '{"prf_position_ret", y_status_result.Value().profile_position_ret}' in source
    assert '{"enc_position_ret", y_status_result.Value().encoder_position_ret}' in source
    assert '{"prf_velocity_ret", y_status_result.Value().profile_velocity_ret}' in source
    assert '{"enc_velocity_ret", y_status_result.Value().encoder_velocity_ret}' in source


def main():
    tests = [
        test_dispatcher_matches_contracts,
        test_app_entry_is_thin,
        test_canonical_targets_are_exported_without_legacy_aliases,
        test_dxf_preview_gate_contract_is_wired,
        test_status_reads_backend_interlock_signals,
        test_motion_coord_status_exposes_feedback_diagnostics,
    ]
    for test in tests:
        test()
    print("transport-gateway compatibility checks passed")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as exc:
        print(f"FAIL: {exc}", file=sys.stderr)
        sys.exit(1)
