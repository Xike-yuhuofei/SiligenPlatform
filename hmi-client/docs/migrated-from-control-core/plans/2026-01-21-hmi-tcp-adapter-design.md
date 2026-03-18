# HMI TCP适配器方案设计

> 创建日期: 2026-01-21
> 状态: 待实施

## 1. 背景与需求

### 1.1 需求概述

为siligen-motion-controller项目开发简易HMI界面，用于：
- 开发调试：参数调试、状态监控、手动控制
- 生产操作：日常操作界面

### 1.2 核心功能

| 功能 | 说明 |
|------|------|
| 状态监控 | 轴位置、速度、IO状态、报警信息 |
| 手动控制 | 点动、回零、参数设置 |
| 轨迹执行 | DXF文件加载、轨迹预览、点胶执行 |

### 1.3 设计约束

- 对现有代码侵入小
- 开发迅速
- 轻便部署

### 1.4 范围与职责边界

- HMI适配器仅负责协议转换与UI交互，不承载权限/配方/追溯等业务逻辑
- 业务校验（如DXF路径白名单、文件格式验证）由Application/Domain层UseCase负责，适配器仅做基本格式检查
- 硬件安全回路（E-stop/门禁/限位）独立于软件，软件`estop`仅触发硬件急停接口
- 机台状态机由Application/Domain层输出，适配器通过`status`响应透传显示

## 2. 方案选型

### 2.1 候选方案对比

| 方案 | 侵入性 | 开发速度 | 轻便性 | 架构质量 |
|------|:------:|:--------:|:------:|:--------:|
| Web界面 (HTTP+前端) | 中 | 中 | 差 | 好 |
| Qt桌面 | 中 | 慢 | 差 | 好 |
| ImGui嵌入 | 中 | 快 | 好 | 中 |
| Python + CLI封装 | 小 | 快 | 好 | **差** |
| **Python + TCP** | 小 | 快 | 好 | **好** |

### 2.2 选定方案

**Python + TCP适配器**

理由：
1. 符合六边形架构：TCP适配器作为新的Driving Adapter，与CLI并列
2. 侵入性可控：只需添加一个新适配器，不修改现有代码
3. 复用现有逻辑：直接调用ApplicationContainer
4. 架构一致性：Python GUI和CLI是平等的客户端

### 2.3 为什么不选CLI封装

CLI封装虽然代码量最少，但存在架构问题：
- 用一个用户界面适配器(Python GUI)调用另一个用户界面适配器(CLI)
- 违反六边形架构的设计意图
- 依赖CLI的文本输出格式，契约脆弱

## 3. 整体架构

```
+---------------------------------------------------------------------+
|                         Python HMI                                  |
|  +-------------+  +-------------+  +-----------------------+        |
|  |  状态监控   |  |  手动控制   |  |     点胶执行          |        |
|  +------+------+  +------+------+  +-----------+-----------+        |
|         +----------------+---------------------+                    |
|                          v                                          |
|                +-----------------+                                  |
|                |   HMI Client    |                                  |
|                |  (TCP + JSON)   |                                  |
|                +--------+--------+                                  |
+-------------------------+-----------------------------------------+
                          | TCP :9000
+--------------------------+------------------------------------------+
|                          v                          C++ Backend     |
|  +----------------------------------------------------------+      |
|  |                    TCP Adapter (新增)                     |      |
|  |  +------------+  +------------+  +--------------------+  |      |
|  |  | TCP Server |  | JSON Parser|  | Command Dispatcher |  |      |
|  |  | (Asio)     |  | (nlohmann) |  | (依赖UseCase接口)  |  |      |
|  |  +-----+------+  +-----+------+  +---------+----------+  |      |
|  +--------+---------------+-------------------+--------------+      |
|           +---------------+-------------------+                     |
|                           v                                         |
|  +----------------------------------------------------------+      |
|  |              UseCase Interfaces (Application Layer)       |      |
|  |  IInitializeSystem | IHomeAxes | IMoveToPosition | ...   |      |
|  +----------------------------------------------------------+      |
|                           ^                                         |
|                           | (main.cpp装配)                          |
|  +----------------------------------------------------------+      |
|  |              ApplicationContainer (Bootstrap层)           |      |
|  |                    解析并注入UseCase实现                   |      |
|  +----------------------------------------------------------+      |
+---------------------------------------------------------------------+
```

