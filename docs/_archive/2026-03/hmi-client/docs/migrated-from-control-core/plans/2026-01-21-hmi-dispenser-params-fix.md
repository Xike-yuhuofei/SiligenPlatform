# HMI 点胶参数传递修复计划

## 问题概述

HMI 界面中的点胶参数（次数、间隔、持续时间）虽然有 UI 输入控件，但参数未被传递到后端执行。

## 架构分析

```
HMI (Python PyQt5)
    ↓ TCP JSON-RPC
TCP Server (C++ Adapter)
    ↓ UseCase 调用
StartDispenserUseCase (Application Layer)
    ↓ Port 接口
IValvePort::StartDispenser(DispenserValveParams)
    ↓ 实现
ValveAdapter (Infrastructure Layer)
```

### 现有支持

后端已完整支持点胶参数：

- `DispenserValveParams` 结构体 (`src/domain/dispensing/ports/IValvePort.h:25-61`)
  - `count`: 点胶次数 (1-1000)
  - `intervalMs`: 点胶间隔 (10-60000 ms)
  - `durationMs`: 单次持续时间 (10-5000 ms)

- `StartDispenserUseCase::Execute()` 已接受 `DispenserValveParams` 参数

### 断点位置

1. **HMI 端**: `protocol.py:118` - `dispenser_start()` 不接受参数
2. **HMI 端**: `main_window.py:1188` - `_on_dispenser_start()` 不传递参数
3. **后端**: `TcpCommandDispatcher.cpp:367` - `HandleDispenserStart()` 不解析参数

## 修复方案

### 步骤 1: 修改 HMI protocol.py

**文件**: `src/adapters/hmi/client/protocol.py`

**修改**: `dispenser_start()` 方法接受参数

```python
def dispenser_start(self, count: int = 1, interval_ms: int = 1000, duration_ms: int = 100) -> bool:
    resp = self._client.send_request("dispenser.start", {
        "count": count,
        "interval_ms": interval_ms,
        "duration_ms": duration_ms
    })
    return "result" in resp
```

### 步骤 2: 修改 HMI main_window.py

**文件**: `src/adapters/hmi/ui/main_window.py`

**修改**: `_on_dispenser_start()` 读取 UI 参数并传递

```python
def _on_dispenser_start(self):
    count = self._dispenser_count.value()
    interval = self._dispenser_interval.value()
    duration = self._dispenser_duration.value()
    self._protocol.dispenser_start(count, interval, duration)
    self.statusBar().showMessage(f"点胶已启动 (次数:{count}, 间隔:{interval}ms, 持续:{duration}ms)")
```

### 步骤 3: 修改后端 TcpCommandDispatcher.cpp

**文件**: `src/adapters/tcp/TcpCommandDispatcher.cpp`

**修改**: `HandleDispenserStart()` 解析参数并传递给 UseCase

```cpp
std::string TcpCommandDispatcher::HandleDispenserStart(const std::string& id, const nlohmann::json& params) {
    if (!startDispenserUseCase_) {
        return JsonProtocol::MakeErrorResponse(id, 2700, "StartDispenserUseCase not available");
    }

    // 解析点胶参数
    Domain::Dispensing::Ports::DispenserValveParams valveParams;
    valveParams.count = params.value("count", 1u);
    valveParams.intervalMs = params.value("interval_ms", 1000u);
    valveParams.durationMs = params.value("duration_ms", 100u);

    // 验证参数
    if (!valveParams.IsValid()) {
        return JsonProtocol::MakeErrorResponse(id, 2702, valveParams.GetValidationError());
    }

    auto result = startDispenserUseCase_->Execute(valveParams);
    if (!result.IsSuccess()) {
        return JsonProtocol::MakeErrorResponse(id, 2701, result.GetError().GetMessage());
    }

    return JsonProtocol::MakeSuccessResponse(id, {{"dispensing", true}});
}
```

## 架构合规性检查

| 规则 | 状态 | 说明 |
|------|------|------|
| HEXAGONAL_DEPENDENCY_DIRECTION | 符合 | Adapter → Application → Domain |
| HEXAGONAL_PORT_ACCESS | 符合 | 通过 UseCase 访问 Port |
| ADAPTER_NO_BYPASS_USECASE | 符合 | 不直接调用 Port |
| PORT_INTERFACE_PURITY | 不涉及 | 不修改 Port 接口 |

## 修改文件清单

1. `src/adapters/hmi/client/protocol.py` - 1 处修改
2. `src/adapters/hmi/ui/main_window.py` - 1 处修改
3. `src/adapters/tcp/TcpCommandDispatcher.cpp` - 1 处修改

## 测试验证

1. 启动后端 TCP 服务器
2. 启动 HMI 应用
3. 连接到后端
4. 设置点胶参数（次数=3, 间隔=500ms, 持续=100ms）
5. 点击"启动"按钮
6. 验证后端日志显示正确的参数值
