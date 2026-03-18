# Semantic Audit Fixes Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 修复语义审计报告中发现的所有 High/Medium 风险问题，包括错误码占位符、单位魔法数、返回值语义混淆等。

**Architecture:**
- Error.h: 补充 ADAPTER_EXCEPTION 等缺失错误码，避免占位符冲突
- ValveAdapter.cpp: 移除 -9999 占位符，使用已定义的 ADAPTER_EXCEPTION (-10000)
- MultiCardMotionAdapter.cpp/HardwareTestAdapter.cpp: 封装单位转换常量 PULSE_PER_SEC_TO_MS
- TriggerCalculator: 使用 std::optional<int32> 替代 -1 返回值
- InterpolationTestUseCase: 封装时间常量 DEFAULT_INTERPOLATION_PERIOD_S
- DXFDispensingExecutionUseCase: 修正 DISPENSER_COUNT 语义
- EmergencyStopUseCase: 重命名 UNKNOWN 为 UNKNOWN_CAUSE

**Tech Stack:**
- C++17 (std::optional, constexpr)
- Hexagonal Architecture (Domain/Infrastructure/Application layers)
- Result<T> error handling pattern
- MultiCard SDK API (pulse/s vs pulse/ms units)

---

## Task 1: 修复 Error.h - 补充缺失错误码

**Files:**
- Modify: `src/shared/types/Error.h:76-80`

**Context:** 当前 ErrorCode 枚举缺少 ADAPTER_EXCEPTION 等关键错误类型，导致 ValveAdapter 使用 -9999 占位符。需要新增 9000-9001 范围的错误码。

**Step 1: 读取并理解当前 ErrorCode 定义**

Run: `rg "enum class ErrorCode" -A 30 src/shared/types/Error.h`
Expected: 看到当前枚举定义，确认 9000 范围未被占用

**Step 2: 在 NOT_IMPLEMENTED 前添加 ADAPTER 相关错误码**

修改 `src/shared/types/Error.h` 第 76-80 行：

```cpp
    // 一般错误 (General errors 9000-9999)
    ADAPTER_EXCEPTION = 9000,       // 适配器内部异常
    ADAPTER_NOT_INITIALIZED = 9001, // 适配器未初始化
    NOT_IMPLEMENTED = 9002,         // 原 9001 改为 9002
    TIMEOUT = 9003,                 // 原 9002 改为 9003
    MOTION_ERROR = 9004,            // 原 9003 改为 9004
    UNKNOWN_ERROR = 9999
```

**Step 3: 同步更新 ErrorCodeToString 函数**

修改 `src/shared/types/Error.h` 第 204-208 行：

```cpp
        // 一般错误 (General errors)
        case ErrorCode::ADAPTER_EXCEPTION: return "ADAPTER_EXCEPTION";
        case ErrorCode::ADAPTER_NOT_INITIALIZED: return "ADAPTER_NOT_INITIALIZED";
        case ErrorCode::NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case ErrorCode::TIMEOUT: return "TIMEOUT";
        case ErrorCode::MOTION_ERROR: return "MOTION_ERROR";
        case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
```

**Step 4: 编译验证错误码定义正确**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`
Expected: 编译成功，无错误码冲突

**Step 5: 提交**

```bash
git add src/shared/types/Error.h
git commit -m "fix(Error): 新增 ADAPTER_EXCEPTION/ADAPTER_NOT_INITIALIZED 错误码

- 将 ADAPTER_EXCEPTION 添加到 9000
- 将 ADAPTER_NOT_INITIALIZED 添加到 9001
- 调整后续错误码编号 (NOT_IMPLEMENTED: 9001→9002, TIMEOUT: 9002→9003, MOTION_ERROR: 9003→9004)
- 同步更新 ErrorCodeToString 函数

修复语义审计报告 Issue #1: ValveAdapter 使用 -9999 占位符

