from __future__ import annotations

import argparse
import json
import os
import sys
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, cast


os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

from PyQt5.QtWidgets import QApplication, QWidget


ROOT = Path(__file__).resolve().parents[3]
HMI_SRC = ROOT / "apps" / "hmi-app" / "src" / "hmi_client"
TEST_KIT_SRC = ROOT / "shared" / "testing" / "test-kit" / "src"
if str(HMI_SRC) not in sys.path:
    sys.path.insert(0, str(HMI_SRC))
if str(TEST_KIT_SRC) not in sys.path:
    sys.path.insert(0, str(TEST_KIT_SRC))

import ui.main_window as main_window_module
from test_kit.evidence_bundle import (  # noqa: E402
    EvidenceBundle,
    EvidenceCaseRecord,
    EvidenceLink,
    default_deterministic_replay,
    infer_verdict,
    trace_fields,
    write_bundle_artifacts,
)


@dataclass
class CaseResult:
    case_id: str
    title: str
    status: str
    sample_id: str
    detail: dict[str, Any]
    note: str = ""


class _DummyPage:
    def setBackgroundColor(self, *_args, **_kwargs) -> None:
        return None


class _FakePreviewView(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.html = ""
        self._page = _DummyPage()

    def page(self) -> _DummyPage:
        return self._page

    def setHtml(self, html: str) -> None:
        self.html = html


class _Signal:
    def __init__(self) -> None:
        self._callbacks: list[Callable[..., None]] = []

    def connect(self, callback: Callable[..., None]) -> None:
        self._callbacks.append(callback)

    def emit(self, *args: Any) -> None:
        for callback in list(self._callbacks):
            callback(*args)


class _FakePreviewSnapshotWorker:
    created: list["_FakePreviewSnapshotWorker"] = []

    def __init__(
        self,
        *,
        host: str,
        port: int,
        artifact_id: str,
        speed_mm_s: float,
        dry_run: bool,
        dry_run_speed_mm_s: float,
    ) -> None:
        self.host = host
        self.port = port
        self.artifact_id = artifact_id
        self.speed_mm_s = speed_mm_s
        self.dry_run = dry_run
        self.dry_run_speed_mm_s = dry_run_speed_mm_s
        self.completed = _Signal()
        self.finished = _Signal()
        type(self).created.append(self)

    def start(self) -> None:
        payload = {
            "snapshot_id": "snapshot-int",
            "snapshot_hash": "hash-int",
            "plan_id": "plan-int",
            "production_baseline": {
                "baseline_id": "baseline-int",
                "baseline_fingerprint": "baseline-fingerprint-int",
            },
            "preview_source": "planned_glue_snapshot",
            "preview_kind": "glue_points",
            "segment_count": 2,
            "glue_point_count": 2,
            "glue_points": [{"x": 0.0, "y": 0.0}, {"x": 10.0, "y": 0.0}],
            "glue_reveal_lengths_mm": [0.0, 10.0],
            "motion_preview": {
                "source": "execution_trajectory_snapshot",
                "kind": "polyline",
                "source_point_count": 8,
                "point_count": 2,
                "is_sampled": True,
                "sampling_strategy": "execution_trajectory_geometry_preserving_clamp",
                "polyline": [{"x": 0.0, "y": 0.0}, {"x": 10.0, "y": 0.0}],
            },
            "total_length_mm": 10.0,
            "estimated_time_s": 0.5,
            "generated_at": "2026-04-02T00:00:00Z",
            "dry_run": False,
        }
        self.completed.emit(True, payload, "")
        self.finished.emit()

    def deleteLater(self) -> None:
        return None


class _FakeProtocol:
    def __init__(self) -> None:
        self.calls: list[tuple[object, ...]] = []

    def dxf_create_artifact(self, filepath: str):
        self.calls.append(("dxf.artifact.create", filepath))
        return True, {"artifact_id": "artifact-int", "segment_count": 7}, ""

    def dxf_get_info(self):
        self.calls.append(("dxf.info",))
        return {"total_length": 12.5, "total_segments": 7}


class _FakeClient:
    def __init__(self, host: str = "127.0.0.1", port: int = 9527) -> None:
        self.host = host
        self.port = port


def _run_online_preview_flow() -> CaseResult:
    app = QApplication.instance() or QApplication([])
    _FakePreviewSnapshotWorker.created.clear()

    original_web_view = getattr(main_window_module, "QWebEngineView", None)
    original_web_engine_flag = getattr(main_window_module, "WEB_ENGINE_AVAILABLE", False)
    original_worker = main_window_module.PreviewSnapshotWorker

    main_window_module.QWebEngineView = _FakePreviewView
    main_window_module.WEB_ENGINE_AVAILABLE = True
    main_window_module.PreviewSnapshotWorker = _FakePreviewSnapshotWorker

    window = cast(Any, main_window_module.MainWindow(launch_mode="offline"))
    try:
        window._requested_launch_mode = "online"
        window._launch_result = None
        window._session_snapshot = None
        window._require_online_mode = lambda _capability: True
        window._protocol = _FakeProtocol()
        window._client = _FakeClient()
        window._connected = True
        window._mode_production.setChecked(True)
        window._mode_dryrun.setChecked(False)
        window._dxf_filepath = str(ROOT / "samples" / "dxf" / "rect_diag.dxf")

        window._on_dxf_load()
        worker = _FakePreviewSnapshotWorker.created[0]
        payload = {
            "artifact_id": window._dxf_artifact_id,
            "plan_id": window._current_plan_id,
            "snapshot_hash": window._current_plan_fingerprint,
            "preview_source": window._preview_source,
            "preview_kind": window._preview_session.state.preview_kind,
            "glue_point_count": window._preview_session.state.glue_point_count,
            "session_plan_id": window._preview_session.state.current_plan_id,
            "session_plan_fingerprint": window._preview_session.state.current_plan_fingerprint,
            "gate_snapshot_hash": window._preview_gate.snapshot.snapshot_hash,
            "status_message": cast(Any, window.statusBar()).currentMessage(),
            "debug_contains_hash": "hash-int" in window._preview_debug_view.toPlainText(),
            "filename_display": window._dxf_filename_display.text(),
            "html_contains_playback_overlay": "id='preview-head'" in cast(Any, window._dxf_view).html,
            "protocol_calls": cast(Any, window._protocol).calls,
            "worker_artifact_id": worker.artifact_id,
            "offline_payload": {
                "motion_preview_source": window._preview_session.state.motion_preview_source,
                "motion_preview_sampling_strategy": window._preview_session.state.motion_preview_sampling_strategy,
                "motion_preview_point_count": window._preview_session.state.motion_preview_point_count,
            },
        }

        assert window._dxf_loaded
        assert payload["artifact_id"] == "artifact-int"
        assert payload["plan_id"] == "plan-int"
        assert payload["snapshot_hash"] == "hash-int"
        assert payload["preview_source"] == "planned_glue_snapshot"
        assert payload["preview_kind"] == "glue_points"
        assert payload["glue_point_count"] == 2
        assert payload["session_plan_id"] == "plan-int"
        assert payload["session_plan_fingerprint"] == "hash-int"
        assert payload["gate_snapshot_hash"] == "hash-int"
        assert payload["status_message"] == "胶点预览已更新，启动前需确认"
        assert payload["debug_contains_hash"] is True
        assert payload["filename_display"] == "rect_diag.dxf"
        assert payload["html_contains_playback_overlay"] is True
        assert payload["worker_artifact_id"] == "artifact-int"
        assert payload["protocol_calls"] == [
            ("dxf.artifact.create", str(ROOT / "samples" / "dxf" / "rect_diag.dxf")),
            ("dxf.info",),
        ]
        assert payload["offline_payload"]["motion_preview_source"] == "execution_trajectory_snapshot"
        assert payload["offline_payload"]["motion_preview_sampling_strategy"] == "execution_trajectory_geometry_preserving_clamp"
        assert payload["offline_payload"]["motion_preview_point_count"] == 2
        assert window._preview_snapshot_worker is None
        assert window._preview_refresh_inflight is False

        return CaseResult(
            case_id="dxf-import-preview-state-flow",
            title="DXF import -> preview snapshot -> app state flow",
            status="passed",
            sample_id="sample.dxf.rect_diag",
            detail=payload,
        )
    finally:
        window.close()
        window.deleteLater()
        main_window_module.QWebEngineView = original_web_view
        main_window_module.WEB_ENGINE_AVAILABLE = original_web_engine_flag
        main_window_module.PreviewSnapshotWorker = original_worker
        app.processEvents()


def _run_offline_preview_block() -> CaseResult:
    app = QApplication.instance() or QApplication([])
    _FakePreviewSnapshotWorker.created.clear()

    original_web_view = getattr(main_window_module, "QWebEngineView", None)
    original_web_engine_flag = getattr(main_window_module, "WEB_ENGINE_AVAILABLE", False)
    original_worker = main_window_module.PreviewSnapshotWorker

    main_window_module.QWebEngineView = _FakePreviewView
    main_window_module.WEB_ENGINE_AVAILABLE = True
    main_window_module.PreviewSnapshotWorker = _FakePreviewSnapshotWorker

    window = cast(Any, main_window_module.MainWindow(launch_mode="offline"))
    try:
        window._protocol = _FakeProtocol()
        window._mode_production.setChecked(True)
        window._mode_dryrun.setChecked(False)
        window._dxf_filepath = str(ROOT / "samples" / "dxf" / "rect_diag.dxf")

        window._on_dxf_load()
        payload = {
            "dxf_loaded": window._dxf_loaded,
            "artifact_id": window._dxf_artifact_id,
            "plan_id": window._current_plan_id,
            "snapshot_hash": window._current_plan_fingerprint,
            "preview_source": window._preview_source,
            "preview_kind": window._preview_session.state.preview_kind,
            "glue_point_count": window._preview_session.state.glue_point_count,
            "motion_preview_source": window._preview_session.state.motion_preview_source,
            "status_message": cast(Any, window.statusBar()).currentMessage(),
            "protocol_calls": cast(Any, window._protocol).calls,
            "html_contains_block_message": "当前生产预览链仅支持在线 gateway 快照" in cast(Any, window._dxf_view).html,
        }

        assert payload["dxf_loaded"] is False
        assert payload["artifact_id"] == ""
        assert payload["plan_id"] == ""
        assert payload["snapshot_hash"] == ""
        assert payload["preview_source"] == ""
        assert payload["preview_kind"] == ""
        assert payload["glue_point_count"] == 0
        assert payload["motion_preview_source"] == ""
        assert payload["status_message"] == "当前生产预览链仅支持在线 gateway 快照"
        assert payload["protocol_calls"] == []
        assert payload["html_contains_block_message"] is True
        assert window._preview_snapshot_worker is None
        assert window._preview_refresh_inflight is False
        assert not _FakePreviewSnapshotWorker.created

        return CaseResult(
            case_id="offline-dxf-preview-block-contract",
            title="Offline DXF load blocks preview contract",
            status="passed",
            sample_id="sample.dxf.rect_diag",
            detail=payload,
        )
    finally:
        window.close()
        window.deleteLater()
        main_window_module.QWebEngineView = original_web_view
        main_window_module.WEB_ENGINE_AVAILABLE = original_web_engine_flag
        main_window_module.PreviewSnapshotWorker = original_worker
        app.processEvents()


def _write_report(report_dir: Path, results: list[CaseResult], overall_status: str) -> tuple[Path, Path]:
    report_dir.mkdir(parents=True, exist_ok=True)
    json_path = report_dir / "preview-flow-summary.json"
    md_path = report_dir / "preview-flow-summary.md"

    payload = {
        "generated_at": datetime.now(timezone.utc).isoformat(),
        "workspace_root": str(ROOT),
        "overall_status": overall_status,
        "results": [asdict(item) for item in results],
    }
    json_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")

    lines = [
        "# Preview Flow Summary",
        "",
        f"- overall_status: `{overall_status}`",
        "",
        "## Cases",
        "",
    ]
    for item in results:
        lines.append(f"- `{item.status}` `{item.case_id}` sample=`{item.sample_id}`")
        if item.note:
            lines.append(f"  note: {item.note}")
        lines.append("  ```json")
        lines.extend(f"  {line}" for line in json.dumps(item.detail, ensure_ascii=False, indent=2).splitlines())
        lines.append("  ```")
    md_path.write_text("\n".join(lines), encoding="utf-8")
    return json_path, md_path


def _write_bundle(report_dir: Path, json_path: Path, md_path: Path, results: list[CaseResult], overall_status: str) -> None:
    bundle = EvidenceBundle(
        bundle_id="preview-flow-integration",
        request_ref="preview-flow-integration",
        producer_lane_ref="full-offline-gate",
        report_root=str(report_dir.resolve()),
        summary_file=str(md_path.resolve()),
        machine_file=str(json_path.resolve()),
        verdict=infer_verdict([item.status for item in results]),
        linked_asset_refs=("sample.dxf.rect_diag",),
        metadata={"overall_status": overall_status},
        case_records=[
            EvidenceCaseRecord(
                case_id=item.case_id,
                name=item.title,
                suite_ref="integration",
                owner_scope="tests/integration",
                primary_layer="L3",
                producer_lane_ref="full-offline-gate",
                status=item.status,
                evidence_profile="integration-report",
                stability_state="stable",
                size_label="medium",
                label_refs=("suite:integration", "kind:integration", "size:medium", "layer:L3"),
                required_assets=("sample.dxf.rect_diag",),
                required_fixtures=("fixture.validation-evidence-bundle",),
                deterministic_replay=default_deterministic_replay(
                    passed=item.status == "passed",
                    seed=0,
                    clock_profile="fixture",
                    repeat_count=1,
                ),
                note=item.note,
                trace_fields=trace_fields(
                    stage_id="L3",
                    artifact_id=item.case_id,
                    module_id="tests/integration",
                    workflow_state="executed",
                    execution_state=item.status,
                    event_name=item.title,
                    failure_code="" if item.status == "passed" else f"preview-flow.{item.case_id}",
                    evidence_path=str(json_path.resolve()),
                ),
            )
            for item in results
        ],
    )
    write_bundle_artifacts(
        bundle=bundle,
        report_root=report_dir,
        summary_json_path=json_path,
        summary_md_path=md_path,
        evidence_links=[
            EvidenceLink(label="rect_diag.dxf", path=str((ROOT / "samples" / "dxf" / "rect_diag.dxf").resolve()), role="sample"),
        ],
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run preview flow integration regression and emit evidence.")
    parser.add_argument(
        "--report-dir",
        default=str(ROOT / "tests" / "reports" / "integration" / "preview-flow-integration"),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    report_dir = Path(args.report_dir).resolve()
    results: list[CaseResult] = []

    try:
        results.append(_run_online_preview_flow())
        results.append(_run_offline_preview_block())
        overall_status = "passed"
    except Exception as exc:
        overall_status = "failed"
        results.append(
            CaseResult(
                case_id="dxf-import-preview-state-flow",
                title="DXF import -> preview snapshot -> app state flow",
                status="failed",
                sample_id="sample.dxf.rect_diag",
                detail={},
                note=str(exc),
            )
        )
        print(f"preview flow integration failed: {exc}", file=sys.stderr)

    json_path, md_path = _write_report(report_dir, results, overall_status)
    _write_bundle(report_dir, json_path, md_path, results, overall_status)
    print(f"preview flow integration overall_status={overall_status}")
    print(f"json report: {json_path}")
    print(f"markdown report: {md_path}")
    return 0 if overall_status == "passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
