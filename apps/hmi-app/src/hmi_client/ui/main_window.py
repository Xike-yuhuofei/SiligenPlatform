"""PyQt5 Main Window for Siligen HMI."""
import os
import sys
import time
import logging
import html
import json
import threading
from pathlib import Path

try:
    from hmi_client.qt_env import configure_qt_environment
except ImportError:  # pragma: no cover - script-mode fallback
    from qt_env import configure_qt_environment
try:
    from hmi_client.module_paths import ensure_hmi_application_path
except ImportError:  # pragma: no cover - script-mode fallback
    from module_paths import ensure_hmi_application_path  # type: ignore

configure_qt_environment(headless=False)
ensure_hmi_application_path()

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QLabel, QPushButton, QLineEdit, QGridLayout, QFrame,
    QSlider, QSpinBox, QDoubleSpinBox, QCheckBox, QTabWidget, QFileDialog,
    QProgressBar, QListWidget, QListWidgetItem, QMessageBox, QDialog, QDialogButtonBox,
    QComboBox, QFormLayout, QScrollArea, QRadioButton, QStatusBar, QPlainTextEdit,
    QTextBrowser
)
from PyQt5.QtCore import QTimer, Qt, QSize, QEvent, QThread, pyqtSignal
from PyQt5.QtGui import QFont, QColor

try:
    from PyQt5.QtWebEngineWidgets import QWebEngineView
    WEB_ENGINE_AVAILABLE = True
except ImportError:
    QWebEngineView = None
    WEB_ENGINE_AVAILABLE = False

from client import (
    BackendManager,
    build_offline_launch_result,
    CommandProtocol,
    LaunchUiState,
    LaunchResult,
    PreviewSnapshotMeta,
    RecoveryWorker,
    StartupWorker,
    TcpClient,
    build_launch_ui_state,
    build_online_capability_message,
    build_recovery_action_decision,
    build_recovery_finished_message,
    build_runtime_degradation_result,
    build_startup_error_message,
    detect_runtime_degradation_result,
    detect_runtime_requalification_result,
    is_online_ready,
    load_supervisor_policy_from_env,
    normalize_launch_mode,
)
from hmi_application.preview_session import (
    MotionPreviewMeta,
    PreviewDiagnosticNotice,
    PreviewSessionOwner,
    PreviewSnapshotWorker,
)
from client.auth import AuthManager
from .dxf_default_paths import build_default_dxf_candidates
from .offline_preview_builder import build_offline_preview_payload
from .styles import DARK_THEME
from .recipe_config_widget import RecipeConfigWidget


_UI_LOGGER = logging.getLogger("hmi.ui")
if not _UI_LOGGER.handlers:
    log_dir = Path("logs")
    log_dir.mkdir(parents=True, exist_ok=True)
    handler = logging.FileHandler(log_dir / "hmi_ui.log", encoding="utf-8")
    formatter = logging.Formatter("%(asctime)s [%(levelname)s] %(message)s")
    handler.setFormatter(formatter)
    _UI_LOGGER.addHandler(handler)
    _UI_LOGGER.setLevel(logging.INFO)
    _UI_LOGGER.propagate = False


DEFAULT_ASPECT_RATIO = 16 / 9
DEFAULT_BASE_WIDTH = 1600
ASPECT_RATIO_ENV = "SILIGEN_HMI_ASPECT_RATIO"
STATUS_LOG_HISTORY_LIMIT = 200


def _parse_aspect_ratio(value: str):
    if not value:
        return None
    text = value.strip()
    for sep in (":", "/"):
        if sep in text:
            left, right = text.split(sep, 1)
            try:
                width = float(left)
                height = float(right)
            except ValueError:
                return None
            if width > 0 and height > 0:
                return width / height
            return None
    try:
        ratio = float(text)
    except ValueError:
        return None
    return ratio if ratio > 0 else None


def _resolve_aspect_ratio() -> float:
    env_ratio = _parse_aspect_ratio(os.getenv(ASPECT_RATIO_ENV, ""))
    return env_ratio if env_ratio else DEFAULT_ASPECT_RATIO


def _resolve_min_size(aspect_ratio: float) -> QSize:
    base_height = max(1, int(round(DEFAULT_BASE_WIDTH / aspect_ratio)))
    return QSize(DEFAULT_BASE_WIDTH, base_height)


class JogButton(QPushButton):
    """Jog button with stable press/release handling for touch and mouse."""

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.setAttribute(Qt.WA_AcceptTouchEvents, True)
        self.setFocusPolicy(Qt.NoFocus)
        self._touch_active = False
        self._touch_id = None

    def event(self, event):
        if event.type() == QEvent.TouchBegin:
            self._touch_active = True
            points = event.touchPoints()
            self._touch_id = points[0].id() if points else None
            self.setDown(True)
            self.grabMouse()
            self.pressed.emit()
            _UI_LOGGER.info("JogButton touch begin text=%s id=%s", self.text(), self._touch_id)
            return True
        if event.type() == QEvent.TouchUpdate:
            return True
        if event.type() in (QEvent.TouchEnd, QEvent.TouchCancel):
            if self._touch_active:
                self._touch_active = False
                self._touch_id = None
                self.setDown(False)
                self.releaseMouse()
                self.released.emit()
                _UI_LOGGER.info("JogButton touch end text=%s", self.text())
            return True
        return super().event(event)

    def mousePressEvent(self, event):
        if self._touch_active:
            event.ignore()
            return
        super().mousePressEvent(event)
        if event.button() == Qt.LeftButton:
            self.grabMouse()
            _UI_LOGGER.info("JogButton mouse press text=%s", self.text())

    def mouseReleaseEvent(self, event):
        if self._touch_active:
            event.ignore()
            return
        if event.button() == Qt.LeftButton:
            self.releaseMouse()
        super().mouseReleaseEvent(event)
        if event.button() == Qt.LeftButton:
            _UI_LOGGER.info("JogButton mouse release text=%s", self.text())