参考: docs/04-development/semantic-audit-report-2025-01-07.md"
```

---

## Task 2: 修复 ValveAdapter.cpp - 移除 -9999 占位符

**Files:**
- Modify: `src/infrastructure/adapters/hardware/ValveAdapter.cpp:865-872`

**Context:** ValveAdapter 使用 -9999 作为适配器异常占位符，但已存在 ValveAdapterErrorCodes::ADAPTER_EXCEPTION = -10000。需要移除重复的 case -9999 分支。

**Step 1: 读取 CreateErrorMessage 函数**

Run: `rg "case -9999" -B 5 -A 5 src/infrastructure/adapters/hardware/ValveAdapter.cpp`
Expected: 找到 case -9999 和 case ValveAdapterErrorCodes::ADAPTER_EXCEPTION 两个分支

**Step 2: 移除 case -9999 分支**

修改 `src/infrastructure/adapters/hardware/ValveAdapter.cpp` 第 864-872 行：

```cpp
// 删除这部分:
//         // 特殊错误
//         case -9999:
//             msg += "调用异常（软件内部异常，请检查日志）";
//             break;

// 保留这部分:
        // 适配器内部错误
        case ValveAdapterErrorCodes::ADAPTER_EXCEPTION:
            msg += "适配器异常（软件内部异常，请检查 stderr 日志）";
            break;
```

**Step 3: 验证没有其他 -9999 引用**

Run: `rg "-9999" src/infrastructure/adapters/hardware/ValveAdapter.cpp`
Expected: 只应找到 namespace 注释中的说明，不再有 case -9999

**Step 4: 编译验证**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`
Expected: 编译成功

**Step 5: 提交**

```bash
git add src/infrastructure/adapters/hardware/ValveAdapter.cpp
git commit -m "fix(ValveAdapter): 移除 -9999 占位符，统一使用 ADAPTER_EXCEPTION

- 删除 case -9999 分支（与 ValveAdapterErrorCodes::ADAPTER_EXCEPTION 重复）
- 保留 case ValveAdapterErrorCodes::ADAPTER_EXCEPTION (-10000)
- 更新注释说明使用专用错误码而非占位符

修复语义审计报告 Issue #1: ValveAdapter 使用 -9999 占位符冲突

参考: docs/04-development/semantic-audit-report-2025-01-07.md"
```

---

## Task 3: 修复 MultiCardMotionAdapter.cpp - 封装速度转换常量

**Files:**
- Modify: `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp:1-150`

**Context:** pulse/s → pulse/ms 转换使用魔法数 1000.0，散落在多处（行 59, 88, 120）。需要封装为 constexpr 常量。

**Step 1: 读取文件开头和转换代码**

Run: `rg "1000\.0" -B 2 -A 2 src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp | head -50`
Expected: 看到三处 `vel_pulses / 1000.0`

**Step 2: 在文件顶部添加 Units 命名空间**

在 `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp` 第 6 行后添加：

```cpp
namespace Siligen::Infrastructure::Adapters {

// 单位转换常量: pulse/s → pulse/ms (MultiCard SDK 要求 pulse/ms 单位)
namespace Units {
    constexpr double PULSE_PER_SEC_TO_MS = 1000.0;
}

using namespace Domain::Ports;
```

**注意**: 需要删除原有的 `namespace Siligen::Infrastructure::Adapters {` (第 6 行)

**Step 3: 替换第一处转换 (行 59)**

```cpp
// 修改前:
double vel_pulse_ms = vel_pulses / 1000.0;  // 转换为 pulse/ms

// 修改后:
double vel_pulse_ms = vel_pulses / Units::PULSE_PER_SEC_TO_MS;  // 转换为 pulse/ms
```

**Step 4: 替换第二处转换 (行 88)**

```cpp
// 修改前:
double vel_pulse_ms = vel_pulses / 1000.0;  // 转换为 pulse/ms

// 修改后:
double vel_pulse_ms = vel_pulses / Units::PULSE_PER_SEC_TO_MS;  // 转换为 pulse/ms
```

**Step 5: 替换第三处转换 (行 120)**

```cpp
// 修改前:
double vel_pulse_ms = vel_pulses / 1000.0;  // 转换为 pulse/ms

// 修改后:
double vel_pulse_ms = vel_pulses / Units::PULSE_PER_SEC_TO_MS;  // 转换为 pulse/ms
```

**Step 6: 检查是否有其他 1000.0 转换**

Run: `rg "/ 1000\.0" src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp`
Expected: 无结果（所有转换已替换）

**Step 7: 编译验证**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`
Expected: 编译成功

**Step 8: 提交**

```bash
git add src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp
git commit -m "refactor(MultiCardMotionAdapter): 封装单位转换常量 PULSE_PER_SEC_TO_MS

- 新增 Units::PULSE_PER_SEC_TO_MS constexpr 常量 (1000.0)
- 替换所有 / 1000.0 魔法数为命名常量
- 提高代码可维护性，避免散落的单位转换魔法数

