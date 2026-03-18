"""
ISA-101 Compliant Color Scheme:
- Red (#FF5722): Stop, Fault, Danger, E-Stop
- Green (#4CAF50): Run, Normal, Allow
- Yellow (#FFC107): Warning, Caution, Pause
- Blue (#2196F3): Info, Selected, Highlight (non-safety)
"""

DARK_THEME = """
/* =================================================================================
   GLOBAL SETTINGS - ISA-101 Compliant
   ================================================================================= */
QMainWindow {
    background-color: #1E1E1E; /* Deep Grey Background */
}

QWidget {
    background-color: #1E1E1E;
    color: #E0E0E0;
    font-family: "Segoe UI", "Roboto", "Arial", sans-serif;
    font-size: 14px;
    selection-background-color: #2196F3;
    selection-color: #FFFFFF;
    outline: none;
}

/* =================================================================================
   CONTAINERS
   ================================================================================= */
QGroupBox {
    border: 1px solid #3D3D3D;
    border-radius: 6px;
    margin-top: 24px; /* Space for title */
    background-color: #252526;
    padding-top: 15px;
}

QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    padding: 0 5px;
    color: #2196F3; /* Blue Accent - ISA-101 Info */
    font-weight: bold;
    font-size: 13px;
    background-color: #1E1E1E;
}

QFrame {
    border: none;
    background: transparent;
}

/* =================================================================================
   BUTTONS
   ================================================================================= */
QPushButton {
    background-color: #333333;
    border: 1px solid #444444;
    border-radius: 4px;
    padding: 8px 16px;
    color: #FFFFFF;
    font-weight: 500;
}

QPushButton:hover {
    background-color: #444444;
    border-color: #2196F3; /* Blue glow on hover */
}

QPushButton:pressed {
    background-color: #2196F3;
    color: #FFFFFF;
    border-color: #2196F3;
}

QPushButton:disabled {
    background-color: #2D2D2D;
    border-color: #333333;
    color: #666666;
}

/* Role: Primary (e.g., Connect, Move, Home All) */
QPushButton[role="primary"] {
    background-color: #007ACC; /* Vivid Blue */
    border: 1px solid #007ACC;
}
QPushButton[role="primary"]:hover {
    background-color: #008BE6;
    border-color: #008BE6;
}
QPushButton[role="primary"]:pressed {
    background-color: #005F9E;
}

/* Role: Danger (e.g., Stop, E-Stop) */
QPushButton[role="danger"] {
    background-color: #D32F2F;
    border: 2px solid #B71C1C;
    color: #FFFFFF;
    font-weight: bold;
}
QPushButton[role="danger"]:hover {
    background-color: #E53935;
    border-color: #D32F2F;
}
QPushButton[role="danger"]:pressed {
    background-color: #B71C1C;
}

/* Role: Warning (e.g., Pause, Disconnect) */
QPushButton[role="warning"] {
    background-color: #2D2D2D;
    border: 1px solid #FF9800; /* Orange border */
    color: #FF9800;
}
QPushButton[role="warning"]:hover {
    background-color: #FF9800;
    color: #000000;
}

/* =================================================================================
   INPUTS (Text, SpinBox)
   ================================================================================= */
QLineEdit, QSpinBox, QDoubleSpinBox {
    background-color: #1E1E1E;
    border: 1px solid #3D3D3D;
    border-radius: 4px;
    padding: 6px;
    color: #2196F3; /* Blue Text - ISA-101 Info */
    font-family: "Consolas", "Courier New", monospace;
    font-weight: bold;
    selection-background-color: #2196F3;
}

QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border: 2px solid #2196F3;
    background-color: #252526;
}

/* SpinBox Buttons */
QSpinBox::up-button, QDoubleSpinBox::up-button,
QSpinBox::down-button, QDoubleSpinBox::down-button {
    background: #333333;
    border: none;
    border-radius: 2px;
    width: 16px;
}
QSpinBox::up-button:hover, QDoubleSpinBox::up-button:hover,
QSpinBox::down-button:hover, QDoubleSpinBox::down-button:hover {
    background: #444444;
}

    /* === PRODUCTION SIDEBAR STYLES (Scheme A: Card-Based) === */

    /* Sidebar Container */
    QWidget[data-testid="tab-production"] > QWidget {
        background-color: #1e1e1e; /* Darker background for the sidebar base */
    }

    /* Card Containers */
    QFrame[class="sidebar-card"] {
        background-color: #2d2d2d;
        border: 1px solid #3d3d3d;
        border-radius: 8px;
    }
    
    /* Input Fields in Sidebar */
    QFrame[class="sidebar-card"] QLineEdit, 
    QFrame[class="sidebar-card"] QSpinBox, 
    QFrame[class="sidebar-card"] QDoubleSpinBox {
        background-color: #1a1a1a;
        border: 1px solid #444;
        border-radius: 4px;
        padding: 4px;
        color: #ddd;
        selection-background-color: #007acc;
    }
    QFrame[class="sidebar-card"] QLineEdit:focus,
    QFrame[class="sidebar-card"] QSpinBox:focus,
    QFrame[class="sidebar-card"] QDoubleSpinBox:focus {
        border: 1px solid #007acc;
    }

    /* Buttons in Sidebar (Standard) */
    QFrame[class="sidebar-card"] QPushButton {
        background-color: #3a3a3a;
        border: 1px solid #555;
        border-radius: 4px;
        color: #eee;
        padding: 5px 10px;
    }
    QFrame[class="sidebar-card"] QPushButton:hover {
        background-color: #4a4a4a;
        border-color: #666;
    }
    QFrame[class="sidebar-card"] QPushButton:pressed {
        background-color: #2a2a2a;
        border-color: #333;
    }
    QFrame[class="sidebar-card"] QPushButton[role="danger"] {
        background-color: #5a1a1a;
        border-color: #8a2a2a;
        color: #ffaaaa;
    }
    QFrame[class="sidebar-card"] QPushButton[role="danger"]:hover {
        background-color: #7a2a2a;
    }
    QFrame[class="sidebar-card"] QPushButton[role="danger"]:pressed {
        background-color: #4a1010;
    }

    /* HERO BUTTON (Start Production) */
    QPushButton[role="production-start"] {
        background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #4CAF50, stop:1 #388E3C);
        border: 1px solid #2E7D32;
        border-radius: 8px;
        color: white;
        font-weight: bold;
    }
    QPushButton[role="production-start"]:hover {
        background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #66BB6A, stop:1 #43A047);
        border: 1px solid #43A047;
    }
    QPushButton[role="production-start"]:pressed {
        background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #2E7D32, stop:1 #1B5E20);
        padding-top: 2px; /* Physical press effect */
    }
    
    /* HERO BUTTON (Dry Run Mode - Warning Color) */
    QPushButton[role="warning"] {
         background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFB74D, stop:1 #F57C00);
         border: 1px solid #EF6C00;
         color: #333; /* Dark text for contrast on orange */
    }
    QPushButton[role="warning"]:hover {
         background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #FFCC80, stop:1 #FB8C00);
    }
    QPushButton[role="warning"]:pressed {
         background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #F57C00, stop:1 #E65100);
    }


    /* Custom Slider */
    QSlider::groove:horizontal {
        border: 1px solid #3d3d3d;
        height: 6px;
        background: #1a1a1a;
        margin: 2px 0;
        border-radius: 3px;
    }
    QSlider::handle:horizontal {
        background: #007acc;
        border: 1px solid #007acc;
        width: 16px;
        height: 16px;
        margin: -6px 0; /* center on groove */
        border-radius: 8px;
    }
    QSlider::handle:horizontal:hover {
        background: #1a9cff;
        border-color: #1a9cff;
    }
    QSlider::handle:horizontal:pressed {
        background: #005c99;
        border-color: #005c99;
    }
    QSlider::sub-page:horizontal {
        background: #3d3d3d; /* Dark fill for left side if needed, or leave transparent for minimal look */
        border-radius: 3px;
    }

    /* Radio Buttons */
    QRadioButton {
        spacing: 8px;
        color: #ddd;
    }
    QRadioButton::indicator {
        width: 18px;
        height: 18px;
        border-radius: 10px;
        border: 2px solid #555;
        background: #2d2d2d;
    }
    QRadioButton::indicator:checked {
        border-color: #007acc;
        background: qradialgradient(cx:0.5, cy:0.5, radius:0.4, fx:0.5, fy:0.5, stop:0 #007acc, stop:1 #2d2d2d);
    }
    QRadioButton::indicator:hover {
        border-color: #777;
    }

    /* Progress Bar (Slim) */
    QProgressBar[data-testid="progress-completion"],
    QProgressBar[data-testid="progress-dxf"] {
        border: none;
        background-color: #1a1a1a;
        border-radius: 2px;
        text-align: center;
    }
    QProgressBar[data-testid="progress-completion"]::chunk,
    QProgressBar[data-testid="progress-dxf"]::chunk {
        background-color: #4CAF50;
        border-radius: 2px;
    }

    /* Labels & Text */
    QLabel {
        color: #ccc;
    }
    QLabel[class="header-text"] {
        font-weight: bold;
        color: #fff;
        font-size: 14px;
    }
    QLabel[class="sub-text"] {
        color: #888;
        font-size: 12px;
    }
    
    /* Stats Numbers */
    QLabel[class="stat-value-large"] {
        font-family: "Segoe UI", "Roboto", sans-serif;
        font-size: 28px;
        font-weight: bold;
        color: #4CAF50; /* Green */
    }
    QLabel[class="stat-value-time"] {
        font-family: "Consolas", "Monospace";
        font-weight: bold;
        color: #ddd;
    }



/* =================================================================================
   TABS
   ================================================================================= */
QTabWidget::pane {
    border: 1px solid #3D3D3D;
    background: #252526;
    border-radius: 4px;
    top: -1px; /* Overlap with tab bar */
}

QTabWidget::tab-bar {
    alignment: center;
}

QTabBar::tab {
    background: #1E1E1E;
    border: 1px solid #3D3D3D;
    color: #888888;
    padding: 10px 24px;
    margin-right: 4px;
    border-top-left-radius: 4px;
    border-top-right-radius: 4px;
    min-width: 80px;
}

QTabBar::tab:hover {
    background: #252526;
    color: #CCCCCC;
}

QTabBar::tab:selected {
    background: #252526;
    border-bottom-color: #252526; /* Blend with pane */
    color: #2196F3;
    font-weight: bold;
    border-top: 2px solid #2196F3; /* Top accent line */
}

/* =================================================================================
   LABELS & READOUTS
   ================================================================================= */
QLabel {
    background: transparent;
    color: #E0E0E0;
    border: none;
}

/* Specific Role for Digital Readouts (e.g. Position) */
QLabel[role="readout"] {
    background-color: #000000;
    border: 1px solid #333333;
    border-radius: 4px;
    color: #00ADB5;
    font-family: "Consolas", "Courier New", monospace;
    font-size: 24px;
    font-weight: bold;
    padding: 6px 10px;
    qproperty-alignment: AlignRight;
}

/* Header Labels (Table Headers) */
QLabel[role="header"] {
    font-weight: bold;
    color: #888888;
    font-size: 13px;
}

/* Axis Labels (X, Y, Z titles) */
QLabel[role="axis_title"] {
    font-weight: bold;
    font-size: 16px;
    color: #E0E0E0;
}

/* Status Frames (Valve status background) */
QFrame[class="status-frame"] {
    background-color: #333337;
    border-radius: 4px;
    border: 1px solid #3E3E42;
}

/* Dynamic State Colors (ON/OFF) */
QLabel[state="on"] {
    font-weight: bold;
    color: #00BCD4; /* Cyan for ON */
}
QLabel[state="off"] {
    font-weight: bold;
    color: #666666; /* Dim Grey for OFF */
}

/* Axis Enabled/Homed Indicators */
QLabel[indicator="true"] {
    color: #4CAF50; /* Green */
    font-weight: bold;
}
QLabel[indicator="false"] {
    color: #444444; /* Dark Grey */
}

/* Status Bar */
QStatusBar {
    background-color: #007ACC; /* Blue strip at bottom */
    color: #FFFFFF;
    font-weight: bold;
}

/* =================================================================================
   STATUS PANEL STYLES
   ================================================================================= */

/* Table Header Labels */
QLabel[role="table-header"] {
    font-weight: bold;
    color: #888888;
}

/* Axis Name Labels */
QLabel[role="axis-name"] {
    font-weight: bold;
    font-size: 16px;
}

/* Status Card Frame */
QFrame[role="status-card"] {
    background-color: #333337;
    border-radius: 4px;
    padding: 10px;
}

/* Status Value Label (e.g., machine state) */
QLabel[role="status-value"] {
    font-weight: bold;
    color: #ffffff;
}

/* Valve Status - ON */
QLabel[role="valve-on"] {
    font-weight: bold;
    color: #4CAF50;
}

/* Valve Status - OFF */
QLabel[role="valve-off"] {
    font-weight: bold;
    color: #aaaaaa;
}

/* Axis Status - Enabled/Homed */
QLabel[role="axis-active"] {
    color: #4CAF50;
}

QLabel[role="axis-inactive"] {
    color: #888888;
}

/* =================================================================================
   LED INDICATORS - ISA-101 Compliant
   ================================================================================= */
QLabel[role="led-off"] {
    background-color: #333333;
    border: 1px solid #555555;
    border-radius: 8px;
}

QLabel[role="led-green"] {
    background-color: #4CAF50;
    border: 1px solid #4CAF50;
    border-radius: 8px;
}

QLabel[role="led-red"] {
    background-color: #FF5722;
    border: 1px solid #FF5722;
    border-radius: 8px;
}

QLabel[role="led-yellow"] {
    background-color: #FFC107;
    border: 1px solid #FFC107;
    border-radius: 8px;
}

QLabel[role="led-blue"] {
    background-color: #2196F3;
    border: 1px solid #2196F3;
    border-radius: 8px;
}

/* =================================================================================
   PROGRESS BAR
   ================================================================================= */
QProgressBar {
    border: 1px solid #3D3D3D;
    border-radius: 4px;
    background-color: #1E1E1E;
    text-align: center;
    color: #FFFFFF;
    font-weight: bold;
}

QProgressBar::chunk {
    background-color: #4CAF50;
    border-radius: 3px;
}

/* =================================================================================
   LIST WIDGET (Alarms)
   ================================================================================= */
QListWidget {
    background-color: #1E1E1E;
    border: 1px solid #3D3D3D;
    border-radius: 4px;
    color: #E0E0E0;
}

QListWidget::item {
    padding: 8px;
    border-bottom: 1px solid #333333;
}

QListWidget::item:selected {
    background-color: #2196F3;
    color: #FFFFFF;
}

/* =================================================================================
   PRODUCTION TAB - Large Buttons
   ================================================================================= */
QPushButton[role="production-start"] {
    background-color: #4CAF50;
    border: none;
    border-radius: 8px;
    color: #FFFFFF;
    font-size: 18px;
    font-weight: bold;
    min-height: 60px;
}

QPushButton[role="production-start"]:hover {
    background-color: #66BB6A;
}

QPushButton[role="production-start"]:pressed {
    background-color: #388E3C;
}

QPushButton[role="production-pause"] {
    background-color: #FFC107;
    border: none;
    border-radius: 8px;
    color: #000000;
    font-size: 18px;
    font-weight: bold;
    min-height: 60px;
}

QPushButton[role="production-pause"]:hover {
    background-color: #FFD54F;
}

QPushButton[role="production-stop"] {
    background-color: #FF5722;
    border: none;
    border-radius: 8px;
    color: #FFFFFF;
    font-size: 18px;
    font-weight: bold;
    min-height: 60px;
}

QPushButton[role="production-stop"]:hover {
    background-color: #FF7043;
}

QPushButton[role="production-dryrun"] {
    background-color: #FF9800;
    border: none;
    border-radius: 8px;
    color: #333333;
    font-size: 18px;
    font-weight: bold;
    min-height: 60px;
}

QPushButton[role="production-dryrun"]:hover {
    background-color: #FFB74D;
}

QPushButton[role="production-dryrun"]:pressed {
    background-color: #F57C00;
}

/* =================================================================================
   STATISTICS PANEL
   ================================================================================= */
QLabel[role="stat-value"] {
    font-family: "Consolas", "Courier New", monospace;
    font-size: 32px;
    font-weight: bold;
    color: #2196F3;
}

QLabel[role="stat-label"] {
    font-size: 12px;
    color: #888888;
}

/* =================================================================================
   GLOBAL STATUS BAR (Top)
   ================================================================================= */
QFrame[role="global-status"] {
    background-color: #252526;
    border: 1px solid #3D3D3D;
    border-radius: 4px;
    padding: 8px;
}

QLabel[role="operation-status"] {
    font-weight: bold;
    color: #4CAF50;
}
"""