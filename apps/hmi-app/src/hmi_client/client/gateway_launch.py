from __future__ import annotations

import configparser
import json
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, cast


_APP_ROOT = Path(__file__).resolve().parents[3]
_WORKSPACE_ROOT = _APP_ROOT.parents[1]
_DEFAULT_SPEC_PATH = _APP_ROOT / "config" / "gateway-launch.json"
_DEFAULT_MACHINE_CONFIG_PATH = _WORKSPACE_ROOT / "config" / "machine" / "machine_config.ini"


@dataclass(frozen=True)
class GatewayLaunchSpec:
    executable: Path
    args: tuple[str, ...] = ()
    cwd: Path | None = None
    env: dict[str, str] = field(default_factory=lambda: {})
    path_entries: tuple[Path, ...] = ()

    def command(self) -> list[str]:
        return [str(self.executable), *self.args]


@dataclass(frozen=True)
class MachineConnectionConfig:
    card_ip: str
    local_ip: str


def _resolve_path(value: str) -> Path:
    return Path(value).expanduser().resolve()


def _resolve_spec_arg_path(raw_value: str, launch_spec: GatewayLaunchSpec) -> Path:
    candidate = Path(raw_value).expanduser()
    if candidate.is_absolute():
        return candidate.resolve()
    if launch_spec.cwd is not None:
        return (launch_spec.cwd / candidate).resolve()
    return (_WORKSPACE_ROOT / candidate).resolve()


def _load_spec_file(path: Path) -> GatewayLaunchSpec:
    payload_raw = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload_raw, dict):
        raise ValueError(f"gateway launch spec 必须是对象: {path}")
    payload = cast(dict[str, Any], payload_raw)

    executable = str(payload.get("executable", "")).strip()
    if not executable:
        raise ValueError(f"gateway launch spec 缺少 executable: {path}")

    args_raw = payload.get("args", [])
    if args_raw and not isinstance(args_raw, list):
        raise ValueError(f"gateway launch spec args 必须是数组: {path}")
    args = cast(list[object], args_raw) if isinstance(args_raw, list) else []

    env_payload_raw = payload.get("env", {})
    if env_payload_raw and not isinstance(env_payload_raw, dict):
        raise ValueError(f"gateway launch spec env 必须是对象: {path}")
    env_payload = cast(dict[str, Any], env_payload_raw) if isinstance(env_payload_raw, dict) else {}

    path_entries_raw = payload.get("pathEntries", [])
    if path_entries_raw and not isinstance(path_entries_raw, list):
        raise ValueError(f"gateway launch spec pathEntries 必须是数组: {path}")
    path_entries = cast(list[object], path_entries_raw) if isinstance(path_entries_raw, list) else []

    cwd = str(payload.get("cwd", "")).strip()
    return GatewayLaunchSpec(
        executable=_resolve_path(executable),
        args=tuple(str(item) for item in args),
        cwd=_resolve_path(cwd) if cwd else None,
        env={str(key): str(value) for key, value in env_payload.items()},
        path_entries=tuple(_resolve_path(str(item)) for item in path_entries),
    )


def resolve_gateway_machine_config_path(launch_spec: GatewayLaunchSpec | None = None) -> Path:
    spec = launch_spec if launch_spec is not None else load_gateway_launch_spec()
    if spec is not None:
        args = list(spec.args)
        for index, arg in enumerate(args):
            if arg not in ("--config", "-c"):
                continue
            if index + 1 >= len(args):
                break
            return _resolve_spec_arg_path(args[index + 1], spec)
    return _DEFAULT_MACHINE_CONFIG_PATH.resolve()


def load_gateway_connection_config(launch_spec: GatewayLaunchSpec | None = None) -> MachineConnectionConfig | None:
    config_path = resolve_gateway_machine_config_path(launch_spec)
    if not config_path.exists():
        return None

    parser = configparser.ConfigParser(interpolation=None, strict=False)
    try:
        with config_path.open("r", encoding="utf-8") as handle:
            parser.read_file(handle)
    except (OSError, configparser.Error):
        return None

    if not parser.has_section("Network"):
        return None

    card_ip = parser.get("Network", "control_card_ip", fallback=parser.get("Network", "card_ip", fallback="")).strip()
    local_ip = parser.get("Network", "local_ip", fallback="").strip()
    if not card_ip or not local_ip:
        return None

    return MachineConnectionConfig(card_ip=card_ip, local_ip=local_ip)


def load_gateway_launch_spec() -> GatewayLaunchSpec | None:
    spec_path_text = os.getenv("SILIGEN_GATEWAY_LAUNCH_SPEC", "").strip()
    if spec_path_text:
        return _load_spec_file(_resolve_path(spec_path_text))

    if _DEFAULT_SPEC_PATH.exists():
        default_spec = _load_spec_file(_DEFAULT_SPEC_PATH)
        if default_spec.executable.exists():
            return default_spec

    return None