修复语义审计报告 Issue #2: 速度单位转换使用散落魔法数

参考: docs/04-development/semantic-audit-report-2025-01-07.md"
```

---

## Task 4: 修复 HardwareTestAdapter.cpp - 同步单位转换常量

**Files:**
- Modify: `src/infrastructure/adapters/hardware/HardwareTestAdapter.cpp`

**Context:** HardwareTestAdapter 同样存在 `/ 1000.0` 魔法数，需要与 MultiCardMotionAdapter 保持一致。

**Step 1: 查找魔法数位置**

Run: `rg "/ 1000\.0" -B 2 -A 2 src/infrastructure/adapters/hardware/HardwareTestAdapter.cpp`
Expected: 找到行 149, 309 两处转换

**Step 2: 在文件顶部添加 Units 命名空间**

参考 Task 3，在适当位置添加：

```cpp
// 单位转换常量: pulse/s → pulse/ms (MultiCard SDK 要求 pulse/ms 单位)
namespace Units {
    constexpr double PULSE_PER_SEC_TO_MS = 1000.0;
}
```

**Step 3: 替换所有 / 1000.0**

将找到的所有 `/ 1000.0` 替换为 `/ Units::PULSE_PER_SEC_TO_MS`

**Step 4: 编译验证**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`
Expected: 编译成功

**Step 5: 提交**

```bash
git add src/infrastructure/adapters/hardware/HardwareTestAdapter.cpp
git commit -m "refactor(HardwareTestAdapter): 封装单位转换常量 PULSE_PER_SEC_TO_MS

- 新增 Units::PULSE_PER_SEC_TO_MS constexpr 常量
- 替换所有 / 1000.0 魔法数为命名常量
- 与 MultiCardMotionAdapter 保持一致

修复语义审计报告 Issue #2: 速度单位转换使用散落魔法数

参考: docs/04-development/semantic-audit-report-2025-01-07.md"
```

---

## Task 5: 修复 TriggerCalculator - 使用 std::optional<int32>

**Files:**
- Modify: `src/domain/motion/TriggerCalculator.h:94-97`
- Modify: `src/domain/motion/TriggerCalculator.cpp:167-187`

**Context:** `FindClosestTrajectoryPoint` 返回 -1 表示"未找到"，与错误码语义混淆。应使用 `std::optional<int32>`。

**Step 1: 修改 TriggerCalculator.h 返回值类型**

修改 `src/domain/motion/TriggerCalculator.h` 第 4-5 行（添加 include）和第 94 行：

```cpp
// 在第 16 行后添加:
#include <optional>

// 修改第 94 行函数声明:
    /**
     * @brief 查找最接近的轨迹点
     * @param trajectory 轨迹
     * @param target_position 目标位置
     * @return 最接近的轨迹点索引，轨迹为空时返回 std::nullopt
     */
    std::optional<int32> FindClosestTrajectoryPoint(
        const std::vector<TrajectoryPoint>& trajectory,
        const Point2D& target_position
    ) const;
```

**Step 2: 修改 TriggerCalculator.cpp 实现**

修改 `src/domain/motion/TriggerCalculator.cpp` 第 1 行和第 167-187 行：

```cpp
// 在文件开头添加:
#include <optional>

// 修改函数实现:
std::optional<int32> TriggerCalculator::FindClosestTrajectoryPoint(
    const std::vector<TrajectoryPoint>& trajectory,
    const Point2D& target_position
) const {
    if (trajectory.empty()) {
        return std::nullopt;
    }

    int32 closest_index = 0;
    float32 min_distance = std::numeric_limits<float32>::max();

    for (size_t i = 0; i < trajectory.size(); ++i) {
        float32 distance = trajectory[i].position.DistanceTo(target_position);
        if (distance < min_distance) {
            min_distance = distance;
            closest_index = static_cast<int32>(i);
        }
    }

    return closest_index;
}
```

**Step 3: 查找并修复所有调用点**

Run: `rg "FindClosestTrajectoryPoint" --type cpp -A 3 -B 3 src/`
Expected: 找到所有调用点，需要处理 optional 返回值

对于每个调用点，修改为：

```cpp
// 修改前:
int32 index = calculator->FindClosestTrajectoryPoint(trajectory, position);
if (index != -1) { ... }

// 修改后:
auto index_opt = calculator->FindClosestTrajectoryPoint(trajectory, position);
if (index_opt.has_value()) {
    int32 index = index_opt.value();
    ...
}
```

