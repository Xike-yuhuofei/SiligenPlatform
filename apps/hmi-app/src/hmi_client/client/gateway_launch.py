from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path


_APP_ROOT = Path(__file__).resolve().parents[3]
_DEFAULT_SPEC_PATH = _APP_ROOT / "config" / "gateway-launch.json"


@dataclass(frozen=True)
class GatewayLaunchSpec:
    executable: Path
    args: tuple[str, ...] = ()
    cwd: Path | None = None
    env: dict[str, str] = field(default_factory=dict)
    path_entries: tuple[Path, ...] = ()

    def command(self) -> list[str]:
        return [str(self.executable), *self.args]


def _resolve_path(value: str) -> Path:
    return Path(value).expanduser().resolve()


def _load_spec_file(path: Path) -> GatewayLaunchSpec:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError(f"gateway launch spec 必须是对象: {path}")

    executable = str(payload.get("executable", "")).strip()
    if not executable:
        raise ValueError(f"gateway launch spec 缺少 executable: {path}")

    args = payload.get("args", [])
    if args and not isinstance(args, list):
        raise ValueError(f"gateway launch spec args 必须是数组: {path}")

    env_payload = payload.get("env", {})
    if env_payload and not isinstance(env_payload, dict):
        raise ValueError(f"gateway launch spec env 必须是对象: {path}")

    path_entries = payload.get("pathEntries", [])
    if path_entries and not isinstance(path_entries, list):
        raise ValueError(f"gateway launch spec pathEntries 必须是数组: {path}")

    cwd = str(payload.get("cwd", "")).strip()
    return GatewayLaunchSpec(
        executable=_resolve_path(executable),
        args=tuple(str(item) for item in args),
        cwd=_resolve_path(cwd) if cwd else None,
        env={str(key): str(value) for key, value in env_payload.items()},
        path_entries=tuple(_resolve_path(str(item)) for item in path_entries),
    )

def load_gateway_launch_spec() -> GatewayLaunchSpec | None:
    spec_path_text = os.getenv("SILIGEN_GATEWAY_LAUNCH_SPEC", "").strip()
    if spec_path_text:
        return _load_spec_file(_resolve_path(spec_path_text))

    if _DEFAULT_SPEC_PATH.exists():
        default_spec = _load_spec_file(_DEFAULT_SPEC_PATH)
        if default_spec.executable.exists():
            return default_spec

    return None