**依赖说明**：
- TCP Adapter只依赖UseCase接口，不直接依赖ApplicationContainer
- ApplicationContainer仅在main.cpp（Bootstrap层）中使用，负责装配
- JSON转换严格限制在TCP Adapter内部，不渗透到Application层

## 4. 通信协议设计

### 4.1 协议格式

- 传输层：TCP
- 数据格式：JSON
- 消息分隔：`\n`换行符
- 协议版本：每个请求/响应包含`version`字段，当前为`1.0`

### 4.2 请求格式

```json
{
  "id": "uuid-1234",
  "version": "1.0",
  "method": "home",
  "params": {"axes": ["X", "Y", "Z"]}
}
```

### 4.3 响应格式

成功响应：
```json
{
  "id": "uuid-1234",
  "version": "1.0",
  "success": true,
  "result": {"message": "Homing completed"}
}
```

错误响应：
```json
{
  "id": "uuid-1234",
  "version": "1.0",
  "success": false,
  "error": {"code": 1001, "message": "Axis X not enabled"}
}
```

### 4.4 支持的方法

| 方法 | 参数 | 说明 |
|------|------|------|
| `connect` | `{card_ip, pc_ip}` | 连接硬件 |
| `disconnect` | `{}` | 断开连接 |
| `ping` | `{}` | 心跳探测，服务端返回`pong` |
| `status` | `{}` | 获取状态 |
| `home` | `{axes: []}` | 轴回零 |
| `jog` | `{axis, direction, speed}` | 点动 |
| `move` | `{x, y, z, u, speed}` | 移动到位置 |
| `stop` | `{}` | 停止运动 |
| `estop` | `{}` | 急停 |
| `dispenser.start` | `{}` | 开始点胶 |
| `dispenser.stop` | `{}` | 停止点胶 |
| `dxf.load` | `{file_path}` | 加载DXF |
| `dxf.execute` | `{}` | 执行点胶 |

### 4.5 状态响应示例

```json
{
  "id": "uuid-5678",
  "version": "1.0",
  "success": true,
  "result": {
    "connected": true,
    "machine_state": "Ready",
    "axes": {
      "X": {"position": 100.5, "velocity": 0, "enabled": true, "homed": true},
      "Y": {"position": 200.3, "velocity": 0, "enabled": true, "homed": true},
      "Z": {"position": 50.0, "velocity": 0, "enabled": true, "homed": false},
      "U": {"position": 0, "velocity": 0, "enabled": false, "homed": false}
    },
    "io": {
      "limit_x_pos": false,
      "limit_x_neg": false,
      "limit_y_pos": false,
      "limit_y_neg": false,
      "emergency_stop": false
    },
    "dispenser": {
      "valve_open": false,
      "supply_open": false
    },
    "alarms": []
  }
}
```

### 4.6 心跳与超时机制

- 客户端定时发送`ping`，服务端返回`pong`响应
- 心跳间隔与超时阈值可配置，连续超时判定断线并触发自动重连

### 4.7 事件推送（长任务进度）

服务器可主动推送事件消息，用于长任务进度、报警、状态变化等通知：

```json
{
  "version": "1.0",
  "event": "dxf.execute.progress",
  "data": {
    "request_id": "uuid-9012",
    "progress": 0.42,
    "message": "Executing trajectory"
  }
}
```

## 5. C++端TCP适配器设计

### 5.1 文件结构

```
src/adapters/tcp/
├── CMakeLists.txt
├── TcpServer.h              # TCP服务器（Boost.Asio）
├── TcpServer.cpp
├── TcpSession.h             # 单个连接会话
├── TcpSession.cpp
├── JsonProtocol.h           # JSON协议解析
├── JsonProtocol.cpp
├── TcpCommandDispatcher.h   # 命令分发到UseCase
└── TcpCommandDispatcher.cpp
```

### 5.2 核心类设计

#### 设计原则

