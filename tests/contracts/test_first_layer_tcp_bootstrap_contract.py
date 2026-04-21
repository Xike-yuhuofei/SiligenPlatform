from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

import pytest


ROOT = Path(__file__).resolve().parents[2]
HIL_DIR = ROOT / "tests" / "e2e" / "hardware-in-loop"
if str(HIL_DIR) not in sys.path:
    sys.path.insert(0, str(HIL_DIR))

import runtime_gateway_harness as harness


SCRIPT_PATHS = (
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_door_interlock.py",
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_estop_chain.py",
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_jog_focus_smoke.py",
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_manual_motion_matrix.py",
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_negative_limit_chain.py",
    ROOT / "tests" / "integration" / "scenarios" / "first-layer" / "run_tcp_precondition_matrix.py",
)

EMPTY_CONNECT_PATTERNS = (
    re.compile(r'send_request\(\s*"connect"\s*,\s*\{\s*\}'),
    re.compile(r'call\(\s*"[^"]+"\s*,\s*"connect"\s*,\s*\{\s*\}'),
)


def _probe_script_root(script_path: Path) -> Path:
    probe_code = "\n".join(
        (
            "import importlib.util",
            "import sys",
            "from pathlib import Path",
            f"script_path = Path(r'{script_path.as_posix()}')",
            "sys.path.insert(0, str(script_path.parent))",
            "spec = importlib.util.spec_from_file_location('codex_first_layer_bootstrap_probe', script_path)",
            "module = importlib.util.module_from_spec(spec)",
            "assert spec is not None and spec.loader is not None",
            "sys.modules[spec.name] = module",
            "spec.loader.exec_module(module)",
            "print(module.ROOT)",
        )
    )
    completed = subprocess.run(
        [sys.executable, "-c", probe_code],
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        timeout=30,
        check=False,
    )
    assert completed.returncode == 0, completed.stderr
    return Path(completed.stdout.strip())


def _write_machine_config(path: Path, *, card_ip: str, local_ip: str, door_input: int = -1) -> None:
    path.write_text(
        "\n".join(
            (
                "[Network]",
                f"control_card_ip={card_ip}",
                f"local_ip={local_ip}",
                "",
                "[Hardware]",
                "mode=Real",
                "",
                "[Interlock]",
                f"safety_door_input={door_input}",
                "",
            )
        ),
        encoding="utf-8",
    )


@pytest.mark.parametrize("script_path", SCRIPT_PATHS, ids=lambda path: path.name)
def test_first_layer_tcp_scripts_resolve_workspace_root(script_path: Path) -> None:
    assert _probe_script_root(script_path) == ROOT


@pytest.mark.parametrize("script_path", SCRIPT_PATHS, ids=lambda path: path.name)
def test_first_layer_tcp_scripts_do_not_send_empty_connect_payload(script_path: Path) -> None:
    source = script_path.read_text(encoding="utf-8")
    for pattern in EMPTY_CONNECT_PATTERNS:
        assert pattern.search(source) is None, f"{script_path.name} still sends empty connect payload"


@pytest.mark.parametrize("script_path", SCRIPT_PATHS, ids=lambda path: path.name)
def test_first_layer_tcp_scripts_use_shared_connect_helper(script_path: Path) -> None:
    source = script_path.read_text(encoding="utf-8")
    assert "load_connection_params" in source or "tcp_connect_and_ensure_ready" in source


def test_prepare_mock_config_uses_canonical_source_by_default() -> None:
    temp_dir = None
    try:
        temp_dir, config_path = harness.prepare_mock_config("codex-first-layer-bootstrap-default-")
        rendered = config_path.read_text(encoding="utf-8")
        params = harness.load_connection_params(config_path)
        assert "mode=Mock" in rendered
        assert params["card_ip"]
        assert params["local_ip"]
    finally:
        if temp_dir is not None:
            temp_dir.cleanup()


def test_prepare_mock_config_honors_custom_source_config(tmp_path: Path) -> None:
    source_config = tmp_path / "machine_config.custom.ini"
    _write_machine_config(source_config, card_ip="10.1.1.7", local_ip="10.1.1.8")

    temp_dir = None
    try:
        temp_dir, config_path = harness.prepare_mock_config(
            "codex-first-layer-bootstrap-custom-",
            source_config=source_config,
        )
        rendered = config_path.read_text(encoding="utf-8")
        params = harness.load_connection_params(config_path)
        assert "mode=Mock" in rendered
        assert params == {"card_ip": "10.1.1.7", "local_ip": "10.1.1.8"}
    finally:
        if temp_dir is not None:
            temp_dir.cleanup()


def test_prepare_mock_config_honors_door_input_override(tmp_path: Path) -> None:
    source_config = tmp_path / "machine_config.custom.ini"
    _write_machine_config(source_config, card_ip="10.2.2.7", local_ip="10.2.2.8", door_input=-1)

    temp_dir = None
    try:
        temp_dir, config_path = harness.prepare_mock_config(
            "codex-first-layer-bootstrap-door-",
            source_config=source_config,
            door_input=6,
        )
        rendered = config_path.read_text(encoding="utf-8")
        assert "mode=Mock" in rendered
        assert "safety_door_input=6" in rendered
    finally:
        if temp_dir is not None:
            temp_dir.cleanup()
