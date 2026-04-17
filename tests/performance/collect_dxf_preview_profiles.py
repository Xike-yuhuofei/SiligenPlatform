from __future__ import annotations

import argparse
import configparser
import json
import math
import os
import platform
import statistics
import subprocess
import sys
import threading
import time
from contextlib import contextmanager
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
REPORT_ROOT = ROOT / "tests" / "reports" / "performance" / "dxf-preview-profiles"
DEFAULT_THRESHOLD_CONFIG = ROOT / "tests" / "baselines" / "performance" / "dxf-preview-profile-thresholds.json"
DEFAULT_MACHINE_CONFIG = ROOT / "config" / "machine" / "machine_config.ini"
DEFAULT_VENDOR_DIR = ROOT / "modules" / "runtime-execution" / "adapters" / "device" / "vendor" / "multicard"
HMI_SRC = ROOT / "apps" / "hmi-app" / "src"
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
TERMINAL_JOB_STATES = {"completed", "failed", "cancelled", "stopped"}

if str(HMI_SRC) not in sys.path:
    sys.path.insert(0, str(HMI_SRC))
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from hmi_client.client.backend_manager import BackendManager  # noqa: E402
from hmi_client.client.gateway_launch import GatewayLaunchSpec, load_gateway_launch_spec  # noqa: E402
from hmi_client.client.protocol import CommandProtocol  # noqa: E402
from hmi_client.client.tcp_client import TcpClient  # noqa: E402
from test_kit.asset_catalog import default_performance_samples, performance_sample_asset_refs  # noqa: E402
from test_kit.control_apps_build import preferred_control_apps_build_root, valid_control_apps_build_roots  # noqa: E402
from test_kit.evidence_bundle import EvidenceBundle, EvidenceCaseRecord, EvidenceLink, trace_fields, write_bundle_artifacts  # noqa: E402


DEFAULT_SAMPLES: dict[str, Path] = default_performance_samples(ROOT)
CONTROL_APPS_BUILD_ROOT = preferred_control_apps_build_root(
    ROOT,
    explicit_build_root=os.getenv("SILIGEN_CONTROL_APPS_BUILD_ROOT"),
).root


def gateway_executable_candidates(
    *,
    workspace_root: Path,
    control_apps_build_root: Path,
) -> tuple[Path, ...]:
    file_name = "siligen_runtime_gateway.exe"
    discovered_roots = valid_control_apps_build_roots(
        workspace_root,
        explicit_build_root=os.getenv("SILIGEN_CONTROL_APPS_BUILD_ROOT"),
    )
    prioritized_roots: list[Path] = []
    seen_roots: set[Path] = set()
    for root in (control_apps_build_root, *discovered_roots):
        resolved_root = root.resolve()
        if resolved_root in seen_roots:
            continue
        seen_roots.add(resolved_root)
        prioritized_roots.append(resolved_root)

    build_root_candidates: list[Path] = []
    for root in prioritized_roots:
        build_root_candidates.extend(
            (
                root / "bin" / file_name,
                root / "bin" / "Debug" / file_name,
                root / "bin" / "Release" / file_name,
                root / "bin" / "RelWithDebInfo" / file_name,
            )
        )
    return tuple(build_root_candidates)


def resolve_default_gateway_executable(
    *,
    workspace_root: Path = ROOT,
    control_apps_build_root: Path = CONTROL_APPS_BUILD_ROOT,
) -> Path:
    candidates = gateway_executable_candidates(
        workspace_root=workspace_root,
        control_apps_build_root=control_apps_build_root,
    )
    for candidate in candidates:
        if candidate.exists():
            return candidate
    return candidates[1]


DEFAULT_GATEWAY_EXE = resolve_default_gateway_executable()


@dataclass(frozen=True)
class LaunchSpecResolution:
    source: str
    spec: GatewayLaunchSpec
    config_path: Path | None = None
    hardware_mode: str = "unknown"
    auto_provisioned_mock: bool = False


@dataclass(frozen=True)
class BackendLease:
    mode: str
    host: str
    port: int
    started_by_script: bool
    manager: BackendManager | None
    launch_spec_source: str
    launch_spec_executable: str


@dataclass(frozen=True)
class PreviewCycleRecord:
    success: bool
    artifact_id: str = ""
    plan_id: str = ""
    plan_fingerprint: str = ""
    snapshot_hash: str = ""
    glue_point_count: int = 0
    motion_preview_point_count: int = 0
    artifact_ms: float | None = None
    plan_prepare_rpc_ms: float | None = None
    snapshot_rpc_ms: float | None = None
    worker_total_ms: float | None = None
    authority_cache_hit: bool | None = None
    authority_joined_inflight: bool | None = None
    authority_wait_ms: float | None = None
    pb_prepare_ms: float | None = None
    path_load_ms: float | None = None
    process_path_ms: float | None = None
    authority_build_ms: float | None = None
    authority_total_ms: float | None = None
    prepare_total_ms: float | None = None
    error: str = ""


@dataclass(frozen=True)
class StartJobCycleRecord:
    success: bool
    artifact_id: str = ""
    plan_id: str = ""
    plan_fingerprint: str = ""
    snapshot_hash: str = ""
    job_id: str = ""
    confirm_rpc_ms: float | None = None
    job_start_rpc_ms: float | None = None
    job_status_rpc_ms: float | None = None
    job_stop_rpc_ms: float | None = None
    job_lifecycle_rpc_ms: float | None = None
    execution_cache_hit: bool | None = None
    execution_joined_inflight: bool | None = None
    execution_wait_ms: float | None = None
    motion_plan_ms: float | None = None
    assembly_ms: float | None = None
    export_ms: float | None = None
    execution_total_ms: float | None = None
    job_status_state: str = ""
    error: str = ""


@dataclass(frozen=True)
class ConcurrentPrepareRecord:
    request_index: int
    success: bool
    plan_id: str = ""
    plan_fingerprint: str = ""
    plan_prepare_rpc_ms: float | None = None
    authority_cache_hit: bool | None = None
    authority_joined_inflight: bool | None = None
    authority_wait_ms: float | None = None
    pb_prepare_ms: float | None = None
    path_load_ms: float | None = None
    process_path_ms: float | None = None
    authority_build_ms: float | None = None
    authority_total_ms: float | None = None
    prepare_total_ms: float | None = None
    error: str = ""


@dataclass(frozen=True)
class ControlCycleRecord:
    round_index: int
    success: bool
    pause_resume_applied: bool = False
    stop_reset_applied: bool = False
    plan_id: str = ""
    job_id: str = ""
    pause_to_running_ms: float | None = None
    stop_to_idle_ms: float | None = None
    stop_rpc_ms: float | None = None
    stop_wait_for_terminal_ms: float | None = None
    stop_wait_for_idle_ms: float | None = None
    stop_terminal_state: str = ""
    stop_terminal_error: str = ""
    stop_idle_desc: str = ""
    rerun_total_ms: float | None = None
    execution_total_ms: float | None = None
    timeout_detected: bool = False
    error: str = ""


@dataclass(frozen=True)
class StopTransitionRecord:
    success: bool
    total_ms: float
    rpc_ms: float | None = None
    wait_for_terminal_ms: float | None = None
    wait_for_idle_ms: float | None = None
    terminal_state: str = ""
    terminal_error: str = ""
    idle_desc: str = ""