**依赖规则**：
- TCP适配器只依赖UseCase接口，不依赖ApplicationContainer
- 装配逻辑在main.cpp（bootstrap层）完成
- JSON转换严格限制在适配器内部，不渗透到Application层

#### TcpServer

```cpp
// TcpServer.h
#pragma once
#include <boost/asio.hpp>

namespace siligen::adapters::tcp {

class TcpCommandDispatcher;  // 前向声明

class TcpServer {
public:
    TcpServer(boost::asio::io_context& io_context,
              uint16_t port,
              TcpCommandDispatcher& dispatcher);  // 依赖Dispatcher，而非Container

    void start();
    void stop();

private:
    void do_accept();

    boost::asio::ip::tcp::acceptor acceptor_;
    TcpCommandDispatcher& dispatcher_;
};

} // namespace siligen::adapters::tcp
```

#### TcpCommandDispatcher

```cpp
// TcpCommandDispatcher.h
#pragma once
#include <nlohmann/json.hpp>
// 只依赖UseCase接口，不依赖ApplicationContainer
#include "application/usecases/system/IInitializeSystemUseCase.h"
#include "application/usecases/system/IGetSystemStatusUseCase.h"
#include "application/usecases/motion/homing/IHomeAxesUseCase.h"
#include "application/usecases/motion/manual/IManualMotionControlUseCase.h"
#include "application/usecases/motion/positioning/IMoveToPositionUseCase.h"
#include "application/usecases/system/IEmergencyStopUseCase.h"

namespace siligen::adapters::tcp {

class TcpCommandDispatcher {
public:
    // 构造函数接收UseCase接口引用，由main.cpp注入
    TcpCommandDispatcher(
        application::IInitializeSystemUseCase& initUseCase,
        application::IGetSystemStatusUseCase& statusUseCase,
        application::IHomeAxesUseCase& homeUseCase,
        application::IManualMotionControlUseCase& jogUseCase,
        application::IMoveToPositionUseCase& moveUseCase,
        application::IEmergencyStopUseCase& estopUseCase
    );

    nlohmann::json dispatch(const nlohmann::json& request);

private:
    // JSON转换在适配器内部完成
    nlohmann::json handleConnect(const nlohmann::json& params);
    nlohmann::json handleStatus(const nlohmann::json& params);
    nlohmann::json handleHome(const nlohmann::json& params);
    nlohmann::json handleJog(const nlohmann::json& params);
    nlohmann::json handleMove(const nlohmann::json& params);
    nlohmann::json handleStop(const nlohmann::json& params);
    nlohmann::json handleEstop(const nlohmann::json& params);

    // DTO → JSON 转换辅助方法
    nlohmann::json toJson(const application::StatusResponse& response);
    nlohmann::json makeSuccessResponse(const std::string& id, const nlohmann::json& result);
    nlohmann::json makeErrorResponse(const std::string& id, int code, const std::string& message);

    // UseCase接口引用（由main.cpp注入）
    application::IInitializeSystemUseCase& initUseCase_;
    application::IGetSystemStatusUseCase& statusUseCase_;
    application::IHomeAxesUseCase& homeUseCase_;
    application::IManualMotionControlUseCase& jogUseCase_;
    application::IMoveToPositionUseCase& moveUseCase_;
    application::IEmergencyStopUseCase& estopUseCase_;
};

} // namespace siligen::adapters::tcp
```

#### Bootstrap层（main.cpp）

```cpp
// src/adapters/tcp/main.cpp - 装配在这里完成
#include <boost/asio.hpp>
#include "application/container/ApplicationContainer.h"
#include "TcpServer.h"
#include "TcpCommandDispatcher.h"

int main(int argc, char* argv[]) {
    // 1. 创建容器并装配（Container只在main中使用）
    auto container = siligen::application::ApplicationContainer("config.ini");
    container.Configure();

    // 2. 解析UseCase接口
    auto& initUseCase = container.Resolve<IInitializeSystemUseCase>();
    auto& statusUseCase = container.Resolve<IGetSystemStatusUseCase>();
    auto& homeUseCase = container.Resolve<IHomeAxesUseCase>();
    auto& jogUseCase = container.Resolve<IManualMotionControlUseCase>();
    auto& moveUseCase = container.Resolve<IMoveToPositionUseCase>();
    auto& estopUseCase = container.Resolve<IEmergencyStopUseCase>();

    // 3. 创建Dispatcher，注入UseCase接口
    auto dispatcher = siligen::adapters::tcp::TcpCommandDispatcher(
        initUseCase, statusUseCase, homeUseCase, jogUseCase, moveUseCase, estopUseCase
    );

    // 4. 创建TCP服务器
    boost::asio::io_context io_context;
    auto server = siligen::adapters::tcp::TcpServer(io_context, 9000, dispatcher);

    server.start();
    io_context.run();

    return 0;
}
```

