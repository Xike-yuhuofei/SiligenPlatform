---
Created: 2026-01-19
Status: ready_for_execution
Owner: @Xike
---

> **Triage(2026-03-17)**：保留在 `docs/plans/` 继续推进。该问题仍对应现场硬件风险，当前代码虽已加入 `CallMC_SetCmpLevel(false)` 兜底，但根因验证与硬件闭环仍未完成。

# 点胶阀空移阶段常亮问题根因分析

## 问题摘要

| 项目 | 内容 |
|------|------|
| **现象** | 空移阶段点胶阀对应通道指示灯常亮，阀疑似持续开启，空移结束后一次性喷出大量热熔胶 |
| **环境** | DXF 点胶流程段与段之间的快速移动（RapidMoveTo），硬件为 YKF-C08NP-P（PNP 输出）+ 点胶电磁阀 |
| **触发机制** | CMP 位置比较输出 |
| **配置** | PulseType=0（正脉冲/高电平有效），CmpChannel=1 |

---

## 代码流程分析

### 正常执行流程

```
段N执行完成
    └─ DispenserOperationGuard 析构
        └─ StopDispenser() [ValveAdapter.cpp:349]
            ├─ CallMC_StopCmpOutputs(mask) [MC_CmpBufStop]
            └─ CallMC_SetCmpLevel(false)  [MC_CmpPluse 设置低电平]

空移阶段 RapidMoveTo() [DXFDispensingExecutionUseCase.cpp:1042]
    ├─ AddInterpolationData(LINEAR)
    ├─ StartCoordinateSystemMotion()
    └─ WaitForMotionComplete()
    └─ 【期望：阀门保持关闭】

段N+1执行开始
    └─ ResetDispenserHardwareState() [预清理]
    └─ StartPositionTriggeredDispenser()
```

### 关键代码路径

| 函数 | 位置 | 作用 |
|------|------|------|
| `StopDispenser()` | ValveAdapter.cpp:349-414 | 停止 CMP + 拉低电平 |
| `CallMC_StopCmpOutputs()` | ValveAdapter.cpp:809-818 | 调用 `MC_CmpBufStop` |
| `CallMC_SetCmpLevel(false)` | ValveAdapter.cpp:755-777 | 调用 `MC_CmpPluse(level=0)` |
| `RapidMoveTo()` | DXFDispensingExecutionUseCase.cpp:1042-1080 | 仅运动，无阀操作 |

---

## 根因假设

### 假设 1（高概率）：MC_CmpBufData 与 MC_CmpPluse 使用不同硬件模块

**分析**：
- `MC_CmpBufData` 是位置比较缓冲区触发功能，使用编码器位置比较
- `MC_CmpPluse` 是直接脉冲/电平输出功能
- 两者可能输出到**不同的硬件输出端口**，或者 `MC_CmpPluse` 无法覆盖 `MC_CmpBufData` 产生的电平状态

**代码证据**：
```cpp
// StartPositionTriggeredDispenser 中：
hardware_->MC_CmpBufSetChannel(dispenser_config_.cmp_channel, 0);  // 设置缓冲区输出通道
hardware_->MC_CmpBufData(...);  // 加载位置触发数据

// CallMC_SetCmpLevel 中：
hardware_->MC_CmpPluse(nChannelMask=1, nPluseType1=0/1, ...);  // 直接脉冲控制
```

**验证方法**：
1. 查阅博派 SDK 文档确认 `MC_CmpBufData` 和 `MC_CmpPluse` 的输出通道关系
2. 测试单独调用 `MC_CmpPluse(low)` 能否关闭由 `MC_CmpBufData` 开启的输出

---

### 假设 2（中概率）：MC_CmpBufData 完成后电平状态残留

**分析**：
- `MC_CmpBufData` 使用 `nPluseType=2`（脉冲模式），每个触发点电平翻转一次
- `nStartLevel=0`（低电平开始）
- 如果触发点数量为奇数，最终电平 = 高
- `MC_CmpBufStop` 只停止触发功能，不重置电平

**代码证据**：
```cpp
// ValveAdapter.cpp:870
short nPluseType = 2;  // 固定为脉冲输出模式（翻转电平）

// CalculateTriggerPositions 计算触发点数量
uint32 trigger_count = static_cast<uint32>(segment_length / interval_mm);
// 触发点数量取决于线段长度，可能为奇数
```

**验证方法**：
1. 在日志中添加触发点数量输出
2. 测试奇数/偶数触发点场景下的最终电平状态

---

### 假设 3（中概率）：时序竞争 - 电平设置被覆盖

**分析**：
- `StopDispenser` 中先调用 `MC_CmpBufStop`，再调用 `MC_CmpPluse(low)`
- 如果 `MC_CmpBufStop` 是异步执行，可能在 `MC_CmpPluse(low)` 之后才完成
- 导致电平设置被 CMP 硬件残留的触发覆盖

**代码证据**：
```cpp
// StopDispenser 中的调用顺序：
int result = CallMC_StopCmpOutputs(BuildCmpChannelMask());  // 步骤1
// ... 错误处理 ...
result = CallMC_SetCmpLevel(false);  // 步骤2（可能被步骤1的延迟操作覆盖）
```

**验证方法**：
1. 在 `MC_CmpBufStop` 和 `MC_CmpPluse` 之间添加延迟（如 10ms）
2. 测试延迟后电平是否稳定

