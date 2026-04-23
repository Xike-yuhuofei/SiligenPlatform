from __future__ import annotations

import threading
import time

from PyQt5.QtCore import QThread, pyqtSignal

from .startup_qt_workers import RecoveryWorker, StartupWorker

# DXF 打开后的自动预览允许更长等待窗口，但该预算不向 resync/confirm/job.start 扩散。
# 真实复杂 DXF 的 plan.prepare 在 mock 链路下可稳定超过 100s，因此自动预览预算提升到 300s 并与 TCP
# 复测窗口对齐，避免 HMI 先超时而后端仍在正常规划。
DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S = 300.0


class PreviewSnapshotWorker(QThread):
    completed = pyqtSignal(bool, object, str)

    def __init__(
        self,
        host: str,
        port: int,
        artifact_id: str,
        speed_mm_s: float,
        dry_run: bool,
        dry_run_speed_mm_s: float,
    ):
        super().__init__()
        self._host = host
        self._port = port
        self._artifact_id = artifact_id
        self._speed_mm_s = speed_mm_s
        self._dry_run = dry_run
        self._dry_run_speed_mm_s = dry_run_speed_mm_s
        self._cancel_lock = threading.Lock()
        self._cancelled = False
        self._client_ref = None

    def cancel(self) -> None:
        with self._cancel_lock:
            self._cancelled = True
            client = self._client_ref
        if client is not None:
            try:
                client.disconnect()
            except Exception:
                pass

    def _is_cancelled(self) -> bool:
        with self._cancel_lock:
            return self._cancelled

    def run(self):
        client = None
        ok = False
        payload = {}
        error = ""
        worker_started_at = time.perf_counter()
        plan_prepare_elapsed_ms = 0
        snapshot_elapsed_ms = 0
        try:
            try:
                from hmi_client.client.tcp_client import TcpClient  # type: ignore
                from hmi_client.client.protocol import CommandProtocol  # type: ignore
            except ImportError:  # pragma: no cover - script-mode fallback
                from client.tcp_client import TcpClient  # type: ignore
                from client.protocol import CommandProtocol  # type: ignore

            if self._is_cancelled():
                return
            client = TcpClient(host=self._host, port=self._port)
            with self._cancel_lock:
                self._client_ref = client
            if not client.connect():
                error = "无法连接后端，请检查TCP链路"
            else:
                protocol = CommandProtocol(client)
                plan_prepare_started_at = time.perf_counter()
                plan_ok, plan_payload, plan_error = protocol.dxf_prepare_plan(
                    artifact_id=self._artifact_id,
                    speed_mm_s=self._speed_mm_s,
                    dry_run=self._dry_run,
                    dry_run_speed_mm_s=self._dry_run_speed_mm_s,
                    timeout=DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S,
                )
                plan_prepare_elapsed_ms = int(round((time.perf_counter() - plan_prepare_started_at) * 1000.0))
                if self._is_cancelled():
                    return
                if not plan_ok:
                    error = plan_error
                else:
                    plan_id = str(plan_payload.get("plan_id", "")).strip()
                    if not plan_id:
                        error = "plan.prepare 返回缺少 plan_id"
                    else:
                        snapshot_started_at = time.perf_counter()
                        ok, payload, error = protocol.dxf_preview_snapshot(
                            plan_id=plan_id,
                            max_polyline_points=4000,
                            max_glue_points=5000,
                            timeout=DXF_OPEN_AUTO_PREVIEW_TIMEOUT_S,
                        )
                        snapshot_elapsed_ms = int(round((time.perf_counter() - snapshot_started_at) * 1000.0))
                        if self._is_cancelled():
                            return
                        if ok and isinstance(payload, dict):
                            payload.setdefault("plan_id", plan_id)
                            plan_fingerprint = str(plan_payload.get("plan_fingerprint", ""))
                            payload.setdefault("plan_fingerprint", plan_fingerprint)
                            payload.setdefault("snapshot_hash", plan_fingerprint)
                            payload.setdefault("dry_run", bool(self._dry_run))
                            payload.setdefault(
                                "preview_validation_classification",
                                str(plan_payload.get("preview_validation_classification", "")),
                            )
                            payload.setdefault(
                                "preview_exception_reason",
                                str(plan_payload.get("preview_exception_reason", "")),
                            )
                            payload.setdefault(
                                "preview_failure_reason",
                                str(plan_payload.get("preview_failure_reason", "")),
                            )
                            payload.setdefault(
                                "preview_diagnostic_code",
                                str(plan_payload.get("preview_diagnostic_code", "")),
                            )
                            performance_profile = plan_payload.get("performance_profile")
                            if isinstance(performance_profile, dict):
                                payload.setdefault("performance_profile", dict(performance_profile))
                            payload["worker_profile"] = {
                                "plan_prepare_rpc_ms": plan_prepare_elapsed_ms,
                                "snapshot_rpc_ms": snapshot_elapsed_ms,
                                "worker_total_ms": int(round((time.perf_counter() - worker_started_at) * 1000.0)),
                            }
        except Exception as exc:  # pragma: no cover - defensive path
            error = str(exc) or "预览快照生成异常"
        finally:
            with self._cancel_lock:
                self._client_ref = None
            if client is not None:
                client.disconnect()
        if self._is_cancelled():
            return
        self.completed.emit(ok, payload if isinstance(payload, dict) else {}, error)
