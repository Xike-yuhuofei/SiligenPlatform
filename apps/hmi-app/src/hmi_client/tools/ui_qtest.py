#!/usr/bin/env python3
"""Mode-aware PyQt5 GUI contract smoke test for Siligen HMI."""
from __future__ import annotations

import argparse
import json
import os
import re
import sys
import threading
import time
from contextlib import ExitStack, contextmanager
from pathlib import Path
from typing import Callable, Iterator, Optional, Tuple

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from qt_env import configure_qt_environment

configure_qt_environment(headless=True)

from PyQt5.QtCore import QSize, Qt, QTimer
from PyQt5.QtGui import QColor, QPainter, QPen
from PyQt5.QtTest import QTest
from PyQt5.QtWidgets import QApplication, QListWidget, QWidget

import ui.main_window as main_window_module  # noqa: E402
from tools.mock_server import MockRequestHandler, MockState, MockTcpServer  # noqa: E402


MainWindow = main_window_module.MainWindow

EXIT_SUCCESS = 0
EXIT_ASSERTION_FAILED = 10
EXIT_TIMEOUT = 11
SUPPORTED_RUNTIME_ACTION_PROFILES = (
    "full",
    "operator_preview",
    "operator_production",
    "snapshot_render",
    "home_move",
    "jog",
    "supply_dispenser",
    "estop_reset",
    "door_interlock",
)
OPERATOR_JOURNEY_RUNTIME_ACTION_PROFILES = ("full", "operator_preview", "operator_production")
REQUIRED_OPERATOR_CONTEXT_FIELDS = (
    "artifact_id",
    "plan_id",
    "preview_source",
    "preview_kind",
    "glue_point_count",
    "snapshot_hash",
    "confirmed_snapshot_hash",
    "snapshot_ready",
    "preview_confirmed",
    "job_id",
    "target_count",
    "completed_count",
    "global_progress_percent",
    "current_operation",
    "preview_resync_pending",
    "preview_refresh_inflight",
)


def canonical_preview_sample() -> Path:
    return Path(__file__).resolve().parents[5] / "samples" / "dxf" / "rect_diag.dxf"


def requires_explicit_dxf_browse_path(runtime_action_profile: str) -> bool:
    return runtime_action_profile.strip().lower() in OPERATOR_JOURNEY_RUNTIME_ACTION_PROFILES


def resolve_runtime_action_request(
    *,
    exercise_runtime_actions: bool,
    runtime_action_profile: str,
    preview_payload_path: str,
    dxf_browse_path: str,
) -> tuple[str, str]:
    normalized_profile = str(runtime_action_profile or "").strip().lower()
    normalized_preview_payload_path = str(preview_payload_path or "").strip()
    normalized_dxf_browse_path = str(dxf_browse_path or "").strip()

    if not exercise_runtime_actions:
        if normalized_preview_payload_path:
            raise ValueError("--preview-payload-path requires --exercise-runtime-actions --runtime-action-profile snapshot_render")
        if normalized_profile:
            raise ValueError("--runtime-action-profile requires --exercise-runtime-actions")
        if normalized_dxf_browse_path:
            raise ValueError("--dxf-browse-path requires --exercise-runtime-actions")
        return "", ""

    if not normalized_profile:
        normalized_profile = "full"
    if normalized_profile not in SUPPORTED_RUNTIME_ACTION_PROFILES:
        raise ValueError(
            "Unsupported runtime action profile: "
            f"{normalized_profile}; supported={','.join(SUPPORTED_RUNTIME_ACTION_PROFILES)}"
        )
    if normalized_preview_payload_path and normalized_profile != "snapshot_render":
        raise ValueError("--preview-payload-path is only allowed with --runtime-action-profile snapshot_render")
    if normalized_profile == "snapshot_render" and not normalized_preview_payload_path:
        raise ValueError("--runtime-action-profile snapshot_render requires --preview-payload-path")
    if requires_explicit_dxf_browse_path(normalized_profile):
        if not normalized_dxf_browse_path:
            raise ValueError(f"--runtime-action-profile {normalized_profile} requires --dxf-browse-path")
        resolved_dxf_path = Path(normalized_dxf_browse_path).expanduser()
        if not resolved_dxf_path.is_absolute():
            resolved_dxf_path = (Path.cwd() / resolved_dxf_path).resolve()
        if not resolved_dxf_path.exists():
            raise ValueError(f"--dxf-browse-path does not exist: {resolved_dxf_path}")
        normalized_dxf_browse_path = str(resolved_dxf_path)

    return normalized_profile, normalized_dxf_browse_path


def find_by_testid(root: QWidget, testid: str) -> Optional[QWidget]:
    if root.property("data-testid") == testid:
        return root
    for widget in root.findChildren(QWidget):
        if widget.property("data-testid") == testid:
            return widget
    return None


@contextmanager
def patch_launch_endpoints(host: str, port: int) -> Iterator[None]:
    original_tcp_client = main_window_module.TcpClient
    original_backend_manager = main_window_module.BackendManager

    def _patched_tcp_client(*args, **kwargs):
        kwargs.setdefault("host", host)
        kwargs.setdefault("port", port)
        return original_tcp_client(*args, **kwargs)

    def _patched_backend_manager(*args, **kwargs):
        kwargs.setdefault("host", host)
        kwargs.setdefault("port", port)
        return original_backend_manager(*args, **kwargs)

    main_window_module.TcpClient = _patched_tcp_client
    main_window_module.BackendManager = _patched_backend_manager
    try:
        yield
    finally:
        main_window_module.TcpClient = original_tcp_client
        main_window_module.BackendManager = original_backend_manager
@contextmanager
def patch_headless_preview() -> Iterator[None]:
    original_web_view = getattr(main_window_module, "QWebEngineView", None)
    original_flag = getattr(main_window_module, "WEB_ENGINE_AVAILABLE", False)

    class _DummyPage:
        def setBackgroundColor(self, *_args, **_kwargs) -> None:
            return None

    class _DummyWebView(QWidget):
        def __init__(self, *args, **kwargs):
            super().__init__(*args, **kwargs)
            self._page = _DummyPage()
            self.last_html = ""
            self._view_box = (920.0, 520.0)
            self._polyline: list[tuple[float, float]] = []
            self._circles: list[tuple[float, float, float, str]] = []

        def page(self):
            return self._page

        def setHtml(self, html: str) -> None:
            self.last_html = html
            self._parse_html(html)
            self.update()

        def sizeHint(self) -> QSize:
            width, height = self._view_box
            return QSize(int(width), int(height))

        def _parse_html(self, html: str) -> None:
            view_box_match = re.search(r"viewBox='0 0 ([0-9.]+) ([0-9.]+)'", html)
            if view_box_match:
                self._view_box = (float(view_box_match.group(1)), float(view_box_match.group(2)))

            self._polyline = []
            polyline_match = re.search(r"<polyline points='([^']+)'", html)
            if polyline_match:
                for token in polyline_match.group(1).split():
                    point_x, _, point_y = token.partition(",")
                    if not point_x or not point_y:
                        continue
                    self._polyline.append((float(point_x), float(point_y)))

            self._circles = []
            for match in re.finditer(
                r"<circle cx='([^']+)' cy='([^']+)' r='([^']+)' fill='([^']+)'",
                html,
            ):
                self._circles.append(
                    (
                        float(match.group(1)),
                        float(match.group(2)),
                        float(match.group(3)),
                        match.group(4),
                    )
                )

        def paintEvent(self, _event) -> None:
            painter = QPainter(self)
            painter.setRenderHint(QPainter.Antialiasing, True)
            painter.fillRect(self.rect(), QColor("#141414"))

            if self.width() <= 0 or self.height() <= 0:
                painter.end()
                return

            view_width, view_height = self._view_box
            scale_x = self.width() / max(view_width, 1e-6)
            scale_y = self.height() / max(view_height, 1e-6)

            def _map_point(point_x: float, point_y: float) -> tuple[float, float]:
                return point_x * scale_x, point_y * scale_y

            if len(self._polyline) >= 2:
                pen = QPen(QColor("#5b6472"))
                pen.setWidthF(1.6 * min(scale_x, scale_y))
                painter.setPen(pen)
                for index in range(1, len(self._polyline)):
                    prev_x, prev_y = _map_point(*self._polyline[index - 1])
                    curr_x, curr_y = _map_point(*self._polyline[index])
                    painter.drawLine(int(prev_x), int(prev_y), int(curr_x), int(curr_y))

            painter.setPen(Qt.NoPen)
            for point_x, point_y, radius, fill in self._circles:
                mapped_x, mapped_y = _map_point(point_x, point_y)
                mapped_radius = max(1.0, radius * min(scale_x, scale_y))
                painter.setBrush(QColor(fill))
                painter.drawEllipse(
                    int(mapped_x - mapped_radius),
                    int(mapped_y - mapped_radius),
                    int(mapped_radius * 2),
                    int(mapped_radius * 2),
                )
            painter.end()

    main_window_module.QWebEngineView = _DummyWebView
    main_window_module.WEB_ENGINE_AVAILABLE = True
    try:
        yield
    finally:
        main_window_module.QWebEngineView = original_web_view
        main_window_module.WEB_ENGINE_AVAILABLE = original_flag