#### JSON转换边界示例

```cpp
// TcpCommandDispatcher.cpp - JSON转换严格在适配器内部
nlohmann::json TcpCommandDispatcher::handleStatus(const nlohmann::json& params) {
    // 1. 调用UseCase，返回DTO（不含JSON）
    auto result = statusUseCase_.Execute();

    if (!result) {
        return makeErrorResponse(params["id"], result.error().code, result.error().message);
    }

    // 2. DTO → JSON 转换在适配器内部完成
    return makeSuccessResponse(params["id"], toJson(result.value()));
}

nlohmann::json TcpCommandDispatcher::toJson(const application::StatusResponse& response) {
    nlohmann::json axesJson;
    for (const auto& [name, axis] : response.axes) {
        axesJson[name] = {
            {"position", axis.position},
            {"velocity", axis.velocity},
            {"enabled", axis.enabled},
            {"homed", axis.homed}
        };
    }
    return {{"connected", response.connected}, {"axes", axesJson}};
}
```

### 5.3 代码量估算

| 模块 | 代码量 |
|------|--------|
| TcpServer | ~50行 |
| TcpSession | ~60行 |
| JsonProtocol | ~30行 |
| TcpCommandDispatcher | ~150行 |
| CMakeLists.txt | ~20行 |
| **总计** | **~310行** |

## 6. Python端HMI设计

### 6.1 文件结构

HMI放在`src/adapters/hmi/`下，与CLI和TCP适配器并列，保持架构一致性。

```
src/adapters/hmi/                # Python HMI适配器（与cli/tcp并列）
├── README.md                    # 说明这是Python项目，不由CMake管理
├── requirements.txt             # 依赖：PyQt5
├── main.py                      # 入口
├── client/
│   ├── __init__.py
│   ├── tcp_client.py            # TCP客户端
│   └── protocol.py              # JSON协议封装
├── ui/
│   ├── __init__.py
│   ├── main_window.py           # 主窗口
│   ├── status_panel.py          # 状态监控面板
│   ├── control_panel.py         # 手动控制面板
│   └── dispense_panel.py        # 点胶执行面板
└── resources/
    └── icons/                   # 图标资源
```

**注意**：`src/adapters/CMakeLists.txt`需要排除hmi目录：
```cmake
add_subdirectory(cli)
add_subdirectory(tcp)
# hmi/ 是Python项目，不由CMake管理
```

### 6.2 核心类设计

#### SiligenClient（异步架构）

采用独立读线程架构，避免UI阻塞，并为未来的事件推送预留通道。

