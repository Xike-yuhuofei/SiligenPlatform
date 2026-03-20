"""Top-level workspace container for P0 and P1 observer workspaces."""

from __future__ import annotations

from typing import Mapping

from PyQt5.QtWidgets import QHBoxLayout, QPushButton, QStackedWidget, QVBoxLayout, QWidget

from ..state.observer_store import ObserverStore, P1Backlink
from .p0_workspace import SimObserverP0Workspace
from .p1_workspace import SimObserverP1Workspace


class SimObserverWorkspace(QWidget):
    def __init__(self, parent: QWidget | None = None) -> None:
        super().__init__(parent)
        self._store: ObserverStore | None = None

        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        top_bar = QHBoxLayout()
        top_bar.setContentsMargins(12, 12, 12, 0)
        top_bar.setSpacing(8)

        self._show_p0_button = QPushButton("P0 结果确认")
        self._show_p0_button.clicked.connect(self._show_p0)
        top_bar.addWidget(self._show_p0_button)

        self._show_p1_button = QPushButton("进入 P1 过程分析")
        self._show_p1_button.clicked.connect(self._show_p1)
        top_bar.addWidget(self._show_p1_button)
        top_bar.addStretch()
        layout.addLayout(top_bar)

        self._stack = QStackedWidget()
        self._p0_workspace = SimObserverP0Workspace(self)
        self._p1_workspace = SimObserverP1Workspace(self)
        self._stack.addWidget(self._p0_workspace)
        self._stack.addWidget(self._p1_workspace)
        layout.addWidget(self._stack)

        self._p0_workspace.set_store_loaded_callback(self._sync_store_from_p0)
        self._p1_workspace.set_return_callback(self._show_p0)
        self._p1_workspace.set_backlink_callback(self._apply_backlink_to_p0)
        self._show_p0()

    def load_store(self, store: ObserverStore) -> None:
        self._p0_workspace.load_store(store)

    def load_mapping(self, payload: Mapping[str, object]) -> None:
        self.load_store(ObserverStore.from_mapping(payload))

    def _sync_store_from_p0(self, store: ObserverStore) -> None:
        self._store = store
        self._p1_workspace.load_store(store)

    def _apply_backlink_to_p0(self, backlink: P1Backlink) -> None:
        self._p0_workspace.apply_backlink(backlink)
        self._show_p0()

    def _show_p0(self) -> None:
        self._stack.setCurrentWidget(self._p0_workspace)
        self._show_p0_button.setEnabled(False)
        self._show_p1_button.setEnabled(True)

    def _show_p1(self) -> None:
        self._stack.setCurrentWidget(self._p1_workspace)
        self._show_p0_button.setEnabled(True)
        self._show_p1_button.setEnabled(False)


__all__ = ["SimObserverWorkspace"]