class LoginDialog(QDialog):
    """Login dialog for user authentication."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("用户登录")
        self.setProperty("data-testid", "dialog-login")
        self.setFixedSize(300, 200)

        layout = QVBoxLayout(self)
        layout.setSpacing(15)

        # Username
        user_layout = QHBoxLayout()
        user_layout.addWidget(QLabel("用户名:"))
        self._username_input = QLineEdit()
        self._username_input.setProperty("data-testid", "input-username")
        self._username_input.setPlaceholderText("operator/tech/engineer")
        user_layout.addWidget(self._username_input)
        layout.addLayout(user_layout)

        # PIN
        pin_layout = QHBoxLayout()
        pin_layout.addWidget(QLabel("密码:"))
        self._pin_input = QLineEdit()
        self._pin_input.setProperty("data-testid", "input-password")
        self._pin_input.setEchoMode(QLineEdit.Password)
        self._pin_input.setPlaceholderText("PIN码")
        pin_layout.addWidget(self._pin_input)
        layout.addLayout(pin_layout)

        # Buttons
        btn_layout = QHBoxLayout()
        self._login_btn = QPushButton("登录")
        self._login_btn.setProperty("data-testid", "btn-login")
        self._login_btn.setProperty("role", "primary")
        self._login_btn.clicked.connect(self.accept)
        btn_layout.addWidget(self._login_btn)

        self._cancel_btn = QPushButton("取消")
        self._cancel_btn.setProperty("data-testid", "btn-login-cancel")
        self._cancel_btn.clicked.connect(self.reject)
        btn_layout.addWidget(self._cancel_btn)
        layout.addLayout(btn_layout)

    @property
    def username(self) -> str:
        return self._username_input.text()

    @property
    def pin(self) -> str:
        return self._pin_input.text()





class AspectRatioContainer(QWidget):
    def __init__(self, content: QWidget, aspect_ratio: float, parent=None):
        super().__init__(parent)
        self._content = content
        self._aspect_ratio = aspect_ratio if aspect_ratio > 0 else DEFAULT_ASPECT_RATIO
        self._content.setParent(self)

    def resizeEvent(self, event):
        rect = self.rect()
        if rect.width() <= 0 or rect.height() <= 0:
            super().resizeEvent(event)
            return
        target_width = rect.width()
        target_height = int(round(target_width / self._aspect_ratio))
        if target_height > rect.height():
            target_height = rect.height()
            target_width = int(round(target_height * self._aspect_ratio))
        offset_x = (rect.width() - target_width) // 2
        offset_y = (rect.height() - target_height) // 2
        self._content.setGeometry(offset_x, offset_y, target_width, target_height)
        super().resizeEvent(event)


class HomeAutoWorker(QThread):
    completed = pyqtSignal(bool, str)

    def __init__(
        self,
        *,
        host: str,
        port: int,
        axes: list[str] | None,
        force_rehome: bool,
        timeout_ms: int = 0,
    ) -> None:
        super().__init__()
        self._host = host
        self._port = port
        self._axes = list(axes) if axes else None
        self._force_rehome = force_rehome
        self._timeout_ms = timeout_ms
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

    def run(self) -> None:
        client = None
        ok = False
        message = ""
        try:
            if self._is_cancelled():
                return
            client = TcpClient(host=self._host, port=self._port)
            with self._cancel_lock:
                self._client_ref = client
            if not client.connect():
                message = "无法连接后端，请检查TCP链路"
            else:
                _protocol = CommandProtocol(client)
                ok, message = _protocol.home_auto(
                    self._axes,
                    force=self._force_rehome,
                    wait_for_completion=True,
                    timeout_ms=self._timeout_ms,
                )
                if self._is_cancelled():
                    return
        except Exception as exc:
            message = str(exc) or "回零执行异常"
        finally:
            if client is not None:
                try:
                    client.disconnect()
                except Exception:
                    pass
            with self._cancel_lock:
                self._client_ref = None
        if not self._is_cancelled():
            self.completed.emit(ok, message)


class MainWindow(QMainWindow):
    def __init__(self, launch_mode: str = "online"):
        super().__init__()
        self._requested_launch_mode = normalize_launch_mode(launch_mode)
        self._launch_result: LaunchResult | None = None
        self._session_snapshot = None
        self._supervisor_stage_events = []
        self._startup_worker = None
        self._recovery_worker = None
        self._pending_recovery_action = ""
        self._supervisor_policy = load_supervisor_policy_from_env()
        self._aspect_ratio = _resolve_aspect_ratio()
        min_size = _resolve_min_size(self._aspect_ratio)
        self.setWindowTitle("思力根运动控制器")
        self.setMinimumSize(min_size)
        self.resize(min_size)
        self.setStyleSheet(DARK_THEME)

        self._client = TcpClient()
        self._protocol = CommandProtocol(self._client)
        self._backend = BackendManager()
        self._jog_speed = 20.0
        self._jog_press_time = None
        self._last_jog_context = None
        self._dxf_loaded = False
        self._dxf_filepath = ""
        self._dxf_artifact_id = ""
        self._current_plan_id = ""
        self._current_plan_fingerprint = ""
        self._current_job_id = ""
        self._dxf_dry_run_speed_custom = None
        self._temp_preview_file = None
        self._dxf_total_length_val = 0.0
        self._dxf_segment_count_cache = 0
        self._dxf_est_time_val = "-"
        self._preview_session = PreviewSessionOwner()
        self._preview_gate = self._preview_session.gate
        self._preview_plan_dry_run = None
        self._preview_source = ""
        self._home_auto_worker = None
        self._home_request_generation = 0
        self._preview_snapshot_worker = None
        self._preview_request_generation = 0
        self._preview_refresh_inflight = False
        self._preview_state_resync_pending = False
        self._last_preview_resync_attempt_ts = 0.0
        self._preview_dom_ready = False
        self._runtime_status_fault = False
        self._last_status_error_notice_ts = 0.0
        self._last_status = None
        self._last_status_ts = 0.0
        self._launch_ui_state = None
        self._sync_preview_session_fields()

        # Production statistics
        self._production_running = False
        self._production_paused = False
        self._production_dry_run = False
        self._completed_count = 0
        self._last_completed_count_seen = 0
        self._target_count = 100
        self._cycle_times = []
        self._last_cycle_start = 0
        self._run_start_time = 0
        self._total_run_time = 0


        # Maintenance tracking
        self._needle_count = 0
        self._last_purge_time = time.time()
        self._maintenance_threshold = 10000
        self._last_alarms_snapshot = []
        self._status_log_entries = []
        self._last_status_log_message = ""

        # User permission management
        self._auth = AuthManager(auto_logout_minutes=5)

        self._setup_ui()
        self._setup_timer()
        self._refresh_launch_status_ui()
        self._apply_mode_capabilities()
        self._show_initial_session_message()
        if self._requested_launch_mode == "online":
            self._auto_startup()
        else:
            self._apply_launch_result(build_offline_launch_result())
        self._update_home_controls_state()
        self._update_preview_refresh_button_state()
        self._update_preview_playback_controls()
        self._set_preview_message_html("胶点预览待生成", "请先加载DXF文件，再点击“刷新预览”。")

    def _setup_ui(self):
        content = QWidget()
        main_layout = QVBoxLayout(content)
        main_layout.setSpacing(10)
        main_layout.setContentsMargins(20, 20, 20, 0) # Bottom 0 to merge with status bar

        # Global Status Bar (Top)
        main_layout.addWidget(self._create_global_status_bar())

        # Main Content Area
        content_layout = QHBoxLayout()
        content_layout.setSpacing(20)

        # Tab Widget - Workflow Oriented
        tabs = QTabWidget()
        tabs.setProperty("data-testid", "main-tabs")
        self._main_tabs = tabs
        self._production_tab = self._create_production_tab()
        self._setup_tab = self._create_setup_tab()
        self._recipe_tab = self._create_recipe_tab()
        self._alarm_panel = self._create_alarm_panel()
        tabs.addTab(self._production_tab, "生产")
        tabs.addTab(self._setup_tab, "设置")
        tabs.addTab(self._recipe_tab, "配置")
        tabs.addTab(self._alarm_panel, "报警")
        content_layout.addWidget(tabs)

        main_layout.addLayout(content_layout)
        
        self._aspect_container = AspectRatioContainer(content, self._aspect_ratio)
        self.setCentralWidget(self._aspect_container)

        # Custom Bottom Status Bar
        self.setStatusBar(self._create_custom_bottom_bar())
        self.statusBar().messageChanged.connect(self._on_status_message_changed)
        self.statusBar().showMessage("状态初始化中")

    def _create_custom_bottom_bar(self) -> QStatusBar:
        bar = QStatusBar()
        bar.setSizeGripEnabled(False)
        bar.setStyleSheet("QStatusBar { background-color: #2d2d2d; border-top: 1px solid #444; color: #ccc; }")
        
        # Container widget for custom stats
        container = QWidget()
        layout = QHBoxLayout(container)
        layout.setContentsMargins(20, 0, 20, 0)
        layout.setSpacing(30)

        # 1. Progress Section (Middle-Left)
        prog_layout = QHBoxLayout()
        prog_layout.addWidget(QLabel("批次:"))
        self._completion_progress = QProgressBar()
        self._completion_progress.setProperty("data-testid", "progress-completion")
        self._completion_progress.setFixedWidth(200)
        self._completion_progress.setFixedHeight(12)
        self._completion_progress.setTextVisible(False)
        self._completion_progress.setValue(0)
        prog_layout.addWidget(self._completion_progress)
        layout.addLayout(prog_layout)

        # 2. Stats Section (Middle-Right)
        
        # UPH
        self._uph_label = QLabel("0")
        self._uph_label.setProperty("data-testid", "label-uph")
        self._uph_label.setStyleSheet("font-weight: bold;")
        layout.addWidget(QLabel("UPH:"))
        layout.addWidget(self._uph_label)

        # Remaining Time
        self._remaining_time_label = QLabel("-")
        self._remaining_time_label.setProperty("data-testid", "label-remaining-time")
        layout.addWidget(QLabel("剩余:"))
        layout.addWidget(self._remaining_time_label)
        
        # Runtime (always visible)
        self._runtime_label = QLabel("00:00:00")
        self._runtime_label.setProperty("data-testid", "label-run-time")
        layout.addWidget(QLabel("运行:"))
        layout.addWidget(self._runtime_label)

        # Add container to status bar (Permanent widget aligns right)
        bar.addPermanentWidget(container)
        
        return bar

    def _create_global_status_bar(self) -> QFrame:
        """Create global status bar at top of window."""
        frame = QFrame()
        frame.setProperty("role", "global-status")
        frame.setProperty("data-testid", "indicator-global-status")
        layout = QHBoxLayout(frame)
        layout.setContentsMargins(10, 5, 10, 5)

        # Operation Status
        layout.addWidget(QLabel("状态:"))
        self._operation_status = QLabel("空闲")
        self._operation_status.setProperty("data-testid", "label-current-operation")
        self._operation_status.setProperty("role", "operation-status")
        layout.addWidget(self._operation_status)

        layout.addSpacing(30)

        layout.addWidget(QLabel("模式:"))
        self._launch_mode_label = QLabel("Online")
        self._launch_mode_label.setProperty("data-testid", "label-launch-mode")
        layout.addWidget(self._launch_mode_label)

        layout.addSpacing(30)

        # Connection Indicators
        layout.addWidget(QLabel("TCP:"))
        self._tcp_led = QLabel()
        self._tcp_led.setProperty("data-testid", "indicator-tcp-status")
        self._tcp_led.setProperty("role", "led-off")
        self._tcp_led.setFixedSize(16, 16)
        layout.addWidget(self._tcp_led)
        self._tcp_state_label = QLabel("未连接")
        self._tcp_state_label.setProperty("data-testid", "label-tcp-state")
        layout.addWidget(self._tcp_state_label)
        layout.addSpacing(10)
        layout.addWidget(QLabel("硬件:"))
        self._hw_led = QLabel()
        self._hw_led.setProperty("data-testid", "indicator-hw-status")
        self._hw_led.setProperty("role", "led-off")
        self._hw_led.setFixedSize(16, 16)
        layout.addWidget(self._hw_led)
        self._hw_state_label = QLabel("不可用")
        self._hw_state_label.setProperty("data-testid", "label-hw-state")
        layout.addWidget(self._hw_state_label)

        layout.addSpacing(30)

        # Progress (when running)
        layout.addWidget(QLabel("进度:"))
        self._global_progress = QProgressBar()
        self._global_progress.setProperty("data-testid", "progress-global")
        self._global_progress.setFixedWidth(200)
        self._global_progress.setValue(0)
        layout.addWidget(self._global_progress)

        layout.addSpacing(30)

        # Maintenance Alert
        self._maintenance_alert = QLabel("")
        self._maintenance_alert.setProperty("data-testid", "alert-maintenance")
        self._maintenance_alert.setStyleSheet("color: #FFC107; font-weight: bold;")
        layout.addWidget(self._maintenance_alert)

        layout.addStretch()

        # Needle Count Display
        layout.addWidget(QLabel("针头:"))
        self._needle_count_label = QLabel("0/10000")
        self._needle_count_label.setProperty("data-testid", "label-needle-count")
        layout.addWidget(self._needle_count_label)

        # Reset Needle Button
        self._reset_needle_btn = QPushButton("重置")
        self._reset_needle_btn.setProperty("data-testid", "btn-reset-needle-count")
        self._reset_needle_btn.clicked.connect(self._on_reset_needle)
        layout.addWidget(self._reset_needle_btn)

        layout.addSpacing(20)

        # User Info
        self._user_label = QLabel("操作员")
        self._user_label.setProperty("data-testid", "label-current-user")
        layout.addWidget(self._user_label)

        self._switch_user_btn = QPushButton("切换")
        self._switch_user_btn.setProperty("data-testid", "btn-switch-user")
        self._switch_user_btn.clicked.connect(self._on_switch_user)
        layout.addWidget(self._switch_user_btn)

        layout.addSpacing(20)

        # E-Stop Button (always visible)
        self._global_estop_btn = QPushButton("急停")
        self._global_estop_btn.setProperty("data-testid", "btn-global-estop")
        self._global_estop_btn.setProperty("role", "danger")
        self._global_estop_btn.setFixedSize(80, 40)
        self._global_estop_btn.clicked.connect(self._on_estop)
        layout.addWidget(self._global_estop_btn)

        return frame

    def _create_production_tab(self) -> QWidget:
        """Create Production tab - Optimized Sidebar + Canvas layout."""
        widget = QWidget()
        widget.setProperty("data-testid", "tab-production")
        
        main_layout = QHBoxLayout(widget)
        main_layout.setSpacing(15)
        main_layout.setContentsMargins(15, 15, 15, 15)

        # === Left Sidebar (Control Panel) ===
        sidebar = QWidget()
        sidebar.setFixedWidth(380)
        
        sidebar_layout = QVBoxLayout(sidebar)
        sidebar_layout.setSpacing(15)
        sidebar_layout.setContentsMargins(0, 0, 0, 0)

        # 1. Job Preparation (File Selection)
        job_frame = QFrame()
        job_frame.setProperty("class", "sidebar-card")
        job_layout = QVBoxLayout(job_frame)
        job_layout.setContentsMargins(15, 15, 15, 15)
        job_layout.setSpacing(12)
        
        # Header
        job_title = QLabel("作业准备")
        job_title.setProperty("class", "header-text")
        job_layout.addWidget(job_title)

        # File display & Browse
        file_row = QHBoxLayout()
        file_row.setSpacing(8)
        self._dxf_filename_display = QLineEdit()
        self._dxf_filename_display.setReadOnly(True)
        self._dxf_filename_display.setPlaceholderText("未加载DXF文件")
        self._dxf_filename_display.setProperty("data-testid", "input-current-recipe")
        file_row.addWidget(self._dxf_filename_display)
        
        browse_btn = QPushButton("浏览...")
        browse_btn.setProperty("data-testid", "btn-dxf-browse")
        browse_btn.setFixedWidth(70)
        browse_btn.clicked.connect(self._on_dxf_browse)
        file_row.addWidget(browse_btn)
        job_layout.addLayout(file_row)

        # Actions
        job_actions_row = QHBoxLayout()
        job_actions_row.setSpacing(8)
        self._prod_home_btn = QPushButton("全部回零")
        self._prod_home_btn.setProperty("data-testid", "btn-production-home-all")
        self._prod_home_btn.setCursor(Qt.PointingHandCursor)
        self._prod_home_btn.clicked.connect(lambda: self._on_home(None))
        job_actions_row.addWidget(self._prod_home_btn)

        self._refresh_preview_btn = QPushButton("刷新预览")
        self._refresh_preview_btn.setProperty("data-testid", "btn-dxf-preview-refresh")
        self._refresh_preview_btn.setCursor(Qt.PointingHandCursor)
        self._refresh_preview_btn.clicked.connect(self._generate_dxf_preview)
        job_actions_row.addWidget(self._refresh_preview_btn)
        job_layout.addLayout(job_actions_row)

        # File Info (Compact)
        self._dxf_info_label = QLabel("段数: - | 长度: - | 预估: - | 预览: 待生成")
        self._dxf_info_label.setProperty("class", "sub-text")
        job_layout.addWidget(self._dxf_info_label)
        
        sidebar_layout.addWidget(job_frame)

        # 2. Parameters (Target, Speed, Mode)
        param_frame = QFrame()
        param_frame.setProperty("class", "sidebar-card")
        param_layout = QGridLayout(param_frame)
        param_layout.setContentsMargins(15, 15, 15, 15)
        param_layout.setVerticalSpacing(12)
        param_layout.setHorizontalSpacing(10)

        # Header
        param_title = QLabel("参数设定")
        param_title.setProperty("class", "header-text")
        param_layout.addWidget(param_title, 0, 0, 1, 2)

        # Target Count
        param_layout.addWidget(QLabel("目标产量:"), 1, 0)
        self._target_input = QSpinBox()
        self._target_input.setProperty("data-testid", "input-target-count")
        self._target_input.setRange(1, 99999)
        self._target_input.setValue(100)
        self._target_input.setFixedHeight(28)
        self._target_input.valueChanged.connect(self._on_target_changed)
        param_layout.addWidget(self._target_input, 1, 1)

        # Speed Control (Slider + SpinBox)
        param_layout.addWidget(QLabel("运行速度:"), 2, 0)
        speed_control_layout = QHBoxLayout()
        speed_control_layout.setSpacing(8)
        self._dxf_speed_slider = QSlider(Qt.Horizontal)
        self._dxf_speed_slider.setRange(1, 100) 
        self._dxf_speed_slider.setValue(20)
        self._dxf_speed_slider.valueChanged.connect(self._on_speed_slider_changed)
        speed_control_layout.addWidget(self._dxf_speed_slider)

        self._dxf_speed = QDoubleSpinBox()
        self._dxf_speed.setProperty("data-testid", "input-dxf-speed")
        self._dxf_speed.setRange(0.1, 100.0)
        self._dxf_speed.setValue(20.0)
        self._dxf_speed.setSuffix(" mm/s")
        self._dxf_speed.setFixedWidth(85)
        self._dxf_speed.valueChanged.connect(self._on_speed_spinbox_changed)
        speed_control_layout.addWidget(self._dxf_speed)
        param_layout.addLayout(speed_control_layout, 2, 1)

        # Mode Switch
        param_layout.addWidget(QLabel("运行模式:"), 3, 0)
        mode_layout = QHBoxLayout()
        self._mode_production = QRadioButton("生产")
        self._mode_production.setChecked(True)
        self._mode_production.toggled.connect(self._on_mode_toggled)
        self._mode_dryrun = QRadioButton("空跑")
        self._mode_dryrun.toggled.connect(self._on_mode_toggled)
        mode_layout.addWidget(self._mode_production)
        mode_layout.addWidget(self._mode_dryrun)
        param_layout.addLayout(mode_layout, 3, 1)
        
        sidebar_layout.addWidget(param_frame)

        # Monitor Frame (Moved up, above Core Control)
        monitor_frame = QFrame()
        monitor_frame.setProperty("class", "sidebar-card")
        monitor_layout = QVBoxLayout(monitor_frame)
        monitor_layout.setContentsMargins(15, 15, 15, 15)
        monitor_layout.setSpacing(12)
        
        monitor_header_layout = QHBoxLayout()
        monitor_title = QLabel("当前进度")
        monitor_title.setProperty("class", "header-text")
        monitor_header_layout.addWidget(monitor_title)
        
        self._reset_counter_btn = QPushButton("重置")
        self._reset_counter_btn.setProperty("data-testid", "btn-reset-counter")
        self._reset_counter_btn.setFlat(True)
        self._reset_counter_btn.setCursor(Qt.PointingHandCursor)
        self._reset_counter_btn.setProperty("class", "text-button")
        self._reset_counter_btn.clicked.connect(self._on_reset_counter)
        monitor_header_layout.addWidget(self._reset_counter_btn, alignment=Qt.AlignRight)
        monitor_layout.addLayout(monitor_header_layout)

        status_row = QHBoxLayout()
        status_row.addWidget(QLabel("完成计数:"))
        
        self._completed_label = QLabel("0 / 0")
        self._completed_label.setProperty("data-testid", "label-completed-count")
        self._completed_label.setProperty("class", "stat-value-large")
        self._completed_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        status_row.addWidget(self._completed_label)
        
        monitor_layout.addLayout(status_row)
        sidebar_layout.addWidget(monitor_frame)

        # Push subsequent widgets to bottom
        sidebar_layout.addStretch()

        # 3. Core Control (Big Buttons) - Kept at bottom of sidebar
        control_frame = QFrame()
        control_frame.setProperty("class", "sidebar-card")
        control_layout = QVBoxLayout(control_frame)
        control_layout.setContentsMargins(15, 15, 15, 15)
        control_layout.setSpacing(12)
        
        control_title = QLabel("生产控制")
        control_title.setProperty("class", "header-text")
        control_layout.addWidget(control_title)

        self._prod_start_btn = QPushButton("启动生产")
        self._prod_start_btn.setProperty("data-testid", "btn-production-start")
        self._prod_start_btn.setProperty("role", "production-start")
        self._prod_start_btn.setCursor(Qt.PointingHandCursor)
        self._prod_start_btn.clicked.connect(self._on_production_start_clicked)
        control_layout.addWidget(self._prod_start_btn)

        # Pause / Stop Row
        sub_control_layout = QHBoxLayout()
        sub_control_layout.setSpacing(10)
        
        self._prod_pause_btn = QPushButton("暂停")
        self._prod_pause_btn.setProperty("data-testid", "btn-production-pause")
        self._prod_pause_btn.setProperty("role", "production-secondary")
        self._prod_pause_btn.setCursor(Qt.PointingHandCursor)
        self._prod_pause_btn.clicked.connect(self._on_production_pause)

        self._prod_resume_btn = QPushButton("恢复")
        self._prod_resume_btn.setProperty("data-testid", "btn-production-resume")
        self._prod_resume_btn.setProperty("role", "production-secondary")
        self._prod_resume_btn.setCursor(Qt.PointingHandCursor)
        self._prod_resume_btn.clicked.connect(self._on_production_resume)
        
        self._prod_stop_btn = QPushButton("停止")
        self._prod_stop_btn.setProperty("data-testid", "btn-production-stop")
        self._prod_stop_btn.setProperty("role", "danger-stop")
        self._prod_stop_btn.setCursor(Qt.PointingHandCursor)
        self._prod_stop_btn.clicked.connect(self._on_production_stop)
        
        sub_control_layout.addWidget(self._prod_pause_btn)
        sub_control_layout.addWidget(self._prod_resume_btn)
        sub_control_layout.addWidget(self._prod_stop_btn)
        control_layout.addLayout(sub_control_layout)
        
        sidebar_layout.addWidget(control_frame)

        # === Right Content (Preview Workspace) ===
        preview_container = QFrame()
        preview_container.setProperty("class", "preview-workspace")
        preview_layout = QVBoxLayout(preview_container)
        preview_layout.setContentsMargins(15, 15, 15, 15)
        preview_layout.setSpacing(12)

        # Header: Title + Playback Controls
        preview_header_layout = QHBoxLayout()
        preview_header_layout.setSpacing(15)
        
        preview_title = QLabel("轨迹与过程预览")
        preview_title.setProperty("class", "header-text")
        preview_header_layout.addWidget(preview_title)
        
        preview_header_layout.addStretch()

        playback_controls_layout = QHBoxLayout()
        playback_controls_layout.setSpacing(8)
        
        self._preview_play_btn = QPushButton("播放")
        self._preview_play_btn.setProperty("data-testid", "btn-preview-play")
        self._preview_play_btn.setProperty("class", "preview-control-btn")
        self._preview_play_btn.setCursor(Qt.PointingHandCursor)
        self._preview_play_btn.clicked.connect(self._on_preview_play)
        playback_controls_layout.addWidget(self._preview_play_btn)
        
        self._preview_pause_btn = QPushButton("暂停")
        self._preview_pause_btn.setProperty("data-testid", "btn-preview-pause")
        self._preview_pause_btn.setProperty("class", "preview-control-btn")
        self._preview_pause_btn.setCursor(Qt.PointingHandCursor)
        self._preview_pause_btn.clicked.connect(self._on_preview_pause)
        playback_controls_layout.addWidget(self._preview_pause_btn)
        
        self._preview_replay_btn = QPushButton("重播")
        self._preview_replay_btn.setProperty("data-testid", "btn-preview-replay")
        self._preview_replay_btn.setProperty("class", "preview-control-btn")
        self._preview_replay_btn.setCursor(Qt.PointingHandCursor)
        self._preview_replay_btn.clicked.connect(self._on_preview_replay)
        playback_controls_layout.addWidget(self._preview_replay_btn)
        
        self._preview_speed_combo = QComboBox()
        self._preview_speed_combo.setProperty("data-testid", "combo-preview-speed")
        self._preview_speed_combo.setProperty("class", "preview-control-combo")
        self._preview_speed_combo.addItem("0.5x", 0.5)
        self._preview_speed_combo.addItem("1.0x", 1.0)
        self._preview_speed_combo.addItem("2.0x", 2.0)
        self._preview_speed_combo.addItem("4.0x", 4.0)
        self._preview_speed_combo.setCurrentIndex(1)
        self._preview_speed_combo.currentIndexChanged.connect(self._on_preview_speed_changed)
        playback_controls_layout.addWidget(self._preview_speed_combo)

        preview_header_layout.addLayout(playback_controls_layout)

        self._preview_playback_status_label = QLabel("动态预览: 不可用")
        self._preview_playback_status_label.setProperty("data-testid", "label-preview-playback-state")
        self._preview_playback_status_label.setProperty("class", "sub-text")
        self._preview_playback_status_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        preview_header_layout.addWidget(self._preview_playback_status_label)

        preview_layout.addLayout(preview_header_layout)

        # Preview Tabs
        self._preview_tabs = QTabWidget()
        self._preview_tabs.setProperty("data-testid", "tabs-preview-detail")
        self._preview_tabs.setProperty("class", "inner-tabs")
        
        preview_view_page = QWidget()
        preview_view_layout = QVBoxLayout(preview_view_page)
        preview_view_layout.setContentsMargins(0, 0, 0, 0)
        preview_view_layout.setSpacing(0)

        if WEB_ENGINE_AVAILABLE:
            self._dxf_view = QWebEngineView()
            self._dxf_view.page().setBackgroundColor(QColor("#1A1A1A"))
            if hasattr(self._dxf_view, "loadFinished"):
                self._dxf_view.loadFinished.connect(self._on_preview_view_load_finished)
            preview_view_layout.addWidget(self._dxf_view)
        else:
            self._dxf_view = None
            hint = QLabel("预览组件不可用\n请安装 PyQtWebEngine")
            hint.setAlignment(Qt.AlignCenter)
            hint.setStyleSheet("color: gray; font-size: 14px;")
            preview_view_layout.addWidget(hint)
        self._preview_tabs.addTab(preview_view_page, "轨迹预览")

        debug_page = QWidget()
        debug_layout = QVBoxLayout(debug_page)
        debug_layout.setContentsMargins(0, 0, 0, 0)
        debug_layout.setSpacing(0)
        self._preview_debug_view = QTextBrowser()
        self._preview_debug_view.setProperty("data-testid", "browser-preview-debug")
        self._preview_debug_view.setOpenExternalLinks(False)
        self._preview_debug_view.setStyleSheet("border: none; background-color: #1A1A1A; padding: 10px;")
        debug_layout.addWidget(self._preview_debug_view)
        self._preview_tabs.addTab(debug_page, "调试信息")

        preview_layout.addWidget(self._preview_tabs, 1)

        # Add to main layout
        main_layout.addWidget(sidebar)
        main_layout.addWidget(preview_container, 1)

        self._update_start_button_state()
        return widget

    def _create_setup_tab(self) -> QWidget:
        """Create Setup tab - for technicians to configure and test."""
        widget = QWidget()
        widget.setProperty("data-testid", "tab-setup")
        layout = QVBoxLayout(widget)
        layout.setSpacing(15)

        # Connection + System + Status
        top_layout = QHBoxLayout()
        top_layout.setSpacing(20)

        left_column = QVBoxLayout()
        left_column.setSpacing(15)
        self._system_panel = self._create_system_panel()
        left_column.addWidget(self._system_panel)
        left_column.addStretch()
        top_layout.addLayout(left_column, 1)
        self._status_panel = self._create_status_panel()
        top_layout.addWidget(self._status_panel, 2)
        layout.addLayout(top_layout)

        # Control Panels in horizontal layout
        controls_layout = QHBoxLayout()
        controls_layout.setSpacing(20)

        # Motion Control
        self._motion_control_panel = self._create_motion_control_panel()
        controls_layout.addWidget(self._motion_control_panel)

        # Dispenser Control
        self._dispenser_control_panel = self._create_dispenser_control_panel()
        controls_layout.addWidget(self._dispenser_control_panel)

        layout.addLayout(controls_layout)
        return widget

    def _create_recipe_tab(self) -> QWidget:
        """Create Recipe/File tab - for DXF and recipe management."""
        self._recipe_config_widget = RecipeConfigWidget(self._protocol, self._auth, parent=self)
        return self._recipe_config_widget

    def _create_system_panel(self) -> QGroupBox:
        group = QGroupBox("系统")
        group.setProperty("data-testid", "panel-system")
        layout = QVBoxLayout(group)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(10)

        self._hw_connect_btn = QPushButton("初始化硬件")
        self._hw_connect_btn.setProperty("data-testid", "btn-hw-init")
        self._hw_connect_btn.setCursor(Qt.PointingHandCursor)
        self._hw_connect_btn.clicked.connect(self._on_hw_connect)
        self._hw_connect_btn.setEnabled(False)
        self._hw_connect_btn.setMinimumHeight(36)
        layout.addWidget(self._hw_connect_btn)

        recovery_group = QGroupBox("会话恢复")
        recovery_group.setProperty("data-testid", "panel-session-recovery")
        recovery_layout = QGridLayout(recovery_group)
        recovery_layout.setContentsMargins(10, 12, 10, 10)
        recovery_layout.setHorizontalSpacing(8)
        recovery_layout.setVerticalSpacing(8)

        self._retry_stage_btn = QPushButton("重试阶段")
        self._retry_stage_btn.setProperty("data-testid", "btn-recovery-retry")
        self._retry_stage_btn.setCursor(Qt.PointingHandCursor)
        self._retry_stage_btn.clicked.connect(self._on_recovery_retry_stage)
        self._retry_stage_btn.setEnabled(False)
        self._retry_stage_btn.setToolTip("重试当前失败阶段")
        self._retry_stage_btn.setMinimumHeight(34)
        recovery_layout.addWidget(self._retry_stage_btn, 0, 0)

        self._restart_session_btn = QPushButton("重启会话")
        self._restart_session_btn.setProperty("data-testid", "btn-recovery-restart")
        self._restart_session_btn.setCursor(Qt.PointingHandCursor)
        self._restart_session_btn.clicked.connect(self._on_recovery_restart_session)
        self._restart_session_btn.setEnabled(False)
        self._restart_session_btn.setToolTip("重新启动在线会话")
        self._restart_session_btn.setMinimumHeight(34)
        recovery_layout.addWidget(self._restart_session_btn, 0, 1)

        self._stop_session_btn = QPushButton("停止会话")
        self._stop_session_btn.setProperty("data-testid", "btn-recovery-stop")
        self._stop_session_btn.setProperty("role", "warning")
        self._stop_session_btn.setCursor(Qt.PointingHandCursor)
        self._stop_session_btn.clicked.connect(self._on_recovery_stop_session)
        self._stop_session_btn.setEnabled(False)
        self._stop_session_btn.setToolTip("停止当前在线会话")
        self._stop_session_btn.setMinimumHeight(34)
        recovery_layout.addWidget(self._stop_session_btn, 1, 0, 1, 2)

        layout.addWidget(recovery_group)

        action_layout = QGridLayout()
        action_layout.setContentsMargins(0, 0, 0, 0)
        action_layout.setHorizontalSpacing(8)
        action_layout.setVerticalSpacing(8)

        self._stop_btn = QPushButton("停止")
        self._stop_btn.setProperty("data-testid", "btn-stop")
        self._stop_btn.setProperty("role", "warning")
        self._stop_btn.setCursor(Qt.PointingHandCursor)
        self._stop_btn.setMinimumHeight(38)
        self._stop_btn.clicked.connect(lambda: self._on_stop("manual_stop_button"))
        action_layout.addWidget(self._stop_btn, 0, 0)

        self._estop_reset_btn = QPushButton("急停复位")
        self._estop_reset_btn.setProperty("data-testid", "btn-estop-reset")
        self._estop_reset_btn.setProperty("role", "warning")
        self._estop_reset_btn.setCursor(Qt.PointingHandCursor)
        self._estop_reset_btn.setMinimumHeight(38)
        self._estop_reset_btn.clicked.connect(self._on_estop_reset)
        action_layout.addWidget(self._estop_reset_btn, 0, 1)

        self._estop_btn = QPushButton("急停")
        self._estop_btn.setProperty("data-testid", "btn-estop")
        self._estop_btn.setProperty("role", "danger")
        self._estop_btn.setCursor(Qt.PointingHandCursor)
        self._estop_btn.setMinimumHeight(38)
        self._estop_btn.clicked.connect(self._on_estop)
        action_layout.addWidget(self._estop_btn, 1, 0, 1, 2)

        layout.addLayout(action_layout)

        return group

    def _create_status_panel(self) -> QGroupBox:
        group = QGroupBox("状态")
        group.setProperty("data-testid", "panel-status")
        layout = QVBoxLayout(group)
        layout.setSpacing(15)

        status_row = QHBoxLayout()
        status_row.setSpacing(20)

        # Axis Status Panel (X/Y only)
        axis_group = QGroupBox("轴状态")
        axis_layout = QGridLayout(axis_group)
        axis_layout.setSpacing(10)

        headers = ["轴", "位置", "速度", "使能", "已回零"]
        for col, header in enumerate(headers):
            lbl = QLabel(header)
            lbl.setProperty("role", "table-header")
            axis_layout.addWidget(lbl, 0, col)

        self._axis_labels = {}
        self._axis_velocity_labels = {}
        self._axis_enabled_labels = {}
        self._axis_homed_labels = {}

        for row, axis in enumerate(["X", "Y"], start=1):
            lbl = QLabel(axis)
            lbl.setProperty("role", "axis-name")
            axis_layout.addWidget(lbl, row, 0)

            pos_label = QLabel("0.000")
            pos_label.setProperty("data-testid", f"axis-position-{axis}")
            pos_label.setProperty("role", "readout")
            self._axis_labels[axis] = pos_label
            axis_layout.addWidget(pos_label, row, 1)

            vel_label = QLabel("0.0")
            vel_label.setProperty("data-testid", f"axis-velocity-{axis}")
            self._axis_velocity_labels[axis] = vel_label
            axis_layout.addWidget(vel_label, row, 2)

            enabled_led = QLabel()
            enabled_led.setProperty("data-testid", f"axis-enabled-{axis}")
            enabled_led.setProperty("role", "led-off")
            enabled_led.setFixedSize(16, 16)
            self._axis_enabled_labels[axis] = enabled_led
            axis_layout.addWidget(enabled_led, row, 3, alignment=Qt.AlignCenter)

            homed_led = QLabel()
            homed_led.setProperty("data-testid", f"axis-homed-{axis}")
            homed_led.setProperty("role", "led-off")
            homed_led.setFixedSize(16, 16)
            self._axis_homed_labels[axis] = homed_led
            axis_layout.addWidget(homed_led, row, 4, alignment=Qt.AlignCenter)

        # IO Status Panel
        io_group = QGroupBox("IO状态")
        io_layout = QGridLayout(io_group)
        io_layout.setSpacing(8)

        self._io_leds = {}
        io_items = [
            ("X+限位", "io-limit-x-pos", 0, 0),
            ("X-限位", "io-limit-x-neg", 0, 1),
            ("Y+限位", "io-limit-y-pos", 1, 0),
            ("Y-限位", "io-limit-y-neg", 1, 1),
            ("急停", "io-estop", 2, 0),
            ("门", "io-door", 2, 1),
        ]
        for label_text, testid, row, col in io_items:
            item_layout = QHBoxLayout()
            led = QLabel()
            led.setProperty("data-testid", testid)
            led.setProperty("role", "led-off")
            led.setFixedSize(16, 16)
            self._io_leds[testid] = led
            item_layout.addWidget(led)
            item_layout.addWidget(QLabel(label_text))
            item_layout.addStretch()
            io_layout.addLayout(item_layout, row, col)

        status_row.addWidget(axis_group, 2)
        status_row.addWidget(io_group, 1)
        layout.addLayout(status_row)

        # Valve Status
        valve_layout = QHBoxLayout()

        state_frame = QFrame()
        state_frame.setProperty("role", "status-card")
        state_layout = QHBoxLayout(state_frame)
        state_layout.addWidget(QLabel("状态:"))
        self._state_label = QLabel("未知")
        self._state_label.setProperty("data-testid", "label-machine-state")
        self._state_label.setProperty("role", "status-value")
        state_layout.addWidget(self._state_label)
        state_layout.addStretch()
        valve_layout.addWidget(state_frame)

        disp_frame = QFrame()
        disp_frame.setProperty("role", "status-card")
        disp_layout = QHBoxLayout(disp_frame)
        disp_layout.addWidget(QLabel("点胶阀:"))
        self._dispenser_label = QLabel("关")
        self._dispenser_label.setProperty("data-testid", "valve-dispenser")
        self._dispenser_label.setProperty("role", "valve-off")
        disp_layout.addWidget(self._dispenser_label)
        disp_layout.addStretch()
        valve_layout.addWidget(disp_frame)

        supply_frame = QFrame()
        supply_frame.setProperty("role", "status-card")
        supply_layout = QHBoxLayout(supply_frame)
        supply_layout.addWidget(QLabel("供料阀:"))
        self._supply_label = QLabel("关")
        self._supply_label.setProperty("data-testid", "valve-supply")
        self._supply_label.setProperty("role", "valve-off")
        supply_layout.addWidget(self._supply_label)
        supply_layout.addStretch()
        valve_layout.addWidget(supply_frame)

        layout.addLayout(valve_layout)
        return group

    def _create_motion_control_panel(self) -> QWidget:
        widget = QWidget()
        widget.setProperty("data-testid", "panel-motion-control")
        layout = QHBoxLayout(widget)
        layout.setSpacing(30)

        # Jog Controls with D-Pad Layout
        jog_group = QGroupBox("点动控制")
        jog_layout = QVBoxLayout(jog_group)

        # Speed presets
        preset_layout = QHBoxLayout()
        preset_layout.addWidget(QLabel("速度:"))
        self._speed_slow_btn = QPushButton("慢速")
        self._speed_slow_btn.setProperty("data-testid", "btn-speed-slow")
        self._speed_slow_btn.clicked.connect(lambda: self._set_speed(10))
        preset_layout.addWidget(self._speed_slow_btn)
        self._speed_medium_btn = QPushButton("中速")
        self._speed_medium_btn.setProperty("data-testid", "btn-speed-medium")
        self._speed_medium_btn.clicked.connect(lambda: self._set_speed(30))
        preset_layout.addWidget(self._speed_medium_btn)
        self._speed_fast_btn = QPushButton("快速")
        self._speed_fast_btn.setProperty("data-testid", "btn-speed-fast")
        self._speed_fast_btn.clicked.connect(lambda: self._set_speed(50))
        preset_layout.addWidget(self._speed_fast_btn)
        jog_layout.addLayout(preset_layout)

        # Speed slider
        speed_layout = QHBoxLayout()
        self._speed_slider = QSlider(Qt.Horizontal)
        self._speed_slider.setProperty("data-testid", "slider-jog-speed")
        self._speed_slider.setRange(1, 100)
        self._speed_slider.setValue(20)
        self._speed_slider.valueChanged.connect(self._on_speed_changed)
        speed_layout.addWidget(self._speed_slider)
        self._speed_label = QLabel("20.0 mm/s")
        self._speed_label.setProperty("data-testid", "label-jog-speed")
        self._speed_label.setFixedWidth(80)
        speed_layout.addWidget(self._speed_label)
        jog_layout.addLayout(speed_layout)

        # D-Pad Layout for X/Y
        dpad_layout = QGridLayout()
        dpad_layout.setSpacing(5)

        # Y+ (top)
        y_plus_btn = JogButton("\u2191")  # Up arrow
        y_plus_btn.setProperty("data-testid", "btn-jog-Y-plus")
        y_plus_btn.setFixedSize(60, 60)
        y_plus_btn.pressed.connect(lambda: self._on_jog("Y", 1))
        y_plus_btn.released.connect(self._on_jog_release)
        dpad_layout.addWidget(y_plus_btn, 0, 1, Qt.AlignCenter)

        # X- (left)
        x_minus_btn = JogButton("\u2190")  # Left arrow
        x_minus_btn.setProperty("data-testid", "btn-jog-X-minus")
        x_minus_btn.setFixedSize(60, 60)
        x_minus_btn.pressed.connect(lambda: self._on_jog("X", -1))
        x_minus_btn.released.connect(self._on_jog_release)
        dpad_layout.addWidget(x_minus_btn, 1, 0, Qt.AlignCenter)

        # Home (center)
        home_btn = QPushButton("\u2302")  # Home symbol
        home_btn.setProperty("data-testid", "btn-jog-home-xy")
        home_btn.setProperty("role", "primary")
        home_btn.setFixedSize(60, 60)
        home_btn.clicked.connect(lambda: self._on_home(["X", "Y"]))
        dpad_layout.addWidget(home_btn, 1, 1, Qt.AlignCenter)

        # X+ (right)
        x_plus_btn = JogButton("\u2192")  # Right arrow
        x_plus_btn.setProperty("data-testid", "btn-jog-X-plus")
        x_plus_btn.setFixedSize(60, 60)
        x_plus_btn.pressed.connect(lambda: self._on_jog("X", 1))
        x_plus_btn.released.connect(self._on_jog_release)
        dpad_layout.addWidget(x_plus_btn, 1, 2, Qt.AlignCenter)

        # Y- (bottom)
        y_minus_btn = JogButton("\u2193")  # Down arrow
        y_minus_btn.setProperty("data-testid", "btn-jog-Y-minus")
        y_minus_btn.setFixedSize(60, 60)
        y_minus_btn.pressed.connect(lambda: self._on_jog("Y", -1))
        y_minus_btn.released.connect(self._on_jog_release)
        dpad_layout.addWidget(y_minus_btn, 2, 1, Qt.AlignCenter)

        jog_layout.addLayout(dpad_layout)
        jog_layout.addStretch()
        layout.addWidget(jog_group)

        # Point-to-Point Move (X/Y only)
        move_group = QGroupBox("点对点移动")
        move_layout = QVBoxLayout(move_group)

        pos_grid = QGridLayout()
        self._move_inputs = {}
        for row, axis in enumerate(["X", "Y"]):
            pos_grid.addWidget(QLabel(f"{axis}:"), row, 0)
            spinbox = QDoubleSpinBox()
            spinbox.setProperty("data-testid", f"input-move-{axis}")
            spinbox.setRange(-1000, 1000)
            spinbox.setDecimals(3)
            spinbox.setSuffix(" mm")
            self._move_inputs[axis] = spinbox
            pos_grid.addWidget(spinbox, row, 1)

        pos_grid.addWidget(QLabel("速度:"), 2, 0)
        self._move_speed = QDoubleSpinBox()
        self._move_speed.setProperty("data-testid", "input-move-speed")
        self._move_speed.setRange(1, 100)
        self._move_speed.setValue(10)
        self._move_speed.setSuffix(" mm/s")
        pos_grid.addWidget(self._move_speed, 2, 1)

        move_layout.addLayout(pos_grid)

        self._move_btn = QPushButton("移动到位置")
        self._move_btn.setProperty("data-testid", "btn-move-to-position")
        self._move_btn.setProperty("role", "primary")
        self._move_btn.clicked.connect(self._on_move_to)
        move_layout.addWidget(self._move_btn)
        move_layout.addStretch()
        layout.addWidget(move_group)

        return widget

    def _create_dispenser_control_panel(self) -> QWidget:
        widget = QWidget()
        widget.setProperty("data-testid", "panel-dispenser-control")
        layout = QHBoxLayout(widget)
        layout.setSpacing(30)

        # Dispenser Parameters
        params_group = QGroupBox("点胶参数")
        params_layout = QGridLayout(params_group)

        params_layout.addWidget(QLabel("次数:"), 0, 0)
        self._dispenser_count = QSpinBox()
        self._dispenser_count.setProperty("data-testid", "input-dispenser-count")
        self._dispenser_count.setRange(1, 1000)
        self._dispenser_count.setValue(10)
        params_layout.addWidget(self._dispenser_count, 0, 1)

        params_layout.addWidget(QLabel("间隔:"), 1, 0)
        self._dispenser_interval = QSpinBox()
        self._dispenser_interval.setProperty("data-testid", "input-dispenser-interval")
        self._dispenser_interval.setRange(0, 10000)
        self._dispenser_interval.setValue(1000)
        self._dispenser_interval.setSuffix(" ms")
        params_layout.addWidget(self._dispenser_interval, 1, 1)

        params_layout.addWidget(QLabel("持续时间:"), 2, 0)
        self._dispenser_duration = QSpinBox()
        self._dispenser_duration.setProperty("data-testid", "input-dispenser-duration")
        self._dispenser_duration.setRange(1, 10000)
        self._dispenser_duration.setValue(15)
        self._dispenser_duration.setSuffix(" ms")
        params_layout.addWidget(self._dispenser_duration, 2, 1)

        layout.addWidget(params_group)

        # Dispenser Valve Control
        valve_group = QGroupBox("点胶阀")
        valve_layout = QVBoxLayout(valve_group)

        self._disp_start_btn = QPushButton("启动")
        self._disp_start_btn.setProperty("data-testid", "btn-dispenser-start")
        self._disp_start_btn.setMinimumHeight(40)
        self._disp_start_btn.clicked.connect(self._on_dispenser_start)
        valve_layout.addWidget(self._disp_start_btn)

        self._disp_pause_btn = QPushButton("暂停")
        self._disp_pause_btn.setProperty("data-testid", "btn-dispenser-pause")
        self._disp_pause_btn.setMinimumHeight(40)
        self._disp_pause_btn.clicked.connect(self._on_dispenser_pause)
        valve_layout.addWidget(self._disp_pause_btn)

        self._disp_resume_btn = QPushButton("继续")
        self._disp_resume_btn.setProperty("data-testid", "btn-dispenser-resume")
        self._disp_resume_btn.setMinimumHeight(40)
        self._disp_resume_btn.clicked.connect(self._on_dispenser_resume)
        valve_layout.addWidget(self._disp_resume_btn)

        self._disp_stop_btn = QPushButton("停止")
        self._disp_stop_btn.setProperty("data-testid", "btn-dispenser-stop")
        self._disp_stop_btn.setMinimumHeight(40)
        self._disp_stop_btn.clicked.connect(self._on_dispenser_stop)
        valve_layout.addWidget(self._disp_stop_btn)

        valve_layout.addStretch()
        layout.addWidget(valve_group)

        # Supply Valve Control
        supply_group = QGroupBox("供料阀")
        supply_layout = QVBoxLayout(supply_group)

        self._supply_open_btn = QPushButton("打开供料")
        self._supply_open_btn.setProperty("data-testid", "btn-supply-open")
        self._supply_open_btn.setMinimumHeight(40)
        self._supply_open_btn.clicked.connect(self._on_supply_open)
        supply_layout.addWidget(self._supply_open_btn)

        self._supply_close_btn = QPushButton("关闭供料")
        self._supply_close_btn.setProperty("data-testid", "btn-supply-close")
        self._supply_close_btn.setMinimumHeight(40)
        self._supply_close_btn.clicked.connect(self._on_supply_close)
        supply_layout.addWidget(self._supply_close_btn)

        supply_layout.addStretch()
        layout.addWidget(supply_group)

        # Purge Control
        purge_group = QGroupBox("清洗")
        purge_layout = QVBoxLayout(purge_group)

        duration_layout = QHBoxLayout()
        duration_layout.addWidget(QLabel("持续时间:"))
        self._purge_duration = QSpinBox()
        self._purge_duration.setProperty("data-testid", "input-purge-duration")
        self._purge_duration.setRange(100, 10000)
        self._purge_duration.setValue(10000)
        self._purge_duration.setToolTip("范围 100-10000 ms，默认 10000 ms")
        self._purge_duration.setSuffix(" ms")
        duration_layout.addWidget(self._purge_duration)
        purge_layout.addLayout(duration_layout)

        self._purge_btn = QPushButton("执行清洗")
        self._purge_btn.setProperty("data-testid", "btn-purge")
        self._purge_btn.setProperty("role", "primary")
        self._purge_btn.setMinimumHeight(40)
        self._purge_btn.clicked.connect(self._on_purge)
        purge_layout.addWidget(self._purge_btn)

        purge_layout.addStretch()
        layout.addWidget(purge_group)

        return widget

    def _create_alarm_panel(self) -> QWidget:
        widget = QWidget()
        widget.setProperty("data-testid", "panel-alarms")
        layout = QVBoxLayout(widget)
        layout.setSpacing(15)

        # Alarm List
        list_group = QGroupBox("活动报警")
        list_layout = QVBoxLayout(list_group)

        self._alarm_list = QListWidget()
        self._alarm_list.setProperty("data-testid", "list-alarms")
        self._alarm_list.setMinimumHeight(150)
        list_layout.addWidget(self._alarm_list)

        btn_layout = QHBoxLayout()
        self._alarm_clear_btn = QPushButton("全部清除")
        self._alarm_clear_btn.setProperty("data-testid", "btn-alarm-clear")
        self._alarm_clear_btn.setProperty("role", "warning")
        self._alarm_clear_btn.clicked.connect(self._on_alarm_clear)
        btn_layout.addWidget(self._alarm_clear_btn)

        self._alarm_ack_btn = QPushButton("确认")
        self._alarm_ack_btn.setProperty("data-testid", "btn-alarm-ack")
        self._alarm_ack_btn.clicked.connect(self._on_alarm_ack)
        btn_layout.addWidget(self._alarm_ack_btn)

        btn_layout.addStretch()
        list_layout.addLayout(btn_layout)

        layout.addWidget(list_group)

        status_log_group = QGroupBox("状态栏日志")
        status_log_layout = QVBoxLayout(status_log_group)

        self._status_log_view = QPlainTextEdit()
        self._status_log_view.setProperty("data-testid", "status-log-view")
        self._status_log_view.setReadOnly(True)
        self._status_log_view.setMinimumHeight(180)
        self._status_log_view.setPlaceholderText("底部左侧状态栏消息会在此保留，便于回看和复制。")
        status_log_layout.addWidget(self._status_log_view)

        status_log_btn_layout = QHBoxLayout()
        self._copy_status_log_btn = QPushButton("复制日志")
        self._copy_status_log_btn.setProperty("data-testid", "btn-status-log-copy")
        self._copy_status_log_btn.clicked.connect(self._on_copy_status_log)
        status_log_btn_layout.addWidget(self._copy_status_log_btn)
        status_log_btn_layout.addStretch()
        status_log_layout.addLayout(status_log_btn_layout)

        layout.addWidget(status_log_group)
        layout.addStretch()

        return widget

    def _on_status_message_changed(self, message: str) -> None:
        text = (message or "").strip()
        if not text or text == self._last_status_log_message:
            return

        entry = f"[{time.strftime('%H:%M:%S')}] {text}"
        self._status_log_entries.append(entry)
        if len(self._status_log_entries) > STATUS_LOG_HISTORY_LIMIT:
            self._status_log_entries = self._status_log_entries[-STATUS_LOG_HISTORY_LIMIT:]
            self._status_log_view.setPlainText("\n".join(self._status_log_entries))
        elif self._status_log_view.toPlainText():
            self._status_log_view.appendPlainText(entry)
        else:
            self._status_log_view.setPlainText(entry)
        self._last_status_log_message = text

    def _on_copy_status_log(self) -> None:
        history = "\n".join(self._status_log_entries)
        if not history:
            self.statusBar().showMessage("暂无可复制的状态栏日志")
            return
        QApplication.clipboard().setText(history)
        self.statusBar().showMessage("状态栏日志已复制")

    def _sync_preview_session_fields(self) -> None:
        state = self._preview_session.state
        self._preview_gate = state.gate
        self._preview_source = state.preview_source
        self._current_plan_id = state.current_plan_id
        self._current_plan_fingerprint = state.current_plan_fingerprint
        self._preview_plan_dry_run = state.preview_plan_dry_run
        self._preview_refresh_inflight = state.preview_refresh_inflight
        self._preview_state_resync_pending = state.preview_state_resync_pending
        self._last_preview_resync_attempt_ts = state.last_preview_resync_attempt_ts
        self._dxf_segment_count_cache = state.dxf_segment_count
        self._dxf_total_length_val = state.dxf_total_length_mm
        self._dxf_est_time_val = state.dxf_estimated_time_text

    def _current_preview_playback_status(self):
        return self._preview_session.local_playback_status()

    def _update_preview_playback_controls(self) -> None:
        if not hasattr(self, "_preview_play_btn"):
            return
        status = self._current_preview_playback_status()
        available = bool(status.available and self._dxf_loaded)
        self._preview_play_btn.setEnabled(available and status.state in ("idle", "paused", "finished"))
        self._preview_pause_btn.setEnabled(available and status.state == "playing")
        self._preview_replay_btn.setEnabled(available)
        self._preview_speed_combo.setEnabled(available)
        if not available:
            self._preview_playback_status_label.setText("动态预览: 不可用")
            return
        self._preview_playback_status_label.setText(
            f"动态预览: {self._preview_session.local_playback_state_text()} | "
            f"{status.elapsed_s:.1f}/{status.duration_s:.1f}s | {status.speed_ratio:.1f}x"
        )

    def _on_preview_view_load_finished(self, ok: bool = True) -> None:
        self._preview_dom_ready = bool(ok)
        if self._preview_dom_ready:
            self._sync_preview_playback_visual_state()

    def _run_preview_script(self, script: str) -> None:
        if not WEB_ENGINE_AVAILABLE or not self._dxf_view or not self._preview_dom_ready:
            return
        page = self._dxf_view.page() if hasattr(self._dxf_view, "page") else None
        if page is None or not hasattr(page, "runJavaScript"):
            return
        page.runJavaScript(script)

    def _sync_preview_playback_visual_state(self) -> None:
        status = self._current_preview_playback_status()
        if not status.available:
            return
        self._run_preview_script(
            f"window.updatePreviewPlayback({status.progress:.6f}, {json.dumps(status.state)});"
        )

    def _apply_preview_playback_status(self, status=None, *, sync_dom: bool = True) -> None:
        if status is None:
            status = self._current_preview_playback_status()
        self._sync_preview_session_fields()
        self._update_preview_playback_controls()
        if sync_dom:
            self._sync_preview_playback_visual_state()

    def _on_preview_play(self) -> None:
        status = self._preview_session.play_local_playback(time.monotonic())
        self._apply_preview_playback_status(status)

    def _on_preview_pause(self) -> None:
        status = self._preview_session.pause_local_playback(time.monotonic())
        self._apply_preview_playback_status(status)

    def _on_preview_replay(self) -> None:
        status = self._preview_session.replay_local_playback(time.monotonic())
        self._apply_preview_playback_status(status)

    def _on_preview_speed_changed(self, _index: int) -> None:
        ratio = float(self._preview_speed_combo.currentData() or 1.0)
        status = self._preview_session.set_local_playback_speed_ratio(ratio, now_monotonic=time.monotonic())
        self._apply_preview_playback_status(status)

    def _tick_preview_playback(self, now_monotonic: float) -> None:
        status = self._preview_session.tick_local_playback(now_monotonic)
        if not status.available:
            self._update_preview_playback_controls()
            return
        self._apply_preview_playback_status(status, sync_dom=status.state in ("idle", "playing", "paused", "finished"))

    def _apply_launch_ui_state(self, ui_state: LaunchUiState) -> None:
        self._launch_ui_state = ui_state
        self._preview_state_resync_pending = ui_state.preview_resync_pending
        self._preview_session.set_resync_pending(ui_state.preview_resync_pending)
        self._launch_mode_label.setText(ui_state.launch_mode_label)
        self._operation_status.setText(ui_state.operation_status_text)
        self._state_label.setText(ui_state.state_label_text)
        self._tcp_state_label.setText(ui_state.tcp_state_label)
        self._hw_state_label.setText(ui_state.hardware_state_label)
        self._update_led(self._tcp_led, ui_state.tcp_led_state)
        self._update_led(self._hw_led, ui_state.hardware_led_state)
        self._sync_preview_session_fields()

    def _current_effective_mode(self) -> str:
        snapshot = self._current_session_snapshot()
        if snapshot is not None:
            return snapshot.mode
        if self._launch_result is not None:
            return self._launch_result.effective_mode
        return self._requested_launch_mode

    def _current_session_snapshot(self):
        return self._session_snapshot

    def _is_offline_mode(self) -> bool:
        return self._current_effective_mode() == "offline"

    def _is_online_ready(self) -> bool:
        return is_online_ready(self._current_session_snapshot())

    def _previous_launch_connected(self) -> bool:
        ui_state = self._launch_ui_state
        return bool(ui_state.connected) if ui_state is not None else False

    def _is_launch_connected(self) -> bool:
        ui_state = self._launch_ui_state
        if ui_state is not None:
            return bool(ui_state.connected)
        snapshot = self._current_session_snapshot()
        return bool(snapshot is not None and snapshot.tcp_state == "ready")

    def _is_launch_hardware_ready(self) -> bool:
        ui_state = self._launch_ui_state
        if ui_state is not None:
            return bool(ui_state.hardware_connected)
        snapshot = self._current_session_snapshot()
        return bool(snapshot is not None and snapshot.hardware_state == "ready")

    def _has_online_capability(self) -> bool:
        return not self._is_offline_mode() and self._is_online_ready()

    def _has_runtime_command_channel(self) -> bool:
        snapshot = self._current_session_snapshot()
        return snapshot is not None and snapshot.mode == "online" and snapshot.tcp_state == "ready"

    def _require_runtime_command_channel(self, capability: str) -> bool:
        if self._is_offline_mode():
            self.statusBar().showMessage(f"Offline 模式下不可用: {capability}")
            return False

        snapshot = self._current_session_snapshot()
        if snapshot is None:
            self.statusBar().showMessage(f"系统启动中: {capability} 暂不可用")
            return False

        if snapshot.tcp_state == "ready":
            return True

        if self._launch_result is not None and self._launch_result.failure_code:
            self.statusBar().showMessage(f"TCP 未就绪({self._launch_result.failure_code}): {capability} 暂不可用")
        else:
            self.statusBar().showMessage(f"TCP 未就绪: {capability} 暂不可用")
        return False

    def _is_session_operation_running(self) -> bool:
        startup_running = self._startup_worker is not None and self._startup_worker.isRunning()
        recovery_running = self._recovery_worker is not None and self._recovery_worker.isRunning()
        return startup_running or recovery_running

    def _update_recovery_controls_state(self) -> None:
        if not all(
            hasattr(self, name)
            for name in ("_retry_stage_btn", "_restart_session_btn", "_stop_session_btn")
        ):
            return
        ui_state = build_launch_ui_state(
            self._requested_launch_mode,
            self._launch_result,
            self._current_session_snapshot(),
            previous_connected=self._previous_launch_connected(),
            has_current_plan=bool(self._preview_session.state.current_plan_id),
            preview_resync_pending=self._preview_session.state.preview_state_resync_pending,
            session_operation_running=self._is_session_operation_running(),
        )
        controls = ui_state.recovery_controls
        self._retry_stage_btn.setEnabled(controls.retry_enabled)
        self._restart_session_btn.setEnabled(controls.restart_enabled)
        self._stop_session_btn.setEnabled(controls.stop_enabled)

    def _refresh_launch_status_ui(self) -> None:
        previous_connected = self._previous_launch_connected()
        ui_state = build_launch_ui_state(
            self._requested_launch_mode,
            self._launch_result,
            self._current_session_snapshot(),
            previous_connected=previous_connected,
            has_current_plan=bool(self._preview_session.state.current_plan_id),
            preview_resync_pending=self._preview_session.state.preview_state_resync_pending,
            session_operation_running=self._is_session_operation_running(),
        )
        self._apply_launch_ui_state(ui_state)
        self._update_recovery_controls_state()
        self._update_preview_refresh_button_state()

    def _apply_mode_capabilities(self) -> None:
        if self._launch_ui_state is None:
            self._refresh_launch_status_ui()
        ui_state = self._launch_ui_state
        if ui_state is None:
            return
        for widget_name in (
            "_motion_control_panel",
            "_dispenser_control_panel",
            "_recipe_tab",
        ):
            widget = getattr(self, widget_name, None)
            if widget is not None:
                widget.setEnabled(ui_state.allow_online_actions)
        if hasattr(self, "_production_tab"):
            # 离线模式仍需允许 DXF 加载、离线预览和本地回放；仅在线动作按钮受 capability 门控。
            self._production_tab.setEnabled(True)
        if hasattr(self, "_system_panel"):
            self._system_panel.setEnabled(ui_state.system_panel_enabled)
        if hasattr(self, "_stop_btn"):
            self._stop_btn.setEnabled(ui_state.stop_enabled)
        if hasattr(self, "_hw_connect_btn"):
            self._hw_connect_btn.setEnabled(ui_state.hw_connect_enabled)
        if hasattr(self, "_global_estop_btn"):
            self._global_estop_btn.setEnabled(ui_state.global_estop_enabled)
        self._apply_production_action_capabilities(ui_state.allow_online_actions)
        self._update_recovery_controls_state()

    def _apply_production_action_capabilities(self, online_actions_allowed: bool) -> None:
        if hasattr(self, "_prod_home_btn"):
            self._prod_home_btn.setEnabled(bool(online_actions_allowed))
        if hasattr(self, "_prod_start_btn"):
            self._prod_start_btn.setEnabled(bool(online_actions_allowed))
        if hasattr(self, "_prod_pause_btn"):
            self._prod_pause_btn.setEnabled(bool(online_actions_allowed))
        if hasattr(self, "_prod_resume_btn"):
            self._prod_resume_btn.setEnabled(bool(online_actions_allowed))
        if hasattr(self, "_prod_stop_btn"):
            self._prod_stop_btn.setEnabled(bool(online_actions_allowed))
        if hasattr(self, "_target_input"):
            self._target_input.setEnabled(bool(online_actions_allowed))

    def _show_initial_session_message(self) -> None:
        if self._requested_launch_mode == "online" and self._current_session_snapshot() is None:
            self.statusBar().showMessage("启动中，等待 Supervisor 首个快照")
            return
        if self._requested_launch_mode == "offline" and self._launch_result is None:
            self.statusBar().showMessage("正在初始化离线会话")

    def _apply_launch_result(self, result: LaunchResult) -> None:
        self._launch_result = result
        self._session_snapshot = result.session_snapshot
        self._refresh_launch_status_ui()
        self._apply_mode_capabilities()
        self._update_home_controls_state()
        if result.user_message:
            self.statusBar().showMessage(result.user_message)

    def _apply_runtime_degradation(
        self,
        *,
        failure_code: str,
        failure_stage: str,
        message: str,
        tcp_state: str | None = None,
        hardware_state: str | None = None,
    ) -> None:
        runtime_result = build_runtime_degradation_result(
            self._launch_result,
            self._current_session_snapshot(),
            self._supervisor_stage_events,
            failure_code=failure_code,
            failure_stage=failure_stage,
            message=message,
            tcp_state=tcp_state,
            hardware_state=hardware_state,
        )
        self._apply_runtime_degradation_result(runtime_result)

    def _apply_runtime_degradation_result(self, runtime_result) -> None:
        if runtime_result is None:
            return
        self._on_supervisor_stage_event(runtime_result.stage_event)
        self._session_snapshot = runtime_result.degraded_snapshot
        self._launch_result = runtime_result.launch_result
        self._refresh_launch_status_ui()
        self._apply_mode_capabilities()
        self._update_home_controls_state()
        self._apply_permissions()
        self.statusBar().showMessage(runtime_result.status_message)

    def _apply_runtime_requalification_result(self, runtime_result) -> None:
        if runtime_result is None:
            return
        self._on_supervisor_stage_event(runtime_result.stage_event)
        self._session_snapshot = runtime_result.recovered_snapshot
        self._launch_result = runtime_result.launch_result
        self._refresh_launch_status_ui()
        self._apply_mode_capabilities()
        self._update_home_controls_state()
        self._apply_permissions()
        self.statusBar().showMessage(runtime_result.status_message)

    def _detect_runtime_degradation(
        self,
        *,
        tcp_connected: bool | None = None,
        hardware_ready: bool | None = None,
        error_message: str | None = None,
    ):
        return detect_runtime_degradation_result(
            self._launch_result,
            self._current_session_snapshot(),
            self._supervisor_stage_events,
            tcp_connected=tcp_connected,
            hardware_ready=hardware_ready,
            error_message=error_message,
        )

    def _detect_runtime_requalification(
        self,
        *,
        tcp_connected: bool | None = None,
        hardware_ready: bool | None = None,
        success_message: str | None = None,
    ):
        return detect_runtime_requalification_result(
            self._launch_result,
            self._current_session_snapshot(),
            self._supervisor_stage_events,
            tcp_connected=tcp_connected,
            hardware_ready=hardware_ready,
            success_message=success_message,
        )

    def _require_online_mode(self, capability: str) -> bool:
        message = build_online_capability_message(
            capability,
            requested_mode=self._requested_launch_mode,
            launch_result=self._launch_result,
            session_snapshot=self._current_session_snapshot(),
        )
        if not message:
            return True
        self.statusBar().showMessage(message)
        return False

    def _setup_timer(self):
        self._timer = QTimer()
        self._timer.timeout.connect(self._update_status)
        self._timer.start(200)

    def _bind_session_worker_signals(self, worker) -> None:
        worker.progress.connect(self._on_startup_progress)
        worker.snapshot.connect(self._on_startup_snapshot)
        worker.stage_event.connect(self._on_supervisor_stage_event)

    def _auto_startup(self):
        """Execute automatic startup sequence."""
        if self._is_session_operation_running():
            return
        self._supervisor_stage_events = []
        self._startup_worker = StartupWorker(
            self._backend,
            self._client,
            self._protocol,
            launch_mode=self._requested_launch_mode,
            policy=self._supervisor_policy,
        )
        self._bind_session_worker_signals(self._startup_worker)
        self._startup_worker.finished.connect(self._on_startup_finished)
        self._update_recovery_controls_state()
        self._startup_worker.start()

    def _start_recovery_action(self, action: str) -> None:
        snapshot = self._current_session_snapshot()
        decision = build_recovery_action_decision(
            action,
            snapshot,
            effective_mode=self._current_effective_mode(),
            session_operation_running=self._is_session_operation_running(),
        )
        if not decision.allowed:
            self.statusBar().showMessage(decision.message)
            return

        self._pending_recovery_action = action
        self._recovery_worker = RecoveryWorker(
            action=action,
            recovery_snapshot=snapshot,
            backend=self._backend,
            client=self._client,
            protocol=self._protocol,
            policy=self._supervisor_policy,
        )
        self._bind_session_worker_signals(self._recovery_worker)
        self._recovery_worker.finished.connect(self._on_recovery_finished)
        self.statusBar().showMessage(f"正在执行恢复动作: {action}")
        self._update_recovery_controls_state()
        self._recovery_worker.start()

    def _on_recovery_retry_stage(self):
        self._start_recovery_action("retry_stage")

    def _on_recovery_restart_session(self):
        self._start_recovery_action("restart_session")

    def _on_recovery_stop_session(self):
        self._start_recovery_action("stop_session")

    def _on_startup_progress(self, message: str, percent: int):
        """Handle startup progress updates."""
        self.statusBar().showMessage(message)
        self._global_progress.setValue(percent)

    def _on_startup_snapshot(self, snapshot) -> None:
        self._session_snapshot = snapshot
        self._refresh_launch_status_ui()
        self._apply_mode_capabilities()
        self._update_home_controls_state()

    def _on_supervisor_stage_event(self, event) -> None:
        self._supervisor_stage_events.append(event)
        # Cap in-memory event list to keep UI runtime footprint bounded.
        if len(self._supervisor_stage_events) > 128:
            self._supervisor_stage_events = self._supervisor_stage_events[-128:]

    def _on_startup_finished(self, result: LaunchResult):
        """Handle startup sequence completion."""
        self._startup_worker = None
        self._apply_launch_result(result)
        self._global_progress.setValue(0)
        if result.success:
            self._hw_connect_btn.setEnabled(False)
            if hasattr(self, '_recipe_config_widget'):
                self._recipe_config_widget._load_recipe_context()
        else:
            self._show_startup_error(result)
        self._apply_permissions()
        self._update_recovery_controls_state()

    def _on_recovery_finished(self, result: LaunchResult):
        action = self._pending_recovery_action
        self._pending_recovery_action = ""
        self._recovery_worker = None
        self._apply_launch_result(result)
        self._global_progress.setValue(0)
        self.statusBar().showMessage(build_recovery_finished_message(action, result))
        self._apply_permissions()
        self._update_recovery_controls_state()

    def _show_startup_error(self, result: LaunchResult):
        """Display startup error dialog."""
        QMessageBox.critical(
            self,
            "Startup Failed",
            build_startup_error_message(result),
        )

    def _update_home_controls_state(self):
        enabled = self._is_online_ready() and not self._is_home_worker_running()
        if hasattr(self, "_home_all_btn"):
            self._home_all_btn.setEnabled(enabled)
        if hasattr(self, "_prod_home_btn"):
            self._prod_home_btn.setEnabled(enabled)
        if hasattr(self, "_home_axis_buttons"):
            for btn in self._home_axis_buttons:
                btn.setEnabled(enabled)

    def _is_home_worker_running(self) -> bool:
        return self._home_auto_worker is not None and self._home_auto_worker.isRunning()

    def _cancel_active_home_worker(self, *, reason: str) -> None:
        worker = self._home_auto_worker
        self._home_auto_worker = None
        if worker is None:
            return
        self._home_request_generation += 1
        try:
            worker.cancel()
        except Exception as exc:
            _UI_LOGGER.warning("home_worker_cancel_failed reason=%s error=%s", reason, exc)
        else:
            _UI_LOGGER.info("home_worker_cancelled reason=%s generation=%s", reason, self._home_request_generation)
        self._update_home_controls_state()

    def _check_home_preconditions(self) -> bool:
        if not self._require_online_mode("回零"):
            return False
        status = self._get_cached_status()
        if not status or not status.connected:
            status = self._protocol.get_status()
            self._last_status = status
            self._last_status_ts = time.monotonic()
        if status and status.connection_state == "degraded":
            self.statusBar().showMessage("硬件连接已降级，无法回零，请重新连接")
            return False
        if not status or not status.connected:
            self.statusBar().showMessage("后端状态未就绪，请稍后再试")
            return False
        if not status.gate_estop_known() or not status.gate_door_known():
            self.statusBar().showMessage("互锁信号状态未知，无法回零")
            return False
        if status.gate_estop_active():
            self.statusBar().showMessage("急停未解除，无法回零")
            return False
        if status.gate_door_active():
            self.statusBar().showMessage("安全门打开，无法回零")
            return False
        return True

    def _check_motion_preconditions(self) -> bool:
        if not self._require_online_mode("点动"):
            return False
        status = self._protocol.get_status()
        if status.connection_state == "degraded":
            self.statusBar().showMessage("硬件连接已降级，无法点动，请重新连接")
            return False
        if not status.connected:
            self.statusBar().showMessage("后端状态不可用，请检查连接")
            return False
        if not status.gate_estop_known() or not status.gate_door_known():
            self.statusBar().showMessage("互锁信号状态未知，无法点动")
            return False
        if status.gate_estop_active():
            self.statusBar().showMessage("急停未解除，无法点动")
            return False
        if status.gate_door_active():
            self.statusBar().showMessage("安全门打开，无法点动")
            return False
        return True

    def _on_connect(self):
        if not self._require_online_mode("TCP连接"):
            return
        self.statusBar().showMessage("P0 模式下连接由 Supervisor 启动会话托管")

    def _on_disconnect(self):
        if not self._require_online_mode("断开连接"):
            return
        self.statusBar().showMessage("P0 模式下断开由 Supervisor 会话控制")

    def _on_hw_connect(self):
        if not self._require_online_mode("硬件初始化"):
            return
        self.statusBar().showMessage("P0 模式下硬件初始化由 Supervisor 启动会话托管")

    def _update_led(self, led: QLabel, state: str):
        """Update LED indicator state: off, green, red, yellow, blue"""
        led.setProperty("role", f"led-{state}")
        led.style().unpolish(led)
        led.style().polish(led)

    def _update_role(self, widget, role: str):
        """Update widget role and refresh style."""
        widget.setProperty("role", role)
        widget.style().unpolish(widget)
        widget.style().polish(widget)

    def _on_home(self, axes, force_rehome=False, allow_go_home=True):
        self._auth.record_activity()
        if self._is_home_worker_running():
            self.statusBar().showMessage("回零进行中，请稍候")
            return
        if not self._check_home_preconditions():
            return
        _ = allow_go_home
        self._home_request_generation += 1
        request_token = self._home_request_generation
        worker = HomeAutoWorker(
            host=self._client.host,
            port=self._client.port,
            axes=axes,
            force_rehome=force_rehome,
        )
        worker.completed.connect(
            lambda ok, message, token=request_token: self._on_home_auto_completed(
                ok,
                message,
                request_token=token,
            )
        )
        worker.finished.connect(worker.deleteLater)
        self._home_auto_worker = worker
        self._update_home_controls_state()
        self.statusBar().showMessage("回零进行中...")
        try:
            worker.start()
        except Exception as exc:
            self._home_auto_worker = None
            self._update_home_controls_state()
            QMessageBox.warning(self, "回零失败", str(exc) or "回零启动失败")
            self.statusBar().showMessage("回零启动失败")

    def _on_home_auto_completed(self, ok: bool, message: str, *, request_token: int) -> None:
        if request_token != self._home_request_generation:
            _UI_LOGGER.info(
                "home_result_ignored reason=stale_worker_result request_token=%s active_generation=%s",
                request_token,
                self._home_request_generation,
            )
            return
        self._home_auto_worker = None
        self._update_home_controls_state()
        if ok:
            status = self._protocol.get_status()
            if status.connected:
                self._last_status = status
                self._last_status_ts = time.monotonic()
                self._runtime_status_fault = False
                requalification = self._detect_runtime_requalification(
                    tcp_connected=True,
                    hardware_ready=True,
                )
                if requalification is not None:
                    self._apply_runtime_requalification_result(requalification)
        if not ok and message:
            QMessageBox.warning(self, "回零失败", message)
        self.statusBar().showMessage(f"回零: {message}" if message else ("回零完成" if ok else "回零失败"))

    def _on_speed_changed(self, value):
        self._jog_speed = float(value)
        self._speed_label.setText(f"{value}.0 mm/s")

    def _set_speed(self, value: int):
        """Set speed from preset buttons."""
        self._speed_slider.setValue(value)
        self._jog_speed = float(value)
        self._speed_label.setText(f"{value}.0 mm/s")

    def _get_cached_status(self, max_age_s: float = 0.6):
        if self._last_status is None:
            return None
        if max_age_s is not None:
            age = time.monotonic() - self._last_status_ts
            if age > max_age_s:
                return None
        return self._last_status

    def _log_motion_snapshot(self, tag: str, axis: str = None) -> None:
        status = self._get_cached_status()
        if not status or not status.connected:
            _UI_LOGGER.warning("%s status unavailable (cache)", tag)
            return
        io = status.io
        _UI_LOGGER.info(
            "%s io estop=%s door=%s lx+=%s lx-=%s ly+=%s ly-=%s",
            tag,
            io.estop,
            io.door,
            io.limit_x_pos,
            io.limit_x_neg,
            io.limit_y_pos,
            io.limit_y_neg,
        )
        if axis:
            axis_status = status.axes.get(axis)
            if axis_status:
                _UI_LOGGER.info(
                    "%s axis=%s pos=%.4f vel=%.4f enabled=%s homed=%s",
                    tag,
                    axis,
                    axis_status.position,
                    axis_status.velocity,
                    axis_status.enabled,
                    axis_status.homed,
                )
            else:
                _UI_LOGGER.info("%s axis=%s status missing", tag, axis)

    def _on_jog(self, axis: str, direction: int):
        self._auth.record_activity()
        if not self._check_motion_preconditions():
            self._jog_press_time = None
            return
        self._jog_press_time = time.monotonic()
        self._last_jog_context = {
            "axis": axis,
            "direction": direction,
            "speed": self._jog_speed,
            "press_time": self._jog_press_time,
        }
        _UI_LOGGER.info("Jog start axis=%s dir=%s speed=%.2f", axis, direction, self._jog_speed)
        self._log_motion_snapshot("Jog pre", axis)
        # 发送连续点动开始指令
        ok, msg = self._protocol.jog(axis, direction, self._jog_speed)
        self._log_motion_snapshot("Jog post", axis)
        if not ok:
            _UI_LOGGER.warning("Jog failed axis=%s dir=%s speed=%.2f msg=%s", axis, direction, self._jog_speed, msg)
            self.statusBar().showMessage(f"点动失败: {msg}" if msg else "点动失败")

    def _on_jog_release(self):
        # 按住即转，松开即停
        if not self._jog_press_time:
            _UI_LOGGER.info("Jog release with no press_time; stopping immediately")
            self._on_stop("jog_release_no_press")
            return
        # 发送停止运动指令
        _UI_LOGGER.info("Jog release: stopping motion")
        self._on_stop("jog_release")

    def _on_stop(self, reason: str = "unknown"):
        elapsed_ms = None
        if self._jog_press_time:
            elapsed_ms = int((time.monotonic() - self._jog_press_time) * 1000)
        axis = None
        direction = None
        speed = None
        if self._last_jog_context:
            axis = self._last_jog_context.get("axis")
            direction = self._last_jog_context.get("direction")
            speed = self._last_jog_context.get("speed")
        _UI_LOGGER.info(
            "Stop requested reason=%s elapsed_ms=%s axis=%s dir=%s speed=%s",
            reason,
            elapsed_ms,
            axis,
            direction,
            speed
        )
        if axis:
            self._log_motion_snapshot("Stop pre", axis)
        self._jog_press_time = None
        self._cancel_active_home_worker(reason=f"stop:{reason}")
        ok = self._protocol.stop()
        if not ok:
            _UI_LOGGER.warning("Stop request failed reason=%s", reason)
        if axis:
            self._log_motion_snapshot("Stop post", axis)

    def _on_estop(self):
        if not self._require_runtime_command_channel("急停"):
            return
        self._cancel_active_home_worker(reason="estop")
        ok, msg = self._protocol.emergency_stop()
        self.statusBar().showMessage(f"急停: {msg}" if ok else (f"急停失败: {msg}" if msg else "急停失败"))

    def _on_estop_reset(self):
        if not self._require_runtime_command_channel("急停复位"):
            return
        ok, msg = self._protocol.estop_reset()
        self.statusBar().showMessage(f"急停复位: {msg}" if ok else (f"急停复位失败: {msg}" if msg else "急停复位失败"))

    def _on_move_to(self):
        if not self._require_online_mode("移动控制"):
            return
        self._auth.record_activity()
        x = self._move_inputs["X"].value()
        y = self._move_inputs["Y"].value()
        speed = self._move_speed.value()
        ok = self._protocol.move_to(x, y, speed)
        self.statusBar().showMessage("移动中..." if ok else "移动失败")

    def _on_dispenser_start(self):
        if not self._require_online_mode("点胶控制"):
            return
        count = self._dispenser_count.value()
        interval = self._dispenser_interval.value()
        duration = self._dispenser_duration.value()
        ok = self._protocol.dispenser_start(count, interval, duration)
        if ok:
            self._increment_needle_count(count)
        if ok:
            self.statusBar().showMessage(f"点胶已启动 (次数:{count}, 间隔:{interval}ms, 持续:{duration}ms)")
        else:
            self.statusBar().showMessage("点胶启动失败")

    def _on_dispenser_pause(self):
        if not self._require_online_mode("点胶控制"):
            return
        ok = self._protocol.dispenser_pause()
        self.statusBar().showMessage("点胶已暂停" if ok else "点胶暂停失败")

    def _on_dispenser_resume(self):
        if not self._require_online_mode("点胶控制"):
            return
        ok = self._protocol.dispenser_resume()
        self.statusBar().showMessage("点胶已继续" if ok else "点胶恢复失败")

    def _on_dispenser_stop(self):
        if not self._require_online_mode("点胶控制"):
            return
        ok = self._protocol.dispenser_stop()
        self.statusBar().showMessage("点胶已停止" if ok else "点胶停止失败")

    def _on_supply_open(self):
        if not self._require_online_mode("供料阀控制"):
            return
        ok = self._protocol.supply_open()
        self.statusBar().showMessage("供料阀已打开" if ok else "供料阀打开失败")

    def _on_supply_close(self):
        if not self._require_online_mode("供料阀控制"):
            return
        ok = self._protocol.supply_close()
        self.statusBar().showMessage("供料阀已关闭" if ok else "供料阀关闭失败")

    def _on_purge(self):
        if not self._require_online_mode("清洗"):
            return
        timeout = self._purge_duration.value()
        ok = self._protocol.purge(timeout)
        if ok:
            self._last_purge_time = time.time()
            self._update_maintenance_display()
            self.statusBar().showMessage(f"清洗中 (超时: {timeout}ms)")
        else:
            self.statusBar().showMessage("清洗启动失败")

    def _on_dxf_browse(self):
        import os
        start_dir = ""
        if self._dxf_filepath:
            start_dir = os.path.dirname(self._dxf_filepath)
        else:
            for candidate in build_default_dxf_candidates(Path(__file__)):
                if candidate.exists():
                    start_dir = str(candidate)
                    break

        filepath, _ = QFileDialog.getOpenFileName(
            self, "选择DXF文件", start_dir, "DXF文件 (*.dxf);;所有文件 (*)"
        )
        if filepath:
            self._dxf_filepath = filepath
            self._dxf_filename_display.setText(filepath)
            # Auto load after selection
            self._on_dxf_load()

    def _on_speed_slider_changed(self, value: int):
        """Sync slider to spinbox."""
        self._dxf_speed.blockSignals(True)
        self._dxf_speed.setValue(float(value))
        self._dxf_speed.blockSignals(False)
        if self._dxf_loaded:
            self._invalidate_preview_plan()
        self._update_dxf_estimated_time()

    def _on_speed_spinbox_changed(self, value: float):
        """Sync spinbox to slider."""
        self._dxf_speed_slider.blockSignals(True)
        self._dxf_speed_slider.setValue(int(value))
        self._dxf_speed_slider.blockSignals(False)
        if self._dxf_loaded:
            self._invalidate_preview_plan()
        self._update_dxf_estimated_time()

    def _on_mode_toggled(self, checked: bool):
        if not checked:
            return
        self._update_start_button_state()
        if not self._dxf_loaded:
            return
        self._invalidate_preview_plan()
        if not self._auto_regenerate_preview_after_mode_change():
            self.statusBar().showMessage("运行模式已变更，请刷新预览并重新确认")

    def _invalidate_preview_plan(self):
        self._cancel_active_preview_worker(reason="preview_input_changed")
        self._preview_session.invalidate_plan()
        self._sync_preview_session_fields()
        self._update_info_label()

    def _auto_regenerate_preview_after_mode_change(self) -> bool:
        if not self._dxf_loaded:
            return False
        if self._is_offline_mode():
            if not self._dxf_filepath or not WEB_ENGINE_AVAILABLE or not self._dxf_view:
                return False
            self._generate_dxf_preview()
            return True
        if not self._dxf_artifact_id:
            return False
        if not self._is_launch_connected():
            return False
        if not WEB_ENGINE_AVAILABLE or not self._dxf_view:
            return False
        self._generate_dxf_preview()
        return True

    def _cancel_active_preview_worker(self, *, reason: str, keep_refresh_inflight: bool = False):
        self._preview_request_generation += 1
        worker = self._preview_snapshot_worker
        self._preview_snapshot_worker = None
        if worker is not None:
            try:
                worker.cancel()
            except Exception as exc:
                _UI_LOGGER.warning("preview_worker_cancel_failed reason=%s error=%s", reason, exc)
        if not keep_refresh_inflight:
            self._set_preview_refresh_inflight(False)
        _UI_LOGGER.info("preview_worker_cancelled reason=%s generation=%s", reason, self._preview_request_generation)

    def _update_dxf_info(self):
        info = self._protocol.dxf_get_info()
        total_segments = info.get("total_segments")
        if total_segments is None:
            total_segments = getattr(self, "_dxf_segment_count_cache", 0)
        self._dxf_segment_count_cache = int(total_segments or 0)
        self._preview_session.update_dxf_info(
            total_length_mm=float(info.get("total_length", 0.0) or 0.0),
            total_segments=self._dxf_segment_count_cache,
            speed_mm_s=self._dxf_speed.value(),
        )
        self._sync_preview_session_fields()
        self._update_info_label()

    def _update_dxf_estimated_time(self, _=None):
        self._preview_session.update_estimated_time(speed_mm_s=self._dxf_speed.value())
        self._sync_preview_session_fields()
        self._update_info_label()

    def _preview_source_text(self, preview_source: str | None = None) -> str:
        return self._preview_session.preview_source_text(preview_source)

    def _preview_source_warning(self, preview_source: str | None = None) -> str:
        return self._preview_session.preview_source_warning(preview_source)

    def _motion_preview_source_text(self, motion_preview_source: str | None = None) -> str:
        return self._preview_session.motion_preview_source_text(motion_preview_source)

    def _confirm_preview_gate(self) -> bool:
        snapshot = self._preview_gate.snapshot
        if snapshot is None:
            return False
        validation = self._preview_session.validate_before_confirmation()
        if not validation.ok:
            self._show_preflight_warning(validation.message)
            return False
        reply = QMessageBox.question(
            self,
            "预览确认",
            self._preview_session.build_confirmation_summary(),
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No,
        )
        if reply != QMessageBox.Yes:
            return False
        plan_id = self._current_plan_id or snapshot.snapshot_id
        ok, payload, error = self._protocol.dxf_preview_confirm(plan_id=plan_id, snapshot_hash=snapshot.snapshot_hash)
        if not ok:
            self._show_preflight_warning(f"预览确认失败: {error or 'Unknown error'}")
            _UI_LOGGER.warning(
                "preview_confirm_failed plan_id=%s snapshot_hash=%s error=%s",
                plan_id,
                snapshot.snapshot_hash,
                error,
            )
            return False

        confirm_result = self._preview_session.apply_confirmation_payload(payload)
        self._sync_preview_session_fields()
        if not confirm_result.ok:
            self._show_preflight_warning(confirm_result.message)
            return False

        _UI_LOGGER.info(
            "preview_confirmed snapshot_id=%s snapshot_hash=%s confirmed_at=%s",
            snapshot.snapshot_id,
            snapshot.snapshot_hash,
            payload.get("confirmed_at", ""),
        )
        self._update_info_label()
        return True

    def _update_info_label(self):
        self._sync_preview_session_fields()
        self._dxf_info_label.setText(self._preview_session.info_label_text())
        self._update_preview_playback_controls()

    def _set_preview_message_html(self, title: str, detail: str = "", is_error: bool = False):
        if not WEB_ENGINE_AVAILABLE or not self._dxf_view:
            self._set_preview_debug_html(title, detail, is_error=is_error)
            self._update_preview_playback_controls()
            return
        title_color = "#ff7b72" if is_error else "#8be9fd"
        detail_html = html.escape(detail) if detail else " "
        message_html = (
            "<html><body style='background:#141414;color:#e8e8e8;font-family:Segoe UI;padding:24px;'>"
            f"<h3 style='margin-top:0;color:{title_color};'>{html.escape(title)}</h3>"
            f"<p style='color:#b8b8b8;line-height:1.6;'>{detail_html}</p>"
            "</body></html>"
        )
        self._preview_dom_ready = False
        self._dxf_view.setHtml(message_html)
        self._set_preview_debug_html(title, detail, is_error=is_error)
        self._update_preview_playback_controls()

    def _set_preview_debug_html(self, title: str, detail: str = "", is_error: bool = False) -> None:
        if not hasattr(self, "_preview_debug_view"):
            return
        title_color = "#ff7b72" if is_error else "#8be9fd"
        detail_html = html.escape(detail) if detail else "暂无额外信息。"
        self._preview_debug_view.setHtml(
            "<html><body style='background:#1e1e1e;color:#e8e8e8;font-family:Segoe UI;padding:18px;'>"
            f"<h3 style='margin-top:0;color:{title_color};'>{html.escape(title)}</h3>"
            f"<p style='color:#b8b8b8;line-height:1.6;'>{detail_html}</p>"
            "</body></html>"
        )

    def _set_preview_refresh_inflight(self, inflight: bool):
        self._preview_session.set_refresh_inflight(inflight)
        self._sync_preview_session_fields()
        self._update_preview_refresh_button_state()

    def _update_preview_refresh_button_state(self):
        if not hasattr(self, "_refresh_preview_btn"):
            return
        if self._is_offline_mode():
            enabled = bool(self._dxf_loaded and WEB_ENGINE_AVAILABLE and self._dxf_view and (not self._preview_refresh_inflight))
        else:
            enabled = self._preview_session.should_enable_refresh(
                offline_mode=self._is_offline_mode(),
                connected=self._is_launch_connected(),
                dxf_loaded=self._dxf_loaded,
            )
        self._refresh_preview_btn.setEnabled(enabled)

    def _is_unrecoverable_preview_resync_error(self, error_message: str, error_code=None) -> bool:
        return self._preview_session.is_unrecoverable_resync_error(error_message, error_code)

    def _degrade_preview_after_unrecoverable_resync(self, plan_id: str, error_message: str):
        result = self._preview_session.handle_resync_error(plan_id, error_message, error_code=3012)
        self._sync_preview_session_fields()
        if result is None:
            return
        self._update_info_label()
        self.statusBar().showMessage(result.status_message)
        self._set_preview_message_html(result.title, result.detail, is_error=result.is_error)
        _UI_LOGGER.warning("preview_resync_aborted reason=unrecoverable plan_id=%s error=%s", plan_id, error_message)

    def _sync_preview_state_from_runtime(self):
        now = time.monotonic()
        if not self._preview_session.should_request_runtime_resync(
            offline_mode=self._is_offline_mode(),
            connected=self._is_launch_connected(),
            production_running=self._production_running,
            current_job_id=self._current_job_id,
            now_monotonic=now,
        ):
            return
        self._sync_preview_session_fields()

        plan_id = self._current_plan_id
        try:
            ok, payload, error, error_code = self._protocol.dxf_preview_snapshot_with_error_details(
                plan_id=plan_id,
                max_polyline_points=4000,
                max_glue_points=5000,
            )
        except Exception as exc:
            _UI_LOGGER.warning("preview_resync_failed plan_id=%s error=%s", plan_id, exc)
            return

        if not ok:
            result = self._preview_session.handle_resync_error(plan_id, error, error_code)
            self._sync_preview_session_fields()
            if result is not None:
                self._update_info_label()
                self.statusBar().showMessage(result.status_message)
                self._set_preview_message_html(result.title, result.detail, is_error=result.is_error)
                _UI_LOGGER.warning(
                    "preview_resync_aborted reason=unrecoverable plan_id=%s error=%s",
                    plan_id,
                    error,
                )
                return
            _UI_LOGGER.warning(
                "preview_resync_failed plan_id=%s error=%s error_code=%s",
                plan_id,
                error,
                error_code,
            )
            return
        if not isinstance(payload, dict):
            result = self._preview_session.handle_invalid_resync_payload()
            self._sync_preview_session_fields()
            self._update_info_label()
            self.statusBar().showMessage(result.status_message)
            self._set_preview_message_html(result.title, result.detail, is_error=result.is_error)
            _UI_LOGGER.warning("preview_resync_failed plan_id=%s error=invalid_payload", plan_id)
            return

        payload = self._preview_session.prepare_resync_payload(
            payload,
            current_dry_run=bool(self._mode_dryrun.isChecked()) if hasattr(self, "_mode_dryrun") else False,
        )
        self._sync_preview_session_fields()
        self._on_preview_snapshot_completed(True, payload, "", source="resync")
        self._preview_session.clear_resync_pending()
        self._sync_preview_session_fields()

    def _render_runtime_preview_html(
        self,
        snapshot: PreviewSnapshotMeta,
        speed_mm_s: float,
        dry_run: bool,
        preview_source: str,
        glue_points: list,
        preview_kind: str,
        glue_reveal_lengths_mm: list[float] | None = None,
        motion_preview: list | None = None,
        execution_polyline: list | None = None,
        motion_preview_meta: MotionPreviewMeta | None = None,
        preview_warning: str = "",
        preview_diagnostic_notice: PreviewDiagnosticNotice | None = None,
        motion_preview_warning: str = "",
        preview_validation_classification: str = "",
        preview_exception_reason: str = "",
        preview_failure_reason: str = "",
        preview_diagnostic_code: str = "",
    ) -> str:
        normalized_source = str(preview_source or "").strip().lower()
        effective_motion_preview = list(motion_preview or execution_polyline or [])
        effective_motion_preview_meta = motion_preview_meta
        if effective_motion_preview_meta is None and effective_motion_preview:
            effective_motion_preview_meta = MotionPreviewMeta(
                source="",
                kind="polyline",
                point_count=len(effective_motion_preview),
                source_point_count=len(effective_motion_preview),
                is_sampled=False,
                sampling_strategy="",
            )
        effective_motion_preview_meta = effective_motion_preview_meta or MotionPreviewMeta()
        warning_banner = ""
        if preview_warning:
            warning_banner = (
                "<div style='margin-bottom:14px;padding:12px 14px;border:1px solid #854d0e;"
                "background:#3b2a12;color:#fde68a;'>"
                f"<strong>预览质量告警。</strong> {html.escape(preview_warning)}"
                "</div>"
            )
        motion_warning_banner = ""
        if motion_preview_warning:
            motion_warning_banner = (
                "<div style='margin-bottom:14px;padding:12px 14px;border:1px solid #854d0e;"
                "background:#2d2110;color:#fde68a;'>"
                f"<strong>运动轨迹预览提示。</strong> {html.escape(motion_preview_warning)}"
                "</div>"
            )
        validation_banner = ""
        if preview_diagnostic_notice is not None:
            validation_banner = (
                "<div style='margin-bottom:14px;padding:12px 14px;border:1px solid #854d0e;"
                "background:#2d2110;color:#fde68a;'>"
                f"<strong>{html.escape(preview_diagnostic_notice.title)}。</strong> "
                f"{html.escape(preview_diagnostic_notice.detail)}"
                "</div>"
            )
        elif preview_validation_classification == "fail":
            blocking_text = preview_failure_reason or preview_exception_reason or "authority 校验失败"
            validation_banner = (
                "<div style='margin-bottom:14px;padding:12px 14px;border:1px solid #7f1d1d;"
                "background:#3a1717;color:#ffd5d5;'>"
                f"<strong>阻断原因。</strong> {html.escape(blocking_text)}"
                "</div>"
            )
        all_points = list(glue_points) + list(effective_motion_preview)
        min_x = min(point[0] for point in all_points)
        max_x = max(point[0] for point in all_points)
        min_y = min(point[1] for point in all_points)
        max_y = max(point[1] for point in all_points)

        width = 920.0
        height = 520.0
        padding = 24.0
        glue_point_spacing_mm = 3.0
        glue_dot_diameter_mm = 1.5
        span_x = max(max_x - min_x, 1e-6)
        span_y = max(max_y - min_y, 1e-6)
        content_width = width - 2 * padding
        content_height = height - 2 * padding
        scale_x_px_per_mm = content_width / span_x
        scale_y_px_per_mm = content_height / span_y
        scale_px_per_mm = min(scale_x_px_per_mm, scale_y_px_per_mm)
        used_width = span_x * scale_px_per_mm
        used_height = span_y * scale_px_per_mm
        offset_x = padding + (content_width - used_width) * 0.5
        offset_y = padding + (content_height - used_height) * 0.5
        dot_diameter_px = glue_dot_diameter_mm * scale_px_per_mm
        dot_radius = max(0.6, min(8.0, dot_diameter_px * 0.5))

        def _map_points(world_points: list[tuple[float, float]]) -> list[tuple[float, float]]:
            mapped_points = []
            for x_value, y_value in world_points:
                mapped_x = offset_x + (x_value - min_x) * scale_px_per_mm
                mapped_y = height - (offset_y + (y_value - min_y) * scale_px_per_mm)
                mapped_points.append((mapped_x, mapped_y))
            return mapped_points

        display_points = _map_points(glue_points)
        display_motion_preview = _map_points(effective_motion_preview)
        playback_data_markup = ""
        playback_overlay_markup = ""

        reveal_lengths: list[float] = []
        if len(display_motion_preview) >= 2:
            playback_overlay_markup = (
                "<circle id='preview-head-shadow' cx='0' cy='0' r='8.2' fill='#3f2a06' opacity='0.92' />"
                "<circle id='preview-head' cx='0' cy='0' r='5.2' fill='#f59e0b' stroke='#fff4d6' stroke-width='1.2' />"
            )
            playback_points_json = json.dumps([[round(point_x, 3), round(point_y, 3)] for point_x, point_y in display_motion_preview])
            cumulative_lengths = [0.0]
            total_display_length = 0.0
            for index in range(1, len(display_motion_preview)):
                prev_x, prev_y = display_motion_preview[index - 1]
                curr_x, curr_y = display_motion_preview[index]
                total_display_length += ((curr_x - prev_x) ** 2 + (curr_y - prev_y) ** 2) ** 0.5
                cumulative_lengths.append(round(total_display_length, 6))
            reveal_lengths, reveal_diagnostics = self._resolve_preview_glue_reveal_lengths(
                glue_points=display_points,
                motion_preview=display_motion_preview,
                glue_reveal_lengths_mm=glue_reveal_lengths_mm,
                scale_px_per_mm=scale_px_per_mm,
                snapshot=snapshot,
                motion_preview_meta=effective_motion_preview_meta,
            )
            playback_lengths_json = json.dumps(cumulative_lengths)
            reveal_lengths_json = json.dumps(reveal_lengths)
            playback_data_markup = (
                "<script>"
                f"window.previewPlaybackData={{points:{playback_points_json},cumulativeLengths:{playback_lengths_json},"
                f"glueRevealLengths:{reveal_lengths_json},totalLength:{total_display_length:.6f}}};"
                "window.updatePreviewPlayback=function(progress,playbackState){"
                "const data=window.previewPlaybackData||{};"
                "const pts=Array.isArray(data.points)?data.points:[];"
                "const cumulative=Array.isArray(data.cumulativeLengths)?data.cumulativeLengths:[];"
                "const revealLengths=Array.isArray(data.glueRevealLengths)?data.glueRevealLengths:[];"
                "const totalLength=Number(data.totalLength||0);"
                "const normalized=Math.max(0,Math.min(1,Number(progress||0)));"
                "const state=String(playbackState||'idle');"
                "const head=document.getElementById('preview-head');"
                "const headShadow=document.getElementById('preview-head-shadow');"
                "const glueDots=Array.from(document.querySelectorAll('[data-preview-glue-point=\"1\"]'));"
                "if(!head||!headShadow||pts.length===0){return;}"
                "const setVisibility=function(element,visible){element.style.display=visible?'':'none';};"
                "const setMarker=function(point){head.setAttribute('cx',point[0].toFixed(2));head.setAttribute('cy',point[1].toFixed(2));"
                "headShadow.setAttribute('cx',point[0].toFixed(2));headShadow.setAttribute('cy',point[1].toFixed(2));};"
                "const syncGlueDots=function(traveledLength){"
                "glueDots.forEach(function(dot,index){"
                "const revealAt=Number(revealLengths[index]||0);"
                "setVisibility(dot,state!=='idle'&&traveledLength+0.25>=revealAt);"
                "});"
                "};"
                "if(state==='idle'){setVisibility(head,false);setVisibility(headShadow,false);syncGlueDots(-1);return;}"
                "setVisibility(head,true);setVisibility(headShadow,true);"
                "if(pts.length===1||totalLength<=0){setMarker(pts[0]);syncGlueDots(totalLength);return;}"
                "const traveled=normalized*totalLength;"
                "let marker=pts[0];"
                "for(let index=1;index<pts.length;index+=1){"
                "const prev=pts[index-1];const curr=pts[index];"
                "const prevLength=Number(cumulative[index-1]||0);const currLength=Number(cumulative[index]||prevLength);"
                "if(traveled>=currLength-1e-6){marker=curr;continue;}"
                "const segmentLength=Math.max(currLength-prevLength,1e-9);"
                "const ratio=Math.max(0,Math.min(1,(traveled-prevLength)/segmentLength));"
                "marker=[prev[0]+((curr[0]-prev[0])*ratio),prev[1]+((curr[1]-prev[1])*ratio)];"
                "break;}"
                "setMarker(marker);syncGlueDots(traveled);"
                "};"
                "document.addEventListener('DOMContentLoaded',function(){window.updatePreviewPlayback(0,'idle');});"
                "</script>"
            )
        points_markup = []
        for index, (point_x, point_y) in enumerate(display_points):
            point_style = "display:none;" if reveal_lengths else ""
            points_markup.append(
                f"<circle id='preview-glue-point-{index}' data-preview-glue-point='1' "
                f"cx='{point_x:.2f}' cy='{point_y:.2f}' r='{dot_radius:.1f}' "
                f"fill='#00d084' style='{point_style}' />"
            )
        point_cloud_svg = "".join(points_markup)
        return (
            "<html><head>"
            "<style>"
            "html,body{height:100%;margin:0;background:#1e1e1e;color:#e8e8e8;font-family:Segoe UI;}"
            "body{display:flex;min-height:100%;}"
            ".preview-root{box-sizing:border-box;display:flex;flex-direction:column;gap:2px;"
            "height:100%;width:100%;padding:0;}"
            ".preview-canvas{flex:1 1 auto;min-height:0;background:#141414;border:1px solid #333;}"
            ".preview-canvas svg{width:100%;height:100%;display:block;}"
            "</style>"
            "</head><body>"
            "<div class='preview-root'>"
            f"{warning_banner}"
            f"{motion_warning_banner}"
            f"{validation_banner}"
            f"<div class='preview-canvas'><svg viewBox='0 0 {width:.0f} {height:.0f}' preserveAspectRatio='xMidYMid meet'>"
            f"{point_cloud_svg}"
            f"{playback_overlay_markup}"
            "</svg></div>"
            "</div>"
            f"{playback_data_markup}"
            "</body></html>"
        )

    def _resolve_preview_glue_reveal_lengths(
        self,
        *,
        glue_points: list[tuple[float, float]],
        motion_preview: list[tuple[float, float]],
        glue_reveal_lengths_mm: list[float] | None,
        scale_px_per_mm: float,
        snapshot: PreviewSnapshotMeta,
        motion_preview_meta: MotionPreviewMeta | None,
    ) -> tuple[list[float], dict[str, object]]:
        service_lengths = list(glue_reveal_lengths_mm or [])
        if service_lengths:
            monotonic = True
            previous_length = service_lengths[0]
            for current_length in service_lengths[1:]:
                if current_length + 1e-6 < previous_length:
                    monotonic = False
                    break
                previous_length = current_length
            if len(service_lengths) == len(glue_points) and monotonic:
                reveal_lengths = [round(length_mm * scale_px_per_mm, 6) for length_mm in service_lengths]
                diagnostics: dict[str, object] = {
                    "sample_points": [
                        {
                            "glue_index": index,
                            "reveal_length_mm": round(service_lengths[index], 6),
                            "reveal_length": reveal_lengths[index],
                            "mode": "authority",
                        }
                        for index in range(min(len(service_lengths), 12))
                    ],
                    "aligned_count": len(reveal_lengths),
                    "fallback_count": 0,
                    "stalled_count": sum(
                        1
                        for index, reveal_length in enumerate(reveal_lengths)
                        if index == 0 or abs(reveal_length - reveal_lengths[index - 1]) <= 1e-6
                    ),
                    "max_segment_jump": 0,
                    "max_distance_sq": 0.0,
                    "final_reveal_length": reveal_lengths[-1] if reveal_lengths else 0.0,
                    "source": "authority_glue_reveal_lengths_mm",
                }
                self._log_preview_glue_reveal_diagnostics(
                    diagnostics,
                    glue_point_count=len(glue_points),
                    motion_point_count=len(motion_preview),
                    snapshot=snapshot,
                    motion_preview_meta=motion_preview_meta,
                )
                return reveal_lengths, diagnostics
            reason = "length_mismatch" if len(service_lengths) != len(glue_points) else "non_monotonic"
            _UI_LOGGER.warning(
                "preview_glue_reveal_compat_warning file=%s snapshot_id=%s reason=%s glue_points=%d reveal_lengths=%d",
                os.path.basename(self._dxf_filepath) if self._dxf_filepath else "-",
                snapshot.snapshot_id,
                reason,
                len(glue_points),
                len(service_lengths),
            )

        reveal_lengths, reveal_diagnostics = self._build_preview_glue_reveal_lengths_with_diagnostics(
            glue_points=glue_points,
            motion_preview=motion_preview,
        )
        reveal_diagnostics["source"] = "legacy_motion_preview_projection"
        self._log_preview_glue_reveal_diagnostics(
            reveal_diagnostics,
            glue_point_count=len(glue_points),
            motion_point_count=len(motion_preview),
            snapshot=snapshot,
            motion_preview_meta=motion_preview_meta,
        )
        return reveal_lengths, reveal_diagnostics

    def _build_preview_glue_reveal_lengths(
        self,
        glue_points: list[tuple[float, float]],
        motion_preview: list[tuple[float, float]],
    ) -> list[float]:
        reveal_lengths, _ = self._build_preview_glue_reveal_lengths_with_diagnostics(
            glue_points=glue_points,
            motion_preview=motion_preview,
        )
        return reveal_lengths

    def _build_preview_glue_reveal_lengths_with_diagnostics(
        self,
        glue_points: list[tuple[float, float]],
        motion_preview: list[tuple[float, float]],
    ) -> tuple[list[float], dict[str, object]]:
        if not glue_points:
            return [], {"sample_points": []}
        if len(motion_preview) <= 1:
            return [0.0 for _ in glue_points], {"sample_points": []}

        def _normalized_direction(index: int) -> tuple[float, float] | None:
            if len(glue_points) <= 1:
                return None
            candidates: list[tuple[float, float]] = []
            if index + 1 < len(glue_points):
                next_x, next_y = glue_points[index + 1]
                curr_x, curr_y = glue_points[index]
                candidates.append((next_x - curr_x, next_y - curr_y))
            if index > 0:
                curr_x, curr_y = glue_points[index]
                prev_x, prev_y = glue_points[index - 1]
                candidates.append((curr_x - prev_x, curr_y - prev_y))
            for delta_x, delta_y in candidates:
                length = (delta_x ** 2 + delta_y ** 2) ** 0.5
                if length > 1e-9:
                    return (delta_x / length, delta_y / length)
            return None

        segment_specs: list[tuple[int, float, float, float, float, float]] = []
        cumulative_length = 0.0
        for index in range(1, len(motion_preview)):
            start_x, start_y = motion_preview[index - 1]
            end_x, end_y = motion_preview[index]
            delta_x = end_x - start_x
            delta_y = end_y - start_y
            segment_length = (delta_x ** 2 + delta_y ** 2) ** 0.5
            if segment_length <= 1e-9:
                continue
            segment_specs.append(
                (
                    index,
                    start_x,
                    start_y,
                    delta_x / segment_length,
                    delta_y / segment_length,
                    cumulative_length,
                )
            )
            cumulative_length += segment_length

        reveal_lengths: list[float] = []
        sample_points: list[dict[str, object]] = []
        aligned_count = 0
        fallback_count = 0
        stalled_count = 0
        max_segment_jump = 0
        previous_segment_index = 1
        max_distance_sq = 0.0
        min_reveal_length = 0.0
        min_segment_index = 1
        for glue_index, (point_x, point_y) in enumerate(glue_points):
            expected_direction = _normalized_direction(glue_index)
            best_aligned: tuple[float, float, int] | None = None
            best_fallback: tuple[float, float, int] | None = None
            for segment_index, start_x, start_y, unit_x, unit_y, start_length in segment_specs:
                if segment_index < min_segment_index:
                    continue
                end_x, end_y = motion_preview[segment_index]
                delta_x = end_x - start_x
                delta_y = end_y - start_y
                segment_length = (delta_x ** 2 + delta_y ** 2) ** 0.5
                if segment_length <= 1e-9:
                    continue
                projection_ratio = (
                    ((point_x - start_x) * delta_x) + ((point_y - start_y) * delta_y)
                ) / (segment_length ** 2)
                projection_ratio = max(0.0, min(1.0, projection_ratio))
                projected_x = start_x + delta_x * projection_ratio
                projected_y = start_y + delta_y * projection_ratio
                distance_sq = ((point_x - projected_x) ** 2) + ((point_y - projected_y) ** 2)
                projected_length = start_length + segment_length * projection_ratio
                if projected_length + 1e-6 < min_reveal_length:
                    continue
                fallback_candidate = (distance_sq, projected_length, segment_index)
                if best_fallback is None or fallback_candidate < best_fallback:
                    best_fallback = fallback_candidate
                if expected_direction is None:
                    continue
                alignment = (expected_direction[0] * unit_x) + (expected_direction[1] * unit_y)
                if alignment < 0.35:
                    continue
                aligned_candidate = (distance_sq, projected_length, segment_index)
                if best_aligned is None or aligned_candidate < best_aligned:
                    best_aligned = aligned_candidate
            selected = best_aligned or best_fallback
            if selected is None:
                reveal_length = min_reveal_length
                selected_segment_index = min_segment_index
                selected_distance_sq = 0.0
                selected_mode = "carry_forward"
            else:
                reveal_length = max(min_reveal_length, selected[1])
                selected_segment_index = selected[2]
                selected_distance_sq = selected[0]
                selected_mode = "aligned" if best_aligned is not None else "fallback"
                min_segment_index = max(1, selected_segment_index - 1)
            if selected_mode == "aligned":
                aligned_count += 1
            elif selected_mode == "fallback":
                fallback_count += 1
            if abs(reveal_length - min_reveal_length) <= 1e-6:
                stalled_count += 1
            if selected_segment_index > previous_segment_index:
                max_segment_jump = max(max_segment_jump, selected_segment_index - previous_segment_index)
            previous_segment_index = selected_segment_index
            max_distance_sq = max(max_distance_sq, float(selected_distance_sq))
            reveal_length = round(reveal_length, 6)
            reveal_lengths.append(reveal_length)
            if glue_index < 12:
                sample_points.append(
                    {
                        "glue_index": glue_index,
                        "segment_index": selected_segment_index,
                        "reveal_length": reveal_length,
                        "distance_sq": round(float(selected_distance_sq), 6),
                        "mode": selected_mode,
                    }
                )
            min_reveal_length = reveal_length
        diagnostics: dict[str, object] = {
            "sample_points": sample_points,
            "aligned_count": aligned_count,
            "fallback_count": fallback_count,
            "stalled_count": stalled_count,
            "max_segment_jump": max_segment_jump,
            "max_distance_sq": round(max_distance_sq, 6),
            "final_reveal_length": reveal_lengths[-1] if reveal_lengths else 0.0,
        }
        return reveal_lengths, diagnostics

    def _log_preview_glue_reveal_diagnostics(
        self,
        diagnostics: dict[str, object],
        *,
        glue_point_count: int,
        motion_point_count: int,
        snapshot: PreviewSnapshotMeta,
        motion_preview_meta: MotionPreviewMeta | None,
    ) -> None:
        filename = os.path.basename(self._dxf_filepath) if self._dxf_filepath else "-"
        meta = motion_preview_meta or MotionPreviewMeta()
        _UI_LOGGER.info(
            "preview_glue_reveal file=%s snapshot_id=%s glue_points=%d motion_points=%d motion_source_points=%d "
            "motion_sampled=%s motion_sampling_strategy=%s source=%s aligned=%s fallback=%s stalled=%s "
            "max_segment_jump=%s max_distance_sq=%s final_reveal_length=%s samples=%s",
            filename,
            snapshot.snapshot_id,
            glue_point_count,
            motion_point_count,
            meta.source_point_count or motion_point_count,
            meta.is_sampled,
            meta.sampling_strategy or "-",
            diagnostics.get("source", "legacy_motion_preview_projection"),
            diagnostics.get("aligned_count", 0),
            diagnostics.get("fallback_count", 0),
            diagnostics.get("stalled_count", 0),
            diagnostics.get("max_segment_jump", 0),
            diagnostics.get("max_distance_sq", 0.0),
            diagnostics.get("final_reveal_length", 0.0),
            json.dumps(diagnostics.get("sample_points", []), ensure_ascii=True),
        )

    def _render_preview_debug_html(
        self,
        snapshot: PreviewSnapshotMeta,
        speed_mm_s: float,
        dry_run: bool,
        preview_source: str,
        glue_points: list,
        preview_kind: str,
        motion_preview: list | None = None,
        motion_preview_meta: MotionPreviewMeta | None = None,
        preview_validation_classification: str = "",
        preview_diagnostic_code: str = "",
    ) -> str:
        mode_text = "空跑" if dry_run else "生产"
        generated_at = html.escape(snapshot.generated_at or "-")
        normalized_source = str(preview_source or "").strip().lower()
        normalized_kind = str(preview_kind or "").strip().lower() or "glue_points"
        effective_motion_preview = list(motion_preview or [])
        effective_motion_preview_meta = motion_preview_meta
        if effective_motion_preview_meta is None and effective_motion_preview:
            effective_motion_preview_meta = MotionPreviewMeta(
                source="",
                kind="polyline",
                point_count=len(effective_motion_preview),
                source_point_count=len(effective_motion_preview),
                is_sampled=False,
                sampling_strategy="",
            )
        effective_motion_preview_meta = effective_motion_preview_meta or MotionPreviewMeta()
        source_text = html.escape(self._preview_source_text(normalized_source))
        source_warning = html.escape(self._preview_source_warning(normalized_source))
        motion_preview_source_text = html.escape(self._motion_preview_source_text(effective_motion_preview_meta.source))
        motion_preview_kind = html.escape(effective_motion_preview_meta.kind or ("polyline" if effective_motion_preview else "-"))
        motion_preview_point_count = effective_motion_preview_meta.point_count or len(effective_motion_preview)
        motion_preview_source_point_count = (
            effective_motion_preview_meta.source_point_count or motion_preview_point_count
        )
        motion_preview_sampling_text = (
            f"是（显示 {motion_preview_point_count} / 源 {motion_preview_source_point_count}）"
            if effective_motion_preview_meta.is_sampled
            else "否"
        )
        motion_preview_sampling_strategy = html.escape(effective_motion_preview_meta.sampling_strategy or "-")
        all_points = list(glue_points) + list(effective_motion_preview)
        min_x = min(point[0] for point in all_points)
        max_x = max(point[0] for point in all_points)
        min_y = min(point[1] for point in all_points)
        max_y = max(point[1] for point in all_points)
        return (
            "<html><body style='background:#1e1e1e;color:#e8e8e8;font-family:Segoe UI;padding:18px;'>"
            "<h3 style='margin-top:0;color:#8be9fd;'>预览调试参数</h3>"
            "<table style='border-collapse:collapse;'>"
            f"<tr><td style='padding:4px 16px 4px 0;'>来源</td><td>{source_text}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>来源说明</td><td>{source_warning}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>主预览语义</td><td>{html.escape(normalized_kind)}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>运动轨迹来源</td><td>{motion_preview_source_text}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>运动轨迹语义</td><td>{motion_preview_kind}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>校验分类</td><td>{html.escape(preview_validation_classification or 'pass')}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>诊断码</td><td>{html.escape(preview_diagnostic_code or '-')}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>模式</td><td>{mode_text}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>速度</td><td>{speed_mm_s:.3f} mm/s</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>段数</td><td>{snapshot.segment_count}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>胶点数</td><td>{snapshot.point_count}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>运动轨迹预览点</td><td>{motion_preview_point_count}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>运动轨迹源点数</td><td>{motion_preview_source_point_count}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>运动轨迹已采样</td><td>{motion_preview_sampling_text}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>运动轨迹采样策略</td><td>{motion_preview_sampling_strategy}</td></tr>"
            "<tr><td style='padding:4px 16px 4px 0;'>胶点圆心距</td><td>3.0 mm</td></tr>"
            "<tr><td style='padding:4px 16px 4px 0;'>胶点直径</td><td>1.5 mm</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>总长度</td><td>{snapshot.total_length_mm:.3f} mm</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>预估时长</td><td>{snapshot.estimated_time_s:.3f} s</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>X范围</td><td>{min_x:.3f} ~ {max_x:.3f}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>Y范围</td><td>{min_y:.3f} ~ {max_y:.3f}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>快照ID</td><td>{html.escape(snapshot.snapshot_id)}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>快照哈希</td><td>{html.escape(snapshot.snapshot_hash)}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>生成时间</td><td>{generated_at}</td></tr>"
            "</table>"
            "</body></html>"
        )

    def _on_preview_snapshot_completed(
        self,
        ok: bool,
        payload: dict,
        error: str,
        source: str = "worker",
        request_token: int | None = None,
    ):
        if request_token is not None and request_token != self._preview_request_generation:
            _UI_LOGGER.info(
                "preview_result_ignored reason=stale_worker_result request_token=%s active_generation=%s",
                request_token,
                self._preview_request_generation,
            )
            return

        self._preview_snapshot_worker = None
        self._set_preview_refresh_inflight(False)

        if not ok:
            failure_message = error or "胶点预览生成失败"
            result = self._preview_session.handle_worker_error(failure_message)
            self._sync_preview_session_fields()
            self._update_info_label()
            self.statusBar().showMessage(result.status_message)
            self._set_preview_message_html(result.title, result.detail, is_error=result.is_error)
            _UI_LOGGER.warning("preview_failed stage=snapshot error=%s", failure_message)
            return

        current_dry_run = self._mode_dryrun.isChecked() if hasattr(self, "_mode_dryrun") else False
        result = self._preview_session.process_snapshot_payload(
            payload,
            current_dry_run=bool(current_dry_run),
            source=source,
        )
        self._sync_preview_session_fields()
        if not result.ok:
            self._update_info_label()
            self.statusBar().showMessage(result.status_message)
            self._set_preview_message_html(result.title, result.detail, is_error=result.is_error)
            _UI_LOGGER.warning("preview_failed source=%s payload_keys=%s payload=%s", source, sorted(payload.keys()), payload)
            return

        self._update_info_label()

        render_started_at = time.perf_counter()
        html_content = self._render_runtime_preview_html(
            snapshot=result.snapshot,
            speed_mm_s=self._dxf_speed.value(),
            dry_run=result.dry_run,
            preview_source=result.preview_source,
            glue_points=list(result.glue_points),
            glue_reveal_lengths_mm=list(result.glue_reveal_lengths_mm),
            motion_preview=list(result.motion_preview),
            motion_preview_meta=result.motion_preview_meta,
            preview_kind=result.preview_kind,
            preview_warning=result.preview_warning,
            preview_diagnostic_notice=result.preview_diagnostic_notice,
            motion_preview_warning=result.motion_preview_warning,
            preview_validation_classification=self._preview_session.state.preview_validation_classification,
            preview_exception_reason=self._preview_session.state.preview_exception_reason,
            preview_failure_reason=self._preview_session.state.preview_failure_reason,
            preview_diagnostic_code=self._preview_session.state.preview_diagnostic_code,
        )
        debug_html = self._render_preview_debug_html(
            snapshot=result.snapshot,
            speed_mm_s=self._dxf_speed.value(),
            dry_run=result.dry_run,
            preview_source=result.preview_source,
            glue_points=list(result.glue_points),
            motion_preview=list(result.motion_preview),
            motion_preview_meta=result.motion_preview_meta,
            preview_kind=result.preview_kind,
            preview_validation_classification=self._preview_session.state.preview_validation_classification,
            preview_diagnostic_code=self._preview_session.state.preview_diagnostic_code,
        )
        self._preview_dom_ready = False
        self._dxf_view.setHtml(html_content)
        self._preview_debug_view.setHtml(debug_html)
        if not hasattr(self._dxf_view, "loadFinished"):
            self._preview_dom_ready = True
            self._sync_preview_playback_visual_state()
        self._apply_preview_playback_status(sync_dom=False)
        render_elapsed_ms = int(round((time.perf_counter() - render_started_at) * 1000.0))
        performance_profile = payload.get("performance_profile", {})
        worker_profile = payload.get("worker_profile", {})
        if isinstance(performance_profile, dict) or isinstance(worker_profile, dict):
            _UI_LOGGER.info(
                "preview_performance_profile snapshot_id=%s authority_cache_hit=%s authority_joined_inflight=%s "
                "authority_wait_ms=%s pb_ms=%s load_ms=%s process_path_ms=%s authority_build_ms=%s "
                "authority_total_ms=%s prepare_total_ms=%s plan_prepare_rpc_ms=%s snapshot_rpc_ms=%s "
                "worker_total_ms=%s render_html_ms=%s",
                result.snapshot.snapshot_id if result.snapshot is not None else "",
                performance_profile.get("authority_cache_hit", "") if isinstance(performance_profile, dict) else "",
                performance_profile.get("authority_joined_inflight", "") if isinstance(performance_profile, dict) else "",
                performance_profile.get("authority_wait_ms", "") if isinstance(performance_profile, dict) else "",
                performance_profile.get("pb_prepare_ms", "") if isinstance(performance_profile, dict) else "",
                performance_profile.get("path_load_ms", "") if isinstance(performance_profile, dict) else "",
                performance_profile.get("process_path_ms", "") if isinstance(performance_profile, dict) else "",
                performance_profile.get("authority_build_ms", "") if isinstance(performance_profile, dict) else "",
                performance_profile.get("authority_total_ms", "") if isinstance(performance_profile, dict) else "",
                performance_profile.get("prepare_total_ms", "") if isinstance(performance_profile, dict) else "",
                worker_profile.get("plan_prepare_rpc_ms", "") if isinstance(worker_profile, dict) else "",
                worker_profile.get("snapshot_rpc_ms", "") if isinstance(worker_profile, dict) else "",
                worker_profile.get("worker_total_ms", "") if isinstance(worker_profile, dict) else "",
                render_elapsed_ms,
            )
        if result.preview_warning and result.snapshot is not None:
            _UI_LOGGER.warning(
                "preview_sampling_warning snapshot_id=%s glue_points=%d motion_source_points=%d",
                result.snapshot.snapshot_id,
                result.snapshot.point_count,
                result.motion_preview_meta.source_point_count if result.motion_preview_meta is not None else 0,
            )
        if result.motion_preview_warning and result.snapshot is not None:
            _UI_LOGGER.warning(
                "motion_preview_warning snapshot_id=%s motion_source=%s detail=%s",
                result.snapshot.snapshot_id,
                result.motion_preview_meta.source if result.motion_preview_meta is not None else "",
                result.motion_preview_warning,
            )
        self.statusBar().showMessage(result.status_message)
        _UI_LOGGER.info(
            "preview_ready snapshot_id=%s snapshot_hash=%s preview_source=%s glue_points=%d motion_points=%d",
            result.snapshot.snapshot_id if result.snapshot is not None else "",
            result.snapshot.snapshot_hash if result.snapshot is not None else "",
            result.preview_source,
            len(result.glue_points),
            len(result.motion_preview),
        )

    def _on_dxf_load(self):
        if not self._dxf_filepath:
            self.statusBar().showMessage("未选择DXF文件")
            return
        self._cancel_active_preview_worker(reason="dxf_reload")
        if self._is_offline_mode():
            self._dxf_artifact_id = "offline-local"
            self._current_job_id = ""
            self._dxf_loaded = True
            self._preview_session.reset_for_loaded_dxf()
            self._sync_preview_session_fields()
            filename = os.path.basename(self._dxf_filepath)
            self._dxf_filename_display.setText(filename)
            self._dxf_filename_display.setToolTip(self._dxf_filepath)
            self._update_info_label()
            self.statusBar().showMessage("DXF已加载，正在生成离线预览")
            self._update_preview_refresh_button_state()
            self._generate_dxf_preview()
            return
        ok, payload, error = self._protocol.dxf_create_artifact(self._dxf_filepath)
        if ok:
            self._dxf_artifact_id = str(payload.get("artifact_id", "")).strip()
            self._current_job_id = ""
            self._dxf_loaded = True
            self._preview_session.reset_for_loaded_dxf(
                segment_count=int(payload.get("segment_count", 0) or 0),
            )
            self._sync_preview_session_fields()
            self._dxf_segments_label = None # Deprecated, clear ref if any
            
            # Update filename display (Basename only)
            filename = os.path.basename(self._dxf_filepath)
            self._dxf_filename_display.setText(filename)
            self._dxf_filename_display.setToolTip(self._dxf_filepath)
            
            # Fetch and display DXF info
            self._update_dxf_info()
            self.statusBar().showMessage("DXF工件已上传，正在生成预览")
            self._update_preview_refresh_button_state()
            # Auto generate preview
            self._generate_dxf_preview()
        else:
            self._dxf_loaded = False
            self._dxf_artifact_id = ""
            self._current_job_id = ""
            self._preview_session.reset_for_loaded_dxf()
            self._sync_preview_session_fields()
            self._update_info_label()
            self._update_preview_refresh_button_state()
            self._set_preview_message_html("胶点预览不可用", "DXF加载失败，无法生成胶点主预览。", is_error=True)
            self.statusBar().showMessage(f"DXF加载失败: {error or 'Unknown error'}")

    def _generate_dxf_preview(self):
        """Generate and display DXF preview in embedded view."""
        if not self._is_offline_mode() and not self._require_online_mode("DXF预览"):
            return
        if self._preview_refresh_inflight:
            self.statusBar().showMessage("胶点预览仍在生成中，请稍后")
            return
        if not self._dxf_loaded:
            self.statusBar().showMessage("请先加载DXF文件后再刷新预览")
            self._set_preview_message_html("尚未加载DXF", "请先选择并上传DXF文件。")
            return
        if not self._dxf_artifact_id:
            result = self._preview_session.handle_local_failure(
                gate_error_message="DXF工件未创建",
                title="胶点预览生成失败",
                detail="DXF工件未创建。",
            )
            self._sync_preview_session_fields()
            self._update_info_label()
            self.statusBar().showMessage(result.status_message)
            self._set_preview_message_html(result.title, result.detail, is_error=result.is_error)
            return
        if not WEB_ENGINE_AVAILABLE or not self._dxf_view:
            result = self._preview_session.handle_local_failure(
                gate_error_message="预览组件不可用",
                title="胶点预览生成失败",
                detail="预览组件不可用。",
            )
            self._sync_preview_session_fields()
            self._update_info_label()
            self.statusBar().showMessage(result.status_message)
            self._set_preview_message_html(result.title, result.detail, is_error=result.is_error)
            return

        speed = self._dxf_speed.value()
        dry_run_mode = self._mode_dryrun.isChecked() if hasattr(self, "_mode_dryrun") else False
        self._preview_session.begin_preview_generation()
        self._sync_preview_session_fields()
        self._update_info_label()
        self._set_preview_refresh_inflight(True)
        self._cleanup_temp_preview()
        if self._is_offline_mode():
            self._set_preview_message_html("胶点预览生成中", "正在生成离线动态预览，请稍候。")
        else:
            self._set_preview_message_html("胶点预览生成中", "正在请求规划胶点快照和执行轨迹辅助层，请稍候。")
        self.statusBar().showMessage("胶点预览生成中...")
        _UI_LOGGER.info("preview_requested speed_mm_s=%.3f file=%s", speed, self._dxf_filepath)
        if self._is_offline_mode():
            try:
                payload = build_offline_preview_payload(
                    self._dxf_filepath,
                    speed_mm_s=speed,
                    dry_run=bool(dry_run_mode),
                )
            except Exception as exc:
                self._set_preview_refresh_inflight(False)
                result = self._preview_session.handle_worker_error(str(exc))
                self._sync_preview_session_fields()
                self._update_info_label()
                self.statusBar().showMessage(result.status_message)
                self._set_preview_message_html(result.title, result.detail, is_error=result.is_error)
                return
            self._on_preview_snapshot_completed(True, payload, "", source="offline_local")
            return
        self._cancel_active_preview_worker(reason="preview_restart", keep_refresh_inflight=True)
        self._preview_request_generation += 1
        request_token = self._preview_request_generation

        worker = PreviewSnapshotWorker(
            host=self._client.host,
            port=self._client.port,
            artifact_id=self._dxf_artifact_id,
            speed_mm_s=speed,
            dry_run=dry_run_mode,
            dry_run_speed_mm_s=speed,
        )
        worker.completed.connect(
            lambda ok, payload, error, token=request_token: self._on_preview_snapshot_completed(
                ok,
                payload,
                error,
                request_token=token,
            )
        )
        worker.finished.connect(worker.deleteLater)
        self._preview_snapshot_worker = worker
        try:
            worker.start()
        except Exception as exc:
            self._preview_snapshot_worker = None
            self._set_preview_refresh_inflight(False)
            result = self._preview_session.handle_worker_error(str(exc))
            self._sync_preview_session_fields()
            self._update_info_label()
            self.statusBar().showMessage(result.status_message)
            self._set_preview_message_html(result.title, result.detail, is_error=result.is_error)

    def _cleanup_temp_preview(self):
        try:
            if self._temp_preview_file and os.path.exists(self._temp_preview_file):
                os.remove(self._temp_preview_file)
        except Exception:
            pass
        self._temp_preview_file = None

    def _update_start_button_state(self):
        """Update start button appearance based on mode."""
        if self._mode_production.isChecked():
            self._prod_start_btn.setText("启动生产")
            self._prod_start_btn.setProperty("role", "production-start")
        else:
            self._prod_start_btn.setText("启动空跑")
            self._prod_start_btn.setProperty("role", "production-dryrun")
        
        # Refresh style
        self._prod_start_btn.style().unpolish(self._prod_start_btn)
        self._prod_start_btn.style().polish(self._prod_start_btn)

    def _show_preflight_warning(self, message: str):
        self.statusBar().showMessage(message)
        QMessageBox.warning(self, "无法启动", message)

    def _check_production_preconditions(self, dry_run: bool = False) -> bool:
        if not self._require_online_mode("生产控制"):
            return False
        status = self._protocol.get_status()
        self._last_status = status
        self._last_status_ts = time.monotonic()
        self._runtime_status_fault = False
        decision = self._preview_session.build_preflight_decision(
            online_ready=self._is_online_ready(),
            connected=self._is_launch_connected(),
            hardware_connected=self._is_launch_hardware_ready(),
            runtime_status_fault=self._runtime_status_fault,
            status=status,
            dry_run=dry_run,
        )
        self._sync_preview_session_fields()
        if decision.requires_confirmation:
            if not self._confirm_preview_gate():
                self._show_preflight_warning(decision.message)
                return False
            decision = self._preview_session.build_preflight_decision(
                online_ready=self._is_online_ready(),
                connected=self._is_launch_connected(),
                hardware_connected=self._is_launch_hardware_ready(),
                runtime_status_fault=self._runtime_status_fault,
                status=status,
                dry_run=dry_run,
            )
            self._sync_preview_session_fields()
        if decision.allowed:
            return True
        self._show_preflight_warning(decision.message)
        if self._preview_source:
            _UI_LOGGER.warning("start_blocked_by_preview_gate reason=%s source=%s", decision.reason.value, self._preview_source)
        return False

    def _on_production_start_clicked(self):
        """Handle start button click."""
        is_dry_run = self._mode_dryrun.isChecked()
        self._start_production_process(dry_run=is_dry_run)

    def _on_dxf_stop(self):
        if not self._require_online_mode("DXF控制"):
            return
        if not self._current_job_id:
            self.statusBar().showMessage("当前没有运行中的DXF任务")
            return
        ok = self._protocol.dxf_job_stop(self._current_job_id)
        if ok:
            self._production_paused = False
            self._operation_status.setText("停止中")
            self.statusBar().showMessage("DXF停止请求已发送，等待后端完成")
        else:
            self.statusBar().showMessage("DXF停止失败")

    def _on_alarm_clear(self):
        if not self._require_online_mode("报警控制"):
            return
        if self._protocol.clear_alarms():
            self._alarm_list.clear()
            self.statusBar().showMessage("报警已清除")
        else:
            self.statusBar().showMessage("清除报警失败")

    def _on_alarm_ack(self):
        if not self._require_online_mode("报警控制"):
            return
        current = self._alarm_list.currentItem()
        if current:
            alarm_id = current.data(Qt.UserRole)
            if alarm_id and self._protocol.acknowledge_alarm(alarm_id):
                self.statusBar().showMessage(f"报警 {alarm_id} 已确认")
            else:
                self.statusBar().showMessage("确认报警失败")
        else:
            self.statusBar().showMessage("未选择报警")



    # Maintenance Methods
    def _on_reset_needle(self):
        """Reset needle usage counter."""
        self._needle_count = 0
        self._update_maintenance_display()
        self.statusBar().showMessage("针头计数已重置")

    def _increment_needle_count(self, amount: int = 1):
        """Increment needle count after each dispense cycle."""
        self._needle_count += max(0, amount)
        self._update_maintenance_display()

    def _update_maintenance_display(self):
        """Update maintenance indicators."""
        self._needle_count_label.setText(f"{self._needle_count}/{self._maintenance_threshold}")

        # Check for maintenance alerts
        alerts = []
        if self._needle_count >= self._maintenance_threshold * 0.9:
            alerts.append(f"针头使用 {self._needle_count}/{self._maintenance_threshold}")

        hours_since_purge = (time.time() - self._last_purge_time) / 3600
        if hours_since_purge >= 2:
            alerts.append(f"距上次清洗 {hours_since_purge:.1f}h")

        if alerts:
            self._maintenance_alert.setText("[!] " + " | ".join(alerts))
        else:
            self._maintenance_alert.setText("")

    # Production Control Methods
    def _start_production_process(self, dry_run: bool = False):
        """Start production cycle (or dry run)."""
        if not self._require_online_mode("生产控制"):
            return
        self._auth.record_activity()
        if not self._dxf_loaded:
            self.statusBar().showMessage("请先加载DXF文件")
            return
        if not self._check_production_preconditions(dry_run=dry_run):
            return

        ok, payload, error = self._protocol.dxf_start_job(
            self._current_plan_id,
            target_count=self._target_count,
            plan_fingerprint=self._current_plan_fingerprint,
        )
        if not ok:
            self._production_running = False
            self._production_paused = False
            self.statusBar().showMessage(f"启动失败: {error}")
            return

        self._current_job_id = str(payload.get("job_id", "")).strip()
        self._production_running = True
        self._production_paused = False
        self._production_dry_run = dry_run
        self._completed_count = 0
        self._last_completed_count_seen = 0
        self._cycle_times = []
        self._total_run_time = 0
        self._run_start_time = time.time()
        self._last_cycle_start = time.time()

        mode_text = "空跑" if dry_run else "生产"
        self._operation_status.setText(f"{mode_text}运行中")
        self._update_production_stats()
        self.statusBar().showMessage(f"{mode_text}已启动")
        performance_profile = payload.get("performance_profile", {})
        if isinstance(performance_profile, dict):
            _UI_LOGGER.info(
                "job_start_performance_profile plan_id=%s plan_fingerprint=%s job_id=%s "
                "execution_cache_hit=%s execution_joined_inflight=%s execution_wait_ms=%s "
                "motion_plan_ms=%s assembly_ms=%s export_ms=%s execution_total_ms=%s",
                self._current_plan_id,
                self._current_plan_fingerprint,
                self._current_job_id,
                performance_profile.get("execution_cache_hit", ""),
                performance_profile.get("execution_joined_inflight", ""),
                performance_profile.get("execution_wait_ms", ""),
                performance_profile.get("motion_plan_ms", ""),
                performance_profile.get("assembly_ms", ""),
                performance_profile.get("export_ms", ""),
                performance_profile.get("execution_total_ms", ""),
            )
        _UI_LOGGER.info(
            "job_started plan_id=%s plan_fingerprint=%s job_id=%s target=%s dry_run=%s",
            self._current_plan_id,
            self._current_plan_fingerprint,
            self._current_job_id,
            self._target_count,
            dry_run,
        )

    def _on_production_pause(self):
        """Pause production."""
        if not self._require_online_mode("生产控制"):
            return
        if not self._production_running or not self._current_job_id:
            self.statusBar().showMessage("当前没有运行中的生产任务")
            return
        if self._protocol.dxf_job_pause(self._current_job_id):
            if self._run_start_time > 0:
                self._total_run_time += time.time() - self._run_start_time
            self._production_paused = True
            self._production_running = False
            self._run_start_time = 0
            self._operation_status.setText("已暂停")
            self.statusBar().showMessage("生产已暂停")
        else:
            self.statusBar().showMessage("暂停失败")

    def _on_production_resume(self):
        """Resume production after a pause."""
        if not self._require_online_mode("生产控制"):
            return
        if not self._production_paused or not self._current_job_id:
            self.statusBar().showMessage("当前没有已暂停的生产任务")
            return

        if self._protocol.dxf_job_resume(self._current_job_id):
            self._production_running = True
            self._production_paused = False
            self._run_start_time = time.time()
            mode_text = "空跑" if self._production_dry_run else "生产"
            self._operation_status.setText(f"{mode_text}运行中")
            self.statusBar().showMessage("生产已恢复")
        else:
            self.statusBar().showMessage("恢复失败")

    def _on_production_stop(self):
        """Stop production."""
        if not self._require_online_mode("生产控制"):
            return
        if not self._current_job_id:
            self.statusBar().showMessage("当前没有运行中的生产任务")
            return
        if self._production_running and self._run_start_time > 0:
            self._total_run_time += time.time() - self._run_start_time
        self._run_start_time = 0
        if self._protocol.dxf_job_stop(self._current_job_id):
            self._production_paused = False
            self._operation_status.setText("停止中")
            self.statusBar().showMessage("停止请求已发送，等待后端完成")
        else:
            self.statusBar().showMessage("停止失败")

    def _on_target_changed(self, value):
        """Update target count."""
        self._target_count = value
        self._target_label.setText(str(value))
        self._update_production_stats()

    def _on_reset_counter(self):
        """Reset production counter."""
        self._completed_count = 0
        self._last_completed_count_seen = 0
        self._cycle_times = []
        self._total_run_time = 0
        self._update_production_stats()
        self.statusBar().showMessage("计数已重置")

    def _record_cycle_complete(self):
        """Record completion of one production cycle."""
        if self._last_cycle_start > 0:
            cycle_time = time.time() - self._last_cycle_start
            self._cycle_times.append(cycle_time)
            if len(self._cycle_times) > 100:
                self._cycle_times = self._cycle_times[-100:]
        self._completed_count += 1
        self._increment_needle_count()
        self._last_cycle_start = time.time()
        self._update_production_stats()

    def _update_production_stats(self):
        """Update production statistics display."""
        # Update combined label "Completed / Target"
        self._completed_label.setText(f"{self._completed_count} / {self._target_count}")

        # Completion rate
        if self._target_count > 0:
            rate = min(100, int(self._completed_count / self._target_count * 100))
            self._completion_progress.setValue(rate)
            # self._global_progress.setValue(rate) # Conflicted with DXF progress

        # Cycle time and UPH
        avg_cycle = 0
        if self._cycle_times:
            avg_cycle = sum(self._cycle_times) / len(self._cycle_times)
            # Cycle time label removed from UI, keeping calculation for UPH
            uph = int(3600 / avg_cycle) if avg_cycle > 0 else 0
            self._uph_label.setText(str(uph))

        # Remaining Time
        remaining_count = max(0, self._target_count - self._completed_count)
        if avg_cycle > 0:
            rem_seconds = remaining_count * avg_cycle
            rem_h = int(rem_seconds // 3600)
            rem_m = int((rem_seconds % 3600) // 60)
            self._remaining_time_label.setText(f"{rem_h}h {rem_m}m")
        else:
            self._remaining_time_label.setText("-")

        # Run time
        total_time = self._total_run_time
        if self._production_running:
            total_time += time.time() - self._run_start_time
        hours = int(total_time // 3600)
        minutes = int((total_time % 3600) // 60)
        seconds = int(total_time % 60)
        self._runtime_label.setText(f"{hours}:{minutes:02d}:{seconds:02d}")

    def _update_status(self):
        self._tick_preview_playback(time.monotonic())

        # Check auto-logout
        if not self._auth.check_activity():
            self._user_label.setText("操作员")
            self._apply_permissions()

        self._update_maintenance_display()

        if not self._is_online_ready():
            return
        try:
            status = self._protocol.get_status()
            if not status.connected:
                self._preview_session.mark_resync_pending()
                self._sync_preview_session_fields()
                degradation = self._detect_runtime_degradation(
                    tcp_connected=True,
                    hardware_ready=False,
                    error_message="运行中硬件状态不可用，在线能力已收敛。",
                )
                if degradation is not None:
                    self._apply_runtime_degradation_result(degradation)
                return
            self._last_status = status
            self._last_status_ts = time.monotonic()
            self._runtime_status_fault = False
            self._last_status_error_notice_ts = 0.0
            self._state_label.setText(status.runtime_state)

            # Update Dispenser Status
            is_dispensing = status.dispenser_valve_open
            self._dispenser_label.setText("开" if is_dispensing else "关")
            self._update_role(self._dispenser_label, "valve-on" if is_dispensing else "valve-off")

            # Update Supply Status
            is_supply_open = status.supply_valve_open
            self._supply_label.setText("开" if is_supply_open else "关")
            self._update_role(self._supply_label, "valve-on" if is_supply_open else "valve-off")

            # Update Axis Status
            for axis, data in status.axes.items():
                if axis in self._axis_labels:
                    self._axis_labels[axis].setText(f"{data.position:.3f}")
                if axis in self._axis_velocity_labels:
                    self._axis_velocity_labels[axis].setText(f"{data.velocity:.1f}")
                if axis in self._axis_enabled_labels:
                    self._update_led(self._axis_enabled_labels[axis], "green" if data.enabled else "off")
                if axis in self._axis_homed_labels:
                    self._update_led(self._axis_homed_labels[axis], "green" if data.homed else "off")

            # Update IO Status
            io = status.io
            io_map = {
                "io-limit-x-pos": io.limit_x_pos,
                "io-limit-x-neg": io.limit_x_neg,
                "io-limit-y-pos": io.limit_y_pos,
                "io-limit-y-neg": io.limit_y_neg,
                "io-estop": status.gate_estop_active(),
                "io-door": status.gate_door_active(),
            }
            for testid, active in io_map.items():
                if testid in self._io_leds:
                    if testid == "io-estop":
                        state = "red" if active else "off"
                    else:
                        state = "yellow" if active else "off"
                    self._update_led(self._io_leds[testid], state)

            # Update DXF Progress
            if self._dxf_loaded:
                backend_active_job_id = str(getattr(status, "active_job_id", "")).strip()
                backend_active_job_state = str(getattr(status, "active_job_state", "")).strip().lower()
                if backend_active_job_id:
                    self._current_job_id = backend_active_job_id
                elif backend_active_job_state in ("", "completed", "failed", "cancelled"):
                    self._current_job_id = ""
                dxf_progress = (
                    self._protocol.dxf_get_job_status(self._current_job_id)
                    if self._current_job_id
                    else {
                        "state": "idle",
                        "overall_progress_percent": int(
                            (self._completed_count / max(1, self._target_count)) * 100
                        ) if self._target_count > 0 else 0,
                        "completed_count": self._completed_count,
                    }
                )
                current = dxf_progress.get("current_segment", 0)
                total = dxf_progress.get("total_segments", 0)
                progress = dxf_progress.get("overall_progress_percent", dxf_progress.get("progress", 0))
                state = dxf_progress.get("state", "")
                error_message = dxf_progress.get("error_message", "")
                if backend_active_job_state and state in ("", "idle"):
                    state = backend_active_job_state
                completed_count = int(dxf_progress.get("completed_count", self._completed_count) or 0)
                if hasattr(self, '_global_progress'):
                    self._global_progress.setValue(int(progress))
                if completed_count > self._last_completed_count_seen:
                    for _ in range(completed_count - self._last_completed_count_seen):
                        self._record_cycle_complete()
                    self._last_completed_count_seen = completed_count
                else:
                    self._completed_count = completed_count

                if state == "paused":
                    self._production_paused = True
                    self._production_running = False
                    self._operation_status.setText("已暂停")
                elif state in ("running", "pending"):
                    self._production_paused = False
                    self._production_running = True
                    if self._run_start_time <= 0:
                        self._run_start_time = time.time()
                    mode_text = "空跑" if self._production_dry_run else "生产"
                    self._operation_status.setText(f"{mode_text}运行中")
                elif state == "stopping":
                    self._production_paused = False
                    self._production_running = False
                    self._operation_status.setText("停止中")
                elif state == "completed":
                    self._production_running = False
                    self._production_paused = False
                    self._current_job_id = ""
                    self._preview_session.mark_resync_pending()
                    self._sync_preview_session_fields()
                    self._operation_status.setText("完成")
                    self.statusBar().showMessage("生产目标已达成")
                elif state == "unknown":
                    self._production_running = False
                    self._production_paused = False
                    self._operation_status.setText("状态未知")
                    self._runtime_status_fault = True
                    if error_message:
                        self.statusBar().showMessage(f"执行状态未知: {error_message}")
                    else:
                        self.statusBar().showMessage("执行状态未知，请检查后端链路")
                elif state in ("failed", "cancelled"):
                    self._production_running = False
                    self._production_paused = False
                    self._current_job_id = ""
                    self._preview_session.mark_resync_pending()
                    self._sync_preview_session_fields()
                    status_text = "失败" if state == "failed" else "已取消"
                    self._operation_status.setText(status_text)
                    if error_message:
                        self.statusBar().showMessage(f"执行失败: {error_message}")
                    else:
                        self.statusBar().showMessage(f"执行{status_text}")

            # Update production stats periodically
            if self._production_running:
                self._update_production_stats()
            else:
                self._sync_preview_state_from_runtime()

            # Update Alarms (only when changed)
            alarms = self._protocol.get_alarms()
            alarms_snapshot = [
                (alarm.get("id"), alarm.get("level"), alarm.get("message"))
                for alarm in alarms
            ]
            if alarms_snapshot != self._last_alarms_snapshot:
                self._alarm_list.clear()
                for alarm in alarms:
                    level = alarm.get("level", "INFO")
                    message = alarm.get("message", "")
                    item = QListWidgetItem(f"[{level}] {message}")
                    item.setData(Qt.UserRole, alarm.get("id"))
                    self._alarm_list.addItem(item)
                self._last_alarms_snapshot = alarms_snapshot
        except Exception as exc:
            _UI_LOGGER.exception("status_update_failed: %s", exc)
            self._preview_session.mark_resync_pending()
            self._sync_preview_session_fields()
            degradation = self._detect_runtime_degradation(
                tcp_connected=False,
                error_message=f"运行中 TCP 连接异常: {exc}",
            )
            if degradation is not None:
                self._apply_runtime_degradation_result(degradation)
            self._runtime_status_fault = True
            self._last_status = None
            self._last_status_ts = 0.0
            self._operation_status.setText("状态异常")
            self._state_label.setText("Status Error")
            now = time.monotonic()
            if now - self._last_status_error_notice_ts >= 1.5:
                self.statusBar().showMessage(f"运行状态更新异常: {exc}")
                self._last_status_error_notice_ts = now

    def closeEvent(self, event):
        """Handle application close event."""
        if self._production_running:
            reply = QMessageBox.question(
                self, "确认退出", "生产正在进行中，确定要退出吗？",
                QMessageBox.Yes | QMessageBox.No, QMessageBox.No
            )
            if reply != QMessageBox.Yes:
                event.ignore()
                return

        self._cleanup_temp_preview()
        home_worker = self._home_auto_worker
        if home_worker is not None:
            self._cancel_active_home_worker(reason="window_close")
            if home_worker.isRunning():
                home_worker.wait(1000)
        if self._preview_snapshot_worker and self._preview_snapshot_worker.isRunning():
            self._preview_snapshot_worker.wait(1000)
        if self._backend:
            self._backend.stop()
        super().closeEvent(event)

    # User Permission Methods
    def _on_switch_user(self):
        """Show login dialog to switch user."""
        dialog = LoginDialog(self)
        if dialog.exec_() == QDialog.Accepted:
            ok, msg = self._auth.login(dialog.username, dialog.pin)
            if ok:
                user = self._auth.current_user
                self._user_label.setText(f"{user.role}")
                self._apply_permissions()
                self.statusBar().showMessage(msg)
            else:
                self.statusBar().showMessage(msg)

    def _apply_permissions(self):
        """Apply permission restrictions based on user level."""
        level = self._auth.current_user.level if self._auth.current_user else 1

        # Setup tab controls (level 2+)
        # Note: Valve buttons (dispenser & supply) and Purge button are always enabled (level 1)
        setup_widgets = [
            self._speed_slider, self._move_btn,
        ]
        for w in setup_widgets:
            w.setEnabled(level >= 2)

        # Recipe save (level 2+)
        recipe_edit_widgets = [
            '_recipe_create_btn',
            '_recipe_update_btn',
            '_recipe_draft_btn',
            '_recipe_save_btn',
            '_recipe_publish_btn',
            '_recipe_version_create_btn',
            '_recipe_activate_btn',
            '_recipe_export_btn',
            '_recipe_import_btn',
        ]
        for name in recipe_edit_widgets:
            if hasattr(self, name):
                getattr(self, name).setEnabled(level >= 2)

        # Reset needle (level 2+)
        if hasattr(self, '_reset_needle_btn'):
            self._reset_needle_btn.setEnabled(level >= 2)

        # Hardware init (level 3)
        if hasattr(self, '_hw_connect_btn'):
            self._hw_connect_btn.setEnabled(level >= 3 and self._is_online_ready())

    def _check_permission(self, required_level: int) -> bool:
        """Check if current user has required permission level."""
        level = self._auth.current_user.level if self._auth.current_user else 1
        if level >= required_level:
            return True
        self.statusBar().showMessage(f"权限不足 (需要等级 {required_level})")
        return False


def main(launch_mode: str = "online") -> int:
    app = QApplication(sys.argv)
    window = MainWindow(launch_mode=launch_mode)
    window.show()
    return app.exec_()
