# 项目轴号约定方案风险分析报告

> **日期**: 2026-01-23
> **分析背景**: DXF执行时X轴不响应问题排查
> **分析范围**: 全项目轴号约定一致性

---

## 一、执行摘要

项目中存在 **三种不同的轴号约定**，且没有统一的转换层，导致代码混乱和潜在的运行时错误。这是一个**架构级技术债务**，不仅导致了当前的 DXF 执行问题，还会在未来的开发和维护中持续产生 bug。

---

## 二、当前约定混乱现状

### 2.1 约定分布表

| 模块/文件 | 约定 | X轴 | Y轴 | 证据 |
|-----------|------|-----|-----|------|
| **TcpCommandDispatcher** (点动) | 0-based | 0 | 1 | `axisNum = 0; if (axisName == "Y") axisNum = 1;` |
| **IHardwareTestPort** | 0-based | 0 | 1 | `@param axis 轴号(0=X, 1=Y)` |
| **test_records_schema.sql** | 0-based | 0 | 1 | `axis INTEGER NOT NULL, -- 移动轴(0=X, 1=Y, 2=Z, 3=U)` |
| **IConfigurationPort** | 0-based | 0 | 1 | `int32 axis = 0; ///< 轴号（0-1）` |
| **SafetyLimitsValidator** | 1-based | 1 | 2 | `if (axis == 1) { // X轴` |
| **ManualMotionControlUseCase** | 1-based | 1 | 2 | `short axis = 1; // 轴号 (1-16)` |
| **IValvePort** | 1-based | 1 | 2 | `short axis = 1; ///< 触发轴号（1-16）` |
| **ArcTriggerPointCalculator** | 1-based | 1 | 2 | `int16 trigger_axis; ///< 触发轴号（1=X轴, 2=Y轴）` |
| **CLI用户输入** | 1-based | 1 | 2 | `PromptInt("轴号(1-2)", 1)` |
| **MultiCard SDK** | 1-based | 1 | 2 | `profile[8]; // 规则profile轴号数组(从1开始)` |

### 2.2 转换点分布

| 位置 | 转换方向 | 代码 |
|------|----------|------|
| `CommandHandlers.cpp:865` | 1-based → 0-based | `axis_index = config.axis - 1` |
| `MultiCardAdapter.cpp:302` | 0-based → 1-based | `sdk_axis = axis_id + 1` |
| `MultiCardAdapter.cpp:426` | 1-based → 0-based (mask) | `motion_mask = (1 << (x_axis - 1))` |

---

## 三、风险分类

### 3.1 高风险：边界转换错误

#### 问题代码 1 - SafetyLimitsValidator.cpp:72-79

```cpp
ValidationResult SafetyLimitsValidator::ValidateAxisPosition(int32 axis, float32 pos_mm, ...) {
    if (axis == 1) {  // X轴 ← 期望 1-based
        min_val = config_.x_min_mm;
    } else if (axis == 2) {  // Y轴
        min_val = config_.y_min_mm;
    } else {
        return CreateError("不支持的轴号: " + std::to_string(axis), ...);
    }
}
```

**风险**: 如果调用方传入 0-based 的轴号（如 `axis=0` 表示 X 轴），会触发"不支持的轴号"错误，导致安全验证失败。

#### 问题代码 2 - CommandHandlers.cpp:865

```cpp
// CLI 用户输入 1-based，转换为 0-based 传递给 UseCase
short axis_index = static_cast<short>(config.axis - 1);
request.axes.push_back(axis_index);
```

**风险**: 如果用户输入 `axis=0`（虽然提示是 1-2），会导致 `axis_index = -1`。

#### 问题代码 3 - MultiCardAdapter.cpp:426

```cpp
// X轴=1, Y轴=2 (1-based SDK)
short x_axis = 1;
short y_axis = 2;
// 但 MC_Update 的 mask 使用 0-based bit 位置
short motion_mask = (1 << (x_axis - 1)) | (1 << (y_axis - 1));  // = 0x03
```

**风险**: 逻辑复杂，容易在其他地方出错。

### 3.2 中风险：跨模块调用不一致

**场景 1**: TCP 命令 → 安全验证

```
TcpCommandDispatcher (0-based: axis=0)
    ↓ 调用
SafetyLimitsValidator::ValidateAxisPosition(axis=0)  ← 期望 1-based
    ↓ 结果
"不支持的轴号: 0" 错误
```

**场景 2**: DXF 执行 → CMP 触发

```
DXFDispensingPlanner (1-based: trigger_axis=1)
    ↓ 传递给
TriggerControllerPort::SetSingleTrigger(axis=1)  ← 如果内部期望 0-based？
    ↓ 结果
触发配置到错误的轴
```

### 3.3 低风险：文档与代码不一致

| 文档/注释 | 代码实际行为 |
|-----------|--------------|
| `IHardwareTestPort.h:66` 注释 `轴号(0=X, 1=Y)` | 0-based |
| `ManualMotionControlUseCase.h:16` 注释 `轴号 (1-16)` | 1-based |
| `MultiCardErrorCodes.cpp:34` 注释 `轴号是否在 [0, 卡轴数-1]` | 0-based |
| `硬件连接实现方案.md:614` 注释 `轴号从1开始，不是0` | 1-based |

