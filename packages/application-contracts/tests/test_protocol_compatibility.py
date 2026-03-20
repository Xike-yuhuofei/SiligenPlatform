import json
import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
CONTRACTS = ROOT / "packages" / "application-contracts"
HMI_PROTOCOL = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "client" / "protocol.py"
HMI_MAIN_WINDOW = ROOT / "apps" / "hmi-app" / "src" / "hmi_client" / "ui" / "main_window.py"
TCP_DISPATCHER = ROOT / "packages" / "transport-gateway" / "src" / "tcp" / "TcpCommandDispatcher.cpp"


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
    assert "dxf-info-total-segments-gap" in ids
    assert "recipe-request-aliases" in ids

    hmi_ui = HMI_MAIN_WINDOW.read_text(encoding="utf-8")
    tcp_source = TCP_DISPATCHER.read_text(encoding="utf-8")
    assert 'info.get("total_segments"' in hmi_ui

    dxf_info_match = re.search(
        r"std::string TcpCommandDispatcher::HandleDxfInfo.*?return GatewayJsonProtocol::MakeSuccessResponse",
        tcp_source,
        re.S,
    )
    assert dxf_info_match, "cannot locate HandleDxfInfo body"
    assert '"total_segments"' not in dxf_info_match.group(0)


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


def main():
    operations = load_operations()
    tests = [
      lambda: test_hmi_methods_are_contracted(operations),
      lambda: test_tcp_methods_match_contracts(operations),
      test_fixtures,
      test_known_compatibility_gaps_are_recorded,
      test_recipe_aliases_are_explicit,
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
