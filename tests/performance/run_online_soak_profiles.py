from __future__ import annotations

import argparse
import json
import subprocess
import sys
import time
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

from test_kit.asset_catalog import default_performance_sample_asset_ids  # noqa: E402
from test_kit.evidence_bundle import (  # noqa: E402
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)


CHILD_RUNNER = ROOT / "tests" / "performance" / "collect_dxf_preview_profiles.py"
DEFAULT_REPORT_ROOT = ROOT / "tests" / "reports" / "performance" / "online-soak-profiles"
DEFAULT_THRESHOLD_CONFIG = ROOT / "tests" / "baselines" / "performance" / "dxf-preview-profile-thresholds.json"


@dataclass(frozen=True)
class SoakProfile:
    profile_id: str
    description: str
    sample_labels: tuple[str, ...]
    long_run_minutes: float
    gate_mode: str = "adhoc"
    include_control_cycles: bool = True
    pause_resume_cycles: int = 3
    stop_reset_rounds: int = 3


@dataclass(frozen=True)
class SoakProfileResult:
    profile_id: str
    status: str
    exit_code: int | None
    long_run_minutes: float
    sample_labels: tuple[str, ...]
    gate_mode: str
    command: list[str]
    report_dir: str
    latest_json: str
    latest_markdown: str
    threshold_gate_status: str
    note: str = ""


SOAK_PROFILES = (
    SoakProfile(
        profile_id="soak-30m-matrix",
        description="30 minute multi-sample soak for the canonical small/medium/large matrix.",
        sample_labels=("small", "medium", "large"),
        long_run_minutes=30.0,
    ),
    SoakProfile(
        profile_id="soak-2h-small",
        description="2 hour extended soak on the default small canonical sample.",
        sample_labels=("small",),
        long_run_minutes=120.0,
    ),
    SoakProfile(
        profile_id="soak-8h-small",
        description="8 hour extended soak on the default small canonical sample.",
        sample_labels=("small",),
        long_run_minutes=480.0,
    ),
    SoakProfile(
        profile_id="soak-24h-small",
        description="24 hour extended soak on the default small canonical sample.",
        sample_labels=("small",),
        long_run_minutes=1440.0,
    ),
)
DEFAULT_SOAK_PROFILE_IDS = tuple(profile.profile_id for profile in SOAK_PROFILES)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run named online soak profiles by orchestrating collect_dxf_preview_profiles.py."
    )
    parser.add_argument("--report-dir", type=Path, default=DEFAULT_REPORT_ROOT)
    parser.add_argument("--profile-id", action="append", default=[])
    parser.add_argument("--python-exe", default=sys.executable or "python")
    parser.add_argument("--gateway-exe", default="")
    parser.add_argument("--config-path", default="")
    parser.add_argument("--launch-spec", default="")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=9527)
    parser.add_argument("--threshold-config", default=str(DEFAULT_THRESHOLD_CONFIG))
    parser.add_argument("--allow-long-profiles", action="store_true")
    return parser.parse_args()


def resolve_profiles(selected_profile_ids: list[str] | tuple[str, ...]) -> tuple[SoakProfile, ...]:
    profile_map = {profile.profile_id: profile for profile in SOAK_PROFILES}
    if not selected_profile_ids:
        selected_profile_ids = list(DEFAULT_SOAK_PROFILE_IDS)
    profiles: list[SoakProfile] = []
    for profile_id in selected_profile_ids:
        if profile_id not in profile_map:
            raise KeyError(f"unknown soak profile_id: {profile_id}")
        profiles.append(profile_map[profile_id])
    return tuple(profiles)


def evaluate_collect_payload_status(payload: dict[str, Any]) -> str:
    threshold_gate = payload.get("threshold_gate", {})
    if isinstance(threshold_gate, dict) and str(threshold_gate.get("status", "")) == "failed":
        return "failed"

    results = payload.get("results", {})
    if not isinstance(results, dict) or not results:
        return "failed"

    for sample_result in results.values():
        if not isinstance(sample_result, dict):
            return "failed"
        for block_name in ("cold", "hot", "singleflight", "control_cycle", "long_run"):
            block = sample_result.get(block_name)
            if not isinstance(block, dict):
                return "failed"
            block_status = str(block.get("status", "missing"))
            if block_status not in {"ok", "skipped"}:
                return "failed"
    return "passed"