---

## 四、风险量化

| 风险类型 | 影响范围 | 发生概率 | 严重程度 | 风险等级 |
|----------|----------|----------|----------|----------|
| 安全验证绕过 | 安全模块 | 中 | 高 | 高 |
| 轴控制错误 | 运动控制 | 高 | 高 | 高 |
| CMP 触发错位 | 点胶控制 | 中 | 中 | 中 |
| 回零轴错误 | 初始化 | 低 | 中 | 低 |
| 维护困难 | 全项目 | 高 | 低 | 低 |

---

## 五、根因分析

```
┌─────────────────────────────────────────────────────────────────┐
│                      项目轴号约定混乱根因                         │
├─────────────────────────────────────────────────────────────────┤
│                                                                 │
│  1. 缺乏统一的轴号类型定义                                        │
│     - 没有 AxisId 类型封装                                       │
│     - 使用原始 short/int32 传递轴号                               │
│                                                                 │
│  2. 缺乏统一的转换层                                              │
│     - 转换逻辑分散在各个模块                                      │
│     - 有的模块转换，有的不转换                                    │
│                                                                 │
│  3. 接口约定不明确                                                │
│     - 部分接口文档说 0-based                                      │
│     - 部分接口文档说 1-based                                      │
│     - 部分接口没有文档                                            │
│                                                                 │
│  4. 历史演进问题                                                  │
│     - 早期代码可能使用一种约定                                    │
│     - 后期代码使用另一种约定                                      │
│     - 没有统一重构                                                │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 六、建议的解决方案

### 方案 A：统一为 0-based（推荐）

**理由**:
- C++ 数组索引天然是 0-based
- 大部分内部接口已经是 0-based
- 只在 SDK 边界做 `+1` 转换

**实施步骤**:
1. 定义 `AxisId` 类型，封装 0-based 轴号
2. 在 `MultiCardAdapter` 边界统一做 `+1` 转换
3. 修改 `SafetyLimitsValidator` 接受 0-based
4. CLI/HMI 用户输入时做 `-1` 转换

### 方案 B：统一为 1-based

**理由**:
- 与 MultiCard SDK 一致
- 用户界面更直观（轴1、轴2）

**实施步骤**:
1. 修改所有内部接口为 1-based
2. 修改 `TcpCommandDispatcher` 的轴号映射
3. 修改数据库 schema 注释

### 方案 C：引入类型安全的轴号（最佳实践）

```cpp
// 定义类型安全的轴号
enum class LogicalAxis : int16 { X = 0, Y = 1, Z = 2, U = 3 };
enum class SdkAxis : int16 { X = 1, Y = 2, Z = 3, U = 4 };

// 转换函数
constexpr SdkAxis ToSdkAxis(LogicalAxis axis) {
    return static_cast<SdkAxis>(static_cast<int16>(axis) + 1);
}

constexpr LogicalAxis ToLogicalAxis(SdkAxis axis) {
    return static_cast<LogicalAxis>(static_cast<int16>(axis) - 1);
}
```

---

## 七、与当前 DXF 执行问题的关联

### 7.1 观察到的现象

- 点动控制：X轴和Y轴都正常工作
- DXF执行：Y轴响应，X轴不响应，点胶阀不响应

### 7.2 可能的根因

1. `axis_map = {1, 2}` 是正确的（符合 SDK 1-based 约定）
2. 但 `MC_Update` 的 mask 参数可能存在约定混淆
3. 或者轴使能状态在 DXF 执行路径中未正确设置

### 7.3 建议的调试方向

1. 检查 `MC_CrdStart` 调用时的日志输出
2. 确认 X 轴在执行前是否已使能（`MC_AxisOn(1)` 是否被调用）
3. 检查 `MC_SetCrdPrm` 的返回值

---

## 八、结论

项目的轴号约定混乱是一个**架构级技术债务**，建议在修复当前问题后，制定统一的轴号约定规范并逐步重构。

**短期行动**:
1. 修复当前 DXF 执行问题
2. 添加日志确认轴号传递路径

**中期行动**:
1. 制定统一的轴号约定规范文档
2. 在关键边界添加断言检查

**长期行动**:
1. 引入类型安全的轴号封装
2. 逐步重构所有模块使用统一约定

---

## 附录：相关代码位置

| 文件 | 行号 | 说明 |
|------|------|------|
| `src/adapters/tcp/TcpCommandDispatcher.cpp` | 412-419 | 点动控制轴号映射 |
| `src/infrastructure/security/SafetyLimitsValidator.cpp` | 72-79 | 安全验证轴号检查 |
| `src/adapters/cli/CommandHandlers.cpp` | 865 | CLI轴号转换 |
| `src/infrastructure/drivers/multicard/MultiCardAdapter.cpp` | 302, 426 | SDK轴号转换 |
| `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.cpp` | 345 | DXF坐标系配置 |
| `src/domain/dispensing/domain-services/ArcTriggerPointCalculator.cpp` | 166-184 | CMP触发轴号 |