```python
# client/tcp_client.py
import socket
import json
import threading
from uuid import uuid4
from typing import Any, Dict, Optional
from PyQt5.QtCore import QObject, pyqtSignal

class SiligenClient(QObject):
    """TCP客户端 - 异步架构"""

    # Qt信号，用于线程安全的UI通知
    response_received = pyqtSignal(dict)  # 响应信号
    event_received = pyqtSignal(dict)     # 事件推送信号（预留）
    connection_lost = pyqtSignal()        # 连接断开信号

    def __init__(self, host: str = 'localhost', port: int = 9000):
        super().__init__()
        self.host = host
        self.port = port
        self._socket: Optional[socket.socket] = None
        self._running = False
        self._read_thread: Optional[threading.Thread] = None

    def connect(self) -> bool:
        """连接到TCP服务器，启动读线程"""
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._socket.connect((self.host, self.port))
        self._running = True
        self._read_thread = threading.Thread(target=self._read_loop, daemon=True)
        self._read_thread.start()
        return True

    def disconnect(self) -> None:
        """断开连接"""
        self._running = False
        if self._socket:
            self._socket.close()
            self._socket = None

    def _read_loop(self) -> None:
        """独立读线程：接收所有服务器消息"""
        buffer = ''
        while self._running:
            try:
                data = self._socket.recv(4096).decode('utf-8')
                if not data:
                    self.connection_lost.emit()
                    break
                buffer += data
                while '\n' in buffer:
                    line, buffer = buffer.split('\n', 1)
                    msg = json.loads(line)
                    if 'event' in msg:
                        self.event_received.emit(msg)  # 服务器推送事件
                    else:
                        self.response_received.emit(msg)  # 请求响应
            except Exception:
                self.connection_lost.emit()
                break

    def send_request(self, method: str, params: Dict[str, Any]) -> str:
        """发送请求（非阻塞），返回请求ID"""
        if not self._socket:
            raise ConnectionError("Not connected to server")

        request_id = str(uuid4())
        request = {'id': request_id, 'version': '1.0', 'method': method, 'params': params}
        message = json.dumps(request) + '\n'
        self._socket.send(message.encode('utf-8'))
        return request_id

    # 便捷方法
    def connect_hardware(self, card_ip: str, pc_ip: str) -> str:
        return self.send_request('connect', {'card_ip': card_ip, 'pc_ip': pc_ip})

    def ping(self) -> str:
        return self.send_request('ping', {})

    def get_status(self) -> str:
        return self.send_request('status', {})

    def home(self, axes: list[str]) -> str:
        return self.send_request('home', {'axes': axes})

    def jog(self, axis: str, direction: int, speed: float) -> str:
        return self.send_request('jog', {'axis': axis, 'direction': direction, 'speed': speed})

    def move(self, x: float, y: float, z: float, u: float, speed: float) -> str:
        return self.send_request('move', {'x': x, 'y': y, 'z': z, 'u': u, 'speed': speed})

    def stop(self) -> str:
        return self.send_request('stop', {})

    def estop(self) -> str:
        return self.send_request('estop', {})
```

**架构说明**：
- UI线程：发送请求（非阻塞）
- 读线程：接收响应，通过Qt信号通知UI
- 好处：UI永不卡死，可接收服务器推送事件

### 6.3 代码量估算

| 模块 | 代码量 |
|------|--------|
| tcp_client.py | ~120行 |
| protocol.py | ~30行 |
| main_window.py | ~150行 |
| status_panel.py | ~120行 |
| control_panel.py | ~150行 |
| dispense_panel.py | ~100行 |
| main.py | ~30行 |
| **总计** | **~700行** |

## 7. 实施计划

### Phase 1: C++端TCP适配器 (4小时)

| 步骤 | 任务 | 产出 |
|------|------|------|
| 1.1 | 创建TcpServer (Boost.Asio) | TcpServer.h/cpp |
| 1.2 | 实现TcpSession | TcpSession.h/cpp |
| 1.3 | 实现JSON协议解析 | JsonProtocol.h/cpp |
| 1.4 | 实现CommandDispatcher | TcpCommandDispatcher.h/cpp |
| 1.5 | 集成到main.cpp | 添加--tcp启动参数 |
| 1.6 | 更新CMakeLists.txt | 构建配置 |

### Phase 2: Python端基础通信 (2小时)

| 步骤 | 任务 | 产出 |
|------|------|------|
| 2.1 | 实现TCP客户端 | tcp_client.py |
| 2.2 | 实现协议封装 | protocol.py |
| 2.3 | 命令行测试验证 | 测试脚本 |

### Phase 3: PyQt5界面 (6小时)

| 步骤 | 任务 | 产出 |
|------|------|------|
| 3.1 | 主窗口框架 | main_window.py |
| 3.2 | 状态监控面板 | status_panel.py |
| 3.3 | 手动控制面板 | control_panel.py |
| 3.4 | 点胶执行面板 | dispense_panel.py |

### Phase 4: 集成测试 (2小时)

| 步骤 | 任务 | 产出 |
|------|------|------|
| 4.1 | 端到端功能测试 | 测试报告 |
| 4.2 | 异常处理验证 | 修复记录 |

### 总计: ~14小时