@dataclass(frozen=True)
class LongRunIterationRecord:
    iteration_index: int
    success: bool
    execution_total_ms: float | None = None
    timeout_detected: bool = False
    error: str = ""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Collect DXF preview/execution profiles and single-flight evidence."
    )
    parser.add_argument(
        "--sample-labels",
        nargs="*",
        default=list(DEFAULT_SAMPLES.keys()),
        help="Sample labels to execute. Defaults to small medium large.",
    )
    parser.add_argument(
        "--sample",
        action="append",
        default=[],
        metavar="LABEL=PATH",
        help="Override or add a sample definition.",
    )
    parser.add_argument("--host", default=os.getenv("SILIGEN_TCP_SERVER_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.getenv("SILIGEN_TCP_SERVER_PORT", "9527")))
    parser.add_argument("--launch-spec", default="")
    parser.add_argument("--config-path", default=str(DEFAULT_MACHINE_CONFIG))
    parser.add_argument("--vendor-dir", default=str(DEFAULT_VENDOR_DIR))
    parser.add_argument("--gateway-exe", default=str(DEFAULT_GATEWAY_EXE))
    parser.add_argument("--recipe-id", required=True)
    parser.add_argument("--version-id", required=True)
    parser.add_argument("--cold-iterations", type=int, default=1)
    parser.add_argument("--hot-warmup-iterations", type=int, default=1)
    parser.add_argument("--hot-iterations", type=int, default=3)
    parser.add_argument("--singleflight-rounds", type=int, default=1)
    parser.add_argument("--singleflight-fanout", type=int, default=4)
    parser.add_argument("--dispensing-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--dry-run", action=argparse.BooleanOptionalAction, default=True)
    parser.add_argument("--dry-run-speed-mm-s", type=float, default=10.0)
    parser.add_argument("--artifact-timeout", type=float, default=120.0)
    parser.add_argument("--prepare-timeout", type=float, default=300.0)
    parser.add_argument("--snapshot-timeout", type=float, default=300.0)
    parser.add_argument("--max-polyline-points", type=int, default=4000)
    parser.add_argument(
        "--include-start-job",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="After preview.confirm, continue with dxf.job.start -> status -> stop and collect execution profiles.",
    )
    parser.add_argument(
        "--include-control-cycles",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Collect pause/resume and stop/reset control-cycle evidence on top of the execution path.",
    )
    parser.add_argument("--pause-resume-cycles", type=int, default=3)
    parser.add_argument("--stop-reset-rounds", type=int, default=3)
    parser.add_argument("--long-run-minutes", type=float, default=0.0)
    parser.add_argument(
        "--baseline-json",
        default="",
        help="Compare the current run against a prior JSON report and emit drift diagnostics.",
    )
    parser.add_argument(
        "--regression-threshold-pct",
        type=float,
        default=10.0,
        help="Positive drift percentage above which a metric is marked as regression in the report.",
    )
    parser.add_argument(
        "--reuse-running",
        action=argparse.BooleanOptionalAction,
        default=False,
        help="Reuse an already listening gateway instead of failing.",
    )
    parser.add_argument("--report-dir", default=str(REPORT_ROOT))
    parser.add_argument("--gate-mode", choices=("adhoc", "nightly-performance"), default="adhoc")
    parser.add_argument("--threshold-config", default=str(DEFAULT_THRESHOLD_CONFIG))
    return parser.parse_args()


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def timestamp_slug() -> str:
    return datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")


def percentile(values: list[float], q: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    if len(ordered) == 1:
        return ordered[0]
    position = (len(ordered) - 1) * q
    lower = math.floor(position)
    upper = math.ceil(position)
    if lower == upper:
        return ordered[lower]
    weight = position - lower
    return ordered[lower] * (1.0 - weight) + ordered[upper] * weight


def summarize_numbers(values: list[float]) -> dict[str, Any]:
    if not values:
        return {
            "count": 0,
            "mean_ms": 0.0,
            "p50_ms": 0.0,
            "p95_ms": 0.0,
            "min_ms": 0.0,
            "max_ms": 0.0,
            "stdev_ms": 0.0,
        }
    mean = statistics.fmean(values)
    stdev = statistics.stdev(values) if len(values) > 1 else 0.0
    return {
        "count": len(values),
        "mean_ms": round(mean, 3),
        "p50_ms": round(statistics.median(values), 3),
        "p95_ms": round(percentile(values, 0.95), 3),
        "min_ms": round(min(values), 3),
        "max_ms": round(max(values), 3),
        "stdev_ms": round(stdev, 3),
    }


def summarize_bool(values: list[bool]) -> dict[str, Any]:
    total = len(values)
    true_count = sum(1 for item in values if item)
    return {
        "count": total,
        "true_count": true_count,
        "false_count": total - true_count,
        "true_rate": round((true_count / total), 4) if total else 0.0,
    }


def summarize_scalar(values: list[float]) -> dict[str, Any]:
    if not values:
        return {
            "count": 0,
            "mean": 0.0,
            "p50": 0.0,
            "p95": 0.0,
            "min": 0.0,
            "max": 0.0,
            "stdev": 0.0,
        }
    mean = statistics.fmean(values)
    stdev = statistics.stdev(values) if len(values) > 1 else 0.0
    return {
        "count": len(values),
        "mean": round(mean, 3),
        "p50": round(statistics.median(values), 3),
        "p95": round(percentile(values, 0.95), 3),
        "min": round(min(values), 3),
        "max": round(max(values), 3),
        "stdev": round(stdev, 3),
    }


def port_open(host: str, port: int, timeout: float = 0.3) -> bool:
    import socket

    try:
        with socket.create_connection((host, port), timeout=timeout):
            return True
    except OSError:
        return False


def load_launch_spec_file(path: Path) -> GatewayLaunchSpec:
    payload = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(payload, dict):
        raise ValueError(f"gateway launch spec must be an object: {path}")
    executable = str(payload.get("executable", "")).strip()
    if not executable:
        raise ValueError(f"gateway launch spec missing executable: {path}")
    args = tuple(str(item) for item in payload.get("args", []))
    cwd_text = str(payload.get("cwd", "")).strip()
    env_payload = payload.get("env", {}) or {}
    path_entries_payload = payload.get("pathEntries", []) or []
    return GatewayLaunchSpec(
        executable=Path(executable).expanduser().resolve(),
        args=args,
        cwd=Path(cwd_text).expanduser().resolve() if cwd_text else None,
        env={str(key): str(value) for key, value in env_payload.items()},
        path_entries=tuple(Path(str(item)).expanduser().resolve() for item in path_entries_payload),
    )


def resolve_config_path_from_spec(spec: GatewayLaunchSpec) -> Path | None:
    args = list(spec.args)
    for index, arg in enumerate(args):
        if arg not in ("--config", "-c"):
            continue
        if index + 1 >= len(args):
            break
        raw_value = str(args[index + 1]).strip()
        if not raw_value:
            break
        candidate = Path(raw_value).expanduser()
        if candidate.is_absolute():
            return candidate.resolve()
        base_dir = spec.cwd or ROOT
        return (base_dir / candidate).resolve()
    return None


def read_hardware_mode(config_path: Path | None) -> str:
    if config_path is None or not config_path.exists():
        return "unknown"
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    setattr(parser, "optionxform", lambda optionstr: optionstr)
    try:
        with config_path.open("r", encoding="utf-8") as handle:
            parser.read_file(handle)
    except (OSError, configparser.Error):
        return "unknown"
    return str(parser.get("Hardware", "mode", fallback="unknown")).strip() or "unknown"


def materialize_mock_config(source_config: Path, report_dir: Path) -> Path:
    parser = configparser.ConfigParser(interpolation=None, strict=False)
    setattr(parser, "optionxform", lambda optionstr: optionstr)
    with source_config.open("r", encoding="utf-8") as handle:
        parser.read_file(handle)
    if not parser.has_section("Hardware"):
        parser.add_section("Hardware")
    parser.set("Hardware", "mode", "Mock")
    for section in parser.sections():
        if not section.startswith("Homing_"):
            continue
        # Perf dry-run uses a temporary mock config. Clamp homing parameters to
        # the validator-accepted mock envelope so nightly-performance can reach
        # preview.confirm -> dxf.job.start without mutating the real machine config.
        for option_name, safe_limit in (("ready_zero_speed_mm_s", 10.0), ("rapid_velocity", 10.0)):
            raw_value = str(parser.get(section, option_name, fallback="")).strip()
            if not raw_value:
                continue
            try:
                numeric_value = float(raw_value)
            except ValueError:
                continue
            if numeric_value > safe_limit:
                parser.set(section, option_name, f"{safe_limit:.1f}")
        home_input_bit = str(parser.get(section, "home_input_bit", fallback="")).strip()
        if not home_input_bit or home_input_bit == "-1":
            parser.set(section, "home_input_bit", "0")
    runtime_dir = report_dir.expanduser().resolve() / "_runtime"
    runtime_dir.mkdir(parents=True, exist_ok=True)
    target_path = runtime_dir / f"{source_config.stem}.mock{source_config.suffix or '.ini'}"
    with target_path.open("w", encoding="utf-8", newline="\n") as handle:
        parser.write(handle)
    return target_path.resolve()


def fallback_launch_spec(args: argparse.Namespace, *, force_mock_config: bool = False) -> LaunchSpecResolution:
    gateway_exe = Path(args.gateway_exe).expanduser().resolve()
    vendor_dir = Path(args.vendor_dir).expanduser().resolve()
    config_path = Path(args.config_path).expanduser().resolve()
    auto_provisioned_mock = False
    if force_mock_config and read_hardware_mode(config_path).lower() != "mock":
        config_path = materialize_mock_config(config_path, Path(args.report_dir))
        auto_provisioned_mock = True
    env = {
        "SILIGEN_MULTICARD_VENDOR_DIR": str(vendor_dir),
        "SILIGEN_TCP_SERVER_HOST": str(args.host),
        "SILIGEN_TCP_SERVER_PORT": str(args.port),
    }
    spec = GatewayLaunchSpec(
        executable=gateway_exe,
        args=("--config", str(config_path), "--port", str(args.port)),
        cwd=ROOT,
        env=env,
        path_entries=tuple(path for path in (gateway_exe.parent, vendor_dir) if path.exists()),
    )
    source = "workspace-fallback:auto-mock" if auto_provisioned_mock else "workspace-fallback"
    return LaunchSpecResolution(
        source=source,
        spec=spec,
        config_path=config_path,
        hardware_mode=read_hardware_mode(config_path),
        auto_provisioned_mock=auto_provisioned_mock,
    )


def resolve_launch_spec(args: argparse.Namespace) -> LaunchSpecResolution:
    if args.launch_spec:
        spec = load_launch_spec_file(Path(args.launch_spec).expanduser().resolve())
        config_path = resolve_config_path_from_spec(spec)
        return LaunchSpecResolution(
            source=f"explicit:{Path(args.launch_spec).expanduser().resolve()}",
            spec=spec,
            config_path=config_path,
            hardware_mode=read_hardware_mode(config_path),
        )

    if args.include_start_job and args.dry_run:
        return fallback_launch_spec(args, force_mock_config=True)

    spec = load_gateway_launch_spec()
    if spec is not None and spec.executable.exists():
        config_path = resolve_config_path_from_spec(spec)
        return LaunchSpecResolution(
            source="default-hmi-launch-spec",
            spec=spec,
            config_path=config_path,
            hardware_mode=read_hardware_mode(config_path),
        )
    return fallback_launch_spec(args)


@contextmanager
def managed_backend(
    host: str,
    port: int,
    launch_spec: LaunchSpecResolution,
    *,
    allow_reuse_running: bool,
):
    already_running = port_open(host, port)
    if already_running:
        if not allow_reuse_running:
            raise RuntimeError(f"{host}:{port} already has a gateway listening; pass --reuse-running to reuse it.")
        lease = BackendLease(
            mode="reused-existing",
            host=host,
            port=port,
            started_by_script=False,
            manager=None,
            launch_spec_source=launch_spec.source,
            launch_spec_executable=str(launch_spec.spec.executable),
        )
        yield lease
        return

    manager = BackendManager(host=host, port=port, launch_spec=launch_spec.spec)
    ok, message = manager.start()
    if not ok:
        raise RuntimeError(f"gateway start failed: {message}")
    ok, message = manager.wait_ready(timeout=10.0)
    if not ok:
        manager.stop()
        raise RuntimeError(f"gateway ready check failed: {message}")

    lease = BackendLease(
        mode="started-by-script",
        host=host,
        port=port,
        started_by_script=True,
        manager=manager,
        launch_spec_source=launch_spec.source,
        launch_spec_executable=str(launch_spec.spec.executable),
    )
    try:
        yield lease
    finally:
        manager.stop()


@contextmanager
def protocol_client(host: str, port: int):
    client = TcpClient(host=host, port=port)
    if not client.connect():
        raise RuntimeError(f"TCP connect failed: {client.last_connect_error or 'unknown'}")
    try:
        yield client, CommandProtocol(client)
    finally:
        client.disconnect()


def ensure_mock_execution_runtime_ready(protocol: CommandProtocol) -> None:
    status = protocol.get_status()
    if not status.connected:
        ok, message = protocol.connect_hardware(timeout=15.0)
        if not ok:
            raise RuntimeError(f"mock connect failed: {message or 'unknown'}")
        status = protocol.get_status()

    axes = [axis for axis in ("X", "Y") if axis in status.axes] or ["X", "Y"]
    needs_ready_zero = any(
        (axis not in status.axes)
        or (not status.axes[axis].enabled)
        or (not status.axes[axis].homed)
        for axis in axes
    )
    if needs_ready_zero:
        ok, message = protocol.home_auto(axes, force=True, wait_for_completion=True, timeout_ms=80000)
        if not ok:
            raise RuntimeError(f"mock ready-zero failed: {message or 'unknown'}")
        status = protocol.get_status()

    unresolved_axes: list[str] = []
    for axis in axes:
        axis_status = status.axes.get(axis)
        if axis_status is None:
            unresolved_axes.append(f"{axis}:missing")
            continue
        if not axis_status.enabled:
            unresolved_axes.append(f"{axis}:disabled")
        if not axis_status.homed:
            unresolved_axes.append(f"{axis}:not_homed")
    if unresolved_axes:
        raise RuntimeError(f"mock execution bootstrap incomplete: {', '.join(unresolved_axes)}")


def maybe_prepare_execution_runtime(
    protocol: CommandProtocol,
    args: argparse.Namespace,
    launch_spec: LaunchSpecResolution,
) -> None:
    if not args.include_start_job:
        return
    if str(launch_spec.hardware_mode).strip().lower() != "mock":
        return
    ensure_mock_execution_runtime_ready(protocol)


def resolve_samples(args: argparse.Namespace) -> dict[str, Path]:
    resolved = dict(DEFAULT_SAMPLES)
    for item in args.sample:
        if "=" not in item:
            raise ValueError(f"--sample must look like LABEL=PATH, got: {item}")
        label, path_text = item.split("=", 1)
        label = label.strip()
        if not label:
            raise ValueError(f"--sample label cannot be empty: {item}")
        resolved[label] = Path(path_text).expanduser().resolve()

    selected: dict[str, Path] = {}
    for label in args.sample_labels:
        if label not in resolved:
            raise ValueError(f"undefined sample label: {label}")
        sample_path = resolved[label].expanduser().resolve()
        if not sample_path.exists():
            raise FileNotFoundError(f"DXF sample not found: {sample_path}")
        selected[label] = sample_path
    return selected


def require_explicit_recipe_context(recipe_id: str, version_id: str) -> dict[str, str]:
    normalized_recipe_id = str(recipe_id or "").strip()
    normalized_version_id = str(version_id or "").strip()
    missing: list[str] = []
    if not normalized_recipe_id:
        missing.append("recipe_id")
    if not normalized_version_id:
        missing.append("version_id")
    if missing:
        raise ValueError("missing explicit recipe context: " + ",".join(missing))
    return {
        "recipe_id": normalized_recipe_id,
        "version_id": normalized_version_id,
        "recipe_context_source": "cli_explicit",
    }


def validate_published_recipe_context(
    protocol: CommandProtocol,
    recipe_id: str,
    version_id: str,
) -> tuple[dict[str, str] | None, str]:
    context = require_explicit_recipe_context(recipe_id, version_id)
    versions_ok, versions_payload, versions_error = protocol.recipe_versions(context["recipe_id"])
    if not versions_ok:
        return None, f"recipe.versions failed: {versions_error or 'unknown'}"
    if not isinstance(versions_payload, list):
        return None, "recipe.versions response missing versions list"

    matched_version = next(
        (
            item
            for item in versions_payload
            if isinstance(item, dict) and str(item.get("id", "")).strip() == context["version_id"]
        ),
        None,
    )
    if not isinstance(matched_version, dict):
        return None, (
            "recipe.versions missing explicit version: "
            f"recipe_id={context['recipe_id']} version_id={context['version_id']}"
        )

    version_status = str(matched_version.get("status", "")).strip().lower()
    if version_status != "published":
        return None, (
            "recipe version is not published: "
            f"recipe_id={context['recipe_id']} version_id={context['version_id']} status={version_status or 'unknown'}"
        )
    return context, ""


def create_artifact(protocol: CommandProtocol, sample_path: Path, timeout: float) -> tuple[str, float]:
    started = time.perf_counter()
    ok, payload, error = protocol.dxf_create_artifact(str(sample_path), timeout=timeout)
    elapsed_ms = (time.perf_counter() - started) * 1000.0
    if not ok:
        raise RuntimeError(f"artifact.create failed: {error}")
    artifact_id = str(payload.get("artifact_id", "")).strip()
    if not artifact_id:
        raise RuntimeError("artifact.create response missing artifact_id")
    return artifact_id, elapsed_ms


def extract_prepare_profile(payload: dict[str, Any]) -> dict[str, Any]:
    raw_profile = payload.get("performance_profile")
    if not isinstance(raw_profile, dict):
        raw_profile = {}
    return {
        "authority_cache_hit": bool(raw_profile.get("authority_cache_hit", False))
        if "authority_cache_hit" in raw_profile
        else None,
        "authority_joined_inflight": bool(raw_profile.get("authority_joined_inflight", False))
        if "authority_joined_inflight" in raw_profile
        else None,
        "authority_wait_ms": float(raw_profile.get("authority_wait_ms", 0.0))
        if "authority_wait_ms" in raw_profile
        else None,
        "pb_prepare_ms": float(raw_profile.get("pb_prepare_ms", 0.0))
        if "pb_prepare_ms" in raw_profile
        else None,
        "path_load_ms": float(raw_profile.get("path_load_ms", 0.0))
        if "path_load_ms" in raw_profile
        else None,
        "process_path_ms": float(raw_profile.get("process_path_ms", 0.0))
        if "process_path_ms" in raw_profile
        else None,
        "authority_build_ms": float(raw_profile.get("authority_build_ms", 0.0))
        if "authority_build_ms" in raw_profile
        else None,
        "authority_total_ms": float(raw_profile.get("authority_total_ms", 0.0))
        if "authority_total_ms" in raw_profile
        else None,
        "prepare_total_ms": float(raw_profile.get("prepare_total_ms", 0.0))
        if "prepare_total_ms" in raw_profile
        else None,
    }


def extract_execution_profile(payload: dict[str, Any]) -> dict[str, Any]:
    raw_profile = payload.get("performance_profile")
    if not isinstance(raw_profile, dict):
        raw_profile = {}
    return {
        "execution_cache_hit": bool(raw_profile.get("execution_cache_hit", False))
        if "execution_cache_hit" in raw_profile
        else None,
        "execution_joined_inflight": bool(raw_profile.get("execution_joined_inflight", False))
        if "execution_joined_inflight" in raw_profile
        else None,
        "execution_wait_ms": float(raw_profile.get("execution_wait_ms", 0.0))
        if "execution_wait_ms" in raw_profile
        else None,
        "motion_plan_ms": float(raw_profile.get("motion_plan_ms", 0.0))
        if "motion_plan_ms" in raw_profile
        else None,
        "assembly_ms": float(raw_profile.get("assembly_ms", 0.0))
        if "assembly_ms" in raw_profile
        else None,
        "export_ms": float(raw_profile.get("export_ms", 0.0))
        if "export_ms" in raw_profile
        else None,
        "execution_total_ms": float(raw_profile.get("execution_total_ms", 0.0))
        if "execution_total_ms" in raw_profile
        else None,
    }


def gateway_process_id(lease: BackendLease) -> int | None:
    manager = lease.manager
    if manager is None:
        return None
    process = getattr(manager, "_process", None)
    pid = getattr(process, "pid", None)
    return int(pid) if isinstance(pid, int) and pid > 0 else None


def collect_process_metrics(pid: int | None) -> dict[str, Any]:
    if pid is None:
        return {}
    command = (
        "$p = Get-Process -Id {pid} -ErrorAction Stop; "
        "@{{WorkingSet64=$p.WorkingSet64; PrivateMemorySize64=$p.PrivateMemorySize64; "
        "HandleCount=$p.HandleCount; ThreadCount=$p.Threads.Count}} | ConvertTo-Json -Compress"
    ).format(pid=pid)
    try:
        completed = subprocess.run(
            ["powershell", "-NoProfile", "-Command", command],
            cwd=str(ROOT),
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=5.0,
        )
    except (subprocess.TimeoutExpired, OSError):
        # Resource sampling is observational only. A single slow or failed
        # Get-Process call must not flip the whole long-run/control-cycle case.
        return {}
    if completed.returncode != 0:
        return {}
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError:
        return {}
    if not isinstance(payload, dict):
        return {}
    return {
        "working_set_mb": round(float(payload.get("WorkingSet64", 0.0)) / (1024.0 * 1024.0), 3),
        "private_memory_mb": round(float(payload.get("PrivateMemorySize64", 0.0)) / (1024.0 * 1024.0), 3),
        "handle_count": int(payload.get("HandleCount", 0)),
        "thread_count": int(payload.get("ThreadCount", 0)),
        "collected_at": utc_now(),
    }


def wait_for_job_state(
    protocol: CommandProtocol,
    job_id: str,
    expected_states: set[str],
    *,
    timeout_s: float = 8.0,
    poll_interval_s: float = 0.05,
) -> tuple[str, str, float]:
    started = time.perf_counter()
    deadline = started + timeout_s
    last_state = ""
    last_error = ""
    normalized_expected = {state.strip().lower() for state in expected_states}
    while time.perf_counter() < deadline:
        payload = protocol.dxf_get_job_status(job_id)
        last_state = str(payload.get("state", "")).strip().lower()
        last_error = str(payload.get("error_message", "")).strip()
        if last_state in normalized_expected:
            return last_state, last_error, (time.perf_counter() - started) * 1000.0
        time.sleep(poll_interval_s)
    return last_state, last_error, (time.perf_counter() - started) * 1000.0


def wait_for_job_stop_completion(
    protocol: CommandProtocol,
    job_id: str,
    *,
    timeout_s: float = 5.0,
    poll_interval_s: float = 0.05,
) -> tuple[str, str]:
    deadline = time.perf_counter() + timeout_s
    last_state = ""
    last_error = ""
    while time.perf_counter() < deadline:
        polled = protocol.dxf_get_job_status(job_id)
        last_state = str(polled.get("state", "")).strip().lower()
        last_error = str(polled.get("error_message", "")).strip()
        if last_state in TERMINAL_JOB_STATES:
            return last_state, last_error
        if last_state == "unknown" and (not last_error or "not running" in last_error.lower()):
            return last_state, last_error
        time.sleep(poll_interval_s)
    return last_state, last_error


def wait_for_idle_status(
    protocol: CommandProtocol,
    *,
    timeout_s: float = 5.0,
    poll_interval_s: float = 0.05,
) -> tuple[bool, str]:
    deadline = time.perf_counter() + timeout_s
    last_desc = ""
    while time.perf_counter() < deadline:
        status = protocol.get_status()
        job_execution = status.job_execution
        job_id = str(job_execution.job_id or "").strip()
        job_state = str(job_execution.state or "").strip().lower()
        last_desc = f"job_execution.job_id={job_id or '-'} job_execution.state={job_state or '-'}"
        if job_state == "idle" or job_state in TERMINAL_JOB_STATES:
            return True, last_desc
        time.sleep(poll_interval_s)
    return False, last_desc


def summarize_resource_series(samples: list[dict[str, Any]]) -> dict[str, Any]:
    if not samples:
        empty_series = {"count": 0, "baseline": 0.0, "min": 0.0, "max": 0.0, "delta_max": 0.0}
        return {
            "sample_count": 0,
            "working_set_mb": dict(empty_series),
            "private_memory_mb": dict(empty_series),
            "handle_count": dict(empty_series),
            "thread_count": dict(empty_series),
        }

    def _metric_summary(metric_name: str) -> dict[str, Any]:
        values = [float(sample.get(metric_name, 0.0)) for sample in samples if metric_name in sample]
        if not values:
            return {"count": 0, "baseline": 0.0, "min": 0.0, "max": 0.0, "delta_max": 0.0}
        baseline = values[0]
        return {
            "count": len(values),
            "baseline": round(baseline, 3),
            "min": round(min(values), 3),
            "max": round(max(values), 3),
            "delta_max": round(max(values) - baseline, 3),
        }

    return {
        "sample_count": len(samples),
        "working_set_mb": _metric_summary("working_set_mb"),
        "private_memory_mb": _metric_summary("private_memory_mb"),
        "handle_count": _metric_summary("handle_count"),
        "thread_count": _metric_summary("thread_count"),
    }


def prepare_and_snapshot_once(
    protocol: CommandProtocol,
    artifact_id: str,
    args: argparse.Namespace,
    *,
    artifact_ms: float | None,
) -> PreviewCycleRecord:
    recipe_context, recipe_context_error = validate_published_recipe_context(
        protocol,
        args.recipe_id,
        args.version_id,
    )
    if recipe_context is None:
        return PreviewCycleRecord(
            success=False,
            artifact_id=artifact_id,
            artifact_ms=artifact_ms,
            error=recipe_context_error or "recipe context validation failed",
        )

    plan_started = time.perf_counter()
    plan_ok, plan_payload, plan_error = protocol.dxf_prepare_plan(
        artifact_id=artifact_id,
        recipe_id=recipe_context["recipe_id"],
        version_id=recipe_context["version_id"],
        speed_mm_s=args.dispensing_speed_mm_s,
        dry_run=args.dry_run,
        dry_run_speed_mm_s=args.dry_run_speed_mm_s,
        timeout=args.prepare_timeout,
    )
    plan_elapsed_ms = (time.perf_counter() - plan_started) * 1000.0
    if not plan_ok:
        return PreviewCycleRecord(
            success=False,
            artifact_id=artifact_id,
            artifact_ms=artifact_ms,
            plan_prepare_rpc_ms=plan_elapsed_ms,
            worker_total_ms=plan_elapsed_ms,
            error=f"plan.prepare failed: {plan_error}",
        )

    plan_id = str(plan_payload.get("plan_id", "")).strip()
    plan_fingerprint = str(plan_payload.get("plan_fingerprint", "")).strip()
    if not plan_id:
        return PreviewCycleRecord(
            success=False,
            artifact_id=artifact_id,
            artifact_ms=artifact_ms,
            plan_prepare_rpc_ms=plan_elapsed_ms,
            worker_total_ms=plan_elapsed_ms,
            error="plan.prepare response missing plan_id",
        )

    snapshot_started = time.perf_counter()
    snapshot_ok, snapshot_payload, snapshot_error = protocol.dxf_preview_snapshot(
        plan_id=plan_id,
        max_polyline_points=args.max_polyline_points,
        timeout=args.snapshot_timeout,
    )
    snapshot_elapsed_ms = (time.perf_counter() - snapshot_started) * 1000.0
    if not snapshot_ok:
        return PreviewCycleRecord(
            success=False,
            artifact_id=artifact_id,
            plan_id=plan_id,
            plan_fingerprint=plan_fingerprint,
            artifact_ms=artifact_ms,
            plan_prepare_rpc_ms=plan_elapsed_ms,
            snapshot_rpc_ms=snapshot_elapsed_ms,
            worker_total_ms=plan_elapsed_ms + snapshot_elapsed_ms,
            error=f"preview.snapshot failed: {snapshot_error}",
        )

    snapshot_hash = str(snapshot_payload.get("snapshot_hash", "")).strip()
    profile = extract_prepare_profile(plan_payload)
    motion_preview_payload = snapshot_payload.get("motion_preview", {})
    if not isinstance(motion_preview_payload, dict):
        motion_preview_payload = {}
    return PreviewCycleRecord(
        success=bool(snapshot_hash),
        artifact_id=artifact_id,
        plan_id=plan_id,
        plan_fingerprint=plan_fingerprint,
        snapshot_hash=snapshot_hash,
        glue_point_count=int(snapshot_payload.get("glue_point_count", 0) or 0),
        motion_preview_point_count=int(motion_preview_payload.get("point_count", 0) or 0),
        artifact_ms=artifact_ms,
        plan_prepare_rpc_ms=plan_elapsed_ms,
        snapshot_rpc_ms=snapshot_elapsed_ms,
        worker_total_ms=plan_elapsed_ms + snapshot_elapsed_ms,
        authority_cache_hit=profile["authority_cache_hit"],
        authority_joined_inflight=profile["authority_joined_inflight"],
        authority_wait_ms=profile["authority_wait_ms"],
        pb_prepare_ms=profile["pb_prepare_ms"],
        path_load_ms=profile["path_load_ms"],
        process_path_ms=profile["process_path_ms"],
        authority_build_ms=profile["authority_build_ms"],
        authority_total_ms=profile["authority_total_ms"],
        prepare_total_ms=profile["prepare_total_ms"],
        error="" if snapshot_hash else "preview.snapshot response missing snapshot_hash",
    )


def run_start_job_cycle(
    protocol: CommandProtocol,
    preview_record: PreviewCycleRecord,
) -> StartJobCycleRecord:
    if not preview_record.success:
        return StartJobCycleRecord(
            success=False,
            artifact_id=preview_record.artifact_id,
            plan_id=preview_record.plan_id,
            plan_fingerprint=preview_record.plan_fingerprint,
            snapshot_hash=preview_record.snapshot_hash,
            error=f"skipped start-job because preview failed: {preview_record.error}",
        )

    confirm_started = time.perf_counter()
    confirm_ok, _, confirm_error = protocol.dxf_preview_confirm(
        preview_record.plan_id,
        preview_record.snapshot_hash,
    )
    confirm_elapsed_ms = (time.perf_counter() - confirm_started) * 1000.0
    if not confirm_ok:
        return StartJobCycleRecord(
            success=False,
            artifact_id=preview_record.artifact_id,
            plan_id=preview_record.plan_id,
            plan_fingerprint=preview_record.plan_fingerprint,
            snapshot_hash=preview_record.snapshot_hash,
            confirm_rpc_ms=confirm_elapsed_ms,
            error=f"preview.confirm failed: {confirm_error}",
        )

    start_started = time.perf_counter()
    start_ok, start_payload, start_error = protocol.dxf_start_job(
        preview_record.plan_id,
        target_count=1,
        plan_fingerprint=preview_record.plan_fingerprint,
    )
    start_elapsed_ms = (time.perf_counter() - start_started) * 1000.0
    if not start_ok:
        return StartJobCycleRecord(
            success=False,
            artifact_id=preview_record.artifact_id,
            plan_id=preview_record.plan_id,
            plan_fingerprint=preview_record.plan_fingerprint,
            snapshot_hash=preview_record.snapshot_hash,
            confirm_rpc_ms=confirm_elapsed_ms,
            job_start_rpc_ms=start_elapsed_ms,
            error=f"dxf.job.start failed: {start_error}",
        )

    job_id = str(start_payload.get("job_id", "")).strip()
    if not job_id:
        return StartJobCycleRecord(
            success=False,
            artifact_id=preview_record.artifact_id,
            plan_id=preview_record.plan_id,
            plan_fingerprint=preview_record.plan_fingerprint,
            snapshot_hash=preview_record.snapshot_hash,
            confirm_rpc_ms=confirm_elapsed_ms,
            job_start_rpc_ms=start_elapsed_ms,
            error="dxf.job.start response missing job_id",
        )

    status_started = time.perf_counter()
    status_payload = protocol.dxf_get_job_status(job_id)
    status_elapsed_ms = (time.perf_counter() - status_started) * 1000.0
    status_error = str(status_payload.get("error_message", "")).strip()
    if status_payload.get("state") == "unknown" and status_error:
        stop_started = time.perf_counter()
        protocol.dxf_job_stop(job_id)
        stop_elapsed_ms = (time.perf_counter() - stop_started) * 1000.0
        return StartJobCycleRecord(
            success=False,
            artifact_id=preview_record.artifact_id,
            plan_id=preview_record.plan_id,
            plan_fingerprint=preview_record.plan_fingerprint,
            snapshot_hash=preview_record.snapshot_hash,
            job_id=job_id,
            confirm_rpc_ms=confirm_elapsed_ms,
            job_start_rpc_ms=start_elapsed_ms,
            job_status_rpc_ms=status_elapsed_ms,
            job_stop_rpc_ms=stop_elapsed_ms,
            job_lifecycle_rpc_ms=confirm_elapsed_ms + start_elapsed_ms + status_elapsed_ms + stop_elapsed_ms,
            error=f"dxf.job.status failed: {status_error}",
        )

    stop_started = time.perf_counter()
    stop_ok = protocol.dxf_job_stop(job_id)
    stop_state = ""
    stop_error = ""
    if stop_ok:
        stop_state, stop_error = wait_for_job_stop_completion(protocol, job_id)
    stop_elapsed_ms = (time.perf_counter() - stop_started) * 1000.0
    if not stop_ok:
        return StartJobCycleRecord(
            success=False,
            artifact_id=preview_record.artifact_id,
            plan_id=preview_record.plan_id,
            plan_fingerprint=preview_record.plan_fingerprint,
            snapshot_hash=preview_record.snapshot_hash,
            job_id=job_id,
            confirm_rpc_ms=confirm_elapsed_ms,
            job_start_rpc_ms=start_elapsed_ms,
            job_status_rpc_ms=status_elapsed_ms,
            job_stop_rpc_ms=stop_elapsed_ms,
            job_lifecycle_rpc_ms=confirm_elapsed_ms + start_elapsed_ms + status_elapsed_ms + stop_elapsed_ms,
            error="dxf.job.stop failed",
        )
    if stop_state not in TERMINAL_JOB_STATES and stop_state != "unknown":
        return StartJobCycleRecord(
            success=False,
            artifact_id=preview_record.artifact_id,
            plan_id=preview_record.plan_id,
            plan_fingerprint=preview_record.plan_fingerprint,
            snapshot_hash=preview_record.snapshot_hash,
            job_id=job_id,
            confirm_rpc_ms=confirm_elapsed_ms,
            job_start_rpc_ms=start_elapsed_ms,
            job_status_rpc_ms=status_elapsed_ms,
            job_stop_rpc_ms=stop_elapsed_ms,
            job_lifecycle_rpc_ms=confirm_elapsed_ms + start_elapsed_ms + status_elapsed_ms + stop_elapsed_ms,
            error=f"dxf.job.stop did not reach terminal state: {stop_state or stop_error or 'timeout'}",
        )

    profile = extract_execution_profile(start_payload)
    return StartJobCycleRecord(
        success=True,
        artifact_id=preview_record.artifact_id,
        plan_id=preview_record.plan_id,
        plan_fingerprint=preview_record.plan_fingerprint,
        snapshot_hash=preview_record.snapshot_hash,
        job_id=job_id,
        confirm_rpc_ms=confirm_elapsed_ms,
        job_start_rpc_ms=start_elapsed_ms,
        job_status_rpc_ms=status_elapsed_ms,
        job_stop_rpc_ms=stop_elapsed_ms,
        job_lifecycle_rpc_ms=confirm_elapsed_ms + start_elapsed_ms + status_elapsed_ms + stop_elapsed_ms,
        execution_cache_hit=profile["execution_cache_hit"],
        execution_joined_inflight=profile["execution_joined_inflight"],
        execution_wait_ms=profile["execution_wait_ms"],
        motion_plan_ms=profile["motion_plan_ms"],
        assembly_ms=profile["assembly_ms"],
        export_ms=profile["export_ms"],
        execution_total_ms=profile["execution_total_ms"],
        job_status_state=str(status_payload.get("state", "")),
    )


def summarize_preview_records(records: list[PreviewCycleRecord]) -> dict[str, Any]:
    successful = [record for record in records if record.success]
    summary = {
        "count": len(records),
        "success_count": len(successful),
        "failure_count": len(records) - len(successful),
        "artifact_ms": summarize_numbers(
            [float(record.artifact_ms) for record in records if record.artifact_ms is not None]
        ),
        "plan_prepare_rpc_ms": summarize_numbers(
            [float(record.plan_prepare_rpc_ms) for record in successful if record.plan_prepare_rpc_ms is not None]
        ),
        "snapshot_rpc_ms": summarize_numbers(
            [float(record.snapshot_rpc_ms) for record in successful if record.snapshot_rpc_ms is not None]
        ),
        "worker_total_ms": summarize_numbers(
            [float(record.worker_total_ms) for record in successful if record.worker_total_ms is not None]
        ),
        "glue_point_count": summarize_scalar([float(record.glue_point_count) for record in successful]),
        "motion_preview_point_count": summarize_scalar(
            [float(record.motion_preview_point_count) for record in successful]
        ),
        "authority_cache_hit": summarize_bool(
            [bool(record.authority_cache_hit) for record in successful if record.authority_cache_hit is not None]
        ),
        "authority_joined_inflight": summarize_bool(
            [
                bool(record.authority_joined_inflight)
                for record in successful
                if record.authority_joined_inflight is not None
            ]
        ),
    }
    for field in (
        "authority_wait_ms",
        "pb_prepare_ms",
        "path_load_ms",
        "process_path_ms",
        "authority_build_ms",
        "authority_total_ms",
        "prepare_total_ms",
    ):
        summary[field] = summarize_numbers(
            [float(getattr(record, field)) for record in successful if getattr(record, field) is not None]
        )
    return summary


def summarize_execution_records(records: list[StartJobCycleRecord]) -> dict[str, Any]:
    successful = [record for record in records if record.success]
    summary = {
        "count": len(records),
        "success_count": len(successful),
        "failure_count": len(records) - len(successful),
        "confirm_rpc_ms": summarize_numbers(
            [float(record.confirm_rpc_ms) for record in successful if record.confirm_rpc_ms is not None]
        ),
        "job_start_rpc_ms": summarize_numbers(
            [float(record.job_start_rpc_ms) for record in successful if record.job_start_rpc_ms is not None]
        ),
        "job_status_rpc_ms": summarize_numbers(
            [float(record.job_status_rpc_ms) for record in successful if record.job_status_rpc_ms is not None]
        ),
        "job_stop_rpc_ms": summarize_numbers(
            [float(record.job_stop_rpc_ms) for record in successful if record.job_stop_rpc_ms is not None]
        ),
        "job_lifecycle_rpc_ms": summarize_numbers(
            [float(record.job_lifecycle_rpc_ms) for record in successful if record.job_lifecycle_rpc_ms is not None]
        ),
        "execution_cache_hit": summarize_bool(
            [bool(record.execution_cache_hit) for record in successful if record.execution_cache_hit is not None]
        ),
        "execution_joined_inflight": summarize_bool(
            [
                bool(record.execution_joined_inflight)
                for record in successful
                if record.execution_joined_inflight is not None
            ]
        ),
    }
    for field in (
        "execution_wait_ms",
        "motion_plan_ms",
        "assembly_ms",
        "export_ms",
        "execution_total_ms",
    ):
        summary[field] = summarize_numbers(
            [float(getattr(record, field)) for record in successful if getattr(record, field) is not None]
        )
    return summary


def execution_payload_for(records: list[StartJobCycleRecord], include_start_job: bool) -> dict[str, Any]:
    if not include_start_job:
        return {"status": "skipped", "reason": "include_start_job=false", "iterations": [], "summary": {}}
    status = "ok"
    error = ""
    if records and not any(record.success for record in records):
        status = "error"
        error = next((record.error for record in records if record.error), "no successful start-job cycles")
    payload = {
        "status": status,
        "iterations": [asdict(record) for record in records],
        "summary": summarize_execution_records(records),
    }
    if error:
        payload["error"] = error
    return payload


def _start_job_after_confirm(
    protocol: CommandProtocol,
    preview_record: PreviewCycleRecord,
    *,
    target_count: int,
) -> tuple[bool, str, dict[str, Any], dict[str, Any], str]:
    confirm_ok, _, confirm_error = protocol.dxf_preview_confirm(
        preview_record.plan_id,
        preview_record.snapshot_hash,
    )
    if not confirm_ok:
        return False, "", {}, {}, f"preview.confirm failed: {confirm_error}"
    start_ok, start_payload, start_error = protocol.dxf_start_job(
        preview_record.plan_id,
        target_count=max(1, int(target_count)),
        plan_fingerprint=preview_record.plan_fingerprint,
    )
    if not start_ok:
        return False, "", {}, {}, f"dxf.job.start failed: {start_error}"
    job_id = str(start_payload.get("job_id", "")).strip()
    if not job_id:
        return False, "", {}, {}, "dxf.job.start response missing job_id"
    status_payload = protocol.dxf_get_job_status(job_id)
    return True, job_id, start_payload, status_payload, ""


def _stop_job_to_idle(protocol: CommandProtocol, job_id: str) -> StopTransitionRecord:
    stop_started = time.perf_counter()
    stop_rpc_started = time.perf_counter()
    stop_ok = protocol.dxf_job_stop(job_id)
    stop_rpc_ms = (time.perf_counter() - stop_rpc_started) * 1000.0
    stop_state = ""
    stop_error = ""
    wait_for_terminal_ms: float | None = None
    wait_for_idle_ms: float | None = None
    idle_desc = ""
    if stop_ok:
        wait_for_terminal_started = time.perf_counter()
        stop_state, stop_error = wait_for_job_stop_completion(protocol, job_id)
        wait_for_terminal_ms = (time.perf_counter() - wait_for_terminal_started) * 1000.0
        wait_for_idle_started = time.perf_counter()
        idle_ok, idle_desc = wait_for_idle_status(protocol)
        wait_for_idle_ms = (time.perf_counter() - wait_for_idle_started) * 1000.0
    else:
        idle_ok = False
    stop_elapsed_ms = (time.perf_counter() - stop_started) * 1000.0
    if not stop_ok:
        return StopTransitionRecord(
            success=False,
            total_ms=stop_elapsed_ms,
            rpc_ms=stop_rpc_ms,
            terminal_state=stop_state,
            terminal_error=stop_error,
            idle_desc="dxf.job.stop failed",
        )
    if stop_state not in TERMINAL_JOB_STATES and stop_state != "unknown":
        return StopTransitionRecord(
            success=False,
            total_ms=stop_elapsed_ms,
            rpc_ms=stop_rpc_ms,
            wait_for_terminal_ms=wait_for_terminal_ms,
            wait_for_idle_ms=wait_for_idle_ms,
            terminal_state=stop_state,
            terminal_error=stop_error,
            idle_desc=f"dxf.job.stop did not reach terminal state: {stop_state or stop_error or 'timeout'}",
        )
    if not idle_ok:
        return StopTransitionRecord(
            success=False,
            total_ms=stop_elapsed_ms,
            rpc_ms=stop_rpc_ms,
            wait_for_terminal_ms=wait_for_terminal_ms,
            wait_for_idle_ms=wait_for_idle_ms,
            terminal_state=stop_state,
            terminal_error=stop_error,
            idle_desc=f"dxf.job.stop did not return to idle: {idle_desc}",
        )
    return StopTransitionRecord(
        success=True,
        total_ms=stop_elapsed_ms,
        rpc_ms=stop_rpc_ms,
        wait_for_terminal_ms=wait_for_terminal_ms,
        wait_for_idle_ms=wait_for_idle_ms,
        terminal_state=stop_state,
        terminal_error=stop_error,
        idle_desc=idle_desc,
    )


def run_control_cycle_round(
    protocol: CommandProtocol,
    artifact_id: str,
    args: argparse.Namespace,
    *,
    round_index: int,
    pause_resume_enabled: bool,
    stop_reset_enabled: bool,
) -> ControlCycleRecord:
    preview_record = prepare_and_snapshot_once(protocol, artifact_id, args, artifact_ms=None)
    if not preview_record.success:
        return ControlCycleRecord(
            round_index=round_index,
            success=False,
            pause_resume_applied=pause_resume_enabled,
            stop_reset_applied=stop_reset_enabled,
            error=f"preview failed: {preview_record.error}",
        )

    start_ok, job_id, start_payload, _, start_error = _start_job_after_confirm(
        protocol,
        preview_record,
        target_count=max(2, args.pause_resume_cycles + 1),
    )
    if not start_ok:
        return ControlCycleRecord(
            round_index=round_index,
            success=False,
            pause_resume_applied=pause_resume_enabled,
            stop_reset_applied=stop_reset_enabled,
            plan_id=preview_record.plan_id,
            error=start_error,
        )

    running_state, running_error, _ = wait_for_job_state(protocol, job_id, {"running", "paused"})
    if running_state not in {"running", "paused"}:
        return ControlCycleRecord(
            round_index=round_index,
            success=False,
            pause_resume_applied=pause_resume_enabled,
            stop_reset_applied=stop_reset_enabled,
            plan_id=preview_record.plan_id,
            job_id=job_id,
            timeout_detected=True,
            error=f"dxf.job.start did not reach running state: {running_state or running_error or 'timeout'}",
        )

    pause_to_running_ms: float | None = None
    if pause_resume_enabled:
        if not protocol.dxf_job_pause(job_id):
            return ControlCycleRecord(
                round_index=round_index,
                success=False,
                pause_resume_applied=True,
                stop_reset_applied=stop_reset_enabled,
                plan_id=preview_record.plan_id,
                job_id=job_id,
                error="dxf.job.pause failed",
            )
        paused_state, paused_error, _ = wait_for_job_state(protocol, job_id, {"paused"})
        if paused_state != "paused":
            return ControlCycleRecord(
                round_index=round_index,
                success=False,
                pause_resume_applied=True,
                stop_reset_applied=stop_reset_enabled,
                plan_id=preview_record.plan_id,
                job_id=job_id,
                timeout_detected=True,
                error=f"dxf.job.pause did not reach paused state: {paused_state or paused_error or 'timeout'}",
            )
        resume_started = time.perf_counter()
        if not protocol.dxf_job_resume(job_id):
            return ControlCycleRecord(
                round_index=round_index,
                success=False,
                pause_resume_applied=True,
                stop_reset_applied=stop_reset_enabled,
                plan_id=preview_record.plan_id,
                job_id=job_id,
                error="dxf.job.resume failed",
            )
        resumed_state, resumed_error, _ = wait_for_job_state(protocol, job_id, {"running"})
        pause_to_running_ms = (time.perf_counter() - resume_started) * 1000.0
        if resumed_state != "running":
            return ControlCycleRecord(
                round_index=round_index,
                success=False,
                pause_resume_applied=True,
                stop_reset_applied=stop_reset_enabled,
                plan_id=preview_record.plan_id,
                job_id=job_id,
                pause_to_running_ms=pause_to_running_ms,
                timeout_detected=True,
                error=f"dxf.job.resume did not return to running state: {resumed_state or resumed_error or 'timeout'}",
            )

    stop_record = _stop_job_to_idle(protocol, job_id)
    if not stop_record.success:
        return ControlCycleRecord(
            round_index=round_index,
            success=False,
            pause_resume_applied=pause_resume_enabled,
            stop_reset_applied=stop_reset_enabled,
            plan_id=preview_record.plan_id,
            job_id=job_id,
            pause_to_running_ms=pause_to_running_ms,
            stop_to_idle_ms=stop_record.total_ms,
            stop_rpc_ms=stop_record.rpc_ms,
            stop_wait_for_terminal_ms=stop_record.wait_for_terminal_ms,
            stop_wait_for_idle_ms=stop_record.wait_for_idle_ms,
            stop_terminal_state=stop_record.terminal_state,
            stop_terminal_error=stop_record.terminal_error,
            stop_idle_desc=stop_record.idle_desc,
            timeout_detected="timeout" in stop_record.idle_desc.lower(),
            error=stop_record.idle_desc,
        )

    rerun_total_ms: float | None = None
    execution_profile = extract_execution_profile(start_payload)
    execution_total_ms = execution_profile.get("execution_total_ms")
    if stop_reset_enabled:
        rerun_started = time.perf_counter()
        rerun_preview, rerun_execution = collect_preview_and_execution_cycle(
            protocol,
            artifact_id,
            args,
            artifact_ms=None,
        )
        rerun_total_ms = (time.perf_counter() - rerun_started) * 1000.0
        if not rerun_preview.success:
            return ControlCycleRecord(
                round_index=round_index,
                success=False,
                pause_resume_applied=pause_resume_enabled,
                stop_reset_applied=True,
                plan_id=preview_record.plan_id,
                job_id=job_id,
                pause_to_running_ms=pause_to_running_ms,
                stop_to_idle_ms=stop_record.total_ms,
                stop_rpc_ms=stop_record.rpc_ms,
                stop_wait_for_terminal_ms=stop_record.wait_for_terminal_ms,
                stop_wait_for_idle_ms=stop_record.wait_for_idle_ms,
                stop_terminal_state=stop_record.terminal_state,
                stop_terminal_error=stop_record.terminal_error,
                stop_idle_desc=stop_record.idle_desc,
                rerun_total_ms=rerun_total_ms,
                execution_total_ms=float(execution_total_ms) if isinstance(execution_total_ms, (int, float)) else None,
                error=f"rerun preview failed: {rerun_preview.error}",
            )
        if rerun_execution is None or not rerun_execution.success:
            rerun_error = rerun_execution.error if rerun_execution is not None else "rerun execution missing"
            return ControlCycleRecord(
                round_index=round_index,
                success=False,
                pause_resume_applied=pause_resume_enabled,
                stop_reset_applied=True,
                plan_id=preview_record.plan_id,
                job_id=job_id,
                pause_to_running_ms=pause_to_running_ms,
                stop_to_idle_ms=stop_record.total_ms,
                stop_rpc_ms=stop_record.rpc_ms,
                stop_wait_for_terminal_ms=stop_record.wait_for_terminal_ms,
                stop_wait_for_idle_ms=stop_record.wait_for_idle_ms,
                stop_terminal_state=stop_record.terminal_state,
                stop_terminal_error=stop_record.terminal_error,
                stop_idle_desc=stop_record.idle_desc,
                rerun_total_ms=rerun_total_ms,
                execution_total_ms=float(execution_total_ms) if isinstance(execution_total_ms, (int, float)) else None,
                timeout_detected="timeout" in rerun_error.lower(),
                error=f"rerun execution failed: {rerun_error}",
            )

    return ControlCycleRecord(
        round_index=round_index,
        success=True,
        pause_resume_applied=pause_resume_enabled,
        stop_reset_applied=stop_reset_enabled,
        plan_id=preview_record.plan_id,
        job_id=job_id,
        pause_to_running_ms=pause_to_running_ms,
        stop_to_idle_ms=stop_record.total_ms,
        stop_rpc_ms=stop_record.rpc_ms,
        stop_wait_for_terminal_ms=stop_record.wait_for_terminal_ms,
        stop_wait_for_idle_ms=stop_record.wait_for_idle_ms,
        stop_terminal_state=stop_record.terminal_state,
        stop_terminal_error=stop_record.terminal_error,
        stop_idle_desc=stop_record.idle_desc,
        rerun_total_ms=rerun_total_ms,
        execution_total_ms=float(execution_total_ms) if isinstance(execution_total_ms, (int, float)) else None,
    )


def summarize_control_cycle_records(records: list[ControlCycleRecord], resource_samples: list[dict[str, Any]]) -> dict[str, Any]:
    successful = [record for record in records if record.success]
    return {
        "count": len(records),
        "success_count": len(successful),
        "failure_count": len(records) - len(successful),
        "pause_to_running_ms": summarize_numbers(
            [float(record.pause_to_running_ms) for record in successful if record.pause_to_running_ms is not None]
        ),
        "stop_to_idle_ms": summarize_numbers(
            [float(record.stop_to_idle_ms) for record in successful if record.stop_to_idle_ms is not None]
        ),
        "stop_rpc_ms": summarize_numbers(
            [float(record.stop_rpc_ms) for record in successful if record.stop_rpc_ms is not None]
        ),
        "stop_wait_for_terminal_ms": summarize_numbers(
            [float(record.stop_wait_for_terminal_ms) for record in successful if record.stop_wait_for_terminal_ms is not None]
        ),
        "stop_wait_for_idle_ms": summarize_numbers(
            [float(record.stop_wait_for_idle_ms) for record in successful if record.stop_wait_for_idle_ms is not None]
        ),
        "rerun_total_ms": summarize_numbers(
            [float(record.rerun_total_ms) for record in successful if record.rerun_total_ms is not None]
        ),
        "execution_total_ms": summarize_numbers(
            [float(record.execution_total_ms) for record in successful if record.execution_total_ms is not None]
        ),
        "timeout_count": sum(1 for record in records if record.timeout_detected),
        **summarize_resource_series(resource_samples),
    }


def collect_control_cycle_profile(
    args: argparse.Namespace,
    launch_spec: LaunchSpecResolution,
    sample_path: Path,
) -> dict[str, Any]:
    if not args.include_control_cycles:
        return {"status": "skipped", "reason": "include_control_cycles=false"}
    if not args.include_start_job:
        return {"status": "skipped", "reason": "include_start_job=false"}
    if args.pause_resume_cycles <= 0 and args.stop_reset_rounds <= 0:
        return {"status": "skipped", "reason": "pause_resume_cycles<=0 and stop_reset_rounds<=0"}

    records: list[ControlCycleRecord] = []
    resource_samples: list[dict[str, Any]] = []
    with managed_backend(args.host, args.port, launch_spec, allow_reuse_running=args.reuse_running) as lease:
        with protocol_client(args.host, args.port) as (_, protocol):
            maybe_prepare_execution_runtime(protocol, args, launch_spec)
            artifact_id, artifact_ms = create_artifact(protocol, sample_path, args.artifact_timeout)
            resource_samples.append(collect_process_metrics(gateway_process_id(lease)))
            total_rounds = max(args.pause_resume_cycles, args.stop_reset_rounds)
            for round_index in range(total_rounds):
                record = run_control_cycle_round(
                    protocol,
                    artifact_id,
                    args,
                    round_index=round_index + 1,
                    pause_resume_enabled=round_index < args.pause_resume_cycles,
                    stop_reset_enabled=round_index < args.stop_reset_rounds,
                )
                records.append(record)
                resource_samples.append(collect_process_metrics(gateway_process_id(lease)))

    error = next((record.error for record in records if record.error), "")
    status = "ok" if records and not error else "error"
    payload = {
        "status": status,
        "mode": "reuse-backend-and-artifact",
        "artifact_warmup_ms": round(artifact_ms, 3),
        "pause_resume_cycles": args.pause_resume_cycles,
        "stop_reset_rounds": args.stop_reset_rounds,
        "iterations": [asdict(record) for record in records],
        "resource_samples": [sample for sample in resource_samples if sample],
        "summary": summarize_control_cycle_records(records, [sample for sample in resource_samples if sample]),
    }
    if error:
        payload["error"] = error
    return payload


def summarize_long_run_records(records: list[LongRunIterationRecord], resource_samples: list[dict[str, Any]]) -> dict[str, Any]:
    successful = [record for record in records if record.success]
    return {
        "count": len(records),
        "success_count": len(successful),
        "failure_count": len(records) - len(successful),
        "execution_total_ms": summarize_numbers(
            [float(record.execution_total_ms) for record in successful if record.execution_total_ms is not None]
        ),
        "timeout_count": sum(1 for record in records if record.timeout_detected),
        **summarize_resource_series(resource_samples),
    }


def collect_long_run_profile(
    args: argparse.Namespace,
    launch_spec: LaunchSpecResolution,
    sample_path: Path,
) -> dict[str, Any]:
    if args.long_run_minutes <= 0:
        return {"status": "skipped", "reason": "long_run_minutes<=0"}
    if not args.include_start_job:
        return {"status": "skipped", "reason": "include_start_job=false"}

    deadline = time.perf_counter() + max(1.0, float(args.long_run_minutes) * 60.0)
    records: list[LongRunIterationRecord] = []
    resource_samples: list[dict[str, Any]] = []
    with managed_backend(args.host, args.port, launch_spec, allow_reuse_running=args.reuse_running) as lease:
        with protocol_client(args.host, args.port) as (_, protocol):
            maybe_prepare_execution_runtime(protocol, args, launch_spec)
            artifact_id, artifact_ms = create_artifact(protocol, sample_path, args.artifact_timeout)
            resource_samples.append(collect_process_metrics(gateway_process_id(lease)))
            preview_record = prepare_and_snapshot_once(protocol, artifact_id, args, artifact_ms=artifact_ms)
            resource_samples.append(collect_process_metrics(gateway_process_id(lease)))
            if not preview_record.success:
                records.append(
                    LongRunIterationRecord(
                        iteration_index=1,
                        success=False,
                        timeout_detected="timeout" in preview_record.error.lower(),
                        error=f"preview setup failed: {preview_record.error}",
                    )
                )
            else:
                iteration_index = 0
                while iteration_index == 0 or time.perf_counter() < deadline:
                    iteration_index += 1
                    execution_record = run_start_job_cycle(protocol, preview_record)
                    if not execution_record.success:
                        records.append(
                            LongRunIterationRecord(
                                iteration_index=iteration_index,
                                success=False,
                                execution_total_ms=execution_record.execution_total_ms,
                                timeout_detected="timeout" in execution_record.error.lower(),
                                error=execution_record.error,
                            )
                        )
                    else:
                        records.append(
                            LongRunIterationRecord(
                                iteration_index=iteration_index,
                                success=True,
                                execution_total_ms=execution_record.execution_total_ms,
                                timeout_detected="timeout" in execution_record.error.lower(),
                                error=execution_record.error,
                            )
                        )
                    resource_samples.append(collect_process_metrics(gateway_process_id(lease)))

    filtered_resource_samples = [sample for sample in resource_samples if sample]
    error = next((record.error for record in records if record.error), "")
    status = "ok" if records and not error else "error"
    payload = {
        "status": status,
        "mode": "reuse-backend-and-artifact",
        "minutes": float(args.long_run_minutes),
        "preview_setup": asdict(preview_record),
        "iterations": [asdict(record) for record in records],
        "resource_samples": filtered_resource_samples,
        "summary": summarize_long_run_records(records, filtered_resource_samples),
    }
    if error:
        payload["error"] = error
    return payload


def collect_preview_and_execution_cycle(
    protocol: CommandProtocol,
    artifact_id: str,
    args: argparse.Namespace,
    *,
    artifact_ms: float | None,
) -> tuple[PreviewCycleRecord, StartJobCycleRecord | None]:
    preview_record = prepare_and_snapshot_once(protocol, artifact_id, args, artifact_ms=artifact_ms)
    if not args.include_start_job:
        return preview_record, None
    return preview_record, run_start_job_cycle(protocol, preview_record)


def collect_cold_profile(
    args: argparse.Namespace,
    launch_spec: LaunchSpecResolution,
    sample_path: Path,
) -> dict[str, Any]:
    if args.cold_iterations <= 0:
        return {"status": "skipped", "reason": "cold_iterations=0"}
    if args.reuse_running and port_open(args.host, args.port):
        return {
            "status": "skipped",
            "reason": "gateway is already running; cold profiling requires exclusive restart. Stop it or disable --reuse-running.",
        }

    preview_records: list[PreviewCycleRecord] = []
    execution_records: list[StartJobCycleRecord] = []
    for _ in range(args.cold_iterations):
        with managed_backend(args.host, args.port, launch_spec, allow_reuse_running=False):
            with protocol_client(args.host, args.port) as (_, protocol):
                maybe_prepare_execution_runtime(protocol, args, launch_spec)
                artifact_id, artifact_ms = create_artifact(protocol, sample_path, args.artifact_timeout)
                preview_record, execution_record = collect_preview_and_execution_cycle(
                    protocol,
                    artifact_id,
                    args,
                    artifact_ms=artifact_ms,
                )
                preview_records.append(preview_record)
                if execution_record is not None:
                    execution_records.append(execution_record)

    return {
        "status": "ok",
        "mode": "restart-backend-per-iteration",
        "preview_iterations": [asdict(record) for record in preview_records],
        "preview_summary": summarize_preview_records(preview_records),
        "execution": execution_payload_for(execution_records, args.include_start_job),
    }


def collect_hot_profile(
    args: argparse.Namespace,
    launch_spec: LaunchSpecResolution,
    sample_path: Path,
) -> dict[str, Any]:
    if args.hot_iterations <= 0:
        return {"status": "skipped", "reason": "hot_iterations=0"}

    with managed_backend(args.host, args.port, launch_spec, allow_reuse_running=args.reuse_running) as lease:
        with protocol_client(args.host, args.port) as (_, protocol):
            maybe_prepare_execution_runtime(protocol, args, launch_spec)
            artifact_id, artifact_ms = create_artifact(protocol, sample_path, args.artifact_timeout)
            warmup_preview_records: list[PreviewCycleRecord] = []
            warmup_execution_records: list[StartJobCycleRecord] = []
            for _ in range(max(0, args.hot_warmup_iterations)):
                preview_record, execution_record = collect_preview_and_execution_cycle(
                    protocol,
                    artifact_id,
                    args,
                    artifact_ms=None,
                )
                warmup_preview_records.append(preview_record)
                if execution_record is not None:
                    warmup_execution_records.append(execution_record)

            measured_preview_records: list[PreviewCycleRecord] = []
            measured_execution_records: list[StartJobCycleRecord] = []
            for _ in range(args.hot_iterations):
                preview_record, execution_record = collect_preview_and_execution_cycle(
                    protocol,
                    artifact_id,
                    args,
                    artifact_ms=None,
                )
                measured_preview_records.append(preview_record)
                if execution_record is not None:
                    measured_execution_records.append(execution_record)

    return {
        "status": "ok",
        "mode": "reuse-backend-and-artifact",
        "backend_mode": lease.mode,
        "artifact_warmup_ms": round(artifact_ms, 3),
        "hot_warmup_preview_iterations": [asdict(record) for record in warmup_preview_records],
        "preview_iterations": [asdict(record) for record in measured_preview_records],
        "preview_summary": summarize_preview_records(measured_preview_records),
        "execution": {
            **execution_payload_for(measured_execution_records, args.include_start_job),
            "warmup_iterations": [asdict(record) for record in warmup_execution_records],
        }
        if args.include_start_job
        else execution_payload_for([], args.include_start_job),
    }


def concurrent_prepare_worker(
    args: argparse.Namespace,
    artifact_id: str,
    request_index: int,
    barrier: threading.Barrier,
    results: list[ConcurrentPrepareRecord | None],
) -> None:
    try:
        with protocol_client(args.host, args.port) as (_, protocol):
            recipe_context, recipe_context_error = validate_published_recipe_context(
                protocol,
                args.recipe_id,
                args.version_id,
            )
            if recipe_context is None:
                results[request_index] = ConcurrentPrepareRecord(
                    request_index=request_index,
                    success=False,
                    error=recipe_context_error or "recipe context validation failed",
                )
                return
            barrier.wait(timeout=max(10.0, args.prepare_timeout))
            started = time.perf_counter()
            ok, payload, error = protocol.dxf_prepare_plan(
                artifact_id=artifact_id,
                recipe_id=recipe_context["recipe_id"],
                version_id=recipe_context["version_id"],
                speed_mm_s=args.dispensing_speed_mm_s,
                dry_run=args.dry_run,
                dry_run_speed_mm_s=args.dry_run_speed_mm_s,
                timeout=args.prepare_timeout,
            )
            elapsed_ms = (time.perf_counter() - started) * 1000.0
            if not ok:
                results[request_index] = ConcurrentPrepareRecord(
                    request_index=request_index,
                    success=False,
                    plan_prepare_rpc_ms=elapsed_ms,
                    error=f"plan.prepare failed: {error}",
                )
                return
            profile = extract_prepare_profile(payload)
            results[request_index] = ConcurrentPrepareRecord(
                request_index=request_index,
                success=True,
                plan_id=str(payload.get("plan_id", "")).strip(),
                plan_fingerprint=str(payload.get("plan_fingerprint", "")).strip(),
                plan_prepare_rpc_ms=elapsed_ms,
                authority_cache_hit=profile["authority_cache_hit"],
                authority_joined_inflight=profile["authority_joined_inflight"],
                authority_wait_ms=profile["authority_wait_ms"],
                pb_prepare_ms=profile["pb_prepare_ms"],
                path_load_ms=profile["path_load_ms"],
                process_path_ms=profile["process_path_ms"],
                authority_build_ms=profile["authority_build_ms"],
                authority_total_ms=profile["authority_total_ms"],
                prepare_total_ms=profile["prepare_total_ms"],
            )
    except Exception as exc:  # noqa: BLE001
        results[request_index] = ConcurrentPrepareRecord(
            request_index=request_index,
            success=False,
            error=str(exc),
        )


def summarize_concurrent_records(records: list[ConcurrentPrepareRecord]) -> dict[str, Any]:
    successful = [record for record in records if record.success]
    unique_plan_ids = sorted({record.plan_id for record in successful if record.plan_id})
    summary = {
        "count": len(records),
        "success_count": len(successful),
        "failure_count": len(records) - len(successful),
        "unique_plan_id_count": len(unique_plan_ids),
        "plan_prepare_rpc_ms": summarize_numbers(
            [float(record.plan_prepare_rpc_ms) for record in successful if record.plan_prepare_rpc_ms is not None]
        ),
        "authority_cache_hit": summarize_bool(
            [bool(record.authority_cache_hit) for record in successful if record.authority_cache_hit is not None]
        ),
        "authority_joined_inflight": summarize_bool(
            [
                bool(record.authority_joined_inflight)
                for record in successful
                if record.authority_joined_inflight is not None
            ]
        ),
    }
    for field in (
        "authority_wait_ms",
        "pb_prepare_ms",
        "path_load_ms",
        "process_path_ms",
        "authority_build_ms",
        "authority_total_ms",
        "prepare_total_ms",
    ):
        summary[field] = summarize_numbers(
            [float(getattr(record, field)) for record in successful if getattr(record, field) is not None]
        )
    return summary


def collect_singleflight_profile(
    args: argparse.Namespace,
    launch_spec: LaunchSpecResolution,
    sample_path: Path,
) -> dict[str, Any]:
    if args.singleflight_rounds <= 0:
        return {"status": "skipped", "reason": "singleflight_rounds=0"}
    if args.singleflight_fanout <= 1:
        return {"status": "skipped", "reason": "singleflight_fanout<=1"}

    rounds: list[dict[str, Any]] = []
    all_request_records: list[ConcurrentPrepareRecord] = []

    with managed_backend(args.host, args.port, launch_spec, allow_reuse_running=args.reuse_running) as lease:
        for round_index in range(args.singleflight_rounds):
            with protocol_client(args.host, args.port) as (_, protocol):
                artifact_id, artifact_ms = create_artifact(protocol, sample_path, args.artifact_timeout)

            barrier = threading.Barrier(args.singleflight_fanout)
            result_slots: list[ConcurrentPrepareRecord | None] = [None] * args.singleflight_fanout
            threads = [
                threading.Thread(
                    target=concurrent_prepare_worker,
                    args=(args, artifact_id, request_index, barrier, result_slots),
                    daemon=True,
                )
                for request_index in range(args.singleflight_fanout)
            ]
            for thread in threads:
                thread.start()
            for thread in threads:
                thread.join(timeout=max(30.0, args.prepare_timeout + 5.0))

            records: list[ConcurrentPrepareRecord] = []
            for request_index, record in enumerate(result_slots):
                if record is None:
                    record = ConcurrentPrepareRecord(
                        request_index=request_index,
                        success=False,
                        error="thread join timeout or worker did not report back",
                    )
                records.append(record)
            all_request_records.extend(records)
            rounds.append(
                {
                    "round_index": round_index + 1,
                    "artifact_id": artifact_id,
                    "artifact_ms": round(artifact_ms, 3),
                    "backend_mode": lease.mode,
                    "requests": [asdict(record) for record in records],
                    "summary": summarize_concurrent_records(records),
                }
            )

    return {
        "status": "ok",
        "mode": "same-artifact-concurrent-prepare",
        "fanout": args.singleflight_fanout,
        "backend_mode": lease.mode,
        "rounds": rounds,
        "summary": summarize_concurrent_records(all_request_records),
    }


def environment_snapshot(launch_spec: LaunchSpecResolution) -> dict[str, Any]:
    return {
        "generated_at": utc_now(),
        "workspace_root": str(ROOT),
        "platform": platform.platform(),
        "python_version": sys.version.splitlines()[0],
        "hostname": platform.node(),
        "cpu_count": os.cpu_count(),
        "gateway_launch_spec_source": launch_spec.source,
        "gateway_executable": str(launch_spec.spec.executable),
        "gateway_args": list(launch_spec.spec.args),
        "gateway_cwd": str(launch_spec.spec.cwd or launch_spec.spec.executable.parent),
        "gateway_env_keys": sorted(launch_spec.spec.env.keys()),
        "gateway_config_path": str(launch_spec.config_path) if launch_spec.config_path is not None else "",
        "gateway_hardware_mode": launch_spec.hardware_mode,
        "gateway_auto_provisioned_mock": launch_spec.auto_provisioned_mock,
    }


def collect_for_sample(
    args: argparse.Namespace,
    launch_spec: LaunchSpecResolution,
    label: str,
    sample_path: Path,
) -> dict[str, Any]:
    result = {
        "label": label,
        "path": str(sample_path),
        "size_bytes": sample_path.stat().st_size,
        "cold": None,
        "hot": None,
        "singleflight": None,
        "control_cycle": None,
        "long_run": None,
    }

    try:
        result["cold"] = collect_cold_profile(args, launch_spec, sample_path)
    except Exception as exc:  # noqa: BLE001
        result["cold"] = {"status": "error", "error": str(exc)}

    try:
        result["hot"] = collect_hot_profile(args, launch_spec, sample_path)
    except Exception as exc:  # noqa: BLE001
        result["hot"] = {"status": "error", "error": str(exc)}

    try:
        result["singleflight"] = collect_singleflight_profile(args, launch_spec, sample_path)
    except Exception as exc:  # noqa: BLE001
        result["singleflight"] = {"status": "error", "error": str(exc)}

    try:
        result["control_cycle"] = collect_control_cycle_profile(args, launch_spec, sample_path)
    except Exception as exc:  # noqa: BLE001
        result["control_cycle"] = {"status": "error", "error": str(exc)}

    try:
        result["long_run"] = collect_long_run_profile(args, launch_spec, sample_path)
    except Exception as exc:  # noqa: BLE001
        result["long_run"] = {"status": "error", "error": str(exc)}

    return result


def add_metric_point(
    points: dict[str, dict[str, Any]],
    *,
    table: str,
    sample_label: str,
    scenario_name: str,
    metric_name: str,
    stat_name: str,
    value: float,
) -> None:
    key = f"{table}.{sample_label}.{scenario_name}.{metric_name}.{stat_name}"
    points[key] = {
        "key": key,
        "table": table,
        "sample_label": sample_label,
        "scenario_name": scenario_name,
        "metric_name": metric_name,
        "stat_name": stat_name,
        "value": round(float(value), 3),
    }


def extend_numeric_summary_points(
    points: dict[str, dict[str, Any]],
    *,
    table: str,
    sample_label: str,
    scenario_name: str,
    summary: dict[str, Any],
    numeric_metrics: tuple[str, ...],
    bool_metrics: tuple[str, ...] = (),
) -> None:
    for metric_name in numeric_metrics:
        metric_summary = summary.get(metric_name, {})
        if not isinstance(metric_summary, dict):
            continue
        for stat_name in ("p50_ms", "p95_ms"):
            value = metric_summary.get(stat_name)
            if isinstance(value, (int, float)):
                add_metric_point(
                    points,
                    table=table,
                    sample_label=sample_label,
                    scenario_name=scenario_name,
                    metric_name=metric_name,
                    stat_name=stat_name,
                    value=float(value),
                )
    for metric_name in bool_metrics:
        metric_summary = summary.get(metric_name, {})
        if not isinstance(metric_summary, dict):
            continue
        value = metric_summary.get("true_rate")
        if isinstance(value, (int, float)):
            add_metric_point(
                points,
                table=table,
                sample_label=sample_label,
                scenario_name=scenario_name,
                metric_name=metric_name,
                stat_name="true_rate",
                value=float(value),
            )


def extract_metric_points(payload: dict[str, Any]) -> dict[str, dict[str, Any]]:
    points: dict[str, dict[str, Any]] = {}
    results = payload.get("results", {})
    if not isinstance(results, dict):
        return points

    for sample_label, result in results.items():
        if not isinstance(result, dict):
            continue
        for scenario_name in ("cold", "hot"):
            scenario = result.get(scenario_name)
            if not isinstance(scenario, dict) or scenario.get("status") != "ok":
                continue

            preview_summary = scenario.get("preview_summary") or scenario.get("summary")
            if isinstance(preview_summary, dict):
                extend_numeric_summary_points(
                    points,
                    table="preview",
                    sample_label=sample_label,
                    scenario_name=scenario_name,
                    summary=preview_summary,
                    numeric_metrics=(
                        "artifact_ms",
                        "plan_prepare_rpc_ms",
                        "snapshot_rpc_ms",
                        "worker_total_ms",
                        "authority_total_ms",
                        "prepare_total_ms",
                    ),
                    bool_metrics=("authority_cache_hit", "authority_joined_inflight"),
                )

            execution_block = scenario.get("execution", {})
            if isinstance(execution_block, dict) and execution_block.get("status") == "ok":
                execution_summary = execution_block.get("summary", {})
                if isinstance(execution_summary, dict):
                    extend_numeric_summary_points(
                        points,
                        table="execution",
                        sample_label=sample_label,
                        scenario_name=scenario_name,
                        summary=execution_summary,
                        numeric_metrics=(
                            "confirm_rpc_ms",
                            "job_start_rpc_ms",
                            "job_status_rpc_ms",
                            "job_stop_rpc_ms",
                            "job_lifecycle_rpc_ms",
                            "execution_wait_ms",
                            "motion_plan_ms",
                            "assembly_ms",
                            "export_ms",
                            "execution_total_ms",
                        ),
                        bool_metrics=("execution_cache_hit", "execution_joined_inflight"),
                    )

        singleflight = result.get("singleflight")
        if isinstance(singleflight, dict) and singleflight.get("status") == "ok":
            singleflight_summary = singleflight.get("summary", {})
            if isinstance(singleflight_summary, dict):
                extend_numeric_summary_points(
                    points,
                    table="singleflight",
                    sample_label=sample_label,
                    scenario_name="singleflight",
                    summary=singleflight_summary,
                    numeric_metrics=("plan_prepare_rpc_ms", "authority_wait_ms", "prepare_total_ms"),
                    bool_metrics=("authority_cache_hit", "authority_joined_inflight"),
                )
        control_cycle = result.get("control_cycle")
        if isinstance(control_cycle, dict) and control_cycle.get("status") == "ok":
            control_summary = control_cycle.get("summary", {})
            if isinstance(control_summary, dict):
                extend_numeric_summary_points(
                    points,
                    table="control_cycle",
                    sample_label=sample_label,
                    scenario_name="control_cycle",
                    summary=control_summary,
                    numeric_metrics=(
                        "pause_to_running_ms",
                        "stop_to_idle_ms",
                        "rerun_total_ms",
                        "execution_total_ms",
                    ),
                )
                for metric_name in (
                    "working_set_mb",
                    "private_memory_mb",
                    "handle_count",
                    "thread_count",
                ):
                    metric_summary = control_summary.get(metric_name, {})
                    if not isinstance(metric_summary, dict):
                        continue
                    value = metric_summary.get("delta_max")
                    if isinstance(value, (int, float)):
                        add_metric_point(
                            points,
                            table="control_cycle",
                            sample_label=sample_label,
                            scenario_name="control_cycle",
                            metric_name=metric_name,
                            stat_name="delta_max",
                            value=float(value),
                        )
                timeout_count = control_summary.get("timeout_count")
                if isinstance(timeout_count, (int, float)):
                    add_metric_point(
                        points,
                        table="control_cycle",
                        sample_label=sample_label,
                        scenario_name="control_cycle",
                        metric_name="timeout_count",
                        stat_name="value",
                        value=float(timeout_count),
                    )
        long_run = result.get("long_run")
        if isinstance(long_run, dict) and long_run.get("status") == "ok":
            long_run_summary = long_run.get("summary", {})
            if isinstance(long_run_summary, dict):
                extend_numeric_summary_points(
                    points,
                    table="long_run",
                    sample_label=sample_label,
                    scenario_name="long_run",
                    summary=long_run_summary,
                    numeric_metrics=("execution_total_ms",),
                )
                for metric_name in (
                    "working_set_mb",
                    "private_memory_mb",
                    "handle_count",
                    "thread_count",
                ):
                    metric_summary = long_run_summary.get(metric_name, {})
                    if not isinstance(metric_summary, dict):
                        continue
                    delta_value = metric_summary.get("delta_max")
                    if isinstance(delta_value, (int, float)):
                        add_metric_point(
                            points,
                            table="long_run",
                            sample_label=sample_label,
                            scenario_name="long_run",
                            metric_name=metric_name,
                            stat_name="delta_max",
                            value=float(delta_value),
                        )
                    max_value = metric_summary.get("max")
                    if isinstance(max_value, (int, float)):
                        add_metric_point(
                            points,
                            table="long_run",
                            sample_label=sample_label,
                            scenario_name="long_run",
                            metric_name=metric_name,
                            stat_name="max",
                            value=float(max_value),
                        )
                timeout_count = long_run_summary.get("timeout_count")
                if isinstance(timeout_count, (int, float)):
                    add_metric_point(
                        points,
                        table="long_run",
                        sample_label=sample_label,
                        scenario_name="long_run",
                        metric_name="timeout_count",
                        stat_name="value",
                        value=float(timeout_count),
                    )
    return points


def compare_against_baseline(
    payload: dict[str, Any],
    baseline_json_path: str,
    regression_threshold_pct: float,
) -> dict[str, Any]:
    baseline_path = Path(baseline_json_path).expanduser().resolve()
    baseline_payload = json.loads(baseline_path.read_text(encoding="utf-8"))
    current_points = extract_metric_points(payload)
    baseline_points = extract_metric_points(baseline_payload)

    entries: list[dict[str, Any]] = []
    regressions: list[dict[str, Any]] = []
    for key in sorted(current_points.keys()):
        current_entry = current_points[key]
        baseline_entry = baseline_points.get(key)
        status = "missing_baseline"
        baseline_value: float | None = None
        drift_pct: float | None = None
        if baseline_entry is not None:
            baseline_value = float(baseline_entry["value"])
            current_value = float(current_entry["value"])
            if baseline_value > 0:
                drift_pct = round(((current_value - baseline_value) / baseline_value) * 100.0, 3)
                if drift_pct > regression_threshold_pct:
                    status = "regression"
                elif drift_pct < 0:
                    status = "improved"
                else:
                    status = "ok"
            else:
                status = "not_comparable"
        entry = {
            **current_entry,
            "baseline_value": baseline_value,
            "drift_pct": drift_pct,
            "status": status,
        }
        entries.append(entry)
        if status == "regression":
            regressions.append(entry)

    return {
        "status": "regression_detected" if regressions else "ok",
        "baseline_json": str(baseline_path),
        "regression_threshold_pct": regression_threshold_pct,
        "entries": entries,
        "regressions": regressions,
        "compared_metric_count": sum(1 for entry in entries if entry["baseline_value"] is not None),
    }


def _lookup_path(payload: dict[str, Any], dotted_path: str) -> tuple[Any, str]:
    current: Any = payload
    for segment in dotted_path.split("."):
        if not isinstance(current, dict) or segment not in current:
            return None, f"missing:{dotted_path}"
        current = current[segment]
    return current, "ok"


def evaluate_threshold_gate(payload: dict[str, Any], gate_mode: str, threshold_config_path: str) -> dict[str, Any]:
    if gate_mode == "adhoc":
        return {
            "status": "skipped",
            "gate_mode": gate_mode,
            "threshold_config": "",
            "checks": [],
            "message": "threshold gate disabled for adhoc sampling",
        }

    config_path = Path(threshold_config_path).expanduser().resolve()
    if not config_path.exists():
        return {
            "status": "failed",
            "gate_mode": gate_mode,
            "threshold_config": str(config_path),
            "checks": [],
            "message": "threshold config missing for nightly-performance gate",
        }

    config_payload = json.loads(config_path.read_text(encoding="utf-8"))
    if str(config_payload.get("schema_version", "")) != "dxf-preview-profile-thresholds.v1":
        return {
            "status": "failed",
            "gate_mode": gate_mode,
            "threshold_config": str(config_path),
            "checks": [],
            "message": "threshold config schema_version mismatch",
        }

    checks: list[dict[str, Any]] = []
    required_samples = config_payload.get("required_samples", [])
    for sample_label in required_samples:
        present = str(sample_label) in payload.get("results", {})
        checks.append(
            {
                "name": f"required-sample:{sample_label}",
                "status": "passed" if present else "failed",
                "expected": "sample present in payload.results",
                "actual": "present" if present else "missing",
            }
        )

    required_scenarios = config_payload.get("required_scenarios", [])
    for entry in required_scenarios:
        sample_label = str(entry.get("sample_label", ""))
        scenario = str(entry.get("scenario", ""))
        expected_status = str(entry.get("expected_status", "ok"))
        scenario_payload = payload.get("results", {}).get(sample_label, {}).get(scenario, {})
        actual_status = str(scenario_payload.get("status", "missing")) if isinstance(scenario_payload, dict) else "missing"
        checks.append(
            {
                "name": f"required-scenario:{sample_label}:{scenario}",
                "status": "passed" if actual_status == expected_status else "failed",
                "expected": f"status={expected_status}",
                "actual": f"status={actual_status}",
            }
        )

    numeric_thresholds = config_payload.get("numeric_thresholds", [])
    for entry in numeric_thresholds:
        name = str(entry.get("name", entry.get("path", "threshold")))
        dotted_path = str(entry.get("path", ""))
        value, lookup_status = _lookup_path(payload, dotted_path)
        if lookup_status != "ok" or not isinstance(value, (int, float)):
            checks.append(
                {
                    "name": name,
                    "status": "failed",
                    "expected": "numeric value present",
                    "actual": lookup_status,
                }
            )
            continue
        min_value = entry.get("min")
        max_value = entry.get("max")
        passed = True
        expected_parts: list[str] = []
        if isinstance(min_value, (int, float)):
            expected_parts.append(f">={float(min_value)}")
            passed = passed and float(value) >= float(min_value)
        if isinstance(max_value, (int, float)):
            expected_parts.append(f"<={float(max_value)}")
            passed = passed and float(value) <= float(max_value)
        checks.append(
            {
                "name": name,
                "status": "passed" if passed and expected_parts else "failed",
                "expected": " and ".join(expected_parts) if expected_parts else "configured threshold",
                "actual": f"value={float(value)}",
                "path": dotted_path,
            }
        )

    status = "passed" if checks and all(check["status"] == "passed" for check in checks) else "failed"
    return {
        "status": status,
        "gate_mode": gate_mode,
        "threshold_config": str(config_path),
        "threshold_config_schema_version": str(config_payload.get("schema_version", "")),
        "checks": checks,
        "message": "threshold gate passed" if status == "passed" else "threshold gate failed",
    }


def preview_row(sample_label: str, scenario_name: str, scenario: dict[str, Any] | None) -> str:
    if not scenario:
        return f"| {sample_label} | {scenario_name} | missing | - | - | - | - | - | - | - |"
    status = str(scenario.get("status", "unknown"))
    if status != "ok":
        detail = scenario.get("reason") or scenario.get("error") or "-"
        return f"| {sample_label} | {scenario_name} | {status} | - | - | - | - | - | - | {detail} |"

    summary = scenario.get("preview_summary") or scenario.get("summary") or {}
    cache_summary = summary.get("authority_cache_hit", {})
    joined_summary = summary.get("authority_joined_inflight", {})
    return (
        f"| {sample_label} | {scenario_name} | ok | "
        f"{summary.get('artifact_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('artifact_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('plan_prepare_rpc_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('plan_prepare_rpc_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('snapshot_rpc_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('snapshot_rpc_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('worker_total_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('worker_total_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('authority_total_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('authority_total_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{cache_summary.get('true_rate', 0.0):.4f} | "
        f"{joined_summary.get('true_rate', 0.0):.4f} |"
    )


def execution_row(sample_label: str, scenario_name: str, scenario: dict[str, Any] | None) -> str:
    if not scenario:
        return f"| {sample_label} | {scenario_name} | missing | - | - | - | - | - | - | - |"

    execution_block = scenario.get("execution", {})
    if not isinstance(execution_block, dict):
        return f"| {sample_label} | {scenario_name} | missing | - | - | - | - | - | - | - |"

    status = str(execution_block.get("status", "unknown"))
    if status != "ok":
        detail = execution_block.get("reason") or execution_block.get("error") or "-"
        return f"| {sample_label} | {scenario_name} | {status} | - | - | - | - | - | - | {detail} |"

    summary = execution_block.get("summary", {})
    cache_summary = summary.get("execution_cache_hit", {})
    joined_summary = summary.get("execution_joined_inflight", {})
    return (
        f"| {sample_label} | {scenario_name} | ok | "
        f"{summary.get('job_start_rpc_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('job_start_rpc_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('execution_total_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('execution_total_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('motion_plan_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('motion_plan_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('assembly_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('assembly_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('export_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('export_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{cache_summary.get('true_rate', 0.0):.4f} | "
        f"{joined_summary.get('true_rate', 0.0):.4f} |"
    )


def singleflight_row(sample_label: str, scenario: dict[str, Any] | None) -> str:
    if not scenario:
        return f"| {sample_label} | missing | - | - | - | - | - |"
    status = str(scenario.get("status", "unknown"))
    if status != "ok":
        detail = scenario.get("reason") or scenario.get("error") or "-"
        return f"| {sample_label} | {status} | - | - | - | - | {detail} |"

    summary = scenario.get("summary", {})
    return (
        f"| {sample_label} | ok | "
        f"{scenario.get('fanout', 0)} x {len(scenario.get('rounds', []))} | "
        f"{summary.get('plan_prepare_rpc_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('plan_prepare_rpc_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('authority_wait_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('authority_wait_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('authority_cache_hit', {}).get('true_rate', 0.0):.4f} | "
        f"{summary.get('authority_joined_inflight', {}).get('true_rate', 0.0):.4f} |"
    )


def control_cycle_row(sample_label: str, scenario: dict[str, Any] | None) -> str:
    if not scenario:
        return f"| {sample_label} | missing | - | - | - | - | - | - | - |"
    status = str(scenario.get("status", "unknown"))
    if status != "ok":
        detail = scenario.get("reason") or scenario.get("error") or "-"
        return f"| {sample_label} | {status} | - | - | - | - | - | - | {detail} |"

    summary = scenario.get("summary", {})
    return (
        f"| {sample_label} | ok | "
        f"{summary.get('pause_to_running_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('pause_to_running_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('stop_to_idle_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('stop_to_idle_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('rerun_total_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('rerun_total_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('working_set_mb', {}).get('delta_max', 0.0):.3f} | "
        f"{summary.get('private_memory_mb', {}).get('delta_max', 0.0):.3f} | "
        f"{summary.get('handle_count', {}).get('delta_max', 0.0):.3f} | "
        f"{summary.get('timeout_count', 0)} |"
    )


def control_cycle_diagnostics_row(sample_label: str, scenario: dict[str, Any] | None) -> str:
    if not scenario:
        return f"| {sample_label} | missing | - | - | - |"
    status = str(scenario.get('status', 'unknown'))
    if status != 'ok':
        detail = scenario.get("reason") or scenario.get("error") or "-"
        return f"| {sample_label} | {status} | - | - | - | {detail} |"

    summary = scenario.get("summary", {})
    return (
        f"| {sample_label} | ok | "
        f"{summary.get('stop_rpc_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('stop_rpc_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('stop_wait_for_terminal_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('stop_wait_for_terminal_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('stop_wait_for_idle_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('stop_wait_for_idle_ms', {}).get('p95_ms', 0.0):.3f} |"
    )


def long_run_row(sample_label: str, scenario: dict[str, Any] | None) -> str:
    if not scenario:
        return f"| {sample_label} | missing | - | - | - | - | - | - |"
    status = str(scenario.get("status", "unknown"))
    if status != "ok":
        detail = scenario.get("reason") or scenario.get("error") or "-"
        return f"| {sample_label} | {status} | - | - | - | - | - | {detail} |"

    summary = scenario.get("summary", {})
    return (
        f"| {sample_label} | ok | "
        f"{summary.get('execution_total_ms', {}).get('p50_ms', 0.0):.3f} / {summary.get('execution_total_ms', {}).get('p95_ms', 0.0):.3f} | "
        f"{summary.get('working_set_mb', {}).get('delta_max', 0.0):.3f} | "
        f"{summary.get('private_memory_mb', {}).get('delta_max', 0.0):.3f} | "
        f"{summary.get('handle_count', {}).get('delta_max', 0.0):.3f} | "
        f"{summary.get('thread_count', {}).get('max', 0.0):.3f} | "
        f"{summary.get('timeout_count', 0)} |"
    )


def render_markdown(payload: dict[str, Any]) -> str:
    lines = [
        "# DXF Preview Profiles",
        "",
        f"- generated_at: `{payload['environment']['generated_at']}`",
        f"- workspace_root: `{payload['environment']['workspace_root']}`",
        f"- gateway_launch_spec_source: `{payload['environment']['gateway_launch_spec_source']}`",
        f"- gateway_executable: `{payload['environment']['gateway_executable']}`",
        f"- gateway_config_path: `{payload['environment'].get('gateway_config_path', '')}`",
        f"- gateway_hardware_mode: `{payload['environment'].get('gateway_hardware_mode', 'unknown')}`",
        f"- gateway_auto_provisioned_mock: `{payload['environment'].get('gateway_auto_provisioned_mock', False)}`",
        f"- host_port: `{payload['gateway']['host']}:{payload['gateway']['port']}`",
        f"- recipe_id: `{payload.get('recipe_context', {}).get('recipe_id', '')}`",
        f"- version_id: `{payload.get('recipe_context', {}).get('version_id', '')}`",
        f"- recipe_context_source: `{payload.get('recipe_context', {}).get('recipe_context_source', '')}`",
        f"- dry_run: `{payload['sampling']['dry_run']}`",
        f"- include_start_job: `{payload['sampling']['include_start_job']}`",
        f"- include_control_cycles: `{payload['sampling']['include_control_cycles']}`",
        f"- pause_resume_cycles: `{payload['sampling']['pause_resume_cycles']}`",
        f"- stop_reset_rounds: `{payload['sampling']['stop_reset_rounds']}`",
        f"- long_run_minutes: `{payload['sampling']['long_run_minutes']}`",
        f"- sample_labels: `{', '.join(payload['sampling']['sample_labels'])}`",
        "",
        "## Samples",
        "",
        "| Label | Path | Size (bytes) |",
        "|---|---|---:|",
    ]

    for label, sample in payload["samples"].items():
        lines.append(f"| {label} | {sample['path']} | {sample['size_bytes']} |")

    lines.extend(
        [
            "",
            "## Preview",
            "",
            "| Sample | Scenario | status | artifact p50/p95 (ms) | prepare p50/p95 (ms) | snapshot p50/p95 (ms) | worker p50/p95 (ms) | authority_total p50/p95 (ms) | cache_hit_rate | joined_inflight_rate |",
            "|---|---|---|---:|---:|---:|---:|---:|---:|---:|",
        ]
    )
    for label, result in payload["results"].items():
        lines.append(preview_row(label, "cold", result.get("cold")))
        lines.append(preview_row(label, "hot", result.get("hot")))

    lines.extend(
        [
            "",
            "## Execution",
            "",
            "| Sample | Scenario | status | start p50/p95 (ms) | execution_total p50/p95 (ms) | motion_plan p50/p95 (ms) | assembly p50/p95 (ms) | export p50/p95 (ms) | cache_hit_rate | joined_inflight_rate |",
            "|---|---|---|---:|---:|---:|---:|---:|---:|---:|",
        ]
    )
    for label, result in payload["results"].items():
        lines.append(execution_row(label, "cold", result.get("cold")))
        lines.append(execution_row(label, "hot", result.get("hot")))

    lines.extend(
        [
            "",
            "## Single Flight",
            "",
            "| Sample | status | fanout x rounds | prepare p50/p95 (ms) | authority_wait p50/p95 (ms) | cache_hit_rate | joined_inflight_rate |",
            "|---|---|---|---:|---:|---:|---:|",
        ]
    )
    for label, result in payload["results"].items():
        lines.append(singleflight_row(label, result.get("singleflight")))

    lines.extend(
        [
            "",
            "## Control Cycle",
            "",
            "| Sample | status | pause->running p50/p95 (ms) | stop->idle p50/p95 (ms) | rerun_total p50/p95 (ms) | working_set delta max (MB) | private_memory delta max (MB) | handle delta max | timeout_count |",
            "|---|---|---:|---:|---:|---:|---:|---:|---:|",
        ]
    )
    for label, result in payload["results"].items():
        lines.append(control_cycle_row(label, result.get("control_cycle")))

    lines.extend(
        [
            "",
            "## Control Cycle Diagnostics",
            "",
            "| Sample | status | stop_rpc p50/p95 (ms) | wait_terminal p50/p95 (ms) | wait_idle p50/p95 (ms) |",
            "|---|---|---:|---:|---:|",
        ]
    )
    for label, result in payload["results"].items():
        lines.append(control_cycle_diagnostics_row(label, result.get("control_cycle")))

    lines.extend(
        [
            "",
            "## Long Run",
            "",
            "| Sample | status | execution_total p50/p95 (ms) | working_set delta max (MB) | private_memory delta max (MB) | handle delta max | thread max | timeout_count |",
            "|---|---|---:|---:|---:|---:|---:|---:|",
        ]
    )
    for label, result in payload["results"].items():
        lines.append(long_run_row(label, result.get("long_run")))

    baseline = payload.get("baseline_comparison")
    if isinstance(baseline, dict):
        lines.extend(
            [
                "",
                "## Baseline Drift",
                "",
                f"- baseline_json: `{baseline.get('baseline_json', '')}`",
                f"- regression_threshold_pct: `{baseline.get('regression_threshold_pct', 0.0)}`",
                f"- comparison_status: `{baseline.get('status', 'unknown')}`",
                "",
                "| Table | Sample | Scenario | Metric | Current | Baseline | Drift | Status |",
                "|---|---|---|---|---:|---:|---:|---|",
            ]
        )
        entries = baseline.get("entries", [])
        if entries:
            for entry in entries:
                drift_value = entry.get("drift_pct")
                drift_text = f"{drift_value:+.3f}%" if isinstance(drift_value, (int, float)) else "n/a"
                if entry.get("baseline_value") is None:
                    lines.append(
                        f"| {entry['table']} | {entry['sample_label']} | {entry['scenario_name']} | "
                        f"{entry['metric_name']}.{entry['stat_name']} | {entry['value']:.3f} | - | n/a | {entry['status']} |"
                    )
                    continue
                lines.append(
                    f"| {entry['table']} | {entry['sample_label']} | {entry['scenario_name']} | "
                    f"{entry['metric_name']}.{entry['stat_name']} | {entry['value']:.3f} | "
                    f"{entry['baseline_value']:.3f} | {drift_text} | {entry['status']} |"
                )
        else:
            lines.append("| - | - | - | - | - | - | - | no comparable metrics |")

    threshold_gate = payload.get("threshold_gate")
    if isinstance(threshold_gate, dict):
        lines.extend(
            [
                "",
                "## Threshold Gate",
                "",
                f"- gate_mode: `{threshold_gate.get('gate_mode', '')}`",
                f"- status: `{threshold_gate.get('status', '')}`",
                f"- threshold_config: `{threshold_gate.get('threshold_config', '')}`",
                f"- message: `{threshold_gate.get('message', '')}`",
                "",
            ]
        )
        checks = threshold_gate.get("checks", [])
        if isinstance(checks, list) and checks:
            lines.extend(
                [
                    "| Check | Status | Expected | Actual |",
                    "|---|---|---|---|",
                ]
            )
            for check in checks:
                lines.append(
                    f"| {check.get('name', '')} | {check.get('status', '')} | {check.get('expected', '')} | {check.get('actual', '')} |"
                )

    lines.extend(
        [
            "",
            "## Notes",
            "",
            "- cold restarts the gateway per iteration to capture artifact.create + authority preview cold path behavior.",
            "- hot reuses the same backend and artifact to show authority cache reuse; when `--include-start-job` is on, it also warms and measures execution assembly reuse.",
            "- single-flight issues concurrent `plan.prepare` requests against the same artifact; use `authority_joined_inflight` and `authority_wait_ms` to judge prepare-side deduplication.",
            "- `--include-start-job` extends each preview cycle with `preview.confirm -> dxf.job.start -> dxf.job.status -> dxf.job.stop`, and the execution table surfaces the structured `dxf.job.start.performance_profile`.",
            "- `--include-control-cycles` reuses the same performance authority and adds pause/resume plus stop/reset/rerun observations on top of the execution path; it does not introduce a second runner.",
            "- `long_run` keeps reusing the same backend and artifact for the configured duration, then samples gateway process WorkingSet/PrivateMemory/HandleCount/ThreadCount through `Get-Process`.",
            "- When `--include-start-job` runs in dry-run mode without an explicit `--launch-spec`, the script pins to the workspace gateway, auto-materializes a temporary `Hardware.mode=Mock` config if needed, then performs `connect -> home.auto` before sampling. That bootstrap time is excluded from the execution table.",
            "- If a single-flight run shows `joined_inflight_rate=0` but cache hit is high, the sample likely completed too quickly and followers landed on hot cache instead of the inflight join window.",
            "- Baseline comparison remains advisory. `threshold_gate` is the only blocking mechanism when `--gate-mode nightly-performance` is used.",
        ]
    )
    return "\n".join(lines) + "\n"


def _performance_case_status(result: dict[str, Any]) -> str:
    raw_statuses: list[str] = []
    for key in ("cold", "hot", "singleflight", "control_cycle", "long_run"):
        block = result.get(key)
        if isinstance(block, dict):
            raw_statuses.append(str(block.get("status", "unknown")))
    normalized = ["passed" if status == "ok" else "skipped" if status == "skipped" else "failed" for status in raw_statuses]
    if not normalized:
        return "incomplete"
    if "failed" in normalized:
        return "failed"
    if all(status == "skipped" for status in normalized):
        return "skipped"
    if any(status == "passed" for status in normalized):
        return "passed"
    return "incomplete"


def _write_evidence_bundle(payload: dict[str, Any], run_dir: Path, summary_json_path: Path, summary_md_path: Path) -> None:
    case_records: list[EvidenceCaseRecord] = []
    linked_assets: set[str] = set()
    for sample_label, sample_info in payload.get("samples", {}).items():
        sample_path = str(sample_info.get("path", ""))
        asset_refs = performance_sample_asset_refs(ROOT, sample_path)
        linked_assets.update(asset_refs)
        case_status = _performance_case_status(payload.get("results", {}).get(sample_label, {}))
        case_records.append(
            EvidenceCaseRecord(
                case_id=f"performance-{sample_label}",
                name=f"performance-{sample_label}",
                suite_ref="performance",
                owner_scope="tests/performance",
                primary_layer="L6",
                producer_lane_ref="nightly-performance",
                status=case_status,
                evidence_profile="performance-report",
                stability_state="stable",
                required_assets=asset_refs,
                required_fixtures=("fixture.dxf-preview-profiler", "fixture.validation-evidence-bundle"),
                risk_tags=("performance", "single-flight"),
                trace_fields=trace_fields(
                    stage_id="L6",
                    artifact_id=f"performance-{sample_label}",
                    module_id="tests/performance",
                    workflow_state="executed",
                    execution_state=case_status,
                    event_name=f"performance-{sample_label}",
                    failure_code=f"performance.{case_status}" if case_status != "passed" else "",
                    evidence_path=str(summary_json_path.resolve()),
                ),
            )
        )

    bundle = EvidenceBundle(
        bundle_id=f"performance-{payload['environment']['generated_at']}",
        request_ref="nightly-performance",
        producer_lane_ref="nightly-performance",
        report_root=str(run_dir.resolve()),
        summary_file=str(summary_md_path.resolve()),
        machine_file=str(summary_json_path.resolve()),
        verdict=(
            "failed"
            if any(record.status == "failed" for record in case_records)
            or str(payload.get("threshold_gate", {}).get("status", "")) == "failed"
            else "passed"
        ),
        linked_asset_refs=tuple(sorted(linked_assets)),
        metadata={
            "sample_labels": payload.get("sampling", {}).get("sample_labels", []),
            "include_start_job": payload.get("sampling", {}).get("include_start_job", False),
            "threshold_gate": payload.get("threshold_gate", {}),
        },
        case_records=case_records,
    )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=run_dir,
        summary_json_path=summary_json_path,
        summary_md_path=summary_md_path,
        evidence_links=[
            EvidenceLink(label="report.json", path=str(summary_json_path.resolve()), role="machine-summary"),
            EvidenceLink(label="report.md", path=str(summary_md_path.resolve()), role="human-summary"),
        ],
    )


def write_reports(payload: dict[str, Any], report_dir: Path) -> dict[str, str]:
    report_dir.mkdir(parents=True, exist_ok=True)
    run_dir = report_dir / timestamp_slug()
    run_dir.mkdir(parents=True, exist_ok=True)
    json_path = run_dir / "report.json"
    markdown_path = run_dir / "report.md"
    latest_json = report_dir / "latest.json"
    latest_markdown = report_dir / "latest.md"

    json_text = json.dumps(payload, ensure_ascii=False, indent=2)
    markdown_text = render_markdown(payload)

    json_path.write_text(json_text, encoding="utf-8")
    markdown_path.write_text(markdown_text, encoding="utf-8")
    latest_json.write_text(json_text, encoding="utf-8")
    latest_markdown.write_text(markdown_text, encoding="utf-8")
    _write_evidence_bundle(payload, run_dir, json_path, markdown_path)
    return {
        "run_dir": str(run_dir),
        "json": str(json_path),
        "markdown": str(markdown_path),
        "latest_json": str(latest_json),
        "latest_markdown": str(latest_markdown),
    }


def main() -> int:
    args = parse_args()
    samples = resolve_samples(args)
    launch_spec = resolve_launch_spec(args)

    payload: dict[str, Any] = {
        "environment": environment_snapshot(launch_spec),
        "recipe_context": require_explicit_recipe_context(args.recipe_id, args.version_id),
        "gateway": {
            "host": args.host,
            "port": args.port,
            "reuse_running": args.reuse_running,
        },
        "sampling": {
            "sample_labels": list(samples.keys()),
            "cold_iterations": args.cold_iterations,
            "hot_warmup_iterations": args.hot_warmup_iterations,
            "hot_iterations": args.hot_iterations,
            "singleflight_rounds": args.singleflight_rounds,
            "singleflight_fanout": args.singleflight_fanout,
            "dispensing_speed_mm_s": args.dispensing_speed_mm_s,
            "dry_run": args.dry_run,
            "dry_run_speed_mm_s": args.dry_run_speed_mm_s,
            "artifact_timeout": args.artifact_timeout,
            "prepare_timeout": args.prepare_timeout,
            "snapshot_timeout": args.snapshot_timeout,
            "max_polyline_points": args.max_polyline_points,
            "include_start_job": args.include_start_job,
            "include_control_cycles": args.include_control_cycles,
            "pause_resume_cycles": args.pause_resume_cycles,
            "stop_reset_rounds": args.stop_reset_rounds,
            "long_run_minutes": args.long_run_minutes,
            "baseline_json": args.baseline_json,
            "regression_threshold_pct": args.regression_threshold_pct,
            "gate_mode": args.gate_mode,
            "threshold_config": args.threshold_config,
        },
        "samples": {
            label: {
                "path": str(path),
                "size_bytes": path.stat().st_size,
            }
            for label, path in samples.items()
        },
        "results": {},
    }

    for label, sample_path in samples.items():
        payload["results"][label] = collect_for_sample(args, launch_spec, label, sample_path)

    if args.baseline_json:
        payload["baseline_comparison"] = compare_against_baseline(
            payload,
            args.baseline_json,
            args.regression_threshold_pct,
        )

    payload["threshold_gate"] = evaluate_threshold_gate(payload, args.gate_mode, args.threshold_config)

    written = write_reports(payload, Path(args.report_dir))
    print(json.dumps(written, ensure_ascii=False, indent=2))
    if str(payload["threshold_gate"].get("status", "")) == "failed" and args.gate_mode != "adhoc":
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
