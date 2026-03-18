# HMI 自动启动后端方案

> Created: 2026-01-23
> Status: planning
> Owner: @Xike

## 1. 需求概述

### 1.1 目标

HMI 适配器启动后自动完成以下流程：
1. 启动后端 TCP Server 进程
2. 建立 TCP 连接
3. 发送硬件初始化命令

HMI 关闭时同时终止后端进程。

### 1.2 当前流程 vs 目标流程

```
当前流程:
用户手动启动后端 → 用户手动启动 HMI → 点击"连接" → 点击"初始化硬件"

目标流程:
用户启动 HMI → 自动完成所有连接 → 界面就绪
```

## 2. 架构分析

### 2.1 六边形架构定位

根据 `.claude/rules/HEXAGONAL.md` 和现有设计文档：

```
HMI (Python)                    后端 (C++)
┌─────────────────────┐        ┌─────────────────────────────┐
│ Driving Adapter     │  TCP   │ Driving Adapter             │
│                     │◀──────▶│ (TCP Server)                │
│ ┌─────────────────┐ │        │                             │
│ │ BackendManager  │─┼────────┼──▶ siligen_tcp_server.exe   │
│ │ (新增)          │ │        │                             │
│ └─────────────────┘ │        └─────────────────────────────┘
│                     │
│ ┌─────────────────┐ │
│ │ TcpClient       │ │
│ └─────────────────┘ │
│                     │
│ ┌─────────────────┐ │
│ │ CommandProtocol │ │
│ └─────────────────┘ │
│                     │
│ ┌─────────────────┐ │
│ │ MainWindow (UI) │ │
│ └─────────────────┘ │
└─────────────────────┘
```

### 2.2 架构合规性

| 规则 | 要求 | 本方案 | 状态 |
|------|------|--------|:----:|
| HEXAGONAL_DEPENDENCY_DIRECTION | adapter -> application | BackendManager 管理外部进程，不涉及 Domain | 合规 |
| HEXAGONAL_LAYER_ORDER | adapter 不反向依赖 | BackendManager 仅管理进程生命周期 | 合规 |
| ADAPTER_NO_BYPASS_USECASE | 不绕过 UseCase | 仍通过 TCP 协议调用 UseCase | 合规 |

### 2.3 HMI 内部分层

```
src/adapters/hmi/
├── client/                    # 通信层 (类似 Infrastructure)
│   ├── tcp_client.py          # TCP 客户端
│   ├── protocol.py            # 命令协议
│   ├── backend_manager.py     # 新增: 后端进程管理
│   └── __init__.py
├── ui/                        # 界面层 (类似 Adapter)
│   ├── main_window.py         # 主窗口
│   └── ...
└── main.py                    # 入口
```

**设计原则**：
- `BackendManager` 放在 `client/` 目录，因为它属于基础设施/通信层
- `MainWindow` 通过组合使用 `BackendManager` 和 `TcpClient`

## 3. 详细设计

### 3.1 新增组件: BackendManager

```python
# client/backend_manager.py
"""后端进程管理器"""
import subprocess
import socket
import time
import atexit
from pathlib import Path
from typing import Optional


class BackendManager:
    """管理后端 TCP Server 进程的生命周期"""

    # 硬编码参数
    DEFAULT_EXE_PATH = "./build/bin/Release/siligen_tcp_server.exe"
    DEFAULT_HOST = "127.0.0.1"
    DEFAULT_PORT = 9527
    STARTUP_TIMEOUT = 5.0  # 秒
    HEALTH_CHECK_INTERVAL = 0.2  # 秒

    def __init__(
        self,
        exe_path: str = DEFAULT_EXE_PATH,
        host: str = DEFAULT_HOST,
        port: int = DEFAULT_PORT
    ):
        self._exe_path = Path(exe_path)
        self._host = host
        self._port = port
        self._process: Optional[subprocess.Popen] = None

        # 注册退出时清理
        atexit.register(self.stop)

    @property
    def is_running(self) -> bool:
        """检查后端进程是否在运行"""
        return self._process is not None and self._process.poll() is None

    def start(self) -> tuple[bool, str]:
        """
        启动后端进程

        Returns:
            (success, message)
        """
        # 检查可执行文件
        if not self._exe_path.exists():
            return False, f"后端程序不存在: {self._exe_path}"

        # 检查端口是否已被占用（可能已有实例运行）
        if self._is_port_in_use():
            return True, "后端已在运行"

        # 启动进程
        try:
            self._process = subprocess.Popen(
                [str(self._exe_path)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                creationflags=subprocess.CREATE_NO_WINDOW  # Windows: 无窗口
            )
        except Exception as e:
            return False, f"启动失败: {e}"

        return True, "后端进程已启动"

    def wait_ready(self, timeout: float = STARTUP_TIMEOUT) -> tuple[bool, str]:
        """
        等待后端就绪（端口可连接）

        Args:
            timeout: 超时时间（秒）

        Returns:
            (success, message)
        """
        start_time = time.time()

        while time.time() - start_time < timeout:
            # 检查进程是否异常退出
            if self._process and self._process.poll() is not None:
                return False, f"后端进程异常退出，返回码: {self._process.returncode}"

            # 尝试连接端口
            if self._is_port_in_use():
                return True, "后端已就绪"

            time.sleep(self.HEALTH_CHECK_INTERVAL)

        return False, f"后端启动超时 ({timeout}s)"

    def stop(self) -> None:
        """停止后端进程"""
        if self._process:
            try:
                self._process.terminate()
                self._process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                self._process.kill()
            except Exception:
                pass
            finally:
                self._process = None

    def _is_port_in_use(self) -> bool:
        """检查端口是否被占用"""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(0.5)
                s.connect((self._host, self._port))
                return True
        except (socket.timeout, ConnectionRefusedError, OSError):
            return False
```