---

### 假设 4（低概率）：PNP 输出电平极性配置错误

**分析**：
- YKF-C08NP-P 是 PNP 输出，高电平时导通（开阀）
- 配置 `PulseType=0`（正脉冲/高电平有效）
- `CallMC_SetCmpLevel(false)` 应该发送低电平

**代码验证**：
```cpp
// CallMC_SetCmpLevel(false) 时：
bool active_high = (dispenser_config_.pulse_type == 0);  // true
short level = false ? (true ? 1 : 0) : (true ? 0 : 1);   // level = 0（低电平）
// 这是正确的
```

**结论**：配置看起来正确，但需要硬件层面确认。

---

## 已实施的修复

根据问题文档，已在 `StopDispenser` 和 `PauseDispenser` 中增加 `CallMC_SetCmpLevel(false)` 保险拉低电平。

**当前代码**（ValveAdapter.cpp:389-390）：
```cpp
// 保险关闭输出电平，避免CMP停触发后电平残留
result = CallMC_SetCmpLevel(false);
```

---

## 推荐验证步骤

### 步骤 1：确认 CMP 通道映射

查阅博派 SDK 文档或联系厂商确认：
- `MC_CmpBufSetChannel(channel, 0)` 设置的通道与 `MC_CmpPluse(channelMask=1)` 的通道是否为同一物理输出
- `MC_CmpPluse` 电平控制能否覆盖 `MC_CmpBufData` 产生的输出状态

### 步骤 2：添加诊断日志

在关键位置添加日志以追踪电平状态：

```cpp
// StopDispenser 中
SILIGEN_LOG_DEBUG_FMT("StopDispenser: 开始停止 CMP, mask=0x%04X", channel_mask);
int result = CallMC_StopCmpOutputs(channel_mask);
SILIGEN_LOG_DEBUG_FMT("StopDispenser: MC_CmpBufStop 返回 %d", result);

// 添加短暂延迟（测试用）
std::this_thread::sleep_for(std::chrono::milliseconds(5));

SILIGEN_LOG_DEBUG("StopDispenser: 开始拉低电平");
result = CallMC_SetCmpLevel(false);
SILIGEN_LOG_DEBUG_FMT("StopDispenser: MC_CmpPluse(low) 返回 %d", result);
```

### 步骤 3：测试延迟方案

在 `MC_CmpBufStop` 和 `MC_CmpPluse` 之间添加 5-10ms 延迟，验证时序假设。

### 步骤 4：硬件层面验证

使用示波器或逻辑分析仪监控 CMP 输出通道：
1. 观察段执行完成后的电平变化
2. 观察空移阶段的电平状态
3. 确认 `MC_CmpPluse(low)` 是否实际产生了低电平输出

---

## 潜在修复方案

### 方案 A：使用 DO 输出替代 CMP 电平控制

如果 `MC_CmpPluse` 无法控制 `MC_CmpBufData` 的输出，考虑使用通用 DO 输出拉低电平：

```cpp
int ValveAdapter::ForceCloseValve() {
    // 使用 MC_SetDo 强制关闭阀门
    return hardware_->MC_SetDo(0, 0);  // Y0 = 0（关闭点胶阀）
}
```

### 方案 B：在 RapidMoveTo 前显式关阀

在 `RapidMoveTo` 调用前主动调用 `StopDispenser` 或 `CloseDispenser`：

```cpp
// DXFDispensingExecutionUseCase::Execute 中
if (i == 0 || distance_to_start > start_tolerance_mm) {
    // 空移前显式关阀
    valve_port_->StopDispenser();

    auto move_result = RapidMoveTo(seg.start_point, distance_to_start);
    // ...
}
```

### 方案 C：使用 MC_CmpBufSetChannel(0, 0) 断开输出

在停止 CMP 后，将输出通道设置为 0（无输出）：

```cpp
int ValveAdapter::ResetDispenserHardwareState(const char* reason, bool strict) noexcept {
    // 1. 停止 CMP 触发
    int stop_result = CallMC_StopCmpOutputs(channel_mask);

    // 2. 断开 CMP 输出通道
    hardware_->MC_CmpBufSetChannel(0, 0);  // 新增：断开输出通道

    // 3. 拉低电平（备用）
    int level_result = CallMC_SetCmpLevel(false);

    return 0;
}
```

---

## 优先级排序

| 优先级 | 行动 | 预期时间 |
|--------|------|----------|
| 1 | 确认博派 SDK 文档中 MC_CmpBufData 与 MC_CmpPluse 的关系 | 1-2 小时 |
| 2 | 添加诊断日志并复现问题 | 0.5 小时 |
| 3 | 测试延迟方案 | 0.5 小时 |
| 4 | 如确认假设1，实施方案 C（断开输出通道） | 1 小时 |
| 5 | 硬件层面验证（示波器） | 视情况 |

---

## 关联文件

- 问题记录：`docs/debug/dispenser-valve-always-on-rapid-move.md`
- ValveAdapter 实现：`src/infrastructure/adapters/dispensing/dispenser/ValveAdapter.cpp`
- DXF 执行流程：`src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.cpp`
- 配置文件：`src/infrastructure/resources/config/machine_config.ini`
- 参考手册：`docs/library/06_reference.md`（CMP API 规范）
