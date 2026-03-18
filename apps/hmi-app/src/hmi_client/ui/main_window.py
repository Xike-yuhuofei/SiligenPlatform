"""PyQt5 Main Window for Siligen HMI."""
import os
import sys
import time
import logging
from pathlib import Path
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QGroupBox, QLabel, QPushButton, QLineEdit, QGridLayout, QFrame,
    QSlider, QSpinBox, QDoubleSpinBox, QCheckBox, QTabWidget, QFileDialog,
    QProgressBar, QListWidget, QListWidgetItem, QMessageBox, QDialog, QDialogButtonBox,
    QComboBox, QFormLayout, QScrollArea, QRadioButton, QStatusBar
)
from PyQt5.QtCore import QTimer, Qt, QSize, QUrl, QEvent
from PyQt5.QtGui import QFont, QColor

try:
    from PyQt5.QtWebEngineWidgets import QWebEngineView
    WEB_ENGINE_AVAILABLE = True
except ImportError:
    QWebEngineView = None
    WEB_ENGINE_AVAILABLE = False

from client import TcpClient, CommandProtocol, BackendManager, StartupWorker, StartupResult
from client.auth import AuthManager
from integrations.dxf_pipeline import DxfPipelinePreviewClient
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


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self._aspect_ratio = _resolve_aspect_ratio()
        min_size = _resolve_min_size(self._aspect_ratio)
        self.setWindowTitle("思力根运动控制器")
        self.setMinimumSize(min_size)
        self.resize(min_size)
        self.setStyleSheet(DARK_THEME)

        self._client = TcpClient()
        self._protocol = CommandProtocol(self._client)
        self._dxf_preview_client = DxfPipelinePreviewClient()
        self._backend = BackendManager()
        self._connected = False
        self._hw_connected = False
        self._jog_speed = 20.0
        self._jog_press_time = None
        self._last_jog_context = None
        self._dxf_loaded = False
        self._dxf_filepath = ""
        self._dxf_dry_run_speed_custom = None
        self._temp_preview_file = None
        self._dxf_total_length_val = 0.0
        self._dxf_segment_count_cache = 0
        self._dxf_est_time_val = "-"
        self._last_status = None
        self._last_status_ts = 0.0

        # Production statistics
        self._production_running = False
        self._production_paused = False
        self._production_dry_run = False
        self._completed_count = 0
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
        self._auto_startup()
        self._update_home_controls_state()

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
        tabs.addTab(self._create_production_tab(), "生产")
        tabs.addTab(self._create_setup_tab(), "设置")
        tabs.addTab(self._create_recipe_tab(), "配置")
        tabs.addTab(self._create_alarm_panel(), "报警")
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

        # Connection Indicators
        layout.addWidget(QLabel("TCP:"))
        self._tcp_led = QLabel()
        self._tcp_led.setProperty("data-testid", "indicator-tcp-status")
        self._tcp_led.setProperty("role", "led-off")
        self._tcp_led.setFixedSize(16, 16)
        layout.addWidget(self._tcp_led)
        layout.addSpacing(10)
        layout.addWidget(QLabel("硬件:"))
        self._hw_led = QLabel()
        self._hw_led.setProperty("data-testid", "indicator-hw-status")
        self._hw_led.setProperty("role", "led-off")
        self._hw_led.setFixedSize(16, 16)
        layout.addWidget(self._hw_led)

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
        self._dxf_info_label = QLabel("段数: - | 长度: - | 预估: -")
        self._dxf_info_label.setProperty("class", "sub-text")
        job_layout.addWidget(self._dxf_info_label)
        
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
        self._mode_production.toggled.connect(self._update_start_button_state)
        self._mode_dryrun = QRadioButton("空跑")
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
        left_column.addWidget(self._create_system_panel())
        left_column.addStretch()
        top_layout.addLayout(left_column, 1)
        top_layout.addWidget(self._create_status_panel(), 2)
        layout.addLayout(top_layout)

        # Control Panels in horizontal layout
        controls_layout = QHBoxLayout()
        controls_layout.setSpacing(20)

        # Motion Control
        controls_layout.addWidget(self._create_motion_control_panel())

        # Dispenser Control
        controls_layout.addWidget(self._create_dispenser_control_panel())

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

        # Home buttons (X/Y only)
        home_layout = QGridLayout()
        self._home_all_btn = QPushButton("全部回零")
        self._home_all_btn.setProperty("data-testid", "btn-home-all")
        self._home_all_btn.setProperty("role", "primary")
        self._home_all_btn.setCursor(Qt.PointingHandCursor)
        self._home_all_btn.clicked.connect(lambda: self._on_home(None))
        home_layout.addWidget(self._home_all_btn, 0, 0, 1, 2)

        self._home_axis_buttons = []
        for i, axis in enumerate(["X", "Y"]):
            btn = QPushButton(f"{axis}轴回零")
            btn.setProperty("data-testid", f"btn-home-{axis}")
            btn.setCursor(Qt.PointingHandCursor)
            btn.clicked.connect(lambda checked, a=axis: self._on_home([a]))
            home_layout.addWidget(btn, 1, i)
            self._home_axis_buttons.append(btn)
        layout.addLayout(home_layout)

        layout.addSpacing(10)

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

    def _setup_timer(self):
        self._timer = QTimer()
        self._timer.timeout.connect(self._update_status)
        self._timer.start(200)

    def _auto_startup(self):
        """Execute automatic startup sequence."""
        self._startup_worker = StartupWorker(
            self._backend,
            self._client,
            self._protocol
        )
        self._startup_worker.progress.connect(self._on_startup_progress)
        self._startup_worker.finished.connect(self._on_startup_finished)
        self._startup_worker.start()

    def _on_startup_progress(self, message: str, percent: int):
        """Handle startup progress updates."""
        self.statusBar().showMessage(message)
        self._global_progress.setValue(percent)

    def _on_startup_finished(self, result: StartupResult):
        """Handle startup sequence completion."""
        if result.success:
            self._connected = True
            self._hw_connected = True
            self._update_led(self._tcp_led, "green")
            self._update_led(self._hw_led, "green")
            self._hw_connect_btn.setEnabled(False)
            self._global_progress.setValue(0)
            self.statusBar().showMessage("System ready")
            self._update_home_controls_state()
            if hasattr(self, '_recipe_config_widget'):
                self._recipe_config_widget._load_recipe_context()
        else:
            self._show_startup_error(result)
            self._global_progress.setValue(0)
            self._update_home_controls_state()

    def _show_startup_error(self, result: StartupResult):
        """Display startup error dialog."""
        stage_names = {
            "backend": "Backend startup",
            "tcp": "TCP connection",
            "hardware": "Hardware initialization"
        }
        stage_name = stage_names.get(result.stage, result.stage)

        QMessageBox.critical(
            self,
            "Startup Failed",
            f"{stage_name} failed:\n{result.message}\n\n"
            "Please check:\n"
            "1. Backend executable exists\n"
            "2. Port is not in use\n"
            "3. Hardware is properly connected"
        )

    def _update_home_controls_state(self):
        enabled = self._connected and self._hw_connected
        if hasattr(self, "_home_all_btn"):
            self._home_all_btn.setEnabled(enabled)
        if hasattr(self, "_prod_home_btn"):
            self._prod_home_btn.setEnabled(enabled)
        if hasattr(self, "_home_axis_buttons"):
            for btn in self._home_axis_buttons:
                btn.setEnabled(enabled)

    def _check_home_preconditions(self) -> bool:
        if not self._connected:
            self.statusBar().showMessage("未连接到后端，请先连接")
            return False
        if not self._hw_connected:
            self.statusBar().showMessage("硬件未初始化，请先初始化硬件")
            return False
        status = self._get_cached_status()
        if not status or not status.connected:
            status = self._protocol.get_status()
            self._last_status = status
            self._last_status_ts = time.monotonic()
        if not status or not status.connected:
            self.statusBar().showMessage("后端状态未就绪，请稍后再试")
            return False
        if status.io.estop:
            self.statusBar().showMessage("急停未解除，无法回零")
            return False
        if status.io.door:
            self.statusBar().showMessage("安全门打开，无法回零")
            return False
        return True

    def _should_go_home(self, axes) -> bool:
        status = self._get_cached_status()
        if not status or not status.connected:
            status = self._protocol.get_status()
            self._last_status = status
            self._last_status_ts = time.monotonic()
        if not status or not status.connected:
            return False
        target_axes = axes if axes else list(status.axes.keys())
        if not target_axes:
            return False
        for axis in target_axes:
            axis_status = status.axes.get(axis)
            if not axis_status or not axis_status.homed:
                return False
        return True

    def _resolve_home_action(self, axes):
        status = self._protocol.get_status()
        self._last_status = status
        self._last_status_ts = time.monotonic()
        if not status or not status.connected:
            return "home", "后端状态未就绪"
        target_axes = axes if axes else list(status.axes.keys())
        if not target_axes:
            return "home", "轴列表为空"
        all_homed = True
        for axis in target_axes:
            axis_status = status.axes.get(axis)
            if not axis_status:
                all_homed = False
                continue
            if not axis_status.homed:
                all_homed = False
        if all_homed:
            return "go", ""
        return "home", ""

    def _check_motion_preconditions(self) -> bool:
        if not self._connected:
            self.statusBar().showMessage("未连接到后端，无法点动")
            return False
        if not self._hw_connected:
            self.statusBar().showMessage("硬件未初始化，无法点动")
            return False
        status = self._protocol.get_status()
        if not status.connected:
            self.statusBar().showMessage("后端状态不可用，请检查连接")
            return False
        if status.io.estop:
            self.statusBar().showMessage("急停未解除，无法点动")
            return False
        if status.io.door:
            self.statusBar().showMessage("安全门打开，无法点动")
            return False
        return True

    def _on_connect(self):
        if self._connected:
            return
        if self._client.connect():
            self._connected = True
            self._hw_connect_btn.setEnabled(True)
            self._update_led(self._tcp_led, "green")
            self.statusBar().showMessage(f"已连接到 {self._client.host}:{self._client.port}")
            self._update_home_controls_state()
        else:
            self.statusBar().showMessage("连接失败")

    def _on_disconnect(self):
        if not self._connected:
            return
        reply = QMessageBox.question(
            self, "确认断开", "确定要断开连接吗？",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No
        )
        if reply != QMessageBox.Yes:
            return
        self._client.disconnect()
        self._connected = False
        self._hw_connected = False
        self._hw_connect_btn.setEnabled(False)
        self._update_led(self._tcp_led, "off")
        self._update_led(self._hw_led, "off")
        self.statusBar().showMessage("已断开")
        self._update_home_controls_state()

    def _on_hw_connect(self):
        ok, msg = self._protocol.connect_hardware()
        if ok:
            self._hw_connected = True
            self._update_led(self._hw_led, "green")
        else:
            self._hw_connected = False
            self._update_led(self._hw_led, "red")
        self.statusBar().showMessage(f"硬件: {msg}" if msg else ("硬件已连接" if ok else "硬件连接失败"))
        self._update_home_controls_state()

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
        action = "home"
        reason = ""
        if allow_go_home:
            action, reason = self._resolve_home_action(axes)
        if reason:
            self.statusBar().showMessage(reason)
            if action == "home":
                return
        if action == "go":
            ok, msg = self._protocol.home_go(axes)
        else:
            ok, msg = self._protocol.home(axes, force=force_rehome)
        if not ok and msg:
            QMessageBox.warning(self, "回零失败", msg)
        action_text = "回零位" if action == "go" else "回零"
        self.statusBar().showMessage(f"{action_text}: {msg}" if msg else (f"{action_text}完成" if ok else f"{action_text}失败"))

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
        ok, msg = self._protocol.emergency_stop()
        self.statusBar().showMessage(f"急停: {msg}")

    def _on_move_to(self):
        self._auth.record_activity()
        x = self._move_inputs["X"].value()
        y = self._move_inputs["Y"].value()
        speed = self._move_speed.value()
        ok = self._protocol.move_to(x, y, speed)
        self.statusBar().showMessage("移动中..." if ok else "移动失败")

    def _on_dispenser_start(self):
        count = self._dispenser_count.value()
        interval = self._dispenser_interval.value()
        duration = self._dispenser_duration.value()
        ok = self._protocol.dispenser_start(count, interval, duration)
        if ok:
            self._increment_needle_count(count)
        self.statusBar().showMessage(f"点胶已启动 (次数:{count}, 间隔:{interval}ms, 持续:{duration}ms)")

    def _on_dispenser_pause(self):
        self._protocol.dispenser_pause()
        self.statusBar().showMessage("点胶已暂停")

    def _on_dispenser_resume(self):
        self._protocol.dispenser_resume()
        self.statusBar().showMessage("点胶已继续")

    def _on_dispenser_stop(self):
        self._protocol.dispenser_stop()
        self.statusBar().showMessage("点胶已停止")

    def _on_supply_open(self):
        self._protocol.supply_open()
        self.statusBar().showMessage("供料阀已打开")

    def _on_supply_close(self):
        self._protocol.supply_close()
        self.statusBar().showMessage("供料阀已关闭")

    def _on_purge(self):
        timeout = self._purge_duration.value()
        ok = self._protocol.purge(timeout)
        if ok:
            self._last_purge_time = time.time()
            self._update_maintenance_display()
        self.statusBar().showMessage(f"清洗中 (超时: {timeout}ms)")

    def _on_dxf_browse(self):
        import os
        start_dir = ""
        if self._dxf_filepath:
            start_dir = os.path.dirname(self._dxf_filepath)
        else:
            workspace_override = os.getenv("SILIGEN_WORKSPACE_ROOT", "").strip()
            workspace_root = None
            if workspace_override:
                workspace_root = Path(workspace_override).expanduser()
            else:
                for candidate in Path(__file__).resolve().parents:
                    if (candidate / "apps").exists() and (candidate / "packages").exists():
                        workspace_root = candidate
                        break

            default_dir_override = os.getenv("SILIGEN_DXF_DEFAULT_DIR", "").strip()
            default_candidates = []
            if default_dir_override:
                default_candidates.append(Path(default_dir_override).expanduser())
            if workspace_root is not None:
                default_candidates.extend(
                    [
                        workspace_root / "uploads" / "dxf",
                        workspace_root / "packages" / "engineering-contracts" / "fixtures" / "cases" / "rect_diag",
                        workspace_root / "control-core" / "uploads" / "dxf",
                    ]
                )

            for candidate in default_candidates:
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
        self._update_dxf_estimated_time()

    def _on_speed_spinbox_changed(self, value: float):
        """Sync spinbox to slider."""
        self._dxf_speed_slider.blockSignals(True)
        self._dxf_speed_slider.setValue(int(value))
        self._dxf_speed_slider.blockSignals(False)
        self._update_dxf_estimated_time()

    def _update_dxf_info(self):
        info = self._protocol.dxf_get_info()
        self._dxf_total_length_val = info.get("total_length", 0.0)
        self._dxf_segment_count_cache = info.get("total_segments", 0)
        self._update_dxf_estimated_time()

    def _update_dxf_estimated_time(self, _=None):
        speed = self._dxf_speed.value()
        if speed <= 0:
            self._dxf_est_time_val = "Inf"
        else:
            time_sec = self._dxf_total_length_val / speed
            self._dxf_est_time_val = f"{time_sec:.1f}s"
        
        self._update_info_label()

    def _update_info_label(self):
        segments = getattr(self, '_dxf_segment_count_cache', 0)
        est = getattr(self, '_dxf_est_time_val', "-")
        self._dxf_info_label.setText(f"段数: {segments} | 长度: {self._dxf_total_length_val:.1f}mm | 预估: {est}")

    def _on_dxf_load(self):
        if not self._dxf_filepath:
            self.statusBar().showMessage("未选择DXF文件")
            return
        ok, segment_count = self._protocol.dxf_load(self._dxf_filepath)
        if ok:
            self._dxf_loaded = True
            self._dxf_segments_label = None # Deprecated, clear ref if any
            
            # Update filename display (Basename only)
            import os
            filename = os.path.basename(self._dxf_filepath)
            self._dxf_filename_display.setText(filename)
            self._dxf_filename_display.setToolTip(self._dxf_filepath)
            
            # Fetch and display DXF info
            self._update_dxf_info()
            self.statusBar().showMessage(f"DXF已加载: {segment_count}段")
            # Auto generate preview
            self._generate_dxf_preview()
        else:
            self._dxf_loaded = False
            # 显示具体的错误信息，而不是通用的"DXF加载失败"
            error_msg = "DXF加载失败"
            if isinstance(segment_count, str):
                error_msg = f"DXF加载失败: {segment_count}"
            self.statusBar().showMessage(error_msg)

    def _generate_dxf_preview(self):
        """Generate and display DXF preview in embedded view."""
        if not self._dxf_loaded:
            return
        if not WEB_ENGINE_AVAILABLE or not self._dxf_view:
            self.statusBar().showMessage("预览组件不可用")
            return

        speed = self._dxf_speed.value()
        self._cleanup_temp_preview()

        preview_result = self._dxf_preview_client.generate_preview(
            dxf_path=self._dxf_filepath,
            speed_mm_s=speed,
            title=f"DXF 预览 - {Path(self._dxf_filepath).name}",
        )
        if not preview_result.ok:
            self.statusBar().showMessage(f"DXF预览生成失败: {preview_result.error_message}")
            self._dxf_view.setHtml(
                "<html><body><h3 style='color:red'>"
                f"预览生成失败: {preview_result.error_message}"
                "</h3></body></html>"
            )
            return

        self._temp_preview_file = preview_result.preview_path
        self._dxf_view.load(QUrl.fromLocalFile(preview_result.preview_path))

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

    def _check_production_preconditions(self) -> bool:
        if not self._connected:
            self._show_preflight_warning("未连接到后端，请先连接")
            return False
        if not self._hw_connected:
            self._show_preflight_warning("硬件未初始化，请先初始化硬件")
            return False

        status = self._protocol.get_status()
        if not status.connected:
            self._show_preflight_warning("后端状态不可用，请检查连接")
            return False
        if status.io.estop:
            self._show_preflight_warning("急停未解除，无法启动")
            return False
        if status.io.door:
            self._show_preflight_warning("安全门打开，无法启动")
            return False

        required_axes = ("X", "Y")
        missing_axes = []
        not_enabled = []
        not_homed = []
        for axis in required_axes:
            axis_status = status.axes.get(axis)
            if axis_status is None:
                missing_axes.append(axis)
                continue
            if not axis_status.enabled:
                not_enabled.append(axis)
            if not axis_status.homed:
                not_homed.append(axis)

        if missing_axes or not_enabled or not_homed:
            parts = []
            if missing_axes:
                parts.append(f"缺少轴状态: {', '.join(missing_axes)}")
            if not_enabled:
                parts.append(f"未使能: {', '.join(not_enabled)}")
            if not_homed:
                parts.append(f"未回零: {', '.join(not_homed)}")
            self._show_preflight_warning(f"无法启动，{'；'.join(parts)}")
            return False

        return True

    def _on_production_start_clicked(self):
        """Handle start button click."""
        is_dry_run = self._mode_dryrun.isChecked()
        self._start_production_process(dry_run=is_dry_run)

    def _on_dxf_stop(self):
        self._protocol.dxf_stop()
        self.statusBar().showMessage("DXF执行已停止")

    def _on_alarm_clear(self):
        if self._protocol.clear_alarms():
            self._alarm_list.clear()
            self.statusBar().showMessage("报警已清除")
        else:
            self.statusBar().showMessage("清除报警失败")

    def _on_alarm_ack(self):
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
        self._auth.record_activity()
        if not self._dxf_loaded:
            self.statusBar().showMessage("请先加载DXF文件")
            return
        if not self._check_production_preconditions():
            return
            
        self._production_running = True
        self._production_paused = False
        self._production_dry_run = dry_run
        self._run_start_time = time.time()
        self._last_cycle_start = time.time()
        
        mode_text = "空跑" if dry_run else "生产"
        self._operation_status.setText(f"{mode_text}运行中")
        
        speed = self._dxf_speed.value() if hasattr(self, '_dxf_speed') else 10.0
        # Use same speed for dry run in simplified mode
        if self._protocol.dxf_execute(speed, dry_run=dry_run, dry_run_speed_mm_s=speed):
            self.statusBar().showMessage(f"{mode_text}已启动")
        else:
            self._production_running = False
            self._production_paused = False
            self.statusBar().showMessage("启动失败")

    def _on_production_pause(self):
        """Pause production."""
        if not self._production_running:
            self.statusBar().showMessage("当前没有运行中的生产任务")
            return
        if self._run_start_time > 0:
            self._total_run_time += time.time() - self._run_start_time
        self._production_running = False
        if self._protocol.dxf_pause():
            self._production_paused = True
            self._operation_status.setText("已暂停")
            self.statusBar().showMessage("生产已暂停")
        else:
            self._production_running = True
            self._run_start_time = time.time()
            self.statusBar().showMessage("暂停失败")

    def _on_production_resume(self):
        """Resume production after a pause."""
        if not self._production_paused:
            self.statusBar().showMessage("当前没有已暂停的生产任务")
            return

        if self._protocol.dxf_resume():
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
        if self._production_running and self._run_start_time > 0:
            self._total_run_time += time.time() - self._run_start_time
        self._production_running = False
        self._production_paused = False
        self._operation_status.setText("已停止")
        self._protocol.dxf_stop()
        self.statusBar().showMessage("生产已停止")

    def _on_target_changed(self, value):
        """Update target count."""
        self._target_count = value
        self._target_label.setText(str(value))
        self._update_production_stats()

    def _on_reset_counter(self):
        """Reset production counter."""
        self._completed_count = 0
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

        if not self._connected:
            return
        try:
            status = self._protocol.get_status()
            self._last_status = status
            self._last_status_ts = time.monotonic()
            self._state_label.setText(status.machine_state)

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
                "io-estop": io.estop,
                "io-door": io.door,
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
                dxf_progress = self._protocol.dxf_get_progress()
                current = dxf_progress.get("current_segment", 0)
                total = dxf_progress.get("total_segments", 0)
                progress = dxf_progress.get("progress", 0)
                running = dxf_progress.get("running", False)
                state = dxf_progress.get("state", "")
                error_message = dxf_progress.get("error_message", "")
                if hasattr(self, '_global_progress'):
                    self._global_progress.setValue(int(progress))
                # self._dxf_current_segment.setText(f"{current} / {total}") # Widget removed

                if state == "paused":
                    self._production_paused = True
                    self._production_running = False
                    self._operation_status.setText("已暂停")
                elif self._production_paused and state == "running":
                    self._production_paused = False
                    self._production_running = True
                    if self._run_start_time <= 0:
                        self._run_start_time = time.time()
                    mode_text = "空跑" if self._production_dry_run else "生产"
                    self._operation_status.setText(f"{mode_text}运行中")

                # Check for cycle completion
                if self._production_running:
                    if state in ("failed", "cancelled"):
                        self._production_running = False
                        self._production_paused = False
                        status_text = "失败" if state == "failed" else "已取消"
                        self._operation_status.setText(status_text)
                        if error_message:
                            self.statusBar().showMessage(f"执行失败: {error_message}")
                        else:
                            self.statusBar().showMessage(f"执行{status_text}")
                    elif state == "completed" or (not running and progress >= 100):
                        self._record_cycle_complete()
                        # Auto-restart if target not reached
                        if self._completed_count < self._target_count:
                            self._last_cycle_start = time.time()
                            speed = self._dxf_speed.value() if hasattr(self, '_dxf_speed') else 10.0
                            self._protocol.dxf_execute(
                                speed,
                                dry_run=self._production_dry_run,
                                dry_run_speed_mm_s=speed,
                            )
                        else:
                            self._production_running = False
                            self._production_paused = False
                            self._operation_status.setText("完成")
                            self.statusBar().showMessage("生产目标已达成")
                    elif not running and progress == 0:
                        # Task not running but production flag still on: treat as abnormal stop
                        self._production_running = False
                        self._production_paused = False
                        self._operation_status.setText("已停止")
                        if error_message:
                            self.statusBar().showMessage(f"执行失败: {error_message}")
                        else:
                            self.statusBar().showMessage("执行未启动或已中断")

            # Update production stats periodically
            if self._production_running:
                self._update_production_stats()

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
        except:
            pass

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
            self._hw_connect_btn.setEnabled(level >= 3 and self._connected)

    def _check_permission(self, required_level: int) -> bool:
        """Check if current user has required permission level."""
        level = self._auth.current_user.level if self._auth.current_user else 1
        if level >= required_level:
            return True
        self.statusBar().showMessage(f"权限不足 (需要等级 {required_level})")
        return False


def main():
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    sys.exit(app.exec_())