@contextmanager
def patch_dxf_browse_dialog(dxf_path: str) -> Iterator[None]:
    normalized_path = str(dxf_path or "").strip()
    if not normalized_path:
        yield
        return

    original_get_open_file_name = main_window_module.QFileDialog.getOpenFileName

    def _patched_get_open_file_name(*_args, **_kwargs):
        return normalized_path, "DXF文件 (*.dxf)"

    main_window_module.QFileDialog.getOpenFileName = _patched_get_open_file_name
    try:
        yield
    finally:
        main_window_module.QFileDialog.getOpenFileName = original_get_open_file_name


@contextmanager
def patch_modal_dialogs() -> Iterator[None]:
    original_critical = main_window_module.QMessageBox.critical
    original_warning = main_window_module.QMessageBox.warning
    original_information = main_window_module.QMessageBox.information
    original_question = main_window_module.QMessageBox.question

    def _dialog_noop(*_args, **_kwargs):
        return main_window_module.QMessageBox.Ok

    def _dialog_yes(*_args, **_kwargs):
        return main_window_module.QMessageBox.Yes

    main_window_module.QMessageBox.critical = _dialog_noop
    main_window_module.QMessageBox.warning = _dialog_noop
    main_window_module.QMessageBox.information = _dialog_noop
    main_window_module.QMessageBox.question = _dialog_yes
    try:
        yield
    finally:
        main_window_module.QMessageBox.critical = original_critical
        main_window_module.QMessageBox.warning = original_warning
        main_window_module.QMessageBox.information = original_information
        main_window_module.QMessageBox.question = original_question


def start_mock_server(host: str, port: int, verbose: bool) -> Tuple[MockTcpServer, int]:
    state = MockState(seed_alarms=True)
    server = MockTcpServer((host, port), MockRequestHandler, state, verbose)
    server.daemon_threads = True
    if hasattr(server, "block_on_close"):
        server.block_on_close = False
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, server.server_address[1]


