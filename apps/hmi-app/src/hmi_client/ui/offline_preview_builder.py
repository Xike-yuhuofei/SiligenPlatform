import json
import os
import subprocess
from pathlib import Path


_PLANNER_CLI_BUILD_CONFIG_ENV = "SILIGEN_PLANNER_CLI_BUILD_CONFIG"
_CONTROL_APPS_BUILD_ROOT_ENV = "SILIGEN_CONTROL_APPS_BUILD_ROOT"


def _resolve_workspace_root() -> Path:
    current = Path(__file__).resolve()
    for parent in current.parents:
        marker = parent / "apps" / "planner-cli" / "run.ps1"
        if marker.exists():
            return parent
    raise FileNotFoundError("未找到 apps/planner-cli，无法生成离线同源预览")


def _resolve_build_roots(workspace_root: Path) -> list[Path]:
    roots: list[Path] = []
    env_root = str(os.getenv(_CONTROL_APPS_BUILD_ROOT_ENV, "")).strip()
    if env_root:
        roots.append(Path(env_root).expanduser())
    roots.append(workspace_root / "build" / "control-apps")
    roots.append(workspace_root / "build")
    local_app_data = str(os.getenv("LOCALAPPDATA", "")).strip()
    if local_app_data:
        roots.append(Path(local_app_data) / "SiligenSuite" / "control-apps-build")
    unique_roots: list[Path] = []
    for root in roots:
        resolved = root.resolve()
        if resolved not in unique_roots:
            unique_roots.append(resolved)
    return unique_roots


def _env_build_root() -> Path | None:
    env_root = str(os.getenv(_CONTROL_APPS_BUILD_ROOT_ENV, "")).strip()
    if not env_root:
        return None
    return Path(env_root).expanduser().resolve()


def _cache_matches_workspace(build_root: Path, workspace_root: Path) -> bool:
    cache_path = build_root / "CMakeCache.txt"
    if not cache_path.exists():
        return False
    prefix = "CMAKE_HOME_DIRECTORY:INTERNAL="
    for line in cache_path.read_text(encoding="utf-8", errors="replace").splitlines():
        if not line.startswith(prefix):
            continue
        configured_source = Path(line[len(prefix) :].strip()).resolve()
        return configured_source == workspace_root.resolve()
    return False


def _resolve_planner_cli_executable(workspace_root: Path) -> Path:
    build_config = _resolve_build_config()
    candidate_rel_paths = [
        Path("bin") / build_config / "siligen_planner_cli.exe",
        Path("bin") / "siligen_planner_cli.exe",
        Path("bin") / "Debug" / "siligen_planner_cli.exe",
        Path("bin") / "Release" / "siligen_planner_cli.exe",
        Path("bin") / "RelWithDebInfo" / "siligen_planner_cli.exe",
    ]
    override_root = _env_build_root()
    for build_root in _resolve_build_roots(workspace_root):
        if build_root != override_root and not _cache_matches_workspace(build_root, workspace_root):
            continue
        for relative_path in candidate_rel_paths:
            candidate = build_root / relative_path
            if candidate.exists():
                return candidate
    raise FileNotFoundError("未找到当前工作区的 siligen_planner_cli.exe，请先构建当前工作区 planner-cli")


def _resolve_build_config() -> str:
    raw = str(os.getenv(_PLANNER_CLI_BUILD_CONFIG_ENV, "")).strip()
    return raw or "Debug"


def _build_command(
    *,
    executable_path: Path,
    input_path: Path,
    speed_mm_s: float,
    dry_run: bool,
) -> list[str]:
    command = [
        str(executable_path),
        "dxf-preview-snapshot",
        "--file",
        str(input_path),
        "--json",
        "--dxf-speed",
        f"{max(float(speed_mm_s or 0.0), 0.1):.6f}",
        "--no-optimize-path",
        "--use-interpolation-planner",
        "--interpolation-algorithm",
        "0",
    ]
    if dry_run:
        command.extend(
            [
                "--dry-run",
                "--dry-run-speed",
                f"{max(float(speed_mm_s or 0.0), 0.1):.6f}",
            ]
        )
    return command


def _extract_json_payload(stdout_text: str) -> dict:
    text = str(stdout_text or "").strip()
    if not text:
        raise RuntimeError("planner-cli 未返回预览 JSON")
    try:
        return json.loads(text)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"planner-cli 返回了非 JSON 内容: {text}") from exc


def build_offline_preview_payload(
    filepath: str | Path,
    *,
    speed_mm_s: float,
    dry_run: bool,
) -> dict:
    input_path = Path(filepath).expanduser().resolve()
    if not input_path.exists():
        raise FileNotFoundError(f"DXF 文件不存在: {input_path}")

    workspace_root = _resolve_workspace_root()
    executable_path = _resolve_planner_cli_executable(workspace_root)
    command = _build_command(
        executable_path=executable_path,
        input_path=input_path,
        speed_mm_s=speed_mm_s,
        dry_run=dry_run,
    )

    completed = subprocess.run(
        command,
        cwd=str(workspace_root),
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        check=False,
    )
    if completed.returncode != 0:
        detail = (completed.stderr or "").strip() or (completed.stdout or "").strip()
        if not detail:
            detail = f"planner-cli 退出码异常: {completed.returncode}"
        raise RuntimeError(detail)

    payload = _extract_json_payload(completed.stdout)
    if not isinstance(payload, dict):
        raise RuntimeError("planner-cli 返回的预览 JSON 不是对象")
    return payload