def _build_command(
    *,
    args: argparse.Namespace,
    profile: SoakProfile,
    profile_report_root: Path,
) -> list[str]:
    command = [
        args.python_exe,
        str(CHILD_RUNNER),
        "--report-dir",
        str(profile_report_root),
        "--sample-labels",
        *profile.sample_labels,
        "--host",
        args.host,
        "--port",
        str(args.port),
        "--include-start-job",
        "--include-control-cycles",
        "--pause-resume-cycles",
        str(profile.pause_resume_cycles),
        "--stop-reset-rounds",
        str(profile.stop_reset_rounds),
        "--long-run-minutes",
        str(profile.long_run_minutes),
        "--gate-mode",
        profile.gate_mode,
        "--threshold-config",
        args.threshold_config,
    ]
    if args.gateway_exe:
        command.extend(["--gateway-exe", args.gateway_exe])
    if args.config_path:
        command.extend(["--config-path", args.config_path])
    if args.launch_spec:
        command.extend(["--launch-spec", args.launch_spec])
    return command


def _run_profile(
    *,
    args: argparse.Namespace,
    suite_report_dir: Path,
    profile: SoakProfile,
) -> SoakProfileResult:
    profile_report_root = suite_report_dir / "profiles" / profile.profile_id
    profile_report_root.mkdir(parents=True, exist_ok=True)
    launcher_log_path = profile_report_root / "launcher.log"
    command = _build_command(args=args, profile=profile, profile_report_root=profile_report_root)
    completed = subprocess.run(
        command,
        cwd=str(ROOT),
        capture_output=True,
        text=True,
        encoding="utf-8",
        check=False,
    )
    combined_output = (completed.stdout or "") + ("\n" if completed.stderr else "") + (completed.stderr or "")
    launcher_log_path.write_text(combined_output, encoding="utf-8")

    latest_json_path = profile_report_root / "latest.json"
    latest_markdown_path = profile_report_root / "latest.md"
    threshold_gate_status = "missing"
    status = "failed"
    note = ""
    if latest_json_path.exists():
        payload = json.loads(latest_json_path.read_text(encoding="utf-8"))
        threshold_gate = payload.get("threshold_gate", {})
        if isinstance(threshold_gate, dict):
            threshold_gate_status = str(threshold_gate.get("status", "missing"))
        status = evaluate_collect_payload_status(payload)
        if status != "passed":
            note = (
                f"threshold_gate_status={threshold_gate_status} "
                f"exit_code={completed.returncode}"
            )
    else:
        note = "collect_dxf_preview_profiles latest.json missing"

    if completed.returncode != 0:
        status = "failed"
        if not note:
            note = f"child exit_code={completed.returncode}"

    return SoakProfileResult(
        profile_id=profile.profile_id,
        status=status,
        exit_code=completed.returncode,
        long_run_minutes=profile.long_run_minutes,
        sample_labels=profile.sample_labels,
        gate_mode=profile.gate_mode,
        command=command,
        report_dir=str(profile_report_root.resolve()),
        latest_json=str(latest_json_path.resolve()) if latest_json_path.exists() else "",
        latest_markdown=str(latest_markdown_path.resolve()) if latest_markdown_path.exists() else "",
        threshold_gate_status=threshold_gate_status,
        note=note,
    )


def _write_report(
    *,
    report_dir: Path,
    profile_results: list[SoakProfileResult],
    selected_profile_ids: tuple[str, ...],
    production_baseline: dict[str, str],
) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "online-soak-profiles-summary.json"
    md_path = report_dir / "online-soak-profiles-summary.md"
    overall_status = "passed" if profile_results and all(item.status == "passed" for item in profile_results) else infer_verdict([item.status for item in profile_results])
    payload = {
        "generated_at": utc_now(),
        "workspace_root": str(ROOT),
        "overall_status": overall_status,
        "signed_publish_authority": "tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1",
        "selected_profile_ids": list(selected_profile_ids),
        "production_baseline": production_baseline,
        "counts": {
            "requested": len(profile_results),
            "passed": sum(1 for item in profile_results if item.status == "passed"),
            "failed": sum(1 for item in profile_results if item.status == "failed"),
        },
        "profiles": [asdict(item) for item in profile_results],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Online Soak Profiles Summary",
        "",
        f"- generated_at: `{payload['generated_at']}`",
        f"- overall_status: `{payload['overall_status']}`",
        f"- signed_publish_authority: `{payload['signed_publish_authority']}`",
        f"- baseline_id: `{production_baseline.get('baseline_id', '')}`",
        f"- baseline_fingerprint: `{production_baseline.get('baseline_fingerprint', '')}`",
        f"- production_baseline_source: `{production_baseline.get('production_baseline_source', '')}`",
        f"- requested: `{payload['counts']['requested']}`",
        f"- passed: `{payload['counts']['passed']}`",
        f"- failed: `{payload['counts']['failed']}`",
        "",
        "## Profiles",
        "",
    ]
    for item in profile_results:
        labels = ",".join(item.sample_labels)
        lines.append(
            f"- `{item.status}` `{item.profile_id}` samples=`{labels}` "
            f"long_run_minutes=`{item.long_run_minutes}` threshold_gate=`{item.threshold_gate_status}`"
        )
        lines.append(f"  report_dir: `{item.report_dir}`")
        if item.latest_json:
            lines.append(f"  latest_json: `{item.latest_json}`")
        if item.note:
            lines.append(f"  note: `{item.note}`")
    lines.append("")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    _write_evidence_bundle(report_dir=report_dir, summary_json_path=json_path, summary_md_path=md_path, payload=payload)
    return json_path, md_path