## 8. 关键设计决策

| 决策点 | 选择 | 理由 |
|--------|------|------|
| 网络库 | Boost.Asio | 项目已有依赖，成熟稳定 |
| 序列化 | JSON (nlohmann) | 项目已有依赖，可读性好，调试方便 |
| 协议分隔 | `\n`换行符 | 简单可靠，便于调试 |
| Python GUI | PyQt5 | 功能完整，跨平台，工业级稳定 |
| 连接模式 | 单连接 | HMI场景足够，简化实现 |
| 端口 | 9000 | 避免与常用端口冲突 |

## 9. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| Boost.Asio学习曲线 | 开发延迟 | 参考现有异步代码模式 |
| JSON解析性能 | 高频请求时延迟 | HMI场景低频，可接受 |
| 网络异常处理 | 连接不稳定 | 实现心跳检测和自动重连 |
| PyQt5部署 | 打包复杂 | 使用PyInstaller打包 |

## 10. 后续扩展

如果未来需要更高级功能，可以扩展：

1. **实时轨迹动画**：添加WebSocket推送，或提高轮询频率
2. **多客户端支持**：TcpServer已支持，只需测试验证
3. **远程访问**：配置防火墙规则，或添加认证机制
4. **日志查看**：添加`logs.query`方法
5. **配置管理**：添加`config.get/set`方法

## 11. 完整目录结构

### 11.1 项目整体结构

```
D:\Projects\Backend_CPP\
│
├── src/
│   ├── adapters/                         # Driving Adapters（驱动适配器）
│   │   ├── CLAUDE.md                     # 适配器层规范
│   │   ├── CMakeLists.txt                # 只包含cli和tcp，排除hmi
│   │   ├── README.md
│   │   │
│   │   ├── cli/                          # C++ CLI适配器
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp
│   │   │   └── CommandHandlers.cpp
│   │   │
│   │   ├── tcp/                          # C++ TCP服务器适配器
│   │   │   ├── CMakeLists.txt
│   │   │   ├── main.cpp                  # Bootstrap层，装配UseCase
│   │   │   ├── TcpServer.h
│   │   │   ├── TcpServer.cpp
│   │   │   ├── TcpSession.h
│   │   │   ├── TcpSession.cpp
│   │   │   ├── JsonProtocol.h
│   │   │   ├── JsonProtocol.cpp
│   │   │   ├── TcpCommandDispatcher.h
│   │   │   └── TcpCommandDispatcher.cpp
│   │   │
│   │   └── hmi/                          # Python HMI适配器（与cli/tcp并列）
│   │       ├── README.md                 # 说明这是Python项目
│   │       ├── requirements.txt
│   │       ├── main.py
│   │       ├── client/
│   │       │   ├── __init__.py
│   │       │   ├── tcp_client.py
│   │       │   └── protocol.py
│   │       ├── ui/
│   │       │   ├── __init__.py
│   │       │   ├── main_window.py
│   │       │   ├── status_panel.py
│   │       │   ├── control_panel.py
│   │       │   └── dispense_panel.py
│   │       └── resources/
│   │           └── icons/
│   │
│   ├── application/                      # Application Layer
│   │   ├── container/
│   │   │   └── ApplicationContainer.h    # 只在main.cpp中使用
│   │   └── usecases/                     # UseCase接口（适配器依赖这些）
│   │       ├── system/
│   │       │   ├── IInitializeSystemUseCase.h
│   │       │   └── IGetSystemStatusUseCase.h
│   │       └── motion/
│   │           ├── homing/IHomeAxesUseCase.h
│   │           └── manual/IManualMotionControlUseCase.h
│   │
│   ├── domain/                           # Domain Layer（不修改）
│   │
│   ├── infrastructure/                   # Driven Adapters（不修改）
│   │   └── adapters/
│   │       ├── motion/
│   │       ├── dispensing/
│   │       └── system/
│   │
│   └── contracts/                        # 原shared/，重命名以明确职责
│       ├── ports/                        # 端口接口定义
│       ├── dto/                          # 跨层数据传输对象
│       └── errors/                       # 错误码定义
│
└── docs/
    └── plans/
        └── 2026-01-21-hmi-tcp-adapter-design.md
```