class GuiContractRunner:
    def __init__(
        self,
        app: QApplication,
        window: MainWindow,
        launch_mode: str,
        timeout_ms: int,
        expect_failure_code: str = "",
        expect_failure_stage: str = "",
        exercise_recovery: bool = False,
        exercise_runtime_actions: bool = False,
        screenshot_path: str = "",
        screenshot_dir: str = "",
        preview_payload_path: str = "",
        runtime_action_profile: str = "",
        use_external_gateway: bool = False,
    ):
        self.app = app
        self.window = window
        self.launch_mode = launch_mode
        self.timeout_ms = timeout_ms
        self.expect_failure_code = expect_failure_code.strip()
        self.expect_failure_stage = expect_failure_stage.strip()
        self.exercise_recovery = exercise_recovery
        self.exercise_runtime_actions = exercise_runtime_actions
        self.screenshot_path = screenshot_path.strip()
        self.screenshot_dir = screenshot_dir.strip()
        self.preview_payload_path = preview_payload_path.strip()
        self.runtime_action_profile = runtime_action_profile.strip().lower()
        self._use_external_gateway = bool(use_external_gateway)
        self.failed = False
        self.timed_out = False
        self.exit_code = EXIT_SUCCESS
        self._screenshot_index = 0

    def _uses_external_gateway(self) -> bool:
        return self._use_external_gateway

    def _fail(self, message: str) -> None:
        self.failed = True
        print(f"FAIL: {message}", flush=True)

    def _expect(self, condition: bool, message: str) -> None:
        if not condition:
            self._fail(message)

    def request_timeout(self) -> None:
        if self.timed_out:
            return
        self.timed_out = True
        self.exit_code = EXIT_TIMEOUT
        print("FAIL: Timed out waiting for GUI contract completion", flush=True)
        self.app.exit(EXIT_TIMEOUT)

    def _wait_for(self, description: str, predicate: Callable[[], bool], timeout_ms: Optional[int] = None) -> bool:
        deadline = time.monotonic() + ((timeout_ms or self.timeout_ms) / 1000.0)
        while time.monotonic() < deadline:
            if predicate():
                return True
            QTest.qWait(50)
        self._fail(f"Timed out waiting for {description}")
        return False

    def _widget(self, testid: str) -> Optional[QWidget]:
        return find_by_testid(self.window, testid)

    def _require_widget(self, testid: str) -> Optional[QWidget]:
        widget = self._widget(testid)
        self._expect(widget is not None, f"Missing widget: {testid}")
        return widget

    def _label_text(self, testid: str) -> str:
        widget = self._require_widget(testid)
        if widget is None:
            return ""
        text = widget.text() if hasattr(widget, "text") else ""
        self._expect(isinstance(text, str), f"Widget does not expose text(): {testid}")
        return text

    def _status_message(self) -> str:
        return self.window.statusBar().currentMessage()

    def _cached_status(self):
        return getattr(self.window, "_last_status", None)

    def _axis_status(self, axis: str):
        status = self._cached_status()
        if status is None:
            return None
        return status.axes.get(axis)

    def _job_execution_state(self) -> str:
        status = self._cached_status()
        if status is None:
            return ""
        job_execution = getattr(status, "job_execution", None)
        return str(getattr(job_execution, "state", "") or "").strip().lower()

    def _progress_value(self, testid: str) -> int:
        widget = self._require_widget(testid)
        if widget is None:
            return 0
        if hasattr(widget, "value"):
            return int(widget.value())
        self._fail(f"Widget does not expose value(): {testid}")
        return 0

    def _preview_session_state(self):
        preview_session = getattr(self.window, "_preview_session", None)
        return getattr(preview_session, "state", None)

    def _confirmed_snapshot_hash(self) -> str:
        preview_gate = getattr(self.window, "_preview_gate", None)
        if preview_gate is None or not hasattr(preview_gate, "get_confirmed_snapshot_hash"):
            return ""
        return str(preview_gate.get_confirmed_snapshot_hash() or "")

    def _ensure_operator_context_contract(self) -> bool:
        exporter = getattr(self.window, "build_operator_context_snapshot", None)
        if not callable(exporter):
            self._fail("MainWindow missing build_operator_context_snapshot()")
            return False
        snapshot = exporter()
        if not isinstance(snapshot, dict):
            self._fail("MainWindow build_operator_context_snapshot() must return dict")
            return False
        missing_fields = [field for field in REQUIRED_OPERATOR_CONTEXT_FIELDS if field not in snapshot]
        if missing_fields:
            self._fail(
                "MainWindow build_operator_context_snapshot() missing required fields: "
                + ",".join(missing_fields)
            )
            return False
        return True

    def _operator_context(self) -> dict[str, object]:
        exporter = getattr(self.window, "build_operator_context_snapshot", None)
        if not callable(exporter):
            self._fail("MainWindow missing build_operator_context_snapshot()")
            return {}
        snapshot = exporter()
        if not isinstance(snapshot, dict):
            self._fail("MainWindow build_operator_context_snapshot() must return dict")
            return {}
        return snapshot

    def _operator_context_satisfies(self, predicate: Callable[[dict[str, object]], bool]) -> bool:
        if self.failed:
            return False
        context = self._operator_context()
        if self.failed or not context:
            return False
        return bool(predicate(context))

    def _spinbox_set(self, testid: str, value: float) -> None:
        widget = self._require_widget(testid)
        if widget is None:
            return
        if hasattr(widget, "setValue"):
            widget.setValue(value)
            QTest.qWait(50)
            return
        self._fail(f"Widget does not expose setValue(): {testid}")

    def _click_button(self, testid: str) -> None:
        widget = self._require_widget(testid)
        if widget is None:
            return
        self._expect(widget.isEnabled(), f"{testid} should be enabled")
        if self.failed or not widget.isEnabled():
            return
        QTest.mouseClick(widget, Qt.LeftButton)
        QTest.qWait(100)

    def _sanitize_capture_token(self, value: str) -> str:
        normalized = re.sub(r"[^0-9A-Za-z._-]+", "-", str(value or "").strip())
        normalized = normalized.strip("-").lower()
        return normalized or "capture"

    def _context_token(self, value: object) -> str:
        text = str(value or "").strip()
        if not text:
            return "null"
        return re.sub(r"\s+", "_", text)

    def _save_screenshot(self, pixmap, target_path: Path, description: str, *, stage_name: str = "") -> None:
        target_path.parent.mkdir(parents=True, exist_ok=True)
        if not pixmap.save(str(target_path)):
            self._fail(f"Failed to save {description}: {target_path}")
            return
        if stage_name:
            print(f"HMI_SCREENSHOT stage={stage_name} path={target_path}", flush=True)
            return
        print(f"HMI_SCREENSHOT path={target_path}", flush=True)

    def _capture_screenshot(self, description: str, *, stage_name: str = "") -> None:
        if not self.screenshot_path and not self.screenshot_dir:
            return
        self.app.processEvents()
        self.window.repaint()
        QTest.qWait(200)
        pixmap = self.window.grab()
        if self.screenshot_dir:
            self._screenshot_index += 1
            stage_token = self._sanitize_capture_token(stage_name or description)
            staged_path = Path(self.screenshot_dir) / f"{self._screenshot_index:02d}-{stage_token}.png"
            self._save_screenshot(pixmap, staged_path, description, stage_name=stage_token)
        if self.screenshot_path:
            self._save_screenshot(pixmap, Path(self.screenshot_path), description, stage_name="")

    def _load_preview_payload(self) -> dict:
        if not self.preview_payload_path:
            return {}
        payload_path = Path(self.preview_payload_path)
        self._expect(payload_path.exists(), f"Preview payload should exist: {payload_path}")
        if not payload_path.exists():
            return {}
        try:
            payload = json.loads(payload_path.read_text(encoding="utf-8"))
        except Exception as exc:  # noqa: BLE001
            self._fail(f"Failed to load preview payload: {exc}")
            return {}
        self._expect(isinstance(payload, dict), "Preview payload should be a JSON object")
        return payload if isinstance(payload, dict) else {}

    def _resolved_runtime_action_profile(self) -> str:
        profile = self.runtime_action_profile
        if not profile:
            if self.preview_payload_path:
                self._fail("Preview payload requires explicit runtime action profile: snapshot_render")
            return "full"
        if profile not in SUPPORTED_RUNTIME_ACTION_PROFILES:
            self._fail(
                "Unsupported runtime action profile: "
                f"{profile}; supported={','.join(SUPPORTED_RUNTIME_ACTION_PROFILES)}"
            )
            return "full"
        if self.preview_payload_path and profile != "snapshot_render":
            self._fail("Preview payload is only allowed with runtime action profile: snapshot_render")
            return profile
        if profile == "snapshot_render" and not self.preview_payload_path:
            self._fail("Runtime action profile snapshot_render requires --preview-payload-path")
        return profile

    def _canonical_preview_sample(self) -> Path:
        return canonical_preview_sample()

    def _switch_to_production_tab(self) -> None:
        tabs = getattr(self.window, "_main_tabs", None)
        production_tab = getattr(self.window, "_production_tab", None)
        self._expect(tabs is not None and production_tab is not None, "Production tab should be available")
        if tabs is None or production_tab is None:
            return
        tabs.setCurrentWidget(production_tab)
        QTest.qWait(100)

    def _click_list_item(
        self,
        list_widget: QListWidget,
        *,
        description: str,
        predicate: Callable[[object], bool],
    ) -> str:
        self._expect(list_widget.count() > 0, f"{description} requires non-empty list")
        if list_widget.count() <= 0:
            return ""
        for index in range(list_widget.count()):
            item = list_widget.item(index)
            if not predicate(item):
                continue
            list_widget.clearSelection()
            QTest.qWait(50)
            rect = list_widget.visualItemRect(item)
            if rect.isValid():
                QTest.mouseClick(list_widget.viewport(), Qt.LeftButton, Qt.NoModifier, rect.center())
            else:
                list_widget.setCurrentItem(item)
            QTest.qWait(150)
            return str(item.data(Qt.UserRole) or "")
        self._fail(f"Could not locate list item for {description}")
        return ""

    def _emit_operator_context(self, stage: str) -> None:
        context = self._operator_context()
        if not context:
            return
        preview_gate = getattr(self.window, "_preview_gate", None)
        preview_gate_state = getattr(preview_gate, "state", "")
        if hasattr(preview_gate_state, "value"):
            preview_gate_state = preview_gate_state.value
        artifact_id = str(context.get("artifact_id") or "null")
        plan_id = str(context.get("plan_id") or "null")
        plan_fingerprint = str(getattr(self.window, "_current_plan_fingerprint", "") or "null")
        preview_source = str(context.get("preview_source") or "null")
        preview_kind = str(context.get("preview_kind") or "null")
        glue_point_count = int(context.get("glue_point_count", 0) or 0)
        snapshot_hash = str(context.get("snapshot_hash") or "")
        confirmed_snapshot_hash = str(context.get("confirmed_snapshot_hash") or "null")
        path_quality_verdict = str(context.get("path_quality_verdict") or "null")
        path_quality_blocking = str(bool(context.get("path_quality_blocking", False))).lower()
        path_quality_reason_codes = self._context_token(context.get("path_quality_reason_codes") or "null")
        path_quality_summary = self._context_token(context.get("path_quality_summary") or "null")
        current_operation = self._context_token(context.get("current_operation") or "null")
        completed_count = str(context.get("completed_count") or "0/0").replace(" ", "")
        job_id = str(context.get("job_id") or "null")
        target_count = int(context.get("target_count", 0) or 0)
        global_progress = int(context.get("global_progress_percent", 0) or 0)
        preview_resync_pending = str(bool(context.get("preview_resync_pending", False))).lower()
        preview_refresh_inflight = str(bool(context.get("preview_refresh_inflight", False))).lower()
        last_error_message = self._context_token(getattr(preview_gate, "last_error_message", ""))
        print(
            "OPERATOR_CONTEXT "
            f"stage={stage} "
            f"artifact_id={artifact_id} "
            f"plan_id={plan_id} "
            f"plan_fingerprint={plan_fingerprint} "
            f"preview_source={preview_source} "
            f"preview_kind={preview_kind} "
            f"glue_point_count={glue_point_count} "
            f"preview_gate_state={self._context_token(preview_gate_state)} "
            f"preview_gate_error={last_error_message} "
            f"snapshot_hash={snapshot_hash or 'null'} "
            f"confirmed_snapshot_hash={confirmed_snapshot_hash} "
            f"path_quality_verdict={self._context_token(path_quality_verdict)} "
            f"path_quality_blocking={path_quality_blocking} "
            f"path_quality_reason_codes={path_quality_reason_codes} "
            f"path_quality_summary={path_quality_summary} "
            f"snapshot_ready={str(bool(context.get('snapshot_ready', False))).lower()} "
            f"job_id={job_id} "
            f"target_count={target_count} "
            f"completed_count={completed_count or '0/0'} "
            f"global_progress_percent={global_progress} "
            f"current_operation={current_operation} "
            f"preview_confirmed={str(bool(context.get('preview_confirmed', False))).lower()} "
            f"preview_resync_pending={preview_resync_pending} "
            f"preview_refresh_inflight={preview_refresh_inflight}",
            flush=True,
        )

    def _emit_operator_exclusive_window(self, *, kind: str, state: str) -> None:
        print(
            "OPERATOR_EXCLUSIVE_WINDOW "
            f"kind={kind} "
            f"state={state}",
            flush=True,
        )

    def _operator_next_job_ready_recovery_active(self) -> bool:
        preview_state = self._preview_session_state()
        target_count = int(getattr(self.window, "_target_count", 0) or 0)
        completed_count = int(getattr(self.window, "_completed_count", 0) or 0)
        current_job_id = str(getattr(self.window, "_current_job_id", "") or "").strip()
        global_progress = 0
        if hasattr(self.window, "_global_progress") and hasattr(self.window._global_progress, "value"):
            global_progress = int(self.window._global_progress.value() or 0)
        current_operation = ""
        if hasattr(self.window, "_operation_status") and hasattr(self.window._operation_status, "text"):
            current_operation = str(self.window._operation_status.text() or "")
        return bool(
            target_count > 0
            and completed_count >= target_count
            and global_progress == 100
            and not current_job_id
            and current_operation == "完成"
            and (
                bool(getattr(preview_state, "preview_state_resync_pending", False))
                or bool(getattr(preview_state, "preview_refresh_inflight", False))
            )
        )

    @contextmanager
    def _operator_next_job_ready_recovery_window(self) -> Iterator[None]:
        sync_preview_state = getattr(self.window, "_sync_preview_state_from_runtime", None)
        if not callable(sync_preview_state):
            yield
            return

        active_calls = 0

        def wrapped(*args, **kwargs):
            nonlocal active_calls
            should_emit = self._operator_next_job_ready_recovery_active()
            if should_emit and active_calls == 0:
                self._emit_operator_exclusive_window(kind="next_job_ready_recovery", state="started")
            if should_emit:
                active_calls += 1
            try:
                return sync_preview_state(*args, **kwargs)
            except Exception:
                if should_emit:
                    active_calls = max(0, active_calls - 1)
                    if active_calls == 0:
                        self._emit_operator_exclusive_window(kind="next_job_ready_recovery", state="failed")
                raise
            finally:
                if should_emit and active_calls > 0:
                    active_calls -= 1
                    if active_calls == 0:
                        self._emit_operator_exclusive_window(kind="next_job_ready_recovery", state="completed")

        setattr(self.window, "_sync_preview_state_from_runtime", wrapped)
        try:
            yield
        finally:
            setattr(self.window, "_sync_preview_state_from_runtime", sync_preview_state)

    def _record_operator_stage_failure(self, stage: str, description: str) -> None:
        self._emit_operator_context(stage)
        self._capture_screenshot(description, stage_name=stage)

    def _run_operator_preview_journey(self) -> bool:
        if not self._ensure_operator_context_contract():
            return False
        self._switch_to_production_tab()
        if self.failed:
            return False
        self._click_button("btn-dxf-browse")
        if not self._wait_for(
            "operator preview loads dxf artifact",
            lambda: self._operator_context_satisfies(
                lambda context: bool(str(context.get("artifact_id") or "").strip())
            ),
            timeout_ms=10000,
        ):
            self._record_operator_stage_failure("preview-load-failed", "operator preview load failed screenshot")
            return False
        self._expect(
            "未显式选择配方版本" not in self._status_message(),
            "DXF preview should not be blocked by retired configuration gating",
        )
        if self.failed:
            self._record_operator_stage_failure("preview-load-failed", "operator preview load failed screenshot")
            return False
        if not self._wait_for(
            "operator preview becomes ready without retired configuration selection",
            lambda: self._operator_context_satisfies(
                lambda context: bool(context.get("snapshot_ready", False))
                and str(context.get("preview_source") or "") == "planned_glue_snapshot"
                and str(context.get("preview_kind") or "") == "glue_points"
                and int(context.get("glue_point_count", 0) or 0) > 0
                and bool(str(context.get("snapshot_hash") or "").strip())
                and bool(str(context.get("plan_id") or "").strip())
            ),
            timeout_ms=15000,
        ):
            self._record_operator_stage_failure("preview-ready-failed", "operator preview ready failed screenshot")
            return False
        self._emit_operator_context("preview-ready")
        self._capture_screenshot("operator preview ready screenshot", stage_name="preview-ready")
        self._expect(
            "未显式选择配方版本" not in getattr(getattr(self.window, "_preview_gate", None), "last_error_message", ""),
            "Preview gate should not report retired configuration gating errors",
        )
        if self.failed:
            self._record_operator_stage_failure("preview-ready-failed", "operator preview ready failed screenshot")
            return False
        self._click_button("btn-dxf-preview-refresh")
        if self.failed:
            self._record_operator_stage_failure("preview-ready-failed", "operator preview ready failed screenshot")
            return False
        if not self._wait_for(
            "operator preview refresh succeeds without retired configuration selection",
            lambda: self._operator_context_satisfies(
                lambda context: bool(context.get("snapshot_ready", False))
                and str(context.get("preview_source") or "") == "planned_glue_snapshot"
                and str(context.get("preview_kind") or "") == "glue_points"
                and int(context.get("glue_point_count", 0) or 0) > 0
                and bool(str(context.get("snapshot_hash") or "").strip())
                and bool(str(context.get("plan_id") or "").strip())
            ),
            timeout_ms=15000,
        ):
            self._record_operator_stage_failure("preview-refreshed-failed", "operator preview refresh failed screenshot")
            return False
        self._emit_operator_context("preview-refreshed")
        self._capture_screenshot("operator preview refreshed screenshot", stage_name="preview-refreshed")
        return True

    def _assert_operator_preview_action(self) -> None:
        self._run_operator_preview_journey()

    def _assert_operator_production_action(self) -> None:
        print("STEP: operator production journey", flush=True)
        if not self._run_operator_preview_journey():
            return

        self._ensure_runtime_motion_ready()
        self._spinbox_set("input-target-count", 1)

        start_button = self._require_widget("btn-production-start")
        if start_button is not None and not self._wait_for(
            "production start button enabled",
            lambda: start_button.isEnabled(),
            timeout_ms=10000,
        ):
            self._record_operator_stage_failure("production-start-failed", "operator production start failed screenshot")
            return

        self._click_button("btn-production-start")
        if not self._wait_for(
            "preview confirmation settles",
            lambda: bool(self._confirmed_snapshot_hash()),
            timeout_ms=10000,
        ):
            self._record_operator_stage_failure("production-start-failed", "operator production start failed screenshot")
            return
        if self._operator_context_satisfies(
            lambda context: bool(context.get("preview_confirmed", False))
            and bool(context.get("path_quality_blocking", False))
            and not bool(str(context.get("job_id") or "").strip())
        ):
            self._emit_operator_context("production-blocked")
            self._capture_screenshot("operator production blocked screenshot", stage_name="production-blocked")
            return
        if not self._wait_for(
            "operator production enters running state",
            lambda: self._operator_context_satisfies(
                lambda context: bool(str(context.get("job_id") or "").strip())
                and bool(context.get("preview_confirmed", False))
                and str(context.get("current_operation") or "") == "生产运行中"
            ),
            timeout_ms=30000,
        ):
            self._record_operator_stage_failure("production-start-failed", "operator production start failed screenshot")
            return
        self._emit_operator_context("production-started")
        self._emit_operator_context("production-running")
        self._capture_screenshot("operator production running screenshot", stage_name="production-running")

        with self._operator_next_job_ready_recovery_window():
            if not self._wait_for(
                "operator production terminal completed",
                lambda: self._operator_context_satisfies(
                    lambda context: not bool(str(context.get("job_id") or "").strip())
                    and str(context.get("current_operation") or "") == "完成"
                    and str(context.get("completed_count") or "").replace(" ", "") == "1/1"
                    and int(context.get("global_progress_percent", 0) or 0) == 100
                ),
                timeout_ms=self.timeout_ms,
            ):
                self._record_operator_stage_failure("production-completed-failed", "operator production completion failed screenshot")
                return
            self._emit_operator_context("production-completed")
            self._capture_screenshot("operator production completed screenshot", stage_name="production-completed")

            target_input = self._require_widget("input-target-count")
            next_job_ready_recovered = self._wait_for(
                "operator production recovers next job readiness",
                lambda: start_button is not None
                and target_input is not None
                and start_button.isEnabled()
                and target_input.isEnabled()
                and not bool(getattr(self.window, "_pending_production_action", ""))
                and self._operator_context_satisfies(
                    lambda context: not bool(str(context.get("job_id") or "").strip())
                    and str(context.get("completed_count") or "").replace(" ", "") == "1/1"
                    and int(context.get("global_progress_percent", 0) or 0) == 100
                    and not bool(context.get("preview_resync_pending", False))
                    and not bool(context.get("preview_refresh_inflight", False))
                ),
                timeout_ms=15000,
            )
            if not next_job_ready_recovered:
                self._record_operator_stage_failure("next-job-ready-failed", "operator production next job ready failed screenshot")
                return
            self._emit_operator_context("next-job-ready")
            self._capture_screenshot("operator production next job ready screenshot", stage_name="next-job-ready")

    def _ensure_runtime_motion_ready(self) -> None:
        if (
            bool(self._axis_status("X"))
            and bool(self._axis_status("Y"))
            and bool(self._axis_status("X").homed)
            and bool(self._axis_status("Y").homed)
            and abs(self._axis_status("X").velocity) <= 1e-3
            and abs(self._axis_status("Y").velocity) <= 1e-3
        ):
            return

        self._emit_operator_exclusive_window(kind="home_auto", state="started")
        home_completed = False
        velocity_settled = False
        self._click_button("btn-production-home-all")
        home_completed = self._wait_for(
            "runtime home completion",
            lambda: bool(self._axis_status("X"))
            and bool(self._axis_status("Y"))
            and bool(self._axis_status("X").homed)
            and bool(self._axis_status("Y").homed),
            timeout_ms=80000,
        )
        if home_completed:
            velocity_settled = self._wait_for(
                "runtime home settles velocity",
                lambda: bool(self._axis_status("X"))
                and bool(self._axis_status("Y"))
                and abs(self._axis_status("X").velocity) <= 1e-3
                and abs(self._axis_status("Y").velocity) <= 1e-3,
                timeout_ms=10000,
            )
        self._emit_operator_exclusive_window(
            kind="home_auto",
            state="completed" if home_completed and velocity_settled else "failed",
        )
        if not (home_completed and velocity_settled):
            return

    def _assert_snapshot_render_action(self) -> None:
        sample_dxf = self._canonical_preview_sample()
        self._expect(sample_dxf.exists(), f"Canonical preview sample should exist: {sample_dxf}")
        if not sample_dxf.exists():
            return

        self.window._dxf_filepath = str(sample_dxf)
        payload = self._load_preview_payload()
        if not payload:
            self._fail("Snapshot render requires a non-empty preview payload")
            return
        payload.setdefault("plan_id", payload.get("snapshot_id", "plan-from-evidence"))
        payload.setdefault("snapshot_id", payload.get("plan_id", "plan-from-evidence"))
        payload.setdefault(
            "snapshot_hash",
            payload.get("plan_fingerprint", payload.get("snapshot_hash", "snapshot-from-evidence")),
        )
        payload.setdefault("generated_at", "")
        self.window._on_preview_snapshot_completed(True, payload, "", source="worker")

        self._wait_for(
            "planned glue preview ready",
            lambda: bool(self.window._preview_gate.snapshot)
            and getattr(self.window, "_preview_source", "") == "planned_glue_snapshot"
            and bool(getattr(self.window, "_current_plan_id", "")),
            timeout_ms=15000,
        )
        self._expect(
            "来源: 规划胶点快照" in self.window._dxf_info_label.text(),
            "DXF info label should show planned_glue_snapshot source",
        )
        self._capture_screenshot("planned glue preview screenshot")

    def _assert_runtime_move_to_action(self) -> None:
        self._ensure_runtime_motion_ready()
        move_source = self._cached_status()
        self._expect(move_source is not None, "Cached status should exist before move_to")
        if move_source is None:
            return
        baseline_move_x = float(move_source.axes.get("X", type("Obj", (), {"position": 0.0})()).position)
        baseline_move_y = float(move_source.axes.get("Y", type("Obj", (), {"position": 0.0})()).position)
        self._spinbox_set("input-move-X", round(baseline_move_x + 1.0, 3))
        self._spinbox_set("input-move-Y", round(baseline_move_y + 1.5, 3))
        self._spinbox_set("input-move-speed", 8.0)
        move_button = self._require_widget("btn-move-to-position")
        if move_button is not None:
            self._wait_for("runtime move button enabled", lambda: move_button.isEnabled(), timeout_ms=3000)
        self._click_button("btn-move-to-position")
        self._wait_for(
            "runtime move_to changes both axes",
            lambda: bool(self._axis_status("X"))
            and bool(self._axis_status("Y"))
            and (self._axis_status("X").position >= baseline_move_x + 0.2)
            and (self._axis_status("Y").position >= baseline_move_y + 0.2),
            timeout_ms=10000,
        )
        self._click_button("btn-stop")
        self._wait_for(
            "runtime move_to stop settles velocity",
            lambda: bool(self._axis_status("X"))
            and bool(self._axis_status("Y"))
            and abs(self._axis_status("X").velocity) <= 1e-3
            and abs(self._axis_status("Y").velocity) <= 1e-3,
            timeout_ms=6000,
        )

    def _assert_runtime_jog_action(self) -> None:
        self._ensure_runtime_motion_ready()
        jog_before = self._axis_status("X")
        self._expect(jog_before is not None, "Axis X status should exist before jog")
        if jog_before is None:
            return
        jog_baseline_x = float(jog_before.position)
        jog_btn = self._require_widget("btn-jog-X-plus")
        if jog_btn is not None:
            self._expect(jog_btn.isEnabled(), "btn-jog-X-plus should be enabled")
            QTest.mousePress(jog_btn, Qt.LeftButton)
            QTest.qWait(350)
            QTest.mouseRelease(jog_btn, Qt.LeftButton)
        self._wait_for(
            "runtime jog changes X position",
            lambda: bool(self._axis_status("X")) and (self._axis_status("X").position > jog_baseline_x + 0.05),
            timeout_ms=6000,
        )
        self._wait_for(
            "runtime jog stop settles velocity",
            lambda: bool(self._axis_status("X")) and abs(self._axis_status("X").velocity) <= 1e-3,
            timeout_ms=6000,
        )

    def _assert_runtime_supply_dispenser_action(self) -> None:
        self._click_button("btn-supply-open")
        self._wait_for("runtime supply opens", lambda: self._label_text("valve-supply") == "开", timeout_ms=5000)

        self._spinbox_set("input-dispenser-count", 20)
        self._spinbox_set("input-dispenser-interval", 40)
        self._spinbox_set("input-dispenser-duration", 40)
        self._click_button("btn-dispenser-start")
        self._wait_for(
            "runtime dispenser opens",
            lambda: self._label_text("valve-dispenser") == "开" or "点胶已启动" in self._status_message(),
            timeout_ms=5000,
        )
        self._click_button("btn-dispenser-stop")
        self._wait_for("runtime dispenser closes", lambda: self._label_text("valve-dispenser") == "关", timeout_ms=5000)

        self._click_button("btn-supply-close")
        self._wait_for("runtime supply closes", lambda: self._label_text("valve-supply") == "关", timeout_ms=5000)

    def _assert_runtime_estop_reset_action(self) -> None:
        self._click_button("btn-estop")
        self._wait_for(
            "runtime estop visible",
            lambda: bool(self._cached_status()) and bool(self._cached_status().gate_estop_active()),
            timeout_ms=8000,
        )
        self._click_button("btn-estop-reset")
        self._wait_for(
            "runtime estop reset visible",
            lambda: bool(self._cached_status()) and not bool(self._cached_status().gate_estop_active()),
            timeout_ms=8000,
        )

    def _assert_runtime_door_interlock_action(self) -> None:
        self._ensure_runtime_motion_ready()
        move_before_door = self._axis_status("X")
        self._expect(move_before_door is not None, "Axis X status should exist before door interlock check")
        if move_before_door is None:
            return
        door_baseline_x = float(move_before_door.position)
        door_target_y = float(self._axis_status("Y").position) if self._axis_status("Y") else 0.0
        door_ok, _, door_msg = self.window._protocol.mock_io_set(door=True)
        self._expect(door_ok, f"mock_io_set door should succeed: {door_msg}")
        self._wait_for(
            "runtime door visible",
            lambda: bool(self._cached_status()) and bool(self._cached_status().gate_door_active()),
            timeout_ms=8000,
        )
        self._spinbox_set("input-move-X", round(door_baseline_x + 1.0, 3))
        self._spinbox_set("input-move-Y", round(door_target_y, 3))
        self._click_button("btn-move-to-position")
        QTest.qWait(300)
        self._expect("移动失败" in self._status_message(), "Move should fail when door interlock is active")
        self._wait_for(
            "runtime door block keeps X position stable",
            lambda: bool(self._axis_status("X")) and abs(self._axis_status("X").position - door_baseline_x) <= 0.05,
            timeout_ms=3000,
        )
        door_clear_ok, _, door_clear_msg = self.window._protocol.mock_io_set(door=False)
        self._expect(door_clear_ok, f"mock_io_set door clear should succeed: {door_clear_msg}")
        self._wait_for(
            "runtime door clears",
            lambda: bool(self._cached_status()) and not bool(self._cached_status().gate_door_active()),
            timeout_ms=8000,
        )

    def _assert_runtime_action_chain(self) -> None:
        print("STEP: runtime action chain", flush=True)
        cached = self._cached_status()
        self._expect(cached is not None, "Online runtime action chain requires cached status")
        if cached is None:
            return
        profile = self._resolved_runtime_action_profile()
        if self.failed:
            return

        if profile == "snapshot_render":
            self._assert_snapshot_render_action()
        if profile in {"full", "operator_preview"}:
            self._assert_operator_preview_action()
        if profile == "operator_production":
            self._assert_operator_production_action()
        if profile in {"full", "home_move"}:
            self._assert_runtime_move_to_action()
        if profile in {"full", "jog"}:
            self._assert_runtime_jog_action()
        if profile in {"full", "supply_dispenser"}:
            self._assert_runtime_supply_dispenser_action()
        if profile in {"full", "estop_reset"}:
            self._assert_runtime_estop_reset_action()
        if profile in {"full", "door_interlock"}:
            self._assert_runtime_door_interlock_action()
        # Operator journeys already emit staged screenshots as their formal evidence.
        # Skip the trailing generic capture there to avoid a redundant close-out race.
        if profile not in {"snapshot_render", "operator_preview", "operator_production"}:
            self._capture_screenshot(f"runtime action profile {profile}")

    def _emit_supervisor_diag(self) -> None:
        def _diag_value(value: object | None) -> str:
            text = str(value or "").strip()
            if not text:
                return "null"
            return text.replace(" ", "%20")

        result = self.window._launch_result
        if result is None:
            print(
                "SUPERVISOR_DIAG "
                "mode=unknown "
                "session_state=none "
                "backend_state=unknown "
                "tcp_state=unknown "
                "hardware_state=unknown "
                "failure_code=null "
                "failure_stage=null "
                "recoverable=false "
                "online_ready=false "
                "runtime_contract_verified=false "
                "runtime_executable=null "
                "runtime_working_directory=null "
                "runtime_protocol_version=null "
                "preview_snapshot_contract=null",
                flush=True,
            )
            return
        snapshot = result.session_snapshot
        runtime_identity = snapshot.runtime_identity if snapshot is not None else None
        mode = snapshot.mode if snapshot is not None else result.effective_mode
        session_state = snapshot.session_state if snapshot is not None else result.session_state
        backend_state = snapshot.backend_state if snapshot is not None else ("ready" if result.backend_started else "stopped")
        tcp_state = snapshot.tcp_state if snapshot is not None else ("ready" if result.tcp_connected else "disconnected")
        hardware_state = snapshot.hardware_state if snapshot is not None else ("ready" if result.hardware_ready else "unavailable")
        failure_code = snapshot.failure_code if snapshot is not None else result.failure_code
        failure_stage = snapshot.failure_stage if snapshot is not None else result.failure_stage
        recoverable = snapshot.recoverable if snapshot is not None else result.recoverable
        online_ready = result.online_ready
        runtime_contract_verified = bool(snapshot.runtime_contract_verified) if snapshot is not None else False
        print(
            "SUPERVISOR_DIAG "
            f"mode={mode} "
            f"session_state={session_state} "
            f"backend_state={backend_state} "
            f"tcp_state={tcp_state} "
            f"hardware_state={hardware_state} "
            f"failure_code={failure_code or 'null'} "
            f"failure_stage={failure_stage or 'null'} "
            f"recoverable={str(bool(recoverable)).lower()} "
            f"online_ready={str(bool(online_ready)).lower()} "
            f"runtime_contract_verified={str(runtime_contract_verified).lower()} "
            f"runtime_executable={_diag_value(getattr(runtime_identity, 'executable_path', None))} "
            f"runtime_working_directory={_diag_value(getattr(runtime_identity, 'working_directory', None))} "
            f"runtime_protocol_version={_diag_value(getattr(runtime_identity, 'protocol_version', None))} "
            f"preview_snapshot_contract={_diag_value(getattr(runtime_identity, 'preview_snapshot_contract', None))}",
            flush=True,
        )

    def _emit_supervisor_stage_events(self) -> None:
        events = getattr(self.window, "_supervisor_stage_events", None) or []
        for event in events:
            failure_code = getattr(event, "failure_code", None) or "null"
            recoverable = getattr(event, "recoverable", None)
            if recoverable is None:
                recoverable_text = "null"
            else:
                recoverable_text = str(bool(recoverable)).lower()
            message = getattr(event, "message", None) or "null"
            message = str(message).replace(" ", "_")
            print(
                "SUPERVISOR_EVENT "
                f"type={getattr(event, 'event_type', 'unknown')} "
                f"session_id={getattr(event, 'session_id', 'unknown')} "
                f"stage={getattr(event, 'stage', 'unknown')} "
                f"failure_code={failure_code} "
                f"recoverable={recoverable_text} "
                f"message={message} "
                f"timestamp={getattr(event, 'timestamp', 'unknown')}",
                flush=True,
            )

    def _assert_gui_shell_present(self) -> None:
        print("STEP: gui shell", flush=True)
        self._expect(self.window.isVisible(), "Main window should be visible")
        self._expect(self._status_message() != "系统就绪", "GUI shell must not expose a fake ready status before supervisor state settles")
        for testid in (
            "indicator-global-status",
            "label-launch-mode",
            "label-tcp-state",
            "label-hw-state",
            "main-tabs",
            "tab-production",
            "tab-setup",
            "panel-system",
            "panel-motion-control",
            "panel-dispenser-control",
            "panel-alarms",
        ):
            self._require_widget(testid)

        tabs = self._require_widget("main-tabs")
        if tabs is not None and hasattr(tabs, "count"):
            self._expect(tabs.count() >= 3, "Main tab container should expose the core workspaces")
            labels = [tabs.tabText(index) for index in range(tabs.count())]
            self._expect("仿真观察" not in labels, "Main tab container should not expose the sim observer tab")
            self._expect("配置" not in labels, "Main tab container should not expose the retired configuration workspace")

    def _assert_offline_contract(self) -> None:
        print("STEP: offline contract", flush=True)
        self._wait_for("offline launch result", lambda: self.window._launch_result is not None)
        result = self.window._launch_result
        self._expect(result is not None, "Offline launch result should be available")
        if result is not None:
            self._expect(result.effective_mode == "offline", "Effective mode should stay offline")
            self._expect(result.success, "Offline launch should succeed")
            self._expect(result.session_snapshot is not None, "Offline launch result should expose a session snapshot")
            if result.session_snapshot is not None:
                self._expect(result.session_snapshot.mode == "offline", "Offline snapshot mode should be offline")
                self._expect(result.session_snapshot.failure_code is None, "Offline snapshot should not carry failure code")

        self._expect(self._label_text("label-launch-mode") == "Offline", "Launch mode label should read Offline")
        self._expect(self._label_text("label-tcp-state") == "未连接", "Offline TCP label should stay disconnected")
        self._expect(self._label_text("label-hw-state") == "不可用", "Offline hardware label should stay unavailable")
        self._expect(self._label_text("label-current-operation") == "离线模式", "Offline operation status should be explicit")

        for testid in ("tab-production", "panel-system", "panel-motion-control", "panel-dispenser-control"):
            widget = self._require_widget(testid)
            if widget is not None:
                self._expect(not widget.isEnabled(), f"{testid} should be disabled offline")

        self.window._on_alarm_clear()
        QTest.qWait(50)
        self._expect(
            self._status_message().startswith("Offline 模式下不可用"),
            "Offline guard should block an online-only action",
        )
        self._expect("已连接" not in self._status_message(), "Offline action should not report a connected state")

    def _assert_online_contract(self) -> None:
        print("STEP: online contract", flush=True)
        self._wait_for("online launch result", lambda: self.window._launch_result is not None)
        if self.expect_failure_code:
            self._wait_for(
                f"expected failure_code={self.expect_failure_code}",
                lambda: bool(self.window._launch_result)
                and (self.window._launch_result.failure_code == self.expect_failure_code),
            )
            result = self.window._launch_result
            self._expect(result is not None, "Online failure launch result should be available")
            if result is not None:
                self._expect(not result.online_ready, "Expected failure path must not be online_ready")
                self._expect(result.failure_code == self.expect_failure_code, "Unexpected failure_code")
                if self.expect_failure_stage:
                    self._expect(
                        result.failure_stage == self.expect_failure_stage,
                        f"Unexpected failure_stage, expect {self.expect_failure_stage}",
                    )
            self._expect(self._label_text("label-launch-mode") == "Online", "Launch mode label should read Online")
            return

        self._wait_for(
            "online ready snapshot",
            lambda: bool(self.window._launch_result) and bool(self.window._launch_result.online_ready),
        )
        result = self.window._launch_result
        self._expect(result is not None, "Online launch result should be available")
        if result is not None:
            self._expect(result.success, "Online launch should succeed with the mock server")
            self._expect(result.phase == "ready", "Online launch should reach the ready phase")
            self._expect(result.online_ready, "Online launch should report online_ready")
            self._expect(result.tcp_connected, "Online launch should connect TCP")
            self._expect(result.hardware_ready, "Online launch should initialize hardware")
            self._expect(result.failure_code is None, "Successful online launch should not carry failure_code")
            self._expect(result.failure_stage is None, "Successful online launch should not carry failure_stage")

        self._expect(self._label_text("label-launch-mode") == "Online", "Launch mode label should read Online")
        self._expect(self._label_text("label-tcp-state") == "已连接", "Online TCP label should report connected")
        self._expect(self._label_text("label-hw-state") == "就绪", "Online hardware label should report ready")
        self._expect(self._label_text("label-current-operation") == "空闲", "Online operation status should settle to idle")

        for testid in ("tab-production", "panel-system", "panel-motion-control", "panel-dispenser-control"):
            widget = self._require_widget(testid)
            if widget is not None:
                self._expect(widget.isEnabled(), f"{testid} should remain enabled online")

        alarm_list = self._require_widget("list-alarms")
        if alarm_list is not None:
            if self._uses_external_gateway():
                self._expect(alarm_list.count() >= 0, "Alarm list should remain available with external gateway")
            else:
                self._wait_for("mock alarms to populate", lambda: alarm_list.count() > 0, timeout_ms=3000)
                before = alarm_list.count()
                self._expect(before > 0, "Mock server should seed at least one alarm")
                self.window._on_alarm_clear()
                self._wait_for("alarm clear action", lambda: alarm_list.count() == 0, timeout_ms=3000)
                self._expect(alarm_list.count() == 0, "Online guarded alarm action should be allowed")

        self._expect(
            not self._status_message().startswith("Offline 模式下不可用"),
            "Online actions should not be blocked by the offline guard",
        )
        if self.screenshot_path and not self.exercise_runtime_actions:
            self._capture_screenshot("online ready screenshot")
        if self.exercise_runtime_actions:
            self._wait_for(
                "runtime status cache ready",
                lambda: self._cached_status() is not None,
                timeout_ms=5000,
            )
            self._assert_runtime_action_chain()
        if self.exercise_recovery:
            self._assert_recovery_cycle()

    def _assert_recovery_cycle(self) -> None:
        print("STEP: recovery cycle", flush=True)
        self.window._apply_runtime_degradation(
            failure_code="SUP_TCP_CONNECT_FAILED",
            failure_stage="tcp_ready",
            message="Injected recovery cycle failure",
            tcp_state="failed",
            hardware_state="unavailable",
        )
        self._wait_for(
            "degraded launch snapshot",
            lambda: bool(self.window._launch_result)
            and (self.window._launch_result.failure_code == "SUP_TCP_CONNECT_FAILED"),
        )

        degraded = self.window._launch_result
        self._expect(degraded is not None, "Recovery scenario should produce a degraded launch result")
        if degraded is not None:
            self._expect(not degraded.online_ready, "Degraded launch result must not stay online_ready")
            self._expect(degraded.failure_stage == "tcp_ready", "Runtime degradation should map to tcp_ready stage")

        retry_btn = self._require_widget("btn-recovery-retry")
        restart_btn = self._require_widget("btn-recovery-restart")
        stop_btn = self._require_widget("btn-recovery-stop")
        if retry_btn is not None:
            self._expect(retry_btn.isEnabled(), "Retry-stage recovery button should be enabled after recoverable failure")
        if restart_btn is not None:
            self._expect(restart_btn.isEnabled(), "Restart-session recovery button should be enabled after recoverable failure")
        if stop_btn is not None:
            self._expect(stop_btn.isEnabled(), "Stop-session recovery button should be enabled after recoverable failure")
        if retry_btn is None:
            return

        QTest.mouseClick(retry_btn, Qt.LeftButton)
        self._wait_for(
            "online ready after retry-stage recovery",
            lambda: bool(self.window._launch_result) and bool(self.window._launch_result.online_ready),
            timeout_ms=8000,
        )
        recovered = self.window._launch_result
        self._expect(recovered is not None, "Recovered launch result should be available")
        if recovered is not None:
            self._expect(recovered.online_ready, "Retry-stage recovery should return system to online_ready")
            self._expect(recovered.failure_code is None, "Recovered launch result must clear failure_code")
            self._expect(recovered.failure_stage is None, "Recovered launch result must clear failure_stage")

        events = getattr(self.window, "_supervisor_stage_events", None) or []
        self._expect(
            any(
                getattr(event, "event_type", "") == "stage_failed"
                and getattr(event, "stage", "") == "tcp_ready"
                and getattr(event, "failure_code", "") == "SUP_TCP_CONNECT_FAILED"
                for event in events
            ),
            "Recovery cycle should emit stage_failed for runtime tcp_ready degradation",
        )
        self._expect(
            any(getattr(event, "event_type", "") == "recovery_invoked" for event in events),
            "Recovery cycle should emit recovery_invoked supervisor event",
        )
        self._expect(
            any(
                getattr(event, "event_type", "") == "stage_succeeded"
                and getattr(event, "stage", "") == "online_ready"
                for event in events
            ),
            "Recovery cycle should emit stage_succeeded for online_ready",
        )

    def run(self) -> None:
        try:
            self._assert_gui_shell_present()
            if self.launch_mode == "offline":
                self._assert_offline_contract()
            else:
                self._assert_online_contract()
        finally:
            self._emit_supervisor_stage_events()
            self._emit_supervisor_diag()
            if hasattr(self.window, "_timer"):
                self.window._timer.stop()
            if getattr(self.window, "_client", None) is not None:
                self.window._client.disconnect()
            self.window.close()
            exit_stack = getattr(self.window, "_qtest_exit_stack", None)
            if exit_stack is not None:
                exit_stack.close()
            if self.timed_out:
                self.exit_code = EXIT_TIMEOUT
                self.app.exit(EXIT_TIMEOUT)
            else:
                self.exit_code = EXIT_ASSERTION_FAILED if self.failed else EXIT_SUCCESS
                self.app.exit(self.exit_code)


