"""P1 workspace for time-process and alert-context analysis."""

from __future__ import annotations

from typing import Callable

from PyQt5.QtCore import Qt
from PyQt5.QtWidgets import (
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QPlainTextEdit,
    QPushButton,
    QSplitter,
    QVBoxLayout,
    QWidget,
)

from ..contracts.observer_status import FailureReason, ObserverState
from ..state.observer_store import ObserverStore, P1Backlink, P1Selection


class SimObserverP1Workspace(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._store: ObserverStore | None = None
        self._current_selection: P1Selection | None = None
        self._suspend_events = False
        self._return_callback: Callable[[], None] | None = None
        self._backlink_callback: Callable[[P1Backlink], None] | None = None

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(12)

        top_bar = QHBoxLayout()
        self._title_label = QLabel("P1 过程分析工作面")
        self._title_label.setStyleSheet("font-size: 16px; font-weight: bold;")
        top_bar.addWidget(self._title_label)
        top_bar.addStretch()

        self._status_label = QLabel("未加载 recording | mode=empty")
        top_bar.addWidget(self._status_label)

        self._backlink_button = QPushButton("回接到 P0")
        self._backlink_button.clicked.connect(self._on_backlink_to_p0)
        top_bar.addWidget(self._backlink_button)

        self._return_button = QPushButton("返回 P0")
        self._return_button.clicked.connect(self._on_return_to_p0)
        top_bar.addWidget(self._return_button)
        layout.addLayout(top_bar)

        body = QSplitter(Qt.Horizontal)
        body.setChildrenCollapsible(False)

        time_group = QGroupBox("时间过程视图")
        time_layout = QVBoxLayout(time_group)
        self._time_anchor_list = QListWidget()
        self._time_anchor_list.currentItemChanged.connect(self._on_time_anchor_changed)
        time_layout.addWidget(self._time_anchor_list)
        body.addWidget(time_group)

        alert_group = QGroupBox("报警上下文视图")
        alert_layout = QVBoxLayout(alert_group)
        self._alert_list = QListWidget()
        self._alert_list.currentItemChanged.connect(self._on_alert_changed)
        alert_layout.addWidget(self._alert_list)
        body.addWidget(alert_group)

        detail_group = QGroupBox("P1 详情与回接")
        detail_layout = QVBoxLayout(detail_group)
        self._detail_text = QPlainTextEdit()
        self._detail_text.setReadOnly(True)
        detail_layout.addWidget(self._detail_text)
        body.addWidget(detail_group)

        body.setStretchFactor(0, 2)
        body.setStretchFactor(1, 2)
        body.setStretchFactor(2, 4)
        layout.addWidget(body)
        self._render_empty_state()

    def set_return_callback(self, callback: Callable[[], None] | None) -> None:
        self._return_callback = callback

    def set_backlink_callback(self, callback: Callable[[P1Backlink], None] | None) -> None:
        self._backlink_callback = callback

    def load_store(self, store: ObserverStore) -> None:
        self._store = store
        self._current_selection = None
        self._render_from_store(select_default=True)

    def _on_time_anchor_changed(self, current: QListWidgetItem | None, previous: QListWidgetItem | None) -> None:
        del previous
        if self._suspend_events or self._store is None or current is None:
            return
        time_anchor_id = current.data(Qt.UserRole)
        if time_anchor_id is None:
            return
        self._current_selection = self._store.select_p1_time_anchor(time_anchor_id)
        self._render_from_store()

    def _on_alert_changed(self, current: QListWidgetItem | None, previous: QListWidgetItem | None) -> None:
        del previous
        if self._suspend_events or self._store is None or current is None:
            return
        alert_id = current.data(Qt.UserRole)
        if alert_id is None:
            return
        self._current_selection = self._store.select_p1_alert(alert_id)
        self._render_from_store()

    def _on_backlink_to_p0(self) -> None:
        if (
            self._backlink_callback is None
            or self._current_selection is None
            or self._current_selection.backlink.backlink_level == "unavailable"
        ):
            return
        self._backlink_callback(self._current_selection.backlink)

    def _on_return_to_p0(self) -> None:
        if self._return_callback is not None:
            self._return_callback()

    def _render_from_store(self, *, select_default: bool = False) -> None:
        if self._store is None:
            self._render_empty_state()
            return

        if select_default:
            self._current_selection = self._store.select_default_time_anchor() or self._store.select_default_alert()

        self._validate_render_state()
        self._set_status_label()
        self._rebuild_time_anchor_list()
        self._rebuild_alert_list()
        self._detail_text.setPlainText(self._build_detail_text())
        self._backlink_button.setEnabled(
            self._current_selection is not None and self._current_selection.backlink.backlink_level != "unavailable"
        )

    def _rebuild_time_anchor_list(self) -> None:
        self._suspend_events = True
        try:
            self._time_anchor_list.clear()
            if self._store is None:
                return
            selected_id = self._current_selection.selected_id if self._current_selection and self._current_selection.selected_kind == "time_anchor" else None
            selected_row = None
            for index, anchor in enumerate(self._store.normalized_data.time_anchors):
                item = QListWidgetItem(f"[{anchor.source_kind or '-'}] {anchor.display_label}")
                item.setData(Qt.UserRole, anchor.object_id)
                self._time_anchor_list.addItem(item)
                if anchor.object_id == selected_id:
                    selected_row = index
            if selected_row is not None:
                self._time_anchor_list.setCurrentRow(selected_row)
        finally:
            self._suspend_events = False

    def _rebuild_alert_list(self) -> None:
        self._suspend_events = True
        try:
            self._alert_list.clear()
            if self._store is None:
                return
            selected_id = self._current_selection.selected_id if self._current_selection and self._current_selection.selected_kind == "alert" else None
            selected_row = None
            for index, alert in enumerate(self._store.normalized_data.alerts):
                item = QListWidgetItem(f"[{alert.alert_time:.3f}s] {alert.display_label}")
                item.setData(Qt.UserRole, alert.object_id)
                self._alert_list.addItem(item)
                if alert.object_id == selected_id:
                    selected_row = index
            if selected_row is not None:
                self._alert_list.setCurrentRow(selected_row)
        finally:
            self._suspend_events = False

    def _set_status_label(self) -> None:
        if self._store is None:
            self._status_label.setText("未加载 recording | mode=empty")
            return
        if self._current_selection is None:
            self._status_label.setText(
                f"time_anchors={len(self._store.time_anchor_ids)} | alerts={len(self._store.alert_ids)} | mode=empty"
            )
            return
        mode = "resolved"
        if self._current_selection.failure_reason is not None:
            mode = "failed"
        elif self._current_selection.time_mapping.mapping_state is not ObserverState.RESOLVED:
            mode = "degraded"
        self._status_label.setText(
            f"time_anchors={len(self._store.time_anchor_ids)} | alerts={len(self._store.alert_ids)} | "
            f"selection={self._current_selection.selected_kind} | mode={mode}"
        )

    def _build_detail_text(self) -> str:
        if self._store is None:
            return "请先在 P0 加载 recording，再进入 P1。"
        if self._current_selection is None:
            return "当前 recording 没有可用的时间锚点或报警入口。"

        selected_object = self._current_selection.selected_object
        lines = [
            f"selection.kind: {self._current_selection.selected_kind}",
            f"selection.id: {self._current_selection.selected_id}",
            f"selection.label: {selected_object.display_label}",
            f"time.mapping.level: {self._current_selection.time_mapping.association_level}",
            f"time.mapping.state: {self._current_selection.time_mapping.mapping_state.value}",
            f"failure.reason: {self._current_selection.failure_reason.value if self._current_selection.failure_reason else '-'}",
        ]
        if self._current_selection.time_mapping.object_id is not None:
            lines.append(f"time.object: {self._current_selection.time_mapping.object_id}")
        if self._current_selection.time_mapping.group_id is not None:
            lines.append(f"time.group: {self._current_selection.time_mapping.group_id}")
        if self._current_selection.time_mapping.sequence_range_ref is not None:
            range_ref = self._current_selection.time_mapping.sequence_range_ref
            lines.append(
                "time.sequence: "
                + ", ".join(str(member_id) for member_id in range_ref.member_segment_ids)
            )
        if self._current_selection.time_mapping.reason is not None:
            lines.append(f"time.reason.code: {self._current_selection.time_mapping.reason.reason_code}")
            lines.append(f"time.reason.text: {self._current_selection.time_mapping.reason.reason_text}")

        if self._current_selection.context_window is not None:
            context_window = self._current_selection.context_window
            lines.append(f"context.state: {context_window.window_state.value}")
            lines.append(f"context.basis: {context_window.window_basis}")
            lines.append(f"context.range: {context_window.window_start} -> {context_window.window_end}")
            if context_window.fallback_mode:
                lines.append(f"context.fallback: {context_window.fallback_mode}")

        if self._current_selection.key_window is not None:
            key_window = self._current_selection.key_window
            lines.append(f"key.state: {key_window.window_state.value}")
            lines.append(f"key.basis: {key_window.window_basis}")
            lines.append(f"key.range: {key_window.key_window_start} -> {key_window.key_window_end}")

        lines.append(f"backlink.level: {self._current_selection.backlink.backlink_level}")
        if self._current_selection.backlink.reason_text:
            lines.append(f"backlink.reason: {self._current_selection.backlink.reason_text}")
        lines.append("source.refs:")
        for source_ref in selected_object.source_refs:
            lines.append(
                f"  - {source_ref.source_name} | {source_ref.source_path} | index={source_ref.source_index}"
            )
        return "\n".join(lines)

    def _validate_render_state(self) -> None:
        if self._store is None or self._current_selection is None:
            return

        time_mapping_state = self._current_selection.time_mapping.mapping_state
        if not isinstance(time_mapping_state, ObserverState):
            raise ValueError("Unsupported time mapping state in P1 workspace.")

        failure_reason = self._current_selection.failure_reason
        if failure_reason is not None and not isinstance(failure_reason, FailureReason):
            raise ValueError("Unsupported failure reason in P1 workspace.")

        if self._current_selection.context_window is not None and not isinstance(
            self._current_selection.context_window.window_state,
            ObserverState,
        ):
            raise ValueError("Unsupported context window state in P1 workspace.")

        if self._current_selection.key_window is not None and not isinstance(
            self._current_selection.key_window.window_state,
            ObserverState,
        ):
            raise ValueError("Unsupported key window state in P1 workspace.")

        backlink = self._current_selection.backlink
        allowed_backlink_levels = {"single_object", "object_group", "sequence_range", "unavailable"}
        if backlink.backlink_level not in allowed_backlink_levels:
            raise ValueError("Unsupported backlink level in P1 workspace.")

        if time_mapping_state is not ObserverState.RESOLVED and backlink.backlink_level != "unavailable":
            raise ValueError("Unresolved P1 mapping must not expose backlink targets.")

        if backlink.backlink_level == "unavailable" and any(
            (
                backlink.object_id is not None,
                backlink.group_id is not None,
                backlink.sequence_range_ref is not None,
                backlink.group_member_ids,
            )
        ):
            raise ValueError("Unavailable P1 backlink must not expose concrete targets.")

    def _render_empty_state(self) -> None:
        self._time_anchor_list.clear()
        self._alert_list.clear()
        self._status_label.setText("未加载 recording | mode=empty")
        self._backlink_button.setEnabled(False)
        self._detail_text.setPlainText("请先在 P0 加载 recording，再进入 P1。")


__all__ = ["SimObserverP1Workspace"]
