"""PyQt5 Main Window for Siligen HMI."""
import os
import sys
import time
import logging
import html
from pathlib import Path

try:
    from hmi_client.qt_env import configure_qt_environment
except ImportError:  # pragma: no cover - script-mode fallback
    from qt_env import configure_qt_environment

configure_qt_environment(headless=False)

from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QLabel, QPushButton, QLineEdit, QGridLayout, QFrame,
    QSlider, QSpinBox, QDoubleSpinBox, QCheckBox, QTabWidget, QFileDialog,
    QProgressBar, QListWidget, QListWidgetItem, QMessageBox, QDialog, QDialogButtonBox,
    QComboBox, QFormLayout, QScrollArea, QRadioButton, QStatusBar
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
    CommandProtocol,
    LaunchResult,
    RecoveryWorker,
    SessionStageEvent,
    SupervisorSession,
    StartupWorker,
    TcpClient,
    is_online_ready,
    launch_result_from_snapshot,
    load_supervisor_policy_from_env,
    normalize_launch_mode,
)
from client.auth import AuthManager
try:
    from hmi_client.features.dispense_preview_gate import (
        DispensePreviewGate,
        PreviewGateState,
        PreviewSnapshotMeta,
        StartBlockReason,
    )
except ImportError:
    from features.dispense_preview_gate import (  # type: ignore
        DispensePreviewGate,
        PreviewGateState,
        PreviewSnapshotMeta,
        StartBlockReason,
    )