### 11.2 C++端新增文件清单

| 文件 | 代码量 | 说明 |
|------|--------|------|
| `src/adapters/tcp/CMakeLists.txt` | ~20行 | 构建配置 |
| `src/adapters/tcp/TcpServer.h` | ~25行 | TCP服务器声明 |
| `src/adapters/tcp/TcpServer.cpp` | ~25行 | TCP服务器实现 |
| `src/adapters/tcp/TcpSession.h` | ~30行 | 会话声明 |
| `src/adapters/tcp/TcpSession.cpp` | ~30行 | 会话实现 |
| `src/adapters/tcp/JsonProtocol.h` | ~15行 | 协议声明 |
| `src/adapters/tcp/JsonProtocol.cpp` | ~15行 | 协议实现 |
| `src/adapters/tcp/TcpCommandDispatcher.h` | ~40行 | 命令分发声明 |
| `src/adapters/tcp/TcpCommandDispatcher.cpp` | ~110行 | 命令分发实现 |
| **总计** | **~310行** | |

### 11.3 Python端新增文件清单

| 文件 | 代码量 | 说明 |
|------|--------|------|
| `hmi/main.py` | ~30行 | 入口 |
| `hmi/requirements.txt` | ~3行 | 依赖 |
| `hmi/client/__init__.py` | ~2行 | 包初始化 |
| `hmi/client/tcp_client.py` | ~120行 | TCP客户端 |
| `hmi/client/protocol.py` | ~30行 | 协议封装 |
| `hmi/ui/__init__.py` | ~2行 | 包初始化 |
| `hmi/ui/main_window.py` | ~150行 | 主窗口 |
| `hmi/ui/status_panel.py` | ~120行 | 状态面板 |
| `hmi/ui/control_panel.py` | ~150行 | 控制面板 |
| `hmi/ui/dispense_panel.py` | ~100行 | 点胶面板 |
| **总计** | **~700行** | |

## 12. 架构合规性分析

### 12.1 六边形架构适配器分类

```
                    Driving Adapters (主动 - 接收外部请求)
                    ┌─────────────────────────────────────┐
                    │                                     │
                    │   ┌─────────┐      ┌─────────┐     │
                    │   │   CLI   │      │   TCP   │ NEW │
                    │   └────┬────┘      └────┬────┘     │
                    │        │                │          │
                    └────────┼────────────────┼──────────┘
                             │                │
                             └───────┬────────┘
                                     │
                                     ▼
                    ┌─────────────────────────────────────┐
                    │         ApplicationContainer        │
                    │              (UseCases)             │
                    └─────────────────┬───────────────────┘
                                      │
                                      ▼
                    ┌─────────────────────────────────────┐
                    │  Hardware  │  Config  │  Logging   │
                    │  Adapter   │  Adapter │  Adapter   │
                    └─────────────────────────────────────┘
                    Driven Adapters (被动 - 被UseCase调用)
```

### 12.2 合规性检查清单

| 检查项 | 规范要求 | 本方案 | 结果 |
|--------|---------|--------|:----:|
| TCP适配器位置 | Driving Adapter在`src/adapters/` | `src/adapters/tcp/` | 通过 |
| 依赖方向 | Adapters -> Application | TCP -> ApplicationContainer | 通过 |
| 禁止访问Domain | Adapters -/-> Domain Models | 只调用UseCase | 通过 |
| 禁止适配器互调 | Adapters -/-> Other Adapters | TCP不调用CLI | 通过 |
| 命名空间 | `siligen::adapters::*` | `siligen::adapters::tcp` | 通过 |

### 12.3 与错误方案的对比

另一份文档`2026-01-21-hmi-architecture-proposal.md`建议的路径存在架构违规：

| 维度 | 错误方案 | 正确方案（本文档） |
|------|---------|-------------------|
| 路径 | `src/infrastructure/adapters/network/` | `src/adapters/tcp/` |
| 适配器类型 | 误认为Driven Adapter | 正确识别为Driving Adapter |
| 架构语义 | 混淆主动/被动适配器 | 符合六边形架构规范 |

**结论**：本方案的目录结构完全符合项目六边形架构规范。
