"""P0 workspace for the simulation observer feature."""

from __future__ import annotations

import math
from pathlib import Path
from typing import Callable, Mapping

from PyQt5.QtCore import Qt, QLineF, QRectF
from PyQt5.QtGui import QColor, QPainterPath, QPen
from PyQt5.QtWidgets import (
    QFileDialog,
    QGraphicsScene,
    QGraphicsView,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QListWidget,
    QListWidgetItem,
    QMessageBox,
    QPlainTextEdit,
    QPushButton,
    QSplitter,
    QVBoxLayout,
    QWidget,
)

from ..contracts.observer_status import FailureReason, ObserverState
from ..contracts.observer_rules import SequenceRangeRef
from ..contracts.observer_types import ObjectId, PathSegmentObject
from ..state.observer_store import ObserverSelection, ObserverStore, P1Backlink
from .path_rendering import ArcRenderInstruction, segment_render_instruction, segment_render_points


class ResultCanvasView(QGraphicsView):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._scene = QGraphicsScene(self)
        self.setScene(self._scene)
        self.setMinimumHeight(260)
        self.setStyleSheet("background-color: #111; border: 1px solid #444;")

    def render_selection(
        self,
        store: ObserverStore | None,
        selection: ObserverSelection | None,
        *,
        focused_object_id: ObjectId | None = None,
        external_primary_object_id: ObjectId | None = None,
        external_group_member_ids: tuple[ObjectId, ...] = (),
        external_sequence_member_ids: tuple[ObjectId, ...] = (),
    ) -> None:
        self._scene.clear()
        if store is None:
            self._scene.addText("未加载 recording")
            return
        if not store.normalized_data.path_segments:
            self._scene.addText("当前 recording 没有路径段")
            return

        highlight_selection = selection is not None and selection.primary_association.selection_state is ObserverState.RESOLVED
        selected_object_id = store.current_primary_object_id if highlight_selection else external_primary_object_id
        group_members = set(store.current_group_member_ids if highlight_selection else external_group_member_ids)
        sequence_members = set(external_sequence_member_ids)

        min_x = min(segment.bbox.min_x for segment in store.normalized_data.path_segments)
        min_y = min(segment.bbox.min_y for segment in store.normalized_data.path_segments)
        max_x = max(segment.bbox.max_x for segment in store.normalized_data.path_segments)
        max_y = max(segment.bbox.max_y for segment in store.normalized_data.path_segments)
        self._scene.setSceneRect(min_x - 20.0, -max_y - 20.0, (max_x - min_x) + 40.0, (max_y - min_y) + 40.0)

        default_pen = QPen(QColor("#666666"), 1.5)
        group_pen = QPen(QColor("#f39c12"), 2.5)
        selected_pen = QPen(QColor("#ff4d4f"), 3.0)
        sequence_pen = QPen(QColor("#2ecc71"), 2.5)
        focus_pen = QPen(QColor("#3498db"), 2.5)
        for segment in store.normalized_data.path_segments:
            pen = default_pen
            if segment.object_id == selected_object_id:
                pen = selected_pen
            elif segment.object_id in group_members:
                pen = group_pen
            elif segment.object_id in sequence_members:
                pen = sequence_pen
            elif focused_object_id is not None and segment.object_id == focused_object_id:
                pen = focus_pen
            render_instruction = segment_render_instruction(segment, store.normalized_data.path_points)
            if isinstance(render_instruction, ArcRenderInstruction):
                self._scene.addPath(self._arc_path(render_instruction), pen)
            else:
                for start_point, end_point in zip(render_instruction.points, render_instruction.points[1:]):
                    self._scene.addLine(
                        QLineF(
                            start_point.x,
                            -start_point.y,
                            end_point.x,
                            -end_point.y,
                        ),
                        pen,
                    )
        self.fitInView(self._scene.sceneRect(), Qt.KeepAspectRatio)

    @staticmethod
    def _arc_path(instruction: ArcRenderInstruction) -> QPainterPath:
        diameter = instruction.radius * 2.0
        rect = QRectF(
            instruction.center.x - instruction.radius,
            -(instruction.center.y + instruction.radius),
            diameter,
            diameter,
        )
        start_angle_deg = -math.degrees(instruction.start_angle_rad)
        sweep_angle_deg = -math.degrees(instruction.sweep_angle_rad)
        path = QPainterPath()
        path.arcMoveTo(rect, start_angle_deg)
        path.arcTo(rect, start_angle_deg, sweep_angle_deg)
        return path