**Step 4: 编译验证**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`
Expected: 编译成功

**Step 5: 提交**

```bash
git add src/domain/motion/TriggerCalculator.h src/domain/motion/TriggerCalculator.cpp
git commit -m "refactor(TriggerCalculator): 使用 std::optional<int32> 替代 -1 返回值

- FindClosestTrajectoryPoint 返回类型改为 std::optional<int32>
- 轨迹为空时返回 std::nullopt 而非 -1
- 避免返回值语义混淆（-1 可能是有效索引）
- 更新所有调用点处理 optional 返回值

修复语义审计报告 Issue #3: 返回值语义混淆

参考: docs/04-development/semantic-audit-report-2025-01-07.md"
```

---

## Task 6: 修复 InterpolationTestUseCase - 封装时间常量

**Files:**
- Modify: `src/application/usecases/InterpolationTestUseCase.cpp:25-30, 319, 345`

**Context:** `const double dt = 0.001` 魔法数表示 1ms 转秒，应封装为命名常量。

**Step 1: 在文件顶部添加 Timing 命名空间**

在 `src/application/usecases/InterpolationTestUseCase.cpp` 第 25 行后添加：

```cpp
using namespace Siligen::Shared::Types;

// 默认插补周期（用于测试）
namespace Timing {
    constexpr double DEFAULT_INTERPOLATION_PERIOD_S = 0.001;  // 1ms = 0.001s
}
```

**Step 2: 替换第一处魔法数 (行 319)**

```cpp
// 修改前:
const double dt = 0.001; // 1ms转换为秒

// 修改后:
const double dt = Timing::DEFAULT_INTERPOLATION_PERIOD_S;
```

**Step 3: 替换第二处魔法数 (行 345)**

```cpp
// 修改前:
const double dt = 0.001; // 1ms转换为秒

// 修改后:
const double dt = Timing::DEFAULT_INTERPOLATION_PERIOD_S;
```

**Step 4: 编译验证**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`
Expected: 编译成功

**Step 5: 提交**

```bash
git add src/application/usecases/InterpolationTestUseCase.cpp
git commit -m "refactor(InterpolationTestUseCase): 封装时间常量 DEFAULT_INTERPOLATION_PERIOD_S

- 新增 Timing::DEFAULT_INTERPOLATION_PERIOD_S constexpr 常量 (0.001s)
- 替换 0.001 魔法数为命名常量，提高可读性
- 统一时间单位转换语义

修复语义审计报告 Issue #4: 时间魔法数可读性差

参考: docs/04-development/semantic-audit-report-2025-01-07.md"
```

---

## Task 7: 修复 DXFDispensingExecutionUseCase - 修正 DISPENSER_COUNT 语义

**Files:**
- Modify: `src/application/usecases/DXFDispensingExecutionUseCase.h:29`

**Context:** `DISPENSER_COUNT = 9999` 注释"足够大"语义不清，应使用 0 表示无限制或 UINT32_MAX。

**Step 1: 读取当前定义**

Run: `rg "DISPENSER_COUNT" -B 2 -A 2 src/application/usecases/DXFDispensingExecutionUseCase.h`
Expected: 看到当前 `static constexpr uint32 DISPENSER_COUNT = 9999;`

**Step 2: 修改常量定义**

修改 `src/application/usecases/DXFDispensingExecutionUseCase.h` 第 29 行：

```cpp
// 修改前:
static constexpr uint32 DISPENSER_COUNT = 9999;           // 点胶次数（足够大）

// 修改后:
static constexpr uint32 UNLIMITED_DISPENSER_COUNT = 0;         // 0 表示无限次点胶
static constexpr uint32 DEFAULT_DISPENSER_COUNT = UINT32_MAX;  // 默认值表示持续到手动停止
```

**Step 3: 检查是否有使用 DISPENSER_COUNT 的代码**

Run: `rg "DISPENSER_COUNT" --type cpp src/`
Expected: 如果有使用，需要更新为 UNLIMITED_DISPENSER_COUNT 或 DEFAULT_DISPENSER_COUNT

**Step 4: 编译验证**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`
Expected: 编译成功

**Step 5: 提交**

```bash
git add src/application/usecases/DXFDispensingExecutionUseCase.h
git commit -m "refactor(DXFDispensingExecutionUseCase): 修正 DISPENSER_COUNT 语义