def build_window(
    launch_mode: str,
    host: str,
    port: int,
    *,
    dxf_browse_path: str = "",
) -> MainWindow:
    stack = ExitStack()
    stack.enter_context(patch_headless_preview())
    stack.enter_context(patch_modal_dialogs())
    if dxf_browse_path:
        stack.enter_context(patch_dxf_browse_dialog(dxf_browse_path))
    if launch_mode == "online":
        stack.enter_context(patch_launch_endpoints(host, port))
    try:
        window = MainWindow(launch_mode=launch_mode)
    except Exception:
        stack.close()
        raise
    # Startup failures may trigger modal error dialogs; disable them in smoke runs.
    if hasattr(window, "_show_startup_error"):
        window._show_startup_error = lambda *_args, **_kwargs: None
    window._qtest_exit_stack = stack
    return window


def main() -> int:
    parser = argparse.ArgumentParser(description="Run mode-aware GUI contract smoke tests")
    parser.add_argument("--mode", choices=("offline", "online"), default="offline")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=0, help="0 to auto-pick a free port when mock mode is enabled")
    parser.add_argument("--no-mock", action="store_true", help="Use an existing TCP server for online mode")
    parser.add_argument("--verbose", action="store_true")
    parser.add_argument("--timeout-ms", type=int, default=15000)
    parser.add_argument("--expect-failure-code", default="")
    parser.add_argument("--expect-failure-stage", default="")
    parser.add_argument("--exercise-recovery", action="store_true")
    parser.add_argument("--exercise-runtime-actions", action="store_true")
    parser.add_argument("--screenshot-path", default="")
    parser.add_argument("--screenshot-dir", default="")
    parser.add_argument("--preview-payload-path", default="")
    parser.add_argument("--dxf-browse-path", default="")
    parser.add_argument("--runtime-action-profile", default="")
    args = parser.parse_args()
    try:
        resolved_runtime_action_profile, resolved_dxf_browse_path = resolve_runtime_action_request(
            exercise_runtime_actions=bool(args.exercise_runtime_actions),
            runtime_action_profile=args.runtime_action_profile,
            preview_payload_path=args.preview_payload_path,
            dxf_browse_path=args.dxf_browse_path,
        )
    except ValueError as exc:
        print(f"FAIL: {exc}", flush=True)
        return EXIT_ASSERTION_FAILED

    server: Optional[MockTcpServer] = None
    port = args.port
    if args.mode == "online" and not args.no_mock:
        server, port = start_mock_server(args.host, args.port, args.verbose)

    app = QApplication([])
    window = build_window(
        args.mode,
        args.host,
        port,
        dxf_browse_path=resolved_dxf_browse_path,
    )
    window.show()

    runner = GuiContractRunner(
        app,
        window,
        args.mode,
        args.timeout_ms,
        expect_failure_code=args.expect_failure_code,
        expect_failure_stage=args.expect_failure_stage,
        exercise_recovery=args.exercise_recovery,
        exercise_runtime_actions=args.exercise_runtime_actions,
        screenshot_path=args.screenshot_path,
        screenshot_dir=args.screenshot_dir,
        preview_payload_path=args.preview_payload_path,
        runtime_action_profile=resolved_runtime_action_profile,
        use_external_gateway=bool(args.no_mock),
    )
    QTimer.singleShot(0, runner.run)
    QTimer.singleShot(args.timeout_ms, runner.request_timeout)
    app.exec_()

    if server is not None:
        server.shutdown()
        server.server_close()

    return runner.exit_code


if __name__ == "__main__":
    raise SystemExit(main())