def _write_evidence_bundle(
    *,
    report_dir: Path,
    summary_json_path: Path,
    summary_md_path: Path,
    payload: dict[str, Any],
) -> None:
    linked_assets = tuple(default_performance_sample_asset_ids(ROOT))
    case_records: list[EvidenceCaseRecord] = []
    for item in payload.get("profiles", []):
        status = str(item.get("status", "failed"))
        evidence_path = str(item.get("latest_json") or item.get("report_dir") or summary_json_path.resolve())
        case_records.append(
            EvidenceCaseRecord(
                case_id=str(item.get("profile_id", "")),
                name=str(item.get("profile_id", "")),
                suite_ref="performance",
                owner_scope="runtime-execution",
                primary_layer="L6",
                producer_lane_ref="nightly-performance",
                status=status,
                evidence_profile="soak-profile",
                stability_state="stable",
                required_assets=linked_assets,
                required_fixtures=("fixture.validation-evidence-bundle",),
                risk_tags=("performance", "soak"),
                note=str(item.get("note", "")),
                trace_fields=trace_fields(
                    stage_id="L6",
                    artifact_id=str(item.get("profile_id", "")),
                    module_id="runtime-execution",
                    workflow_state="executed",
                    execution_state=status,
                    event_name="online-soak-profiles",
                    failure_code="" if status == "passed" else f"soak.{status}",
                    evidence_path=evidence_path,
                ),
            )
        )

    bundle = EvidenceBundle(
        bundle_id=f"online-soak-profiles-{int(time.time())}",
        request_ref="online-soak-profiles",
        producer_lane_ref="nightly-performance",
        report_root=str(report_dir.resolve()),
        summary_file=str(summary_md_path.resolve()),
        machine_file=str(summary_json_path.resolve()),
        verdict=str(payload.get("overall_status", "incomplete")),
        case_records=case_records,
        linked_asset_refs=linked_assets,
        offline_prerequisites=("L0", "L1", "L2", "L3", "L4", "L5"),
        skip_justification=(
            "soak profiles are G8 supplementary evidence and do not replace the controlled HIL signed publish path"
        ),
        metadata={
            "selected_profile_ids": payload.get("selected_profile_ids", []),
            "signed_publish_authority": payload.get("signed_publish_authority", ""),
            "production_baseline": payload.get("production_baseline", {}),
        },
    )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=summary_json_path,
        summary_md_path=summary_md_path,
        evidence_links=[
            EvidenceLink(
                label="online-soak-profiles-summary.json",
                path=str(summary_json_path.resolve()),
                role="machine-summary",
            ),
            EvidenceLink(
                label="online-soak-profiles-summary.md",
                path=str(summary_md_path.resolve()),
                role="human-summary",
            ),
        ],
    )


def main() -> int:
    args = parse_args()
    selected_profiles = resolve_profiles(tuple(args.profile_id))
    if any(profile.long_run_minutes > 30.0 for profile in selected_profiles) and not args.allow_long_profiles:
        raise SystemExit(
            "extended soak profiles (>30m) require --allow-long-profiles to avoid accidental long-running execution"
        )
    report_dir = args.report_dir / time.strftime("%Y%m%d-%H%M%S")
    profile_results = [_run_profile(args=args, suite_report_dir=report_dir, profile=profile) for profile in selected_profiles]
    production_baseline: dict[str, str] = {}
    for item in profile_results:
        if not item.latest_json:
            continue
        latest_json_path = Path(item.latest_json)
        if not latest_json_path.exists():
            continue
        baseline = json.loads(latest_json_path.read_text(encoding="utf-8")).get("production_baseline", {})
        if isinstance(baseline, dict) and baseline:
            production_baseline = baseline
            break
    json_path, md_path = _write_report(
        report_dir=report_dir,
        profile_results=profile_results,
        selected_profile_ids=tuple(profile.profile_id for profile in selected_profiles),
        production_baseline=production_baseline,
    )
    overall_status = json.loads(json_path.read_text(encoding="utf-8")).get("overall_status", "failed")
    print(f"online soak profiles complete: status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