- 新增 UNLIMITED_DISPENSER_COUNT = 0 (表示无限次)
- 新增 DEFAULT_DISPENSER_COUNT = UINT32_MAX (默认持续到手动停止)
- 移除语义不清的 9999 占位符

修复语义审计报告 Issue #5: 9999 语义不清

参考: docs/04-development/semantic-audit-report-2025-01-07.md"
```

---

## Task 8: 修复 EmergencyStopUseCase - 重命名 UNKNOWN 为 UNKNOWN_CAUSE

**Files:**
- Modify: `src/application/usecases/EmergencyStopUseCase.h:36, 48`

**Context:** `UNKNOWN = 6` 无法区分"未初始化"和"确实未知"，应重命名为 `UNKNOWN_CAUSE` 以明确语义。

**Step 1: 修改枚举值**

修改 `src/application/usecases/EmergencyStopUseCase.h` 第 36 行：

```cpp
// 修改前:
    UNKNOWN = 6             // 未知原因 (Unknown reason)

// 修改后:
    UNKNOWN_CAUSE = 6       // 未知原因（已确认无法归类）
    // 注意: 未初始化状态应使用 std::optional<EmergencyStopReason> 表示
```

**Step 2: 更新 EmergencyStopReasonToString 函数**

修改 `src/application/usecases/EmergencyStopUseCase.h` 第 48 行：

```cpp
// 修改前:
        case EmergencyStopReason::UNKNOWN: return "UNKNOWN";

// 修改后:
        case EmergencyStopReason::UNKNOWN_CAUSE: return "UNKNOWN_CAUSE";
```

**Step 3: 检查所有使用 UNKNOWN 的代码**

Run: `rg "EmergencyStopReason::UNKNOWN" --type cpp src/`
Expected: 找到所有使用点，更新为 UNKNOWN_CAUSE

**Step 4: 编译验证**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`
Expected: 编译成功

**Step 5: 提交**

```bash
git add src/application/usecases/EmergencyStopUseCase.h
git commit -m "refactor(EmergencyStopUseCase): 重命名 UNKNOWN 为 UNKNOWN_CAUSE

- 将 EmergencyStopReason::UNKNOWN 重命名为 UNKNOWN_CAUSE
- 更明确表达\"已确认无法归类\"的语义
- 更新 EmergencyStopReasonToString 函数
- 更新所有使用点

修复语义审计报告 Issue #6: 枚举语义不清

参考: docs/04-development/semantic-audit-report-2025-01-07.md"
```

---

## Task 9: 添加 Medium 优先级修复 - DXFDispensingExecutionUseCase.cpp 单位转换

**Files:**
- Modify: `src/application/usecases/DXFDispensingExecutionUseCase.cpp:200`

**Context:** `DISPENSER_INTERVAL_MS / 1000.0f` ms→秒转换需要使用常量。

**Step 1: 查找魔法数位置**

Run: `rg "DISPENSER_INTERVAL_MS / 1000" -B 2 -A 2 src/application/usecases/DXFDispensingExecutionUseCase.cpp`
Expected: 找到行 200 的转换代码

**Step 2: 封装转换常量**

在文件顶部添加：

```cpp
// 单位转换常量
namespace Timing {
    constexpr double MILLIS_TO_SECONDS = 0.001;  // ms → s
}
```

**Step 3: 替换魔法数**

```cpp
// 修改前:
double interval_s = DISPENSER_INTERVAL_MS / 1000.0f;

// 修改后:
double interval_s = DISPENSER_INTERVAL_MS * Timing::MILLIS_TO_SECONDS;
```

**Step 4: 编译并提交**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`

```bash
git add src/application/usecases/DXFDispensingExecutionUseCase.cpp
git commit -m "refactor(DXFDispensingExecutionUseCase): 封装 ms→s 转换常量

- 新增 Timing::MILLIS_TO_SECONDS 常量
- 替换 / 1000.0f 魔法数

修复语义审计报告 Medium Issue"
```

---

## Task 10: 添加 Medium 优先级修复 - CMPTestUseCase 单位转换

**Files:**
- Modify: `src/application/usecases/CMPTestUseCase.cpp:497`

**Context:** `1000.0f / samplingRateHz` Hz→ms 转换需要使用常量。

**Step 1: 查找魔法数位置**

Run: `rg "1000\.0f / samplingRateHz" -B 2 -A 2 src/application/usecases/CMPTestUseCase.cpp`
Expected: 找到行 497

**Step 2: 封装转换常量**

```cpp
// 单位转换常量
namespace Timing {
    constexpr double HZ_TO_MILLIS = 1000.0;  // Hz → ms
}
```

**Step 3: 替换魔法数**

```cpp
// 修改前:
double period_ms = 1000.0f / samplingRateHz;

