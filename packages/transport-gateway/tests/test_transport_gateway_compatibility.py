import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CONTRACTS = ROOT / "packages" / "application-contracts"
TCP_DISPATCHER = ROOT / "packages" / "transport-gateway" / "src" / "tcp" / "TcpCommandDispatcher.cpp"
APP_MAIN = ROOT / "control-core" / "apps" / "control-tcp-server" / "main.cpp"
APP_CMAKE = ROOT / "control-core" / "apps" / "control-tcp-server" / "CMakeLists.txt"
LEGACY_ADAPTER_CMAKE = ROOT / "control-core" / "src" / "adapters" / "tcp" / "CMakeLists.txt"
LEGACY_MODULE_CMAKE = ROOT / "control-core" / "modules" / "control-gateway" / "CMakeLists.txt"


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


def test_cmake_aliases_forward_to_canonical_targets():
    app_cmake = APP_CMAKE.read_text(encoding="utf-8")
    legacy_adapter = LEGACY_ADAPTER_CMAKE.read_text(encoding="utf-8")
    legacy_module = LEGACY_MODULE_CMAKE.read_text(encoding="utf-8")

    assert "siligen_transport_gateway" in app_cmake
    assert "siligen_runtime_host" in app_cmake
    assert "siligen_control_gateway_tcp_adapter" in legacy_adapter
    assert "siligen_tcp_adapter" in legacy_adapter
    assert "siligen_transport_gateway" in legacy_adapter
    assert "siligen_control_gateway" in legacy_module
    assert "siligen_transport_gateway_protocol" in legacy_module


def main():
    tests = [
        test_dispatcher_matches_contracts,
        test_app_entry_is_thin,
        test_cmake_aliases_forward_to_canonical_targets,
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