### 3.2 启动流程类

```python
# client/startup_sequence.py
"""启动序列管理"""
from dataclasses import dataclass
from typing import Callable, Optional
from PyQt5.QtCore import QObject, pyqtSignal, QThread

from .backend_manager import BackendManager
from .tcp_client import TcpClient
from .protocol import CommandProtocol


@dataclass
class StartupResult:
    """启动结果"""
    success: bool
    stage: str  # "backend" | "tcp" | "hardware"
    message: str


class StartupWorker(QThread):
    """后台启动线程"""

    progress = pyqtSignal(str, int)  # (message, percent)
    finished = pyqtSignal(StartupResult)

    def __init__(
        self,
        backend: BackendManager,
        client: TcpClient,
        protocol: CommandProtocol
    ):
        super().__init__()
        self._backend = backend
        self._client = client
        self._protocol = protocol

    def run(self):
        # Stage 1: 启动后端 (0-30%)
        self.progress.emit("正在启动后端...", 10)
        ok, msg = self._backend.start()
        if not ok:
            self.finished.emit(StartupResult(False, "backend", msg))
            return

        self.progress.emit("等待后端就绪...", 20)
        ok, msg = self._backend.wait_ready()
        if not ok:
            self.finished.emit(StartupResult(False, "backend", msg))
            return
        self.progress.emit("后端已就绪", 30)

        # Stage 2: 连接 TCP (30-60%)
        self.progress.emit("正在连接 TCP...", 40)
        if not self._client.connect():
            self.finished.emit(StartupResult(False, "tcp", "TCP 连接失败"))
            return
        self.progress.emit("TCP 已连接", 60)

        # Stage 3: 初始化硬件 (60-100%)
        self.progress.emit("正在初始化硬件...", 70)
        ok, msg = self._protocol.connect_hardware()
        if not ok:
            self.finished.emit(StartupResult(False, "hardware", msg))
            return

        self.progress.emit("硬件已就绪", 100)
        self.finished.emit(StartupResult(True, "complete", "启动完成"))
```

### 3.3 MainWindow 修改

```python
# ui/main_window.py 修改要点

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        # ... 现有初始化 ...

        # 新增: 后端管理器
        self._backend = BackendManager()

        # 修改: 启动时自动连接
        self._auto_startup()

    def _auto_startup(self):
        """自动启动序列"""
        self._startup_worker = StartupWorker(
            self._backend,
            self._client,
            self._protocol
        )
        self._startup_worker.progress.connect(self._on_startup_progress)
        self._startup_worker.finished.connect(self._on_startup_finished)
        self._startup_worker.start()

    def _on_startup_progress(self, message: str, percent: int):
        """启动进度回调"""
        self.statusBar().showMessage(message)
        # 可选: 更新进度条

    def _on_startup_finished(self, result: StartupResult):
        """启动完成回调"""
        if result.success:
            self._connected = True
            self._hw_connected = True
            self._update_led(self._tcp_led, "green")
            self._update_led(self._hw_led, "green")
            self._connect_btn.setEnabled(False)
            self._disconnect_btn.setEnabled(True)
            self.statusBar().showMessage("系统就绪")
        else:
            # 显示错误对话框
            self._show_startup_error(result)

    def _show_startup_error(self, result: StartupResult):
        """显示启动错误"""
        stage_names = {
            "backend": "后端启动",
            "tcp": "TCP 连接",
            "hardware": "硬件初始化"
        }
        stage_name = stage_names.get(result.stage, result.stage)

        QMessageBox.critical(
            self,
            "启动失败",
            f"{stage_name}失败:\n{result.message}\n\n"
            "请检查:\n"
            "1. 后端程序是否存在\n"
            "2. 端口是否被占用\n"
            "3. 硬件是否正确连接"
        )

    def closeEvent(self, event):
        """窗口关闭时清理"""
        self._timer.stop()
        self._client.disconnect()

        # 新增: 关闭后端
        self._backend.stop()

        event.accept()
```