from .dxf_default_paths import build_default_dxf_candidates
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

    def run(self):
        client = TcpClient(host=self._host, port=self._port)
        ok = False
        payload = {}
        error = ""
        try:
            if not client.connect():
                error = "无法连接后端，请检查TCP链路"
            else:
                protocol = CommandProtocol(client)
                plan_ok, plan_payload, plan_error = protocol.dxf_prepare_plan(
                    artifact_id=self._artifact_id,
                    speed_mm_s=self._speed_mm_s,
                    dry_run=self._dry_run,
                    dry_run_speed_mm_s=self._dry_run_speed_mm_s,
                )
                if not plan_ok:
                    ok = False
                    payload = {}
                    error = plan_error
                else:
                    plan_id = str(plan_payload.get("plan_id", "")).strip()
                    if not plan_id:
                        ok = False
                        payload = {}
                        error = "plan.prepare 返回缺少 plan_id"
                    else:
                        ok, payload, error = protocol.dxf_preview_snapshot(plan_id=plan_id, max_polyline_points=4000)
                        if ok and isinstance(payload, dict):
                            payload.setdefault("plan_id", plan_id)
                            payload.setdefault("plan_fingerprint", str(plan_payload.get("plan_fingerprint", "")))
                            payload.setdefault("snapshot_hash", str(plan_payload.get("plan_fingerprint", "")))
                            payload.setdefault("dry_run", bool(self._dry_run))
        except Exception as exc:
            error = str(exc) or "预览快照生成异常"
        finally:
            client.disconnect()
        self.completed.emit(ok, payload if isinstance(payload, dict) else {}, error)


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
        self._preview_gate = DispensePreviewGate()
        self._preview_plan_dry_run = None
        self._preview_snapshot_worker = None
        self._preview_refresh_inflight = False
        self._preview_state_resync_pending = False
        self._last_preview_resync_attempt_ts = 0.0
        self._runtime_status_fault = False
        self._last_status_error_notice_ts = 0.0
        self._last_status = None
        self._last_status_ts = 0.0
        self._connected = False
        self._hw_connected = False

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

        # User permission management
        self._auth = AuthManager(auto_logout_minutes=5)

        self._setup_ui()
        self._setup_timer()
        self._refresh_launch_status_ui()
        self._apply_mode_capabilities()
        if self._requested_launch_mode == "online":
            self._auto_startup()
        else:
            self._apply_launch_result(
                LaunchResult(
                    requested_mode="offline",
                    effective_mode="offline",
                    phase="offline",
                    success=True,
                    backend_started=False,
                    tcp_connected=False,
                    hardware_ready=False,
                    user_message="Offline 模式已启用，本次启动不会尝试连接 gateway。",
                )
            )
        self._update_home_controls_state()
        self._update_preview_refresh_button_state()
        self._set_preview_message_html("轨迹预览待生成", "请先加载DXF文件，再点击“刷新预览”。")

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
        self.statusBar().showMessage("系统就绪")

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
        main_layout.setSpacing(0)
        main_layout.setContentsMargins(0, 0, 0, 0)

        # === Left Sidebar (Control Panel) ===
        sidebar = QWidget()
        sidebar.setFixedWidth(400)
        # Background color is now handled by CSS 'QWidget[data-testid="tab-production"] > QWidget'
        # but we keep border here or move to CSS. Let's rely on CSS for the look, 
        # but set specific object name or class if needed. 
        # Ideally, we just apply the class to the Frames inside.
        
        sidebar_layout = QVBoxLayout(sidebar)
        sidebar_layout.setSpacing(15) # Reduced spacing as cards have padding
        sidebar_layout.setContentsMargins(15, 20, 15, 20)

        # 1. Job Preparation (File Selection)
        job_frame = QFrame()
        job_frame.setProperty("class", "sidebar-card")
        job_layout = QVBoxLayout(job_frame)
        job_layout.setContentsMargins(15, 15, 15, 15)
        job_layout.setSpacing(10)
        
        # Header
        job_title = QLabel("作业准备")
        job_title.setProperty("class", "header-text")
        job_layout.addWidget(job_title)

        # File display & Browse
        file_row = QHBoxLayout()
        self._dxf_filename_display = QLineEdit()
        self._dxf_filename_display.setReadOnly(True)
        self._dxf_filename_display.setPlaceholderText("未加载DXF文件")
        self._dxf_filename_display.setProperty("data-testid", "input-current-recipe")
        # Inline style removed, using QSS
        file_row.addWidget(self._dxf_filename_display)
        
        browse_btn = QPushButton("浏览...")
        browse_btn.setProperty("data-testid", "btn-dxf-browse")
        browse_btn.setFixedWidth(80)
        browse_btn.clicked.connect(self._on_dxf_browse)
        file_row.addWidget(browse_btn)
        job_layout.addLayout(file_row)

        # Home All Button (Added per user request)
        self._prod_home_btn = QPushButton("全部回零")
        self._prod_home_btn.setProperty("data-testid", "btn-production-home-all")
        self._prod_home_btn.setCursor(Qt.PointingHandCursor)
        self._prod_home_btn.clicked.connect(lambda: self._on_home(None))
        job_layout.addWidget(self._prod_home_btn)

        # File Info (Compact)
        self._dxf_info_label = QLabel("段数: - | 长度: - | 预估: - | 预览: 待生成")
        self._dxf_info_label.setProperty("class", "sub-text")
        job_layout.addWidget(self._dxf_info_label)

        self._refresh_preview_btn = QPushButton("刷新预览")
        self._refresh_preview_btn.setProperty("data-testid", "btn-dxf-preview-refresh")
        self._refresh_preview_btn.setCursor(Qt.PointingHandCursor)
        self._refresh_preview_btn.clicked.connect(self._generate_dxf_preview)
        job_layout.addWidget(self._refresh_preview_btn)
        
        sidebar_layout.addWidget(job_frame)

        # 2. Parameters (Target, Speed, Mode)
        param_frame = QFrame()
        param_frame.setProperty("class", "sidebar-card")
        param_layout = QGridLayout(param_frame)
        param_layout.setContentsMargins(15, 15, 15, 15)
        param_layout.setVerticalSpacing(15)
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
        self._target_input.setFixedHeight(30)
        self._target_input.valueChanged.connect(self._on_target_changed)
        param_layout.addWidget(self._target_input, 1, 1)

        # Speed Control (Slider + SpinBox)
        param_layout.addWidget(QLabel("运行速度:"), 2, 0)
        
        speed_control_layout = QHBoxLayout()
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
        self._dxf_speed.setFixedWidth(90)
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

        # Reset Counter (Moved here from removed Monitor Frame)
        self._reset_counter_btn = QPushButton("重置计数")
        self._reset_counter_btn.setProperty("data-testid", "btn-reset-counter")
        self._reset_counter_btn.setFlat(True)
        self._reset_counter_btn.setStyleSheet("color: #888; border: none; font-size: 11px; text-align: left;")
        self._reset_counter_btn.setCursor(Qt.PointingHandCursor)
        self._reset_counter_btn.clicked.connect(self._on_reset_counter)
        param_layout.addWidget(self._reset_counter_btn, 4, 1, alignment=Qt.AlignRight)
        
        sidebar_layout.addWidget(param_frame)

        # 3. Core Control (Big Buttons)
        control_frame = QFrame()
        control_frame.setProperty("class", "sidebar-card")
        control_layout = QVBoxLayout(control_frame)
        control_layout.setContentsMargins(15, 15, 15, 15)
        
        self._prod_start_btn = QPushButton("启动生产")
        self._prod_start_btn.setProperty("data-testid", "btn-production-start")
        self._prod_start_btn.setMinimumHeight(60)
        # Inline font style removed, handled by CSS
        self._prod_start_btn.setProperty("role", "production-start")
        self._prod_start_btn.setCursor(Qt.PointingHandCursor)
        self._prod_start_btn.clicked.connect(self._on_production_start_clicked)
        control_layout.addWidget(self._prod_start_btn)

        # Pause / Stop Row
        sub_control_layout = QHBoxLayout()
        sub_control_layout.setSpacing(15)
        
        self._prod_pause_btn = QPushButton("暂停")
        self._prod_pause_btn.setProperty("data-testid", "btn-production-pause")
        self._prod_pause_btn.setMinimumHeight(40)
        self._prod_pause_btn.setCursor(Qt.PointingHandCursor)
        self._prod_pause_btn.clicked.connect(self._on_production_pause)

        self._prod_resume_btn = QPushButton("恢复")
        self._prod_resume_btn.setProperty("data-testid", "btn-production-resume")
        self._prod_resume_btn.setMinimumHeight(40)
        self._prod_resume_btn.setCursor(Qt.PointingHandCursor)
        self._prod_resume_btn.clicked.connect(self._on_production_resume)
        
        self._prod_stop_btn = QPushButton("停止")
        self._prod_stop_btn.setProperty("data-testid", "btn-production-stop")
        self._prod_stop_btn.setMinimumHeight(40)
        self._prod_stop_btn.setProperty("role", "danger")
        self._prod_stop_btn.setCursor(Qt.PointingHandCursor)
        self._prod_stop_btn.clicked.connect(self._on_production_stop)
        
        sub_control_layout.addWidget(self._prod_pause_btn)
        sub_control_layout.addWidget(self._prod_resume_btn)
        sub_control_layout.addWidget(self._prod_stop_btn)
        control_layout.addLayout(sub_control_layout)
        
        sidebar_layout.addWidget(control_frame)

        # Push subsequent widgets to bottom
        sidebar_layout.addStretch()

        # Monitor Frame (Restored to Sidebar Bottom)
        monitor_frame = QFrame()
        monitor_frame.setProperty("class", "sidebar-card")
        monitor_layout = QVBoxLayout(monitor_frame)
        monitor_layout.setContentsMargins(15, 15, 15, 15)
        
        monitor_title = QLabel("当前进度")
        monitor_title.setProperty("class", "header-text")
        monitor_layout.addWidget(monitor_title)

        status_row = QHBoxLayout()
        status_row.addWidget(QLabel("完成计数:"))
        
        self._completed_label = QLabel("0 / 0")
        self._completed_label.setProperty("data-testid", "label-completed-count")
        self._completed_label.setStyleSheet("font-weight: bold; color: #4CAF50; font-size: 14px;")
        self._completed_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        status_row.addWidget(self._completed_label)
        
        monitor_layout.addLayout(status_row)
        sidebar_layout.addWidget(monitor_frame)

        # === Right Content (Preview) ===
        preview_container = QWidget()
        preview_layout = QVBoxLayout(preview_container)
        preview_layout.setContentsMargins(0, 0, 0, 0)
        
        if WEB_ENGINE_AVAILABLE:
            self._dxf_view = QWebEngineView()
            self._dxf_view.page().setBackgroundColor(QColor("#1E1E1E"))
            preview_layout.addWidget(self._dxf_view)
        else:
            self._dxf_view = None
            hint = QLabel("预览组件不可用\n请安装 PyQtWebEngine")
            hint.setAlignment(Qt.AlignCenter)
            hint.setStyleSheet("color: gray; font-size: 14px;")
            preview_layout.addWidget(hint)

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
        layout.setSpacing(10)

        self._hw_connect_btn = QPushButton("初始化硬件")
        self._hw_connect_btn.setProperty("data-testid", "btn-hw-init")
        self._hw_connect_btn.setCursor(Qt.PointingHandCursor)
        self._hw_connect_btn.clicked.connect(self._on_hw_connect)
        self._hw_connect_btn.setEnabled(False)
        layout.addWidget(self._hw_connect_btn)

        self._stop_btn = QPushButton("停止")
        self._stop_btn.setProperty("data-testid", "btn-stop")
        self._stop_btn.setProperty("role", "warning")
        self._stop_btn.setCursor(Qt.PointingHandCursor)
        self._stop_btn.setMinimumHeight(40)
        self._stop_btn.clicked.connect(lambda: self._on_stop("manual_stop_button"))
        layout.addWidget(self._stop_btn)

        self._estop_btn = QPushButton("急停")
        self._estop_btn.setProperty("data-testid", "btn-estop")
        self._estop_btn.setProperty("role", "danger")
        self._estop_btn.setCursor(Qt.PointingHandCursor)
        self._estop_btn.setMinimumHeight(50)
        self._estop_btn.clicked.connect(self._on_estop)
        layout.addWidget(self._estop_btn)

        self._estop_reset_btn = QPushButton("急停复位")
        self._estop_reset_btn.setProperty("data-testid", "btn-estop-reset")
        self._estop_reset_btn.setProperty("role", "warning")
        self._estop_reset_btn.setCursor(Qt.PointingHandCursor)
        self._estop_reset_btn.setMinimumHeight(40)
        self._estop_reset_btn.clicked.connect(self._on_estop_reset)
        layout.addWidget(self._estop_reset_btn)

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
        layout.addStretch()

        return widget

    def _current_effective_mode(self) -> str:
        snapshot = self._current_session_snapshot()
        if snapshot is not None:
            return snapshot.mode
        if self._launch_result is not None:
            return self._launch_result.effective_mode
        return self._requested_launch_mode

    def _current_session_snapshot(self):
        if self._launch_result is not None:
            return self._launch_result.session_snapshot
        return self._session_snapshot

    def _is_offline_mode(self) -> bool:
        return self._current_effective_mode() == "offline"

    def _is_online_ready(self) -> bool:
        return is_online_ready(self._current_session_snapshot())

    def _has_online_capability(self) -> bool:
        return not self._is_offline_mode() and self._is_online_ready()

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
        if self._is_offline_mode() or self._is_session_operation_running():
            retry_enabled = False
            restart_enabled = False
            stop_enabled = False
        else:
            snapshot = self._current_session_snapshot()
            failed = snapshot is not None and snapshot.session_state == "failed"
            recoverable = bool(snapshot.recoverable) if failed else False
            ready = snapshot is not None and is_online_ready(snapshot)
            retry_enabled = failed and recoverable
            restart_enabled = failed and recoverable
            stop_enabled = failed or ready
        self._retry_stage_btn.setEnabled(retry_enabled)
        self._restart_session_btn.setEnabled(restart_enabled)
        self._stop_session_btn.setEnabled(stop_enabled)

    @staticmethod
    def _label_for_tcp_state(state: str) -> str:
        if state == "ready":
            return "已连接"
        if state == "connecting":
            return "连接中"
        if state == "failed":
            return "连接失败"
        return "未连接"

    @staticmethod
    def _label_for_hw_state(state: str) -> str:
        if state == "ready":
            return "就绪"
        if state == "probing":
            return "初始化中"
        if state == "failed":
            return "初始化失败"
        if state == "unavailable":
            return "不可用"
        return "未初始化"

    def _refresh_launch_status_ui(self) -> None:
        effective_mode = self._current_effective_mode()
        self._launch_mode_label.setText("Offline" if effective_mode == "offline" else "Online")
        previous_connected = bool(getattr(self, "_connected", False))

        if self._is_offline_mode():
            self._connected = False
            self._hw_connected = False
            if previous_connected and self._current_plan_id:
                self._preview_state_resync_pending = True
            self._operation_status.setText("离线模式")
            self._tcp_state_label.setText("未连接")
            self._hw_state_label.setText("不可用")
            self._state_label.setText("Offline")
            self._update_led(self._tcp_led, "off")
            self._update_led(self._hw_led, "off")
            self._update_recovery_controls_state()
            self._update_preview_refresh_button_state()
            return

        snapshot = self._current_session_snapshot()
        if snapshot is None:
            self._connected = False
            self._hw_connected = False
            if previous_connected and self._current_plan_id:
                self._preview_state_resync_pending = True
            self._update_led(self._tcp_led, "off")
            self._update_led(self._hw_led, "off")
            self._tcp_state_label.setText("未连接")
            self._hw_state_label.setText("未初始化")
            self._operation_status.setText("启动中")
            self._state_label.setText("Starting")
            self._update_recovery_controls_state()
            return

        self._connected = snapshot.tcp_state == "ready"
        self._hw_connected = snapshot.hardware_state == "ready"
        if self._current_plan_id and self._connected != previous_connected:
            self._preview_state_resync_pending = True
        self._tcp_state_label.setText(self._label_for_tcp_state(snapshot.tcp_state))
        self._hw_state_label.setText(self._label_for_hw_state(snapshot.hardware_state))
        self._update_led(
            self._tcp_led,
            "green" if snapshot.tcp_state == "ready" else ("red" if snapshot.tcp_state == "failed" else "off"),
        )
        self._update_led(
            self._hw_led,
            "green" if snapshot.hardware_state == "ready" else ("red" if snapshot.hardware_state == "failed" else "off"),
        )

        if self._is_online_ready():
            self._operation_status.setText("空闲")
            self._state_label.setText("Ready")
            self._update_recovery_controls_state()
            return

        if snapshot.session_state == "starting":
            self._operation_status.setText("启动中")
            self._state_label.setText("Starting")
        elif snapshot.session_state == "failed":
            self._operation_status.setText("启动失败")
            self._state_label.setText("Launch Failed")
        elif snapshot.session_state == "stopping":
            self._operation_status.setText("停止中")
            self._state_label.setText("Stopping")
        else:
            self._operation_status.setText("未就绪")
            self._state_label.setText("Not Ready")
        self._update_recovery_controls_state()
        self._update_preview_refresh_button_state()

    def _apply_mode_capabilities(self) -> None:
        allow_online_actions = self._has_online_capability()
        for widget_name in (
            "_production_tab",
            "_motion_control_panel",
            "_dispenser_control_panel",
            "_recipe_tab",
        ):
            widget = getattr(self, widget_name, None)
            if widget is not None:
                widget.setEnabled(allow_online_actions)
        if hasattr(self, "_system_panel"):
            self._system_panel.setEnabled(not self._is_offline_mode())
        if hasattr(self, "_stop_btn"):
            self._stop_btn.setEnabled(allow_online_actions)
        if hasattr(self, "_hw_connect_btn"):
            self._hw_connect_btn.setEnabled(allow_online_actions and not self._is_session_operation_running())
        if hasattr(self, "_global_estop_btn"):
            self._global_estop_btn.setEnabled(allow_online_actions)
        self._update_recovery_controls_state()

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
        snapshot = self._current_session_snapshot()
        result = self._launch_result
        if snapshot is None or result is None:
            return
        degraded = SupervisorSession.build_runtime_failure_snapshot(
            snapshot,
            failure_code=failure_code,
            failure_stage=failure_stage,
            message=message,
            tcp_state=tcp_state,
            hardware_state=hardware_state,
            recoverable=True,
        )
        session_id = self._current_supervisor_session_id()
        event = SessionStageEvent(
            event_type="stage_failed",
            session_id=session_id,
            stage=degraded.failure_stage,
            timestamp=degraded.updated_at,
            failure_code=degraded.failure_code,
            recoverable=degraded.recoverable,
            message=degraded.last_error_message,
        )
        self._apply_runtime_degradation_snapshot(degraded, event)

    def _apply_runtime_degradation_snapshot(self, degraded_snapshot, stage_event=None) -> None:
        result = self._launch_result
        if degraded_snapshot is None or result is None:
            return
        if stage_event is not None:
            self._on_supervisor_stage_event(stage_event)
        self._session_snapshot = degraded_snapshot
        self._launch_result = launch_result_from_snapshot(
            requested_mode=result.requested_mode,
            snapshot=degraded_snapshot,
            user_message=degraded_snapshot.last_error_message,
        )
        message = degraded_snapshot.last_error_message or "运行态退化，在线能力已收敛。"
        self._refresh_launch_status_ui()
        self._apply_mode_capabilities()
        self._update_home_controls_state()
        self._apply_permissions()
        self.statusBar().showMessage(message)

    def _current_supervisor_session_id(self) -> str:
        if self._supervisor_stage_events:
            last_event = self._supervisor_stage_events[-1]
            session_id = getattr(last_event, "session_id", "")
            if isinstance(session_id, str) and session_id:
                return session_id
        # Fallback session id for runtime failures emitted outside worker lifecycle.
        return f"runtime-{int(time.time() * 1000)}"

    def _detect_runtime_degradation(
        self,
        *,
        tcp_connected: bool | None = None,
        hardware_ready: bool | None = None,
        error_message: str | None = None,
    ):
        return SupervisorSession.detect_runtime_degradation_with_event(
            self._current_session_snapshot(),
            session_id=self._current_supervisor_session_id(),
            tcp_connected=tcp_connected,
            hardware_ready=hardware_ready,
            error_message=error_message,
        )

    def _show_offline_unavailable(self, capability: str) -> None:
        self.statusBar().showMessage(f"Offline 模式下不可用: {capability}")

    def _require_online_mode(self, capability: str) -> bool:
        if self._is_offline_mode():
            self._show_offline_unavailable(capability)
            return False
        if not self._is_online_ready():
            result = self._launch_result
            if result is not None and result.failure_code:
                self.statusBar().showMessage(
                    f"系统未就绪({result.failure_code}): {capability} 不可用"
                )
            else:
                self.statusBar().showMessage(f"系统启动中: {capability} 暂不可用")
            return False
        return True

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
        if self._is_offline_mode():
            self._show_offline_unavailable("会话恢复")
            return
        if self._is_session_operation_running():
            self.statusBar().showMessage("会话操作进行中，请稍候")
            return
        snapshot = self._current_session_snapshot()
        if snapshot is None:
            self.statusBar().showMessage("无可用会话快照，无法执行恢复动作")
            return
        if action in ("retry_stage", "restart_session"):
            if snapshot.session_state != "failed":
                self.statusBar().showMessage("当前会话未处于失败态，恢复动作不可用")
                return
            if not snapshot.recoverable:
                self.statusBar().showMessage("当前失败不可恢复，仅允许停止会话")
                return
        if action == "stop_session" and snapshot.mode != "online":
            self.statusBar().showMessage("仅在线会话支持停止动作")
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
        if result.success and result.online_ready:
            self.statusBar().showMessage(f"恢复动作完成({action})，系统已就绪")
        elif action == "stop_session" and result.session_state == "idle":
            self.statusBar().showMessage("会话已停止")
        else:
            code = result.failure_code or "SUP_UNKNOWN"
            self.statusBar().showMessage(f"恢复动作失败({action}): {code}")
        self._apply_permissions()
        self._update_recovery_controls_state()

    def _show_startup_error(self, result: LaunchResult):
        """Display startup error dialog."""
        stage_names = {
            "backend": "Backend startup",
            "tcp": "TCP connection",
            "hardware": "Hardware initialization",
            "backend_starting": "Backend starting",
            "backend_ready": "Backend ready",
            "tcp_connecting": "TCP connecting",
            "tcp_ready": "TCP ready",
            "hardware_probing": "Hardware probing",
            "hardware_ready": "Hardware ready",
            "online_ready": "Online ready",
        }
        stage_name = stage_names.get(result.phase, result.phase)
        code_text = f"\nFailure Code: {result.failure_code}" if result.failure_code else ""
        stage_text = f"\nFailure Stage: {result.failure_stage}" if result.failure_stage else ""

        QMessageBox.critical(
            self,
            "Startup Failed",
            f"{stage_name} failed:\n{result.user_message}{code_text}{stage_text}\n\n"
            "Please check:\n"
            "1. Backend executable exists\n"
            "2. Port is not in use\n"
            "3. Hardware is properly connected"
        )

    def _update_home_controls_state(self):
        enabled = self._is_online_ready()
        if hasattr(self, "_home_all_btn"):
            self._home_all_btn.setEnabled(enabled)
        if hasattr(self, "_prod_home_btn"):
            self._prod_home_btn.setEnabled(enabled)
        if hasattr(self, "_home_axis_buttons"):
            for btn in self._home_axis_buttons:
                btn.setEnabled(enabled)

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
        if not self._check_home_preconditions():
            return
        _ = allow_go_home
        ok, msg = self._protocol.home_auto(axes, force=force_rehome)
        if not ok and msg:
            QMessageBox.warning(self, "回零失败", msg)
        self.statusBar().showMessage(f"回零: {msg}" if msg else ("回零完成" if ok else "回零失败"))

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
        ok = self._protocol.stop()
        if not ok:
            _UI_LOGGER.warning("Stop request failed reason=%s", reason)
        if axis:
            self._log_motion_snapshot("Stop post", axis)

    def _on_estop(self):
        if not self._require_online_mode("急停"):
            return
        ok, msg = self._protocol.emergency_stop()
        self.statusBar().showMessage(f"急停: {msg}" if ok else (f"急停失败: {msg}" if msg else "急停失败"))

    def _on_estop_reset(self):
        if not self._require_online_mode("急停复位"):
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
        if not self._require_online_mode("DXF加载"):
            return
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
        if self._dxf_loaded:
            self._invalidate_preview_plan()
            self.statusBar().showMessage("运行模式已变更，请刷新预览并重新确认")

    def _invalidate_preview_plan(self):
        self._preview_gate.mark_input_changed()
        self._current_plan_id = ""
        self._current_plan_fingerprint = ""
        self._preview_plan_dry_run = None
        self._preview_state_resync_pending = False
        self._last_preview_resync_attempt_ts = 0.0
        self._update_info_label()

    def _update_dxf_info(self):
        info = self._protocol.dxf_get_info()
        self._dxf_total_length_val = info.get("total_length", 0.0)
        total_segments = info.get("total_segments")
        if total_segments is None:
            total_segments = getattr(self, "_dxf_segment_count_cache", 0)
        self._dxf_segment_count_cache = int(total_segments or 0)
        self._update_dxf_estimated_time()

    def _update_dxf_estimated_time(self, _=None):
        speed = self._dxf_speed.value()
        if speed <= 0:
            self._dxf_est_time_val = "Inf"
        else:
            time_sec = self._dxf_total_length_val / speed
            self._dxf_est_time_val = f"{time_sec:.1f}s"
        
        self._update_info_label()

    def _preview_state_text(self) -> str:
        state_map = {
            "dirty": "待生成",
            "generating": "生成中",
            "ready_unsigned": "待确认",
            "ready_signed": "已确认",
            "stale": "已过期",
            "failed": "失败",
        }
        return state_map.get(self._preview_gate.state.value, self._preview_gate.state.value)

    def _preview_block_message(self, reason: StartBlockReason) -> str:
        if reason == StartBlockReason.PREVIEW_MISSING:
            return "请先生成轨迹预览"
        if reason == StartBlockReason.PREVIEW_GENERATING:
            return "轨迹预览仍在生成中，请稍后"
        if reason == StartBlockReason.PREVIEW_FAILED:
            if self._preview_gate.last_error_message:
                return f"轨迹预览失败: {self._preview_gate.last_error_message}"
            return "轨迹预览失败，请重新生成"
        if reason == StartBlockReason.PREVIEW_STALE:
            return "轨迹参数已变更，需重新生成并确认预览"
        if reason == StartBlockReason.CONFIRM_MISSING:
            return "请先确认轨迹预览"
        if reason == StartBlockReason.HASH_MISMATCH:
            return "预览快照与执行快照不一致，请重新生成并确认"
        return "预检失败"

    def _confirm_preview_gate(self) -> bool:
        snapshot = self._preview_gate.snapshot
        if snapshot is None:
            return False
        summary = (
            f"段数: {snapshot.segment_count}\n"
            f"点数: {snapshot.point_count}\n"
            f"长度: {snapshot.total_length_mm:.1f}mm\n"
            f"预估: {snapshot.estimated_time_s:.1f}s\n\n"
            "确认以上轨迹预览后继续执行？"
        )
        reply = QMessageBox.question(
            self,
            "预览确认",
            summary,
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

        confirmed = bool(payload.get("confirmed", False))
        if not confirmed:
            self._show_preflight_warning("预览确认未生效，请重试")
            return False

        confirmed_hash = str(payload.get("snapshot_hash", snapshot.snapshot_hash)).strip()
        if confirmed_hash and confirmed_hash != snapshot.snapshot_hash:
            self._preview_gate.preview_failed("确认返回的快照哈希与当前预览不一致")
            self._show_preflight_warning("预览确认哈希不一致，请重新生成预览")
            return False

        gate_confirmed = self._preview_gate.confirm_current_snapshot()
        if not gate_confirmed:
            self._show_preflight_warning("本地预览确认状态更新失败")
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
        segments = getattr(self, '_dxf_segment_count_cache', 0)
        est = getattr(self, '_dxf_est_time_val', "-")
        preview_text = self._preview_state_text()
        self._dxf_info_label.setText(
            f"段数: {segments} | 长度: {self._dxf_total_length_val:.1f}mm | 预估: {est} | 预览: {preview_text}"
        )

    def _set_preview_message_html(self, title: str, detail: str = "", is_error: bool = False):
        if not WEB_ENGINE_AVAILABLE or not self._dxf_view:
            return
        title_color = "#ff7b72" if is_error else "#8be9fd"
        detail_html = html.escape(detail) if detail else " "
        self._dxf_view.setHtml(
            "<html><body style='background:#141414;color:#e8e8e8;font-family:Segoe UI;padding:24px;'>"
            f"<h3 style='margin-top:0;color:{title_color};'>{html.escape(title)}</h3>"
            f"<p style='color:#b8b8b8;line-height:1.6;'>{detail_html}</p>"
            "</body></html>"
        )

    def _set_preview_refresh_inflight(self, inflight: bool):
        self._preview_refresh_inflight = bool(inflight)
        self._update_preview_refresh_button_state()

    def _update_preview_refresh_button_state(self):
        if not hasattr(self, "_refresh_preview_btn"):
            return
        enabled = (
            not self._is_offline_mode()
            and getattr(self, "_connected", False)
            and self._dxf_loaded
            and not self._preview_refresh_inflight
        )
        self._refresh_preview_btn.setEnabled(enabled)

    def _is_unrecoverable_preview_resync_error(self, error_message: str, error_code=None) -> bool:
        normalized = str(error_message or "").strip().lower()
        if not normalized:
            return False
        unrecoverable_tokens = (
            "plan not found",
            "not found",
            "plan is not latest",
            "not latest",
            "invalid state",
            "plan_id is required",
        )
        if any(token in normalized for token in unrecoverable_tokens):
            return True
        # 3012 代表 snapshot 阶段后端错误；仅在消息可判定不可恢复时视为终止重试。
        if error_code == 3012 and "plan" in normalized and ("not" in normalized or "invalid" in normalized):
            return True
        return False

    def _degrade_preview_after_unrecoverable_resync(self, plan_id: str, error_message: str):
        self._preview_state_resync_pending = False
        self._last_preview_resync_attempt_ts = 0.0
        self._current_plan_id = ""
        self._current_plan_fingerprint = ""
        self._preview_plan_dry_run = None
        self._preview_gate.preview_failed("运行时预览已失效，请重新生成并确认")
        self._update_info_label()
        self.statusBar().showMessage("运行时预览已失效，请重新生成并确认")
        self._set_preview_message_html(
            "轨迹预览已失效",
            "运行时计划状态已变化，请重新生成并确认预览。",
            is_error=True,
        )
        _UI_LOGGER.warning(
            "preview_resync_aborted reason=unrecoverable plan_id=%s error=%s",
            plan_id,
            error_message,
        )

    def _sync_preview_state_from_runtime(self):
        if not self._preview_state_resync_pending:
            return
        if self._is_offline_mode() or not self._connected or not self._current_plan_id:
            return
        if self._preview_refresh_inflight:
            return
        if self._production_running or self._current_job_id:
            return

        now = time.monotonic()
        if now - self._last_preview_resync_attempt_ts < 2.0:
            return
        self._last_preview_resync_attempt_ts = now

        plan_id = self._current_plan_id
        try:
            ok, payload, error, error_code = self._protocol.dxf_preview_snapshot_with_error_details(
                plan_id=plan_id,
                max_polyline_points=4000,
            )
        except Exception as exc:
            _UI_LOGGER.warning("preview_resync_failed plan_id=%s error=%s", plan_id, exc)
            return

        if not ok:
            if self._is_unrecoverable_preview_resync_error(error, error_code):
                self._degrade_preview_after_unrecoverable_resync(plan_id, error)
                return
            _UI_LOGGER.warning(
                "preview_resync_failed plan_id=%s error=%s error_code=%s",
                plan_id,
                error,
                error_code,
            )
            return
        if not isinstance(payload, dict):
            self._preview_state_resync_pending = False
            self._preview_gate.preview_failed("运行时预览同步返回异常，请手动刷新预览")
            self._update_info_label()
            self.statusBar().showMessage(self._preview_block_message(StartBlockReason.PREVIEW_FAILED))
            self._set_preview_message_html("轨迹预览同步失败", "返回结果格式异常，请手动刷新预览。", is_error=True)
            _UI_LOGGER.warning("preview_resync_failed plan_id=%s error=invalid_payload", plan_id)
            return

        payload.setdefault("plan_id", plan_id)
        if self._preview_plan_dry_run is not None:
            payload.setdefault("dry_run", bool(self._preview_plan_dry_run))
        elif hasattr(self, "_mode_dryrun"):
            payload.setdefault("dry_run", bool(self._mode_dryrun.isChecked()))
        self._on_preview_snapshot_completed(True, payload, "", source="resync")
        self._preview_state_resync_pending = False

    def _render_runtime_preview_html(
        self,
        snapshot: PreviewSnapshotMeta,
        speed_mm_s: float,
        dry_run: bool,
        trajectory_points: list,
    ) -> str:
        mode_text = "空跑" if dry_run else "生产"
        generated_at = html.escape(snapshot.generated_at or "-")
        min_x = min(point[0] for point in trajectory_points)
        max_x = max(point[0] for point in trajectory_points)
        min_y = min(point[1] for point in trajectory_points)
        max_y = max(point[1] for point in trajectory_points)

        width = 920.0
        height = 520.0
        padding = 24.0
        glue_point_spacing_mm = 3.0
        glue_dot_diameter_mm = 1.5
        span_x = max(max_x - min_x, 1e-6)
        span_y = max(max_y - min_y, 1e-6)
        scale_x_px_per_mm = (width - 2 * padding) / span_x
        scale_y_px_per_mm = (height - 2 * padding) / span_y
        dot_diameter_px = glue_dot_diameter_mm * min(scale_x_px_per_mm, scale_y_px_per_mm)
        dot_radius = max(0.6, min(8.0, dot_diameter_px * 0.5))

        mapped_points = []
        for x_value, y_value in trajectory_points:
            mapped_x = padding + ((x_value - min_x) / span_x) * (width - 2 * padding)
            mapped_y = height - (padding + ((y_value - min_y) / span_y) * (height - 2 * padding))
            mapped_points.append((mapped_x, mapped_y))
        display_points = mapped_points
        points_markup = []
        for point_x, point_y in display_points:
            points_markup.append(
                f"<circle cx='{point_x:.2f}' cy='{point_y:.2f}' r='{dot_radius:.1f}' fill='#00d084' />"
            )
        point_cloud_svg = "".join(points_markup)
        return (
            "<html><body style='background:#1e1e1e;color:#e8e8e8;font-family:Segoe UI;padding:18px;'>"
            "<p style='color:#b8b8b8;'>"
            "轨迹图按快照点集以点状方式渲染，模拟真实胶点分布；执行前确认与哈希校验仍生效。"
            "</p>"
            f"<svg viewBox='0 0 {width:.0f} {height:.0f}' style='width:100%;height:56vh;background:#141414;border:1px solid #333;'>"
            f"{point_cloud_svg}"
            "</svg>"
            "<table style='border-collapse:collapse;'>"
            f"<tr><td style='padding:4px 16px 4px 0;'>模式</td><td>{mode_text}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>速度</td><td>{speed_mm_s:.3f} mm/s</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>段数</td><td>{snapshot.segment_count}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>点数</td><td>{snapshot.point_count}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>预览采样点</td><td>{len(display_points)}</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>胶点圆心距</td><td>{glue_point_spacing_mm:.1f} mm</td></tr>"
            f"<tr><td style='padding:4px 16px 4px 0;'>胶点直径</td><td>{glue_dot_diameter_mm:.1f} mm</td></tr>"
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

    def _extract_preview_points(self, payload: dict) -> list[tuple[float, float]]:
        points = []
        for raw in payload.get("trajectory_polyline", []) or []:
            if not isinstance(raw, dict):
                continue
            try:
                x_value = float(raw.get("x"))
                y_value = float(raw.get("y"))
            except (TypeError, ValueError):
                continue
            points.append((x_value, y_value))
        return points

    def _on_preview_snapshot_completed(self, ok: bool, payload: dict, error: str, source: str = "worker"):
        self._preview_snapshot_worker = None
        self._set_preview_refresh_inflight(False)

        if not ok:
            failure_message = error or "运行时快照生成失败"
            self._preview_gate.preview_failed(failure_message)
            self._update_info_label()
            self.statusBar().showMessage(self._preview_block_message(StartBlockReason.PREVIEW_FAILED))
            self._set_preview_message_html("轨迹预览生成失败", failure_message, is_error=True)
            _UI_LOGGER.warning("preview_failed stage=snapshot error=%s", failure_message)
            return

        snapshot_id = str(payload.get("snapshot_id", payload.get("plan_id", ""))).strip()
        snapshot_hash = str(payload.get("snapshot_hash", payload.get("plan_fingerprint", ""))).strip()
        if not snapshot_id or not snapshot_hash:
            self._preview_gate.preview_failed("运行时快照缺少 snapshot_id/snapshot_hash")
            self._update_info_label()
            self.statusBar().showMessage(self._preview_block_message(StartBlockReason.PREVIEW_FAILED))
            self._set_preview_message_html("轨迹预览生成失败", "返回结果缺少快照标识。", is_error=True)
            _UI_LOGGER.warning("preview_failed stage=snapshot_validate payload=%s", payload)
            return

        trajectory_points = self._extract_preview_points(payload)
        if not trajectory_points:
            self._preview_gate.preview_failed("运行时快照缺少轨迹点集")
            self._update_info_label()
            self.statusBar().showMessage(self._preview_block_message(StartBlockReason.PREVIEW_FAILED))
            self._set_preview_message_html("轨迹预览生成失败", "返回结果缺少 trajectory_polyline。", is_error=True)
            _UI_LOGGER.warning("preview_failed stage=polyline_missing payload=%s", payload)
            return

        backend_preview_state = str(payload.get("preview_state", "snapshot_ready")).strip().lower()
        preview_dry_run = bool(payload.get("dry_run", False))
        self._current_plan_id = str(payload.get("plan_id", snapshot_id)).strip() or snapshot_id
        self._current_plan_fingerprint = snapshot_hash
        self._preview_plan_dry_run = preview_dry_run
        self._dxf_segment_count_cache = int(payload.get("segment_count", 0) or 0)
        self._dxf_total_length_val = float(payload.get("total_length_mm", 0.0) or 0.0)
        estimated_time = float(payload.get("estimated_time_s", 0.0) or 0.0)
        self._dxf_est_time_val = f"{estimated_time:.1f}s" if estimated_time > 0 else "-"

        snapshot = PreviewSnapshotMeta(
            snapshot_id=snapshot_id,
            snapshot_hash=snapshot_hash,
            segment_count=int(payload.get("segment_count", 0) or 0),
            point_count=int(payload.get("point_count", len(trajectory_points)) or len(trajectory_points)),
            total_length_mm=float(payload.get("total_length_mm", 0.0) or 0.0),
            estimated_time_s=float(payload.get("estimated_time_s", 0.0) or 0.0),
            generated_at=str(payload.get("generated_at", "")),
        )
        self._preview_gate.preview_ready(snapshot)
        if backend_preview_state == "confirmed":
            if not self._preview_gate.confirm_current_snapshot():
                self._preview_gate.preview_failed("运行时状态同步失败：confirmed 快照无效")
                self._update_info_label()
                self.statusBar().showMessage(self._preview_block_message(StartBlockReason.PREVIEW_FAILED))
                self._set_preview_message_html("轨迹预览同步失败", "运行时确认态与本地快照不一致。", is_error=True)
                _UI_LOGGER.warning("preview_sync_failed stage=confirmed_state payload=%s", payload)
                return
        self._update_info_label()

        html_content = self._render_runtime_preview_html(
            snapshot=snapshot,
            speed_mm_s=self._dxf_speed.value(),
            dry_run=preview_dry_run,
            trajectory_points=trajectory_points,
        )
        self._dxf_view.setHtml(html_content)
        current_dry_run = self._mode_dryrun.isChecked() if hasattr(self, "_mode_dryrun") else False
        if current_dry_run != preview_dry_run:
            self._invalidate_preview_plan()
            self.statusBar().showMessage("运行模式已变更，请刷新预览并重新确认")
            _UI_LOGGER.warning(
                "preview_staled_by_mode_change preview_dry_run=%s current_dry_run=%s",
                preview_dry_run,
                current_dry_run,
            )
        else:
            if source == "resync":
                if self._preview_gate.state == PreviewGateState.READY_SIGNED:
                    self.statusBar().showMessage("已从运行时同步预览状态（已确认）")
                else:
                    self.statusBar().showMessage("已从运行时同步预览状态")
            else:
                if self._preview_gate.state == PreviewGateState.READY_SIGNED:
                    self.statusBar().showMessage("轨迹预览已更新（运行时已确认）")
                else:
                    self.statusBar().showMessage("轨迹预览已更新，启动前需确认")
        _UI_LOGGER.info(
            "preview_ready snapshot_id=%s snapshot_hash=%s sampled_points=%d",
            snapshot.snapshot_id,
            snapshot.snapshot_hash,
            len(trajectory_points),
        )

    def _on_dxf_load(self):
        if not self._require_online_mode("DXF加载"):
            return
        if not self._dxf_filepath:
            self.statusBar().showMessage("未选择DXF文件")
            return
        ok, payload, error = self._protocol.dxf_create_artifact(self._dxf_filepath)
        if ok:
            self._dxf_artifact_id = str(payload.get("artifact_id", "")).strip()
            self._current_plan_id = ""
            self._current_plan_fingerprint = ""
            self._preview_plan_dry_run = None
            self._current_job_id = ""
            self._dxf_loaded = True
            self._preview_gate.reset_for_loaded_dxf()
            self._dxf_segments_label = None # Deprecated, clear ref if any
            self._dxf_segment_count_cache = int(payload.get("segment_count", 0) or 0)
            self._dxf_total_length_val = 0.0
            self._dxf_est_time_val = "-"
            
            # Update filename display (Basename only)
            import os
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
            self._current_plan_id = ""
            self._current_plan_fingerprint = ""
            self._preview_plan_dry_run = None
            self._current_job_id = ""
            self._preview_gate.reset_for_loaded_dxf()
            self._update_info_label()
            self._update_preview_refresh_button_state()
            self._set_preview_message_html("轨迹预览不可用", "DXF加载失败，无法生成点状轨迹预览。", is_error=True)
            self.statusBar().showMessage(f"DXF加载失败: {error or 'Unknown error'}")

    def _generate_dxf_preview(self):
        """Generate and display DXF preview in embedded view."""
        if not self._require_online_mode("DXF预览"):
            return
        if self._preview_refresh_inflight:
            self.statusBar().showMessage("轨迹预览仍在生成中，请稍后")
            return
        if not self._dxf_loaded:
            self.statusBar().showMessage("请先加载DXF文件后再刷新预览")
            self._set_preview_message_html("尚未加载DXF", "请先选择并上传DXF文件。")
            return
        if not self._dxf_artifact_id:
            self._preview_gate.preview_failed("DXF工件未创建")
            self._update_info_label()
            self.statusBar().showMessage("DXF工件未创建")
            self._set_preview_message_html("轨迹预览生成失败", "DXF工件未创建。", is_error=True)
            return
        if not WEB_ENGINE_AVAILABLE or not self._dxf_view:
            self._preview_gate.preview_failed("预览组件不可用")
            self._update_info_label()
            self.statusBar().showMessage("预览组件不可用")
            return

        speed = self._dxf_speed.value()
        dry_run_mode = self._mode_dryrun.isChecked() if hasattr(self, "_mode_dryrun") else False
        self._preview_gate.begin_preview()
        self._update_info_label()
        self._set_preview_refresh_inflight(True)
        self._cleanup_temp_preview()
        self._set_preview_message_html("轨迹预览生成中", "正在请求运行时快照点集，请稍候。")
        self.statusBar().showMessage("轨迹预览生成中...")
        _UI_LOGGER.info("preview_requested speed_mm_s=%.3f file=%s", speed, self._dxf_filepath)

        worker = PreviewSnapshotWorker(
            host=self._client.host,
            port=self._client.port,
            artifact_id=self._dxf_artifact_id,
            speed_mm_s=speed,
            dry_run=dry_run_mode,
            dry_run_speed_mm_s=speed,
        )
        worker.completed.connect(self._on_preview_snapshot_completed)
        worker.finished.connect(worker.deleteLater)
        self._preview_snapshot_worker = worker
        try:
            worker.start()
        except Exception as exc:
            self._preview_snapshot_worker = None
            self._set_preview_refresh_inflight(False)
            self._preview_gate.preview_failed(str(exc))
            self._update_info_label()
            self.statusBar().showMessage(self._preview_block_message(StartBlockReason.PREVIEW_FAILED))
            self._set_preview_message_html("轨迹预览生成失败", str(exc), is_error=True)

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
        if not self._connected:
            self._show_preflight_warning("未连接到后端，请先连接")
            return False
        if not self._hw_connected:
            self._show_preflight_warning("硬件未初始化，请先初始化硬件")
            return False
        if self._runtime_status_fault:
            self._show_preflight_warning("运行状态采集异常，请先恢复状态链路后再启动")
            return False
        if self._preview_refresh_inflight:
            self._show_preflight_warning("轨迹预览仍在生成中，请稍后再启动")
            return False

        status = self._protocol.get_status()
        if not status.connected:
            self._show_preflight_warning("后端状态不可用，请检查连接")
            return False
        if not status.gate_estop_known() or not status.gate_door_known():
            self._show_preflight_warning("互锁信号状态未知，禁止启动")
            return False
        if status.gate_estop_active():
            self._show_preflight_warning("急停未解除，禁止启动")
            return False
        if status.gate_door_active():
            self._show_preflight_warning("安全门打开，禁止启动")
            return False
        self._last_status = status
        self._last_status_ts = time.monotonic()
        self._runtime_status_fault = False
        if not self._current_plan_id or not self._current_plan_fingerprint:
            self._show_preflight_warning("预览计划未生成，请重新生成预览")
            return False
        if self._preview_plan_dry_run is None:
            self._show_preflight_warning("预览模式未知，请刷新预览并重新确认")
            return False
        if bool(dry_run) != bool(self._preview_plan_dry_run):
            preview_dry_run = self._preview_plan_dry_run
            self._invalidate_preview_plan()
            self._show_preflight_warning("运行模式已变更，请刷新预览并重新确认")
            _UI_LOGGER.warning(
                "start_blocked_by_mode_mismatch requested_dry_run=%s preview_dry_run=%s",
                dry_run,
                preview_dry_run,
            )
            return False

        gate_decision = self._preview_gate.decision_for_start()
        if gate_decision.reason == StartBlockReason.CONFIRM_MISSING:
            if not self._confirm_preview_gate():
                self._show_preflight_warning(self._preview_block_message(StartBlockReason.CONFIRM_MISSING))
                return False
            gate_decision = self._preview_gate.decision_for_start()

        if not gate_decision.allowed:
            block_message = self._preview_block_message(gate_decision.reason)
            self._show_preflight_warning(block_message)
            _UI_LOGGER.warning("start_blocked_by_preview_gate reason=%s", gate_decision.reason.value)
            return False

        hash_decision = self._preview_gate.validate_execution_snapshot_hash(self._current_plan_fingerprint)
        if not hash_decision.allowed:
            self._show_preflight_warning(self._preview_block_message(hash_decision.reason))
            _UI_LOGGER.warning(
                "start_blocked_by_preview_hash confirmed=%s runtime=%s",
                self._preview_gate.get_confirmed_snapshot_hash(),
                self._current_plan_fingerprint,
            )
            return False

        return True

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
                self._preview_state_resync_pending = True
                degradation = self._detect_runtime_degradation(
                    tcp_connected=True,
                    hardware_ready=False,
                    error_message="运行中硬件状态不可用，在线能力已收敛。",
                )
                if degradation is not None:
                    degraded_snapshot, stage_event = degradation
                    self._apply_runtime_degradation_snapshot(degraded_snapshot, stage_event)
                return
            self._last_status = status
            self._last_status_ts = time.monotonic()
            self._runtime_status_fault = False
            self._last_status_error_notice_ts = 0.0
            self._state_label.setText(status.machine_state)
            backend_hw_connected = bool(status.connected)
            if backend_hw_connected != self._hw_connected:
                self._hw_connected = backend_hw_connected
                self._refresh_launch_status_ui()

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
                    self._preview_state_resync_pending = True
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
                    self._preview_state_resync_pending = True
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
            self._preview_state_resync_pending = True
            degradation = self._detect_runtime_degradation(
                tcp_connected=False,
                error_message=f"运行中 TCP 连接异常: {exc}",
            )
            if degradation is not None:
                degraded_snapshot, stage_event = degradation
                self._apply_runtime_degradation_snapshot(degraded_snapshot, stage_event)
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