// 修改后:
double period_ms = Timing::HZ_TO_MILLIS / samplingRateHz;
```

**Step 4: 编译并提交**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`

```bash
git add src/application/usecases/CMPTestUseCase.cpp
git commit -m "refactor(CMPTestUseCase): 封装 Hz→ms 转换常量

- 新增 Timing::HZ_TO_MILLIS 常量
- 替换 1000.0f 魔法数

修复语义审计报告 Medium Issue"
```

---

## Task 11: 验证所有修复

**Files:**
- Test: 全量编译
- Test: 静态分析

**Step 1: 全量编译验证**

Run: `cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_runtime siligen_tcp_server -j 4`
Expected: 所有目标编译成功，无警告

**Step 2: 搜索剩余占位符错误码**

Run: `rg "\\b(-9999|9999)\\b" src/ --include="*.cpp" --include="*.h"`
Expected: 只应找到注释或测试代码中的引用，不应出现在实际代码逻辑中

**Step 3: 搜索剩余单位魔法数**

Run: `rg "/ 1000\\b|\\* 0\\.001\\b|1000\\.0 /" src/ --include="*.cpp" --include="*.h"`
Expected: 只应找到注释或 vendor 代码，业务代码中不应存在

**Step 4: 搜索剩余 -1 返回值**

Run: `rg "return -1;" src/ --include="*.cpp" | rg -v "vendor|test|backup"`
Expected: 只应在特定上下文中使用（如索引查找失败），且有明确语义

**Step 5: 创建验证报告**

创建 `docs/04-development/semantic-fix-verification-2025-01-07.md`:

```markdown
# Semantic Fixes Verification Report

**Date**: 2025-01-07
**Tasks Completed**: 10/10
**Compilation**: PASS
**Static Analysis**: PASS

## Fixes Applied

| Task | File | Issue | Status |
|------|------|-------|--------|
| 1 | Error.h | 新增 ADAPTER_EXCEPTION 错误码 | ✅ |
| 2 | ValveAdapter.cpp | 移除 -9999 占位符 | ✅ |
| 3 | MultiCardMotionAdapter.cpp | 封装 PULSE_PER_SEC_TO_MS | ✅ |
| 4 | HardwareTestAdapter.cpp | 封装 PULSE_PER_SEC_TO_MS | ✅ |
| 5 | TriggerCalculator.{h,cpp} | 使用 std::optional<int32> | ✅ |
| 6 | InterpolationTestUseCase.cpp | 封装 DEFAULT_INTERPOLATION_PERIOD_S | ✅ |
| 7 | DXFDispensingExecutionUseCase.h | 修正 DISPENSER_COUNT 语义 | ✅ |
| 8 | EmergencyStopUseCase.h | 重命名 UNKNOWN→UNKNOWN_CAUSE | ✅ |
| 9 | DXFDispensingExecutionUseCase.cpp | 封装 ms→s 转换常量 | ✅ |
| 10 | CMPTestUseCase.cpp | 封装 Hz→ms 转换常量 | ✅ |

## Remaining Items

- Low 优先级: 第三方库 httplib.h, json.hpp 中的 -1 返回值（无需修复）
- 自动化: 添加 clang-tidy 检查规则（可选，后续 PR）
```

**Step 6: 提交验证报告**

```bash
git add docs/04-development/semantic-fix-verification-2025-01-07.md
git commit -m "docs: 添加语义修复验证报告

记录所有语义审计修复任务的完成状态

参考: docs/04-development/semantic-audit-report-2025-01-07.md"
```

---

## 附录：调用点查找命令

```bash
# 查找 FindClosestTrajectoryPoint 调用点
rg "FindClosestTrajectoryPoint" --type cpp -A 3 -B 3 src/

# 查找 EmergencyStopReason::UNKNOWN 使用点
rg "EmergencyStopReason::UNKNOWN" --type cpp src/

# 查找 DISPENSER_COUNT 使用点
rg "DISPENSER_COUNT" --type cpp src/
```