### 3.4 __init__.py 更新

```python
# client/__init__.py
from .tcp_client import TcpClient
from .protocol import CommandProtocol, MachineStatus, AxisStatus
from .auth import AuthManager, User
from .recipe import Recipe, RecipeManager
from .backend_manager import BackendManager  # 新增

__all__ = [
    "TcpClient",
    "CommandProtocol",
    "MachineStatus",
    "AxisStatus",
    "AuthManager",
    "User",
    "Recipe",
    "RecipeManager",
    "BackendManager",  # 新增
]
```

## 4. 文件变更清单

### 4.1 新增文件

| 文件 | 代码量 | 说明 |
|------|--------|------|
| `client/backend_manager.py` | ~80 行 | 后端进程管理器 |
| `client/startup_sequence.py` | ~70 行 | 启动序列管理 |

### 4.2 修改文件

| 文件 | 修改内容 |
|------|----------|
| `client/__init__.py` | 导出 `BackendManager` |
| `ui/main_window.py` | 集成自动启动逻辑，修改 `closeEvent` |

### 4.3 代码量统计

| 类型 | 行数 |
|------|------|
| 新增代码 | ~150 行 |
| 修改代码 | ~50 行 |
| **总计** | **~200 行** |

## 5. 错误处理策略

### 5.1 启动失败处理

| 失败阶段 | 处理方式 |
|----------|----------|
| 后端不存在 | 显示错误对话框，提示检查路径 |
| 启动超时 | 显示错误对话框，提示检查日志 |
| TCP 连接失败 | 显示错误对话框，允许手动重试 |
| 硬件初始化失败 | 显示错误对话框，允许手动重试 |

### 5.2 运行时错误处理

| 错误类型 | 处理方式 |
|----------|----------|
| 后端进程崩溃 | 检测到连接断开，提示用户，可选自动重启 |
| TCP 连接断开 | 显示连接断开提示，LED 变红 |

## 6. 保留手动控制

为了调试目的，保留手动连接按钮的功能：

- "连接"按钮：当自动启动失败时可用于手动连接
- "断开"按钮：允许手动断开连接
- "初始化硬件"按钮：允许手动重新初始化

自动启动成功后：
- "连接"按钮禁用
- "断开"按钮启用
- "初始化硬件"按钮禁用（已完成）

## 7. 实施步骤

### Step 1: 创建 BackendManager

1. 创建 `client/backend_manager.py`
2. 实现进程启动、等待就绪、停止功能
3. 更新 `client/__init__.py`

### Step 2: 创建 StartupWorker

1. 创建 `client/startup_sequence.py`
2. 实现三阶段启动逻辑
3. 添加进度信号

### Step 3: 修改 MainWindow

1. 添加 `_backend` 成员
2. 添加 `_auto_startup()` 方法
3. 添加进度和完成回调
4. 修改 `closeEvent()` 关闭后端

### Step 4: 测试验证

1. 正常启动流程
2. 后端已运行场景
3. 启动失败场景
4. 关闭时清理验证

## 8. 约束条件

### 8.1 硬编码参数

```python
# 在 BackendManager 类中定义
DEFAULT_EXE_PATH = "./build/bin/Release/siligen_tcp_server.exe"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 9527
STARTUP_TIMEOUT = 5.0
```

### 8.2 Windows 特定处理

```python
# subprocess 创建标志
creationflags=subprocess.CREATE_NO_WINDOW  # 无控制台窗口
```

### 8.3 进程清理

- 使用 `atexit.register()` 确保异常退出时也能清理
- `closeEvent()` 中显式调用 `stop()`

## 9. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|----------|
| 后端启动慢 | 用户等待 | 显示进度条，设置合理超时 |
| 端口冲突 | 启动失败 | 检测端口占用，复用已有实例 |
| 僵尸进程 | 资源泄漏 | atexit + closeEvent 双重保障 |
| 权限问题 | 无法启动 | 捕获异常，友好提示 |

## 10. 架构合规声明

本方案符合以下项目规则：

- **HEXAGONAL.md**: BackendManager 作为 HMI 适配器的内部组件，不违反依赖方向
- **PORTS_ADAPTERS.md**: 不涉及 Port 接口修改
- **FORBIDDEN.md**: 不在根目录创建文件
- **NAMESPACE.md**: Python 项目不受 C++ 命名空间规则约束

## 11. 待确认事项

1. ~~后端可执行文件路径~~ -> `./build/bin/Release/siligen_tcp_server.exe`
2. ~~HMI 关闭时是否关闭后端~~ -> 是
3. ~~配置方式~~ -> 硬编码

---

**状态**: 等待实施确认