class SimObserverP0Workspace(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._store: ObserverStore | None = None
        self._current_selection: ObserverSelection | None = None
        self._current_structure_object_id: ObjectId | None = None
        self._structure_note: str | None = None
        self._suspend_selection_events = False
        self._store_loaded_callback: Callable[[ObserverStore], None] | None = None
        self._external_primary_object_id: ObjectId | None = None
        self._external_group_member_ids: tuple[ObjectId, ...] = tuple()
        self._external_sequence_range: SequenceRangeRef | None = None

        layout = QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(12)

        top_bar = QHBoxLayout()
        self._title_label = QLabel("P0 结果确认工作面")
        self._title_label.setStyleSheet("font-size: 16px; font-weight: bold;")
        top_bar.addWidget(self._title_label)
        top_bar.addStretch()

        self._status_label = QLabel("未加载 recording")
        self._status_label.setProperty("data-testid", "sim-observer-status")
        top_bar.addWidget(self._status_label)

        self._load_button = QPushButton("加载 Recording")
        self._load_button.setProperty("data-testid", "btn-sim-observer-load")
        self._load_button.clicked.connect(self._on_load_recording)
        top_bar.addWidget(self._load_button)
        layout.addLayout(top_bar)

        body_splitter = QSplitter(Qt.Horizontal)
        body_splitter.setChildrenCollapsible(False)

        summary_group = QGroupBox("结果摘要视图")
        summary_layout = QVBoxLayout(summary_group)
        self._summary_list = QListWidget()
        self._summary_list.setProperty("data-testid", "list-sim-observer-summary")
        self._summary_list.currentItemChanged.connect(self._on_issue_changed)
        summary_layout.addWidget(self._summary_list)
        body_splitter.addWidget(summary_group)

        right_splitter = QSplitter(Qt.Vertical)
        right_splitter.setChildrenCollapsible(False)
        top_right = QSplitter(Qt.Horizontal)
        top_right.setChildrenCollapsible(False)

        canvas_group = QGroupBox("结果画布视图")
        canvas_layout = QVBoxLayout(canvas_group)
        self._canvas_view = ResultCanvasView()
        self._canvas_hint = QLabel("显示路径与当前选中对象/对象组高亮")
        self._canvas_hint.setStyleSheet("color: #888;")
        canvas_layout.addWidget(self._canvas_view)
        canvas_layout.addWidget(self._canvas_hint)
        top_right.addWidget(canvas_group)

        structure_group = QGroupBox("路径结构视图")
        structure_layout = QVBoxLayout(structure_group)
        self._structure_list = QListWidget()
        self._structure_list.setProperty("data-testid", "list-sim-observer-structure")
        self._structure_list.currentItemChanged.connect(self._on_structure_changed)
        structure_layout.addWidget(self._structure_list)
        top_right.addWidget(structure_group)

        detail_group = QGroupBox("上下文详情视图")
        detail_layout = QVBoxLayout(detail_group)
        self._detail_text = QPlainTextEdit()
        self._detail_text.setReadOnly(True)
        self._detail_text.setProperty("data-testid", "text-sim-observer-detail")
        detail_layout.addWidget(self._detail_text)

        right_splitter.addWidget(top_right)
        right_splitter.addWidget(detail_group)

        body_splitter.addWidget(right_splitter)
        body_splitter.setStretchFactor(0, 2)
        body_splitter.setStretchFactor(1, 5)
        layout.addWidget(body_splitter)
        self._render_global_empty_state()

    def load_store(self, store: ObserverStore) -> None:
        self._store = store
        self._current_selection = None
        self._current_structure_object_id = None
        self._structure_note = None
        self._clear_external_backlink_state()
        self._render_from_store(select_default=True)
        if self._store_loaded_callback is not None:
            self._store_loaded_callback(store)

    def load_mapping(self, payload: Mapping[str, object]) -> None:
        self.load_store(ObserverStore.from_mapping(payload))

    def set_store_loaded_callback(self, callback: Callable[[ObserverStore], None] | None) -> None:
        self._store_loaded_callback = callback

    def apply_backlink(self, backlink: P1Backlink | None) -> None:
        if self._store is None:
            return
        self._clear_external_backlink_state()
        if backlink is None:
            self._render_from_store()
            return

        issue_id = self._preferred_issue_for_backlink(backlink)
        if issue_id is not None:
            self._current_selection = self._store.reselect_issue(issue_id)
            self._current_structure_object_id = self._preferred_structure_focus(self._current_selection)

        if backlink.backlink_level == "single_object":
            self._external_primary_object_id = backlink.object_id
            self._current_structure_object_id = backlink.object_id
        elif backlink.backlink_level == "object_group":
            self._external_group_member_ids = backlink.group_member_ids
            if backlink.group_member_ids:
                self._current_structure_object_id = backlink.group_member_ids[0]
        elif backlink.backlink_level == "sequence_range":
            self._external_sequence_range = backlink.sequence_range_ref
            if backlink.sequence_range_ref is not None:
                self._current_structure_object_id = backlink.sequence_range_ref.start_segment_id

        self._structure_note = f"P1 回接: {backlink.reason_text}" if backlink.reason_text else "P1 已回接到 P0。"
        self._render_from_store()

    def _on_load_recording(self) -> None:
        file_path, _ = QFileDialog.getOpenFileName(
            self,
            "选择 recording JSON",
            str(Path.cwd()),
            "JSON Files (*.json);;All Files (*)",
        )
        if not file_path:
            return
        try:
            self.load_store(ObserverStore.from_file(file_path))
        except Exception as exc:
            QMessageBox.warning(self, "加载失败", str(exc))

    def _on_issue_changed(self, current: QListWidgetItem | None, previous: QListWidgetItem | None) -> None:
        del previous
        if self._suspend_selection_events or self._store is None or current is None:
            return
        issue_id = current.data(Qt.UserRole)
        if issue_id is None:
            return
        self._clear_external_backlink_state()
        self._current_selection = self._store.select_issue(issue_id)
        self._current_structure_object_id = self._preferred_structure_focus(self._current_selection)
        self._structure_note = None
        self._render_from_store()

    def _on_structure_changed(self, current: QListWidgetItem | None, previous: QListWidgetItem | None) -> None:
        del previous
        if self._suspend_selection_events or self._store is None or current is None:
            return
        object_id = current.data(Qt.UserRole)
        if object_id is None:
            return

        self._clear_external_backlink_state()
        self._current_structure_object_id = object_id
        if not self._store.issue_ids:
            self._structure_note = "当前 recording 无 issue，结构焦点已切换到路径事实。"
            self._render_from_store()
            return
        current_primary_object_id = self._store.current_primary_object_id
        current_group_member_ids = set(self._store.current_group_member_ids)
        if object_id == current_primary_object_id or object_id in current_group_member_ids:
            self._structure_note = "结构视图当前焦点位于已选 issue 的对象上下文。"
            self._render_from_store()
            return

        matched_issue_ids = self._store.find_issue_ids_for_object(object_id)
        if len(matched_issue_ids) == 1:
            self._current_selection = self._store.select_issue(matched_issue_ids[0])
            self._structure_note = "结构反选已回接到唯一 issue。"
        elif len(matched_issue_ids) > 1:
            self._structure_note = "当前对象命中多个 issue，未自动切换摘要。"
        else:
            self._structure_note = "当前对象没有可唯一回接的 issue。"
        self._render_from_store()

    def _render_from_store(self, *, select_default: bool = False) -> None:
        if self._store is None:
            self._render_global_empty_state()
            return

        if select_default:
            self._current_selection = self._store.select_default_issue()
            self._current_structure_object_id = self._preferred_structure_focus(self._current_selection)
        self._ensure_structure_focus()

        self._validate_render_state()
        self._set_status_label()
        self._rebuild_summary_list()
        self._rebuild_structure_list()
        self._canvas_view.render_selection(
            self._store,
            self._current_selection,
            focused_object_id=self._current_structure_object_id,
            external_primary_object_id=self._external_primary_object_id,
            external_group_member_ids=self._external_group_member_ids,
            external_sequence_member_ids=(
                self._external_sequence_range.member_segment_ids if self._external_sequence_range is not None else tuple()
            ),
        )
        self._canvas_hint.setText(self._build_canvas_hint())
        self._detail_text.setPlainText(self._build_detail_text())

    def _rebuild_summary_list(self) -> None:
        self._suspend_selection_events = True
        try:
            self._summary_list.clear()
            if self._store is None:
                return
            if not self._store.issue_ids:
                self._summary_list.addItem("当前 recording 没有 issue")
                return

            selected_issue_id = self._current_selection.selected_id if self._current_selection is not None else None
            selected_row = 0
            for index, issue in enumerate(self._store.normalized_data.issues):
                label = f"[{issue.issue_level}] {issue.display_label}"
                item = QListWidgetItem(label)
                item.setData(Qt.UserRole, issue.object_id)
                self._summary_list.addItem(item)
                if issue.object_id == selected_issue_id:
                    selected_row = index
            self._summary_list.setCurrentRow(selected_row)
        finally:
            self._suspend_selection_events = False

    def _rebuild_structure_list(self) -> None:
        self._suspend_selection_events = True
        try:
            self._structure_list.clear()
            if self._store is None:
                return
            selected_object_id = None
            group_member_ids: set[ObjectId] = set()
            sequence_member_ids: set[ObjectId] = set()
            if self._current_selection is not None and self._current_selection.primary_association.selection_state is ObserverState.RESOLVED:
                selected_object_id = self._store.current_primary_object_id
                group_member_ids = set(self._store.current_group_member_ids)
            if self._external_primary_object_id is not None:
                selected_object_id = self._external_primary_object_id
            if self._external_group_member_ids:
                group_member_ids.update(self._external_group_member_ids)
            if self._external_sequence_range is not None:
                sequence_member_ids.update(self._external_sequence_range.member_segment_ids)

            selected_row = None
            for index, segment in enumerate(self._store.current_path_segments_for_structure()):
                prefix = "  "
                if segment.object_id == selected_object_id:
                    prefix = ">>"
                elif segment.object_id in group_member_ids:
                    prefix = "--"
                elif segment.object_id in sequence_member_ids:
                    prefix = "=="
                elif self._current_structure_object_id == segment.object_id:
                    prefix = "[]"
                item = QListWidgetItem(
                    f"{prefix} 段 {segment.segment_index} | {segment.segment_type} | "
                    f"({segment.start_point.x:.1f}, {segment.start_point.y:.1f}) -> "
                    f"({segment.end_point.x:.1f}, {segment.end_point.y:.1f})"
                )
                item.setData(Qt.UserRole, segment.object_id)
                if segment.object_id == selected_object_id:
                    item.setBackground(QColor("#3d2f2f"))
                elif segment.object_id in group_member_ids:
                    item.setBackground(QColor("#3d3624"))
                elif segment.object_id in sequence_member_ids:
                    item.setBackground(QColor("#1f3f2f"))
                elif self._current_structure_object_id == segment.object_id:
                    item.setBackground(QColor("#213448"))
                    selected_row = index
                self._structure_list.addItem(item)

            if selected_row is None and self._current_structure_object_id is not None:
                for index in range(self._structure_list.count()):
                    if self._structure_list.item(index).data(Qt.UserRole) == self._current_structure_object_id:
                        selected_row = index
                        break
            if selected_row is not None:
                self._structure_list.setCurrentRow(selected_row)
        finally:
            self._suspend_selection_events = False

    def _preferred_structure_focus(self, selection: ObserverSelection | None) -> ObjectId | None:
        if selection is None:
            return None
        if selection.primary_association.selection_state is not ObserverState.RESOLVED:
            return None
        if selection.primary_association.primary_object_id is not None:
            return selection.primary_association.primary_object_id
        group_member_ids = self._store.current_group_member_ids if self._store is not None else tuple()
        return group_member_ids[0] if group_member_ids else None

    def _ensure_structure_focus(self) -> None:
        if self._store is None or self._current_structure_object_id is not None:
            return
        path_segments = self._store.current_path_segments_for_structure()
        if path_segments:
            self._current_structure_object_id = path_segments[0].object_id

    def _set_status_label(self) -> None:
        if self._store is None:
            self._status_label.setText("未加载 recording | mode=empty")
            return
        if not self._store.issue_ids:
            self._status_label.setText(
                f"issues=0 | segments={len(self._store.normalized_data.path_segments)} | default=- | mode=empty"
            )
            return
        default_issue_id = self._store.default_issue_id or "-"
        current_state = (
            self._current_selection.primary_association.selection_state.value
            if self._current_selection is not None
            else "-"
        )
        render_mode = self._render_mode()
        self._status_label.setText(
            f"issues={len(self._store.issue_ids)} | "
            f"segments={len(self._store.normalized_data.path_segments)} | "
            f"default={default_issue_id} | mode={render_mode} | state={current_state}"
        )

    def _build_canvas_hint(self) -> str:
        if self._store is None:
            return "未加载 recording。"
        if not self._store.issue_ids:
            if self._current_structure_object_id is not None:
                return "当前 recording 无 P0 issue；蓝色=结构焦点，可浏览路径事实。"
            return "当前 recording 无 P0 issue，只显示路径底图。"
        if self._external_sequence_range is not None:
            return "绿色=来自 P1 的顺序区间回接，蓝色=结构焦点。"
        if self._current_selection is None:
            return "未进入有效 issue 入口。"
        if self._current_selection.primary_association.selection_state is ObserverState.RESOLVED:
            return "红色=主对象，橙色=对象组成员，蓝色=结构焦点。"
        return (
            "当前 issue 处于失败/降级态，画布不做伪对象高亮；"
            "仅保留路径底图和结构焦点。"
        )

    def _render_mode(self) -> str:
        if self._store is None or not self._store.issue_ids:
            return "empty"
        if self._current_selection is None:
            return "idle"
        if self._current_selection.primary_association.selection_state is ObserverState.RESOLVED:
            return "resolved"
        if self._current_selection.failure_reason is not None:
            return "failed"
        return "degraded"

    def _validate_render_state(self) -> None:
        if self._store is None or self._current_selection is None:
            return

        selection_state = self._current_selection.primary_association.selection_state
        if not isinstance(selection_state, ObserverState):
            raise ValueError("Unsupported observer state in P0 workspace.")

        failure_reason = self._current_selection.failure_reason
        if failure_reason is not None and not isinstance(failure_reason, FailureReason):
            raise ValueError("Unsupported failure reason in P0 workspace.")

        primary_association = self._current_selection.primary_association
        if selection_state is not ObserverState.RESOLVED and (
            primary_association.primary_object_id is not None or primary_association.primary_group_id is not None
        ):
            raise ValueError("Non-resolved P0 selection must not expose concrete targets.")

        if self._current_selection.selected_kind == "issue" and self._store.current_issue is None:
            raise ValueError("Current P0 selection points to a missing issue.")

    def _build_detail_text(self) -> str:
        if self._store is None:
            return "请先加载 recording，然后从结果摘要视图选择一个 issue。"
        if not self._store.issue_ids:
            return self._build_path_fact_detail_text()
        if self._current_selection is None:
            return "当前 recording 有 issue，但还没有进入有效的默认入口。"

        summary_title = "-"
        issue = self._store.current_issue
        if issue is not None:
            summary_title = f"[{issue.issue_level}] {issue.display_label}"

        lines = [
            f"摘要入口: {summary_title}",
            f"selection.kind: {self._current_selection.selected_kind}",
            f"selection.id: {self._current_selection.selected_id}",
            f"association.level: {self._current_selection.primary_association.association_level}",
            f"association.state: {self._current_selection.primary_association.selection_state.value}",
            f"primary.object: {self._current_selection.primary_association.primary_object_id or '-'}",
            f"primary.group: {self._current_selection.primary_association.primary_group_id or '-'}",
            f"anchor.type: {self._current_selection.primary_association.primary_anchor_type or '-'}",
            f"failure.reason: {self._current_selection.failure_reason.value if self._current_selection.failure_reason else '-'}",
            f"structure.focus: {self._current_structure_object_id or '-'}",
        ]
        if self._structure_note:
            lines.append(f"structure.note: {self._structure_note}")
        if self._external_sequence_range is not None:
            lines.append(
                "external.sequence: "
                + ", ".join(str(member_id) for member_id in self._external_sequence_range.member_segment_ids)
            )

        reason = self._current_selection.primary_association.selection_reason
        if reason is not None:
            lines.append(f"reason.code: {reason.reason_code}")
            lines.append(f"reason.text: {reason.reason_text}")
        if self._current_selection.group_result is not None:
            lines.append(f"group.state: {self._current_selection.group_result.state.value}")
            lines.append(f"group.cardinality: {self._current_selection.group_result.group_cardinality}")
            lines.append(
                "group.members: "
                + ", ".join(str(member_id) for member_id in self._current_selection.group_result.member_object_ids)
            )
        primary_object = self._store.current_primary_object
        if isinstance(primary_object, PathSegmentObject):
            lines.extend(self._segment_fact_lines("primary.segment", primary_object))
        if issue is not None:
            self._append_source_ref_lines(lines, issue.source_refs)
        return "\n".join(lines)

    def _build_path_fact_detail_text(self) -> str:
        if self._store is None:
            return "请先加载 recording，然后从结果摘要视图选择一个 issue。"
        focused_segment = self._focused_segment()
        lines = [
            "当前 recording 没有可用于 P0 的 issue 入口。",
            "browse.mode: path_facts_only",
            f"segments: {len(self._store.normalized_data.path_segments)}",
            f"path.points: {len(self._store.normalized_data.path_points)}",
            f"time_anchors: {len(self._store.normalized_data.time_anchors)}",
            f"alerts: {len(self._store.normalized_data.alerts)}",
            f"structure.focus: {self._current_structure_object_id or '-'}",
        ]
        if self._structure_note:
            lines.append(f"structure.note: {self._structure_note}")
        if focused_segment is None:
            lines.append("hint: 可在结构视图中选择一段路径查看几何事实。")
            return "\n".join(lines)
        lines.extend(self._segment_fact_lines("focused.segment", focused_segment))
        self._append_source_ref_lines(lines, focused_segment.source_refs)
        return "\n".join(lines)

    def _focused_segment(self) -> PathSegmentObject | None:
        if self._store is None or self._current_structure_object_id is None:
            return None
        for segment in self._store.normalized_data.path_segments:
            if segment.object_id == self._current_structure_object_id:
                return segment
        return None

    def _segment_fact_lines(self, label: str, segment: PathSegmentObject) -> list[str]:
        if self._store is None:
            point_count = 2
            render_mode = "polyline_fallback"
        else:
            point_count = len(segment_render_points(segment, self._store.normalized_data.path_points))
            render_mode = segment_render_instruction(segment, self._store.normalized_data.path_points).mode
        return [
            f"{label}: {segment.segment_index} | {segment.segment_type} | point_count={point_count}",
            f"segment.render.mode: {render_mode}",
            (
                "segment.range: "
                f"({segment.start_point.x:.1f}, {segment.start_point.y:.1f}) -> "
                f"({segment.end_point.x:.1f}, {segment.end_point.y:.1f})"
            ),
            (
                "segment.bbox: "
                f"({segment.bbox.min_x:.1f}, {segment.bbox.min_y:.1f}) -> "
                f"({segment.bbox.max_x:.1f}, {segment.bbox.max_y:.1f})"
            ),
        ]

    def _append_source_ref_lines(self, lines: list[str], source_refs: tuple[object, ...]) -> None:
        lines.append("source.refs:")
        lines.append(f"source.ref.count: {len(source_refs)}")
        if source_refs:
            for source_ref in source_refs:
                lines.append(
                    f"  - {source_ref.source_name} | {source_ref.source_path} | "
                    f"index={source_ref.source_index}"
                )
            return
        lines.append("  - (none)")

    def _render_global_empty_state(self) -> None:
        self._canvas_view.render_selection(None, None)
        self._summary_list.clear()
        self._structure_list.clear()
        self._status_label.setText("未加载 recording | mode=empty")
        self._canvas_hint.setText("未加载 recording。")
        self._detail_text.setPlainText("请先加载 recording，然后从结果摘要视图选择一个 issue。")

    def _clear_external_backlink_state(self) -> None:
        self._external_primary_object_id = None
        self._external_group_member_ids = tuple()
        self._external_sequence_range = None

    def _preferred_issue_for_backlink(self, backlink: P1Backlink) -> ObjectId | None:
        if self._store is None:
            return None
        if backlink.backlink_level == "single_object" and backlink.object_id is not None:
            matched_issue_ids = self._store.find_issue_ids_for_object(backlink.object_id)
            return matched_issue_ids[0] if len(matched_issue_ids) == 1 else None
        if backlink.backlink_level == "object_group" and backlink.group_member_ids:
            matched_issue_ids = self._store.find_issue_ids_for_objects(backlink.group_member_ids)
            return matched_issue_ids[0] if len(matched_issue_ids) == 1 else None
        if backlink.backlink_level == "sequence_range" and backlink.sequence_range_ref is not None:
            matched_issue_ids = self._store.find_issue_ids_for_objects(backlink.sequence_range_ref.member_segment_ids)
            return matched_issue_ids[0] if len(matched_issue_ids) == 1 else None
        return None
