# 语义审查高优先级修复实施计划

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 修复语义审查报告中发现的 5 个高优先级问题：ValveAdapter catch 块、UNKNOWN_ERROR 滥用、Domain 层单位泄露、时间单位混乱、空 catch 吞错

**架构：**
- ValveAdapter: 将 `int` 返回类型改为 `Result<int>`，使用专用错误码
- Error.h: 拆分 UNKNOWN_ERROR 为细粒度错误码
- TrajectoryExecutor: 移除 PULSES_PER_MM，单位转换移至 Adapter 层
- 类型系统: 引入强类型时间/单位包装器

**Tech Stack:** C++17, Result<T>, std::chrono, Hexagonal Architecture

---

## Task 1: ValveAdapter catch 块修复（记录异常 + 专用错误码）

**Files:**
- Modify: `src/infrastructure/adapters/hardware/ValveAdapter.cpp`
- Reference: `docs/04-development/semantic-audit-report.md` (H1, H4)

**背景:** 当前 catch 块返回 `-9999`，与 SDK 错误码混在一起，且丢失异常信息。

---

### Step 1: 定义适配器专用错误码常量

在 `ValveAdapter.cpp` 顶部添加专用错误码命名空间：

```cpp
// ============================================================
// ValveAdapter 专用错误码（与 MultiCard SDK 错误码区分）
// ============================================================
// MultiCard SDK 错误码范围: -1 ~ -99
// ValveAdapter 内部错误码: -10000 ~ -10999
namespace ValveAdapterErrorCodes {
    constexpr int ADAPTER_EXCEPTION = -10000;        // 适配器内部异常
    constexpr int INVALID_PARAMETER = -10001;        // 参数验证失败
    constexpr int STATE_INVALID = -10002;            // 状态不合法
    constexpr int HARDWARE_NOT_READY = -10003;       // 硬件未就绪
}
```

**位置**: 在 `#include` 语句之后，namespace 定义之前

---

### Step 2: 修复 CallMC_CmpPluse 的 catch 块

**文件:** `src/infrastructure/adapters/hardware/ValveAdapter.cpp:623-628`

**修改前:**
```cpp
catch (...) {
    return -9999;  // 异常情况返回特殊错误码
}
```

**修改后:**
```cpp
catch (const std::exception& e) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_CmpPluse] Exception: %s\n", e.what());
    std::fprintf(stderr, "  - channel=%d, count=%u, interval=%u, duration=%u\n",
                channel, count, intervalMs, durationMs);
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
} catch (...) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_CmpPluse] Unknown exception\n");
    std::fprintf(stderr, "  - channel=%d, count=%u, interval=%u, duration=%u\n",
                channel, count, intervalMs, durationMs);
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
}
```

---

### Step 3: 修复 CallMC_SetExtDoBit 的 catch 块

**文件:** `src/infrastructure/adapters/hardware/ValveAdapter.cpp:646-648`

**修改前:**
```cpp
catch (...) {
    return -9999;
}
```

**修改后:**
```cpp
catch (const std::exception& e) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_SetExtDoBit] Exception: %s\n", e.what());
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
} catch (...) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_SetExtDoBit] Unknown exception\n");
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
}
```

---

### Step 4: 修复 CallMC_StopCmpPluse 的 catch 块

**文件:** `src/infrastructure/adapters/hardware/ValveAdapter.cpp:663-665`

**修改前:**
```cpp
catch (...) {
    return -9999;
}
```

**修改后:**
```cpp
catch (const std::exception& e) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_StopCmpPluse] Exception: %s\n", e.what());
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
} catch (...) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_StopCmpPluse] Unknown exception\n");
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
}
```

---

### Step 5: 修复 CallMC_CmpBufData 的 catch 块

**文件:** `src/infrastructure/adapters/hardware/ValveAdapter.cpp:723-725`

**修改前:**
```cpp
catch (...) {
    return -9999;  // 异常情况返回特殊错误码
}
```

**修改后:**
```cpp
catch (const std::exception& e) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_CmpBufData] Exception: %s\n", e.what());
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
} catch (...) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_CmpBufData] Unknown exception\n");
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
}
```

---

### Step 6: 修复 CallMC_EncOff 的 catch 块

**文件:** `src/infrastructure/adapters/hardware/ValveAdapter.cpp:749-751`

**修改前:**
```cpp
catch (...) {
    return -9999;  // 异常情况返回特殊错误码
}
```

**修改后:**
```cpp
catch (const std::exception& e) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_EncOff] Exception: %s\n", e.what());
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
} catch (...) {
    std::fprintf(stderr, "[ValveAdapter::CallMC_EncOff] Unknown exception\n");
    return ValveAdapterErrorCodes::ADAPTER_EXCEPTION;
}
```

---

### Step 7: 更新 CreateErrorMessage 函数

**文件:** `src/infrastructure/adapters/hardware/ValveAdapter.cpp:829`

在 case `-9999:` 之后添加适配器错误码的处理：

```cpp
// 适配器内部错误
case ValveAdapterErrorCodes::ADAPTER_EXCEPTION:
    msg += "适配器异常（软件内部异常，请检查 stderr 日志）";
    break;
```

---

### Step 8: 编译验证

运行编译命令确认修改没有语法错误：

```bash
cmake --build build\stage4-all-modules-check --config Debug --target siligen_device_hal_hardware -j8
```

**预期输出:** 编译成功，无错误

---

### Step 9: 提交修复

```bash
git add src/infrastructure/adapters/hardware/ValveAdapter.cpp
git commit -m "fix(ValveAdapter): 使用专用错误码替代 -9999，记录异常信息

- 定义 ValveAdapterErrorCodes 命名空间，使用 -10000 范围
- 所有 catch 块记录异常到 stderr
- 区分 SDK 错误码与适配器内部错误

Ref: docs/04-development/semantic-audit-report.md H1, H4"
```

---

## Task 2: Logger 空 catch 块修复

**Files:**
- Modify: `src/infrastructure/logging/Logger.cpp:170, 181, 198`
- Reference: `docs/04-development/semantic-audit-report.md` (H4)

**背景:** Logger 捕获异常但不记录，导致调试时无法追踪问题。

---

### Step 1: 修复 Logger.cpp:170 的空 catch 块

**文件:** `src/infrastructure/logging/Logger.cpp:168-172`

**修改前:**
```cpp
} catch (...) {
    // 静默忽略
}
```

**修改后:**
```cpp
} catch (const std::exception& e) {
    std::fprintf(stderr, "[Logger] Exception in FlushQueue: %s\n", e.what());
} catch (...) {
    std::fprintf(stderr, "[Logger] Unknown exception in FlushQueue\n");
}
```

---

### Step 2: 修复 Logger.cpp:181 的空 catch 块

**文件:** `src/infrastructure/logging/Logger.cpp:179-183`

**修改前:**
```cpp
} catch (...) {}
```

**修改后:**
```cpp
} catch (const std::exception& e) {
    std::fprintf(stderr, "[Logger] Exception in WriteToFile: %s\n", e.what());
} catch (...) {
    std::fprintf(stderr, "[Logger] Unknown exception in WriteToFile\n");
}
```

---

### Step 3: 修复 Logger.cpp:198 的空 catch 块

**文件:** `src/infrastructure/logging/Logger.cpp:196-200`

**修改前:**
```cpp
} catch (...) {}
```

**修改后:**
```cpp
} catch (const std::exception& e) {
    std::fprintf(stderr, "[Logger] Exception in RotateLogFile: %s\n", e.what());
} catch (...) {
    std::fprintf(stderr, "[Logger] Unknown exception in RotateLogFile\n");
}
```

---

### Step 4: 编译验证

```bash
cmake --build build\stage4-all-modules-check --config Debug --target siligen_spdlog_adapter -j8
```

**预期输出:** 编译成功

---

### Step 5: 提交修复

```bash
git add src/infrastructure/logging/Logger.cpp
git commit -m "fix(Logger): 空 catch 块至少记录到 stderr

- 所有 catch 块区分 std::exception 和未知异常
- 使用 fprintf(stderr, ...) 确保异常可见
- 保持 Logger 的异常安全性（析构函数不抛异常）

Ref: docs/04-development/semantic-audit-report.md H4"
```

---

## Task 3: 时间单位后缀统一

**Files:**
- Modify: `src/shared/types/Types.h`
- Modify: `src/shared/types/HardwareConfiguration.h`
- Reference: `docs/04-development/semantic-audit-report.md` (H5, M2)

**背景:** `CONNECTION_TIMEOUT` 缺少单位后缀，容易误用。

---

### Step 1: 修复 Types.h 中的 CONNECTION_TIMEOUT

**文件:** `src/shared/types/Types.h:33`

**修改前:**
```cpp
constexpr int32 CONNECTION_TIMEOUT = 5000;
```

**修改后:**
```cpp
constexpr int32 CONNECTION_TIMEOUT_MS = 5000;
```

---

### Step 2: 搜索所有使用 CONNECTION_TIMEOUT 的地方

```bash
# 在项目根目录运行
grep -r "CONNECTION_TIMEOUT" --include="*.cpp" --include="*.h" src/
```

**预期输出:** 找到所有引用位置，需要一并更新

---

### Step 3: 更新所有引用（示例）

如果发现使用 `CONNECTION_TIMEOUT` 的代码，更新为 `CONNECTION_TIMEOUT_MS`：

```cpp
// 修改前
if (elapsed > CONNECTION_TIMEOUT) { ... }

// 修改后
if (elapsed > CONNECTION_TIMEOUT_MS) { ... }
```

---

### Step 4: 编译验证

```bash
cmake --build build\stage4-all-modules-check --config Debug --target siligen_shared_kernel -j8
```

**预期输出:** 编译成功

---

### Step 5: 提交修复

```bash
git add src/shared/types/Types.h
git commit -m "refactor(types): CONNECTION_TIMEOUT 添加 _MS 后缀

- 重命名为 CONNECTION_TIMEOUT_MS 明确单位
- 符合物理量变量命名规范

Ref: docs/04-development/semantic-audit-report.md M2"
```

---

## Task 4: 创建 UnitConverter 类（单位转换集中管理）

**Files:**
- Create: `src/infrastructure/adapters/hardware/UnitConverter.h`
- Create: `src/infrastructure/adapters/hardware/UnitConverter.cpp`
- Modify: `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp`
- Modify: `src/infrastructure/hardware/CMakeLists.txt`
- Reference: `docs/04-development/semantic-audit-report.md` (H3, M3)

**背景:** 魔法数 1000.0 散落多处，单位转换逻辑未集中管理。

---

### Step 1: 创建 UnitConverter.h

**文件:** `src/infrastructure/adapters/hardware/UnitConverter.h`

```cpp
#pragma once

#include <array>
#include <cstdint>

namespace Siligen::Infrastructure::Adapters {

/**
 * @brief 单位转换器 - 集中管理 Domain 层与硬件层之间的单位转换
 *
 * 职责:
 * - Domain 层使用标准单位 (mm, mm/s, mm/s²)
 * - 硬件层使用脉冲单位 (pulse, pulse/s, pulse/ms)
 * - 此类负责双向转换，隐藏硬件实现细节
 */
class UnitConverter {
public:
    /**
     * @brief 构造函数
     * @param pulses_per_mm 每轴的脉冲/毫米比值 (长度 8，对应 X,Y,Z,A,B,C,U,V)
     */
    explicit UnitConverter(const std::array<double, 8>& pulses_per_mm);

    // === 位置转换 ===

    /// mm 转 pulse (绝对位置)
    long PositionToPulses(size_t axis_index, double position_mm) const noexcept;

    /// pulse 转 mm (绝对位置)
    double PulsesToPosition(size_t axis_index, long pulses) const noexcept;

    /// mm 转 pulse (相对位移)
    long DistanceToPulses(size_t axis_index, double distance_mm) const noexcept;

    /// pulse 转 mm (相对位移)
    double PulsesToDistance(size_t axis_index, long pulses) const noexcept;

    // === 速度转换 ===

    /// mm/s 转 pulse/ms (MultiCard SDK 要求的单位)
    double VelocityToPulsePerMs(size_t axis_index, double velocity_mm_s) const noexcept;

    /// pulse/ms 转 mm/s
    double PulsePerMsToVelocity(size_t axis_index, double vel_pulse_ms) const noexcept;

    // === 加速度转换 ===

    /// mm/s² 转 pulse/ms²
    double AccelerationToPulsePerMs2(size_t axis_index, double accel_mm_s2) const noexcept;

    /// pulse/ms² 转 mm/s²
    double PulsePerMs2ToAcceleration(size_t axis_index, double accel_pulse_ms2) const noexcept;

    // === 验证 ===

    /// 检查轴索引是否有效
    bool IsValidAxis(size_t axis_index) const noexcept;

    /// 获取指定轴的脉冲/毫米比值
    double GetPulsesPerMm(size_t axis_index) const noexcept;

private:
    std::array<double, 8> pulses_per_mm_;  // 每轴独立配置
};

} // namespace Siligen::Infrastructure::Adapters
```

---

### Step 2: 创建 UnitConverter.cpp

**文件:** `src/infrastructure/adapters/hardware/UnitConverter.cpp`

```cpp
#include "UnitConverter.h"
#include <stdexcept>
#include <cstdio>

namespace Siligen::Infrastructure::Adapters {

UnitConverter::UnitConverter(const std::array<double, 8>& pulses_per_mm)
    : pulses_per_mm_(pulses_per_mm)
{
    // 验证配置
    for (size_t i = 0; i < 8; ++i) {
        if (pulses_per_mm_[i] <= 0.0) {
            std::fprintf(stderr, "[UnitConverter] Invalid pulses_per_mm for axis %zu: %f\n",
                        i, pulses_per_mm_[i]);
            pulses_per_mm_[i] = 1000.0;  // 默认值
        }
    }
}

long UnitConverter::PositionToPulses(size_t axis_index, double position_mm) const noexcept {
    if (!IsValidAxis(axis_index)) return 0;
    return static_cast<long>(position_mm * pulses_per_mm_[axis_index]);
}

double UnitConverter::PulsesToPosition(size_t axis_index, long pulses) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    return static_cast<double>(pulses) / pulses_per_mm_[axis_index];
}

long UnitConverter::DistanceToPulses(size_t axis_index, double distance_mm) const noexcept {
    return PositionToPulses(axis_index, distance_mm);
}

double UnitConverter::PulsesToDistance(size_t axis_index, long pulses) const noexcept {
    return PulsesToPosition(axis_index, pulses);
}

double UnitConverter::VelocityToPulsePerMs(size_t axis_index, double velocity_mm_s) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    // mm/s -> pulse/s -> pulse/ms
    double pulse_per_s = velocity_mm_s * pulses_per_mm_[axis_index];
    return pulse_per_s / 1000.0;
}

double UnitConverter::PulsePerMsToVelocity(size_t axis_index, double vel_pulse_ms) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    // pulse/ms -> pulse/s -> mm/s
    double pulse_per_s = vel_pulse_ms * 1000.0;
    return pulse_per_s / pulses_per_mm_[axis_index];
}

double UnitConverter::AccelerationToPulsePerMs2(size_t axis_index, double accel_mm_s2) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    // mm/s² -> pulse/s² -> pulse/ms²
    double pulse_per_s2 = accel_mm_s2 * pulses_per_mm_[axis_index];
    return pulse_per_s2 / 1000000.0;  // / 1000²
}

double UnitConverter::PulsePerMs2ToAcceleration(size_t axis_index, double accel_pulse_ms2) const noexcept {
    if (!IsValidAxis(axis_index)) return 0.0;
    // pulse/ms² -> pulse/s² -> mm/s²
    double pulse_per_s2 = accel_pulse_ms2 * 1000000.0;  // * 1000²
    return pulse_per_s2 / pulses_per_mm_[axis_index];
}

bool UnitConverter::IsValidAxis(size_t axis_index) const noexcept {
    if (axis_index >= 8) {
        std::fprintf(stderr, "[UnitConverter] Invalid axis index: %zu\n", axis_index);
        return false;
    }
    return true;
}

double UnitConverter::GetPulsesPerMm(size_t axis_index) const noexcept {
    if (!IsValidAxis(axis_index)) return 1000.0;
    return pulses_per_mm_[axis_index];
}

} // namespace Siligen::Infrastructure::Adapters
```

---

### Step 3: 更新 CMakeLists.txt

**文件:** `src/infrastructure/hardware/CMakeLists.txt`

添加 UnitConverter 到源文件列表：

```cmake
# 在现有的源文件列表中添加
list(APPEND HARDWARE_SOURCES
    # ...
    ../adapters/hardware/UnitConverter.cpp
)
```

---

### Step 4: 编译验证

```bash
cmake --build build\stage4-all-modules-check --config Debug --target siligen_device_hal_hardware -j8
```

**预期输出:** 编译成功

---

### Step 5: 提交新文件

```bash
git add src/infrastructure/adapters/hardware/UnitConverter.h
git add src/infrastructure/adapters/hardware/UnitConverter.cpp
git add src/infrastructure/hardware/CMakeLists.txt
git commit -m "feat(hardware): 创建 UnitConverter 集中管理单位转换

- 隐藏 Domain 层 (mm) 与硬件层 (pulse) 之间的转换
- 支持每轴独立配置脉冲/毫米比值
- 提供 Position/Velocity/Acceleration 双向转换

下一步: 在 MultiCardMotionAdapter 中使用

Ref: docs/04-development/semantic-audit-report.md H3, M3"
```

---

## Task 5: 在 MultiCardMotionAdapter 中使用 UnitConverter

**Files:**
- Modify: `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.h`
- Modify: `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp`
- Reference: `docs/04-development/semantic-audit-report.md` (H3, M3)

**背景:** 替换散落的 `/ 1000.0` 和 `* 1000.0` 魔法数。

---

### Step 1: 在 MultiCardMotionAdapter.h 中添加 UnitConverter 成员

**文件:** `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.h`

在 private 成员中添加：

```cpp
private:
    // ...
    Adapters::UnitConverter unit_converter_;
```

同时在构造函数声明中添加初始化：

```cpp
explicit MultiCardMotionAdapter(std::shared_ptr<IMultiCardWrapper> hardware,
                                const std::array<double, 8>& pulses_per_mm);
```

---

### Step 2: 更新 MultiCardMotionAdapter.cpp 构造函数

**文件:** `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp`

```cpp
MultiCardMotionAdapter::MultiCardMotionAdapter(
    std::shared_ptr<IMultiCardWrapper> hardware,
    const std::array<double, 8>& pulses_per_mm)
    : hardware_(std::move(hardware))
    , unit_converter_(pulses_per_mm)
{
    // ...
}
```

---

### Step 3: 替换 MoveAxisToPosition 中的单位转换

**文件:** `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp:50-60`

**修改前:**
```cpp
// 3. 设置速度
// ⚠️ MC_SetVel 期望的单位是 pulse/ms，不是 pulse/s
double vel_pulses = unit_converter_.VelocityMmSToPS(velocity);
double vel_pulse_ms = vel_pulses / 1000.0;  // 转换为 pulse/ms
ret = hardware_->MC_SetVel(axis, vel_pulse_ms);
```

**修改后:**
```cpp
// 3. 设置速度 (SDK 期望 pulse/ms)
double vel_pulse_ms = unit_converter_.VelocityToPulsePerMs(axis, velocity);
ret = hardware_->MC_SetVel(axis, vel_pulse_ms);
```

---

### Step 4: 替换 RelativeMove 中的单位转换

**文件:** `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp:85-89`

**修改前:**
```cpp
long distance_pulses = MMToPulses(distance);
// ⚠️ MC_MoveRel 的速度参数单位也是 pulse/ms
double vel_pulses = unit_converter_.VelocityMmSToPS(velocity);
double vel_pulse_ms = vel_pulses / 1000.0;  // 转换为 pulse/ms
short ret = hardware_->MC_MoveRel(1, axis, static_cast<double>(distance_pulses), vel_pulse_ms);
```

**修改后:**
```cpp
long distance_pulses = unit_converter_.DistanceToPulses(axis, distance);
double vel_pulse_ms = unit_converter_.VelocityToPulsePerMs(axis, velocity);
short ret = hardware_->MC_MoveRel(1, axis, static_cast<double>(distance_pulses), vel_pulse_ms);
```

---

### Step 5: 替换 SynchronizedMove 中的单位转换

**文件:** `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp:117-120`

**修改前:**
```cpp
long target_pulses = MMToPulses(cmd.target_position);
// ⚠️ MC_SetVel 期望的单位是 pulse/ms，不是 pulse/s
double vel_pulses = unit_converter_.VelocityMmSToPS(cmd.velocity);
double vel_pulse_ms = vel_pulses / 1000.0;  // 转换为 pulse/ms
```

**修改后:**
```cpp
long target_pulses = unit_converter_.PositionToPulses(cmd.axis, cmd.target_position);
double vel_pulse_ms = unit_converter_.VelocityToPulsePerMs(cmd.axis, cmd.velocity);
```

---

### Step 6: 删除旧的 MMToPulses 函数

**文件:** `src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp`

移除 `MMToPulses` 和 `PulsesToMM` 函数，已被 UnitConverter 替代。

---

### Step 7: 编译验证

```bash
cmake --build build\stage4-all-modules-check --config Debug --target siligen_device_hal_hardware -j8
```

**预期输出:** 编译成功

---

### Step 8: 提交修复

```bash
git add src/infrastructure/adapters/hardware/MultiCardMotionAdapter.h
git add src/infrastructure/adapters/hardware/MultiCardMotionAdapter.cpp
git commit -m "refactor(MultiCardMotionAdapter): 使用 UnitConverter 替代魔法数

- 替换所有 / 1000.0 和 * 1000.0 为 UnitConverter 方法
- 删除 MMToPulses/PulsesToMM 函数
- 速度转换: VelocityToPulsePerMs
- 位置转换: PositionToPulses / PulsesToPosition

Ref: docs/04-development/semantic-audit-report.md H3, M3"
```

---

## Task 6: 删除 Domain 层的 PULSES_PER_MM

**Files:**
- Modify: `src/domain/motion/TrajectoryExecutor.cpp`
- Reference: `docs/04-development/semantic-audit-report.md` (H3)

**背景:** Domain 层不应该知道"脉冲"这个硬件单位。

---

### Step 1: 移除 PULSES_PER_MM 常量

**文件:** `src/domain/motion/TrajectoryExecutor.cpp:11`

**删除:**
```cpp
// 硬件转换系数：每毫米的脉冲数
constexpr double PULSES_PER_MM = 1000.0;
```

---

### Step 2: 更新 ExecuteTrajectory 中的使用

**文件:** `src/domain/motion/TrajectoryExecutor.cpp:121-122`

**修改前:**
```cpp
double cx = pt.arc_center.x * PULSES_PER_MM;
double cy = pt.arc_center.y * PULSES_PER_MM;
```

**修改后:**
```cpp
// 注意：圆弧中心使用 mm 单位，Adapter 层会转换为 pulse
double cx = pt.arc_center.x;
double cy = pt.arc_center.y;
```

---

### Step 3: 更新触发点位置转换

**文件:** `src/domain/motion/TrajectoryExecutor.cpp:215`

**修改前:**
```cpp
triggers.push_back(CMPTriggerPoint(static_cast<int32>(pt.trigger_position_mm * PULSES_PER_MM),
    DispensingAction::PULSE, pt.trigger_pulse_width_us, 0));
```

**修改后:**
```cpp
// Domain 层使用 mm 单位，Adapter 层负责转换为 pulse
triggers.push_back(CMPTriggerPoint(static_cast<int32>(pt.trigger_position_mm),
    DispensingAction::PULSE, pt.trigger_pulse_width_us, 0));
```

---

### Step 4: 更新 ConvertMMToPulses 函数（临时兼容）

**文件:** `src/domain/motion/TrajectoryExecutor.cpp:347-348`

**修改前:**
```cpp
x = static_cast<long>(point.x * PULSES_PER_MM);
y = static_cast<long>(point.y * PULSES_PER_MM);
```

**添加 TODO 注释:**
```cpp
// TODO: 移除此函数，单位转换应在 Adapter 层完成
// 临时实现：直接返回 mm 值，假设硬件接口会处理转换
x = static_cast<long>(point.x);
y = static_cast<long>(point.y);
```

---

### Step 5: 编译验证

```bash
cmake --build build\stage4-all-modules-check --config Debug --target siligen_control_application -j8
```

**预期输出:** 编译成功

---

### Step 6: 添加技术债务跟踪注释

在文件顶部添加：

```cpp
// ============================================================
// 技术债务: Domain 层单位转换
// ============================================================
// TODO: 移除所有脉冲单位相关逻辑
// - CMPTriggerPoint 应该使用 mm 单位
// - 硬件接口 (IHardwareController) 应该接受 mm 单位
// - Adapter 层负责所有 mm <-> pulse 转换
//
// 当前状态: 部分完成，等待 IHardwareController 接口重构
// ============================================================
```

---

### Step 7: 提交修复

```bash
git add src/domain/motion/TrajectoryExecutor.cpp
git commit -m "refactor(domain): 移除 PULSES_PER_MM 硬编码常数

- 删除 Domain 层的硬件单位知识
- 添加 TODO 标记单位转换迁移
- 符合 Hexagonal 架构边界原则

技术债务: 等待 IHardwareController 接口重构以完全分离

Ref: docs/04-development/semantic-audit-report.md H3"
```

---

## Task 7: 添加细粒度错误码（拆分 UNKNOWN_ERROR）

**Files:**
- Modify: `src/shared/types/Error.h`
- Reference: `docs/04-development/semantic-audit-report.md` (H2)

**背景:** UNKNOWN_ERROR = 9999 被滥用，丢失真实错误语义。

---

### Step 1: 在 ErrorCode 枚举中添加细粒度错误码

**文件:** `src/shared/types/Error.h:70-81`

**在 UNKNOWN_ERROR 之前添加:**
```cpp
    // 文件系统错误 (File system errors 9001-9099)
    FILE_NOT_FOUND = 9001,
    FILE_PARSE_ERROR = 9002,
    FILE_IO_ERROR = 9003,

    // 网络错误 (Network errors 9100-9199)
    NETWORK_CONNECTION_FAILED = 9101,
    NETWORK_TIMEOUT = 9102,
    NETWORK_ERROR = 9199,

    // 硬件错误 (Hardware errors 9200-9299)
    HARDWARE_TIMEOUT = 9201,
    HARDWARE_NOT_RESPONDING = 9202,
    HARDWARE_COMMAND_FAILED = 9203,

    // 适配器错误 (Adapter errors 9300-9399)
    ADAPTER_EXCEPTION = 9301,
    ADAPTER_UNKNOWN_EXCEPTION = 9302,

    // 一般错误 (General errors 9900-9999)
    NOT_IMPLEMENTED = 9901,
    TIMEOUT = 9902,
    MOTION_ERROR = 9903,
```

---

### Step 2: 在 ErrorDescriptions.h 中添加错误消息映射

**文件:** `src/shared/errors/ErrorDescriptions.h`

```cpp
// 文件系统错误
case Types::ErrorCode::FILE_NOT_FOUND:
    return "文件不存在";
case Types::ErrorCode::FILE_PARSE_ERROR:
    return "文件解析失败";
case Types::ErrorCode::FILE_IO_ERROR:
    return "文件读写错误";

// 网络错误
case Types::ErrorCode::NETWORK_CONNECTION_FAILED:
    return "网络连接失败";
case Types::ErrorCode::NETWORK_TIMEOUT:
    return "网络超时";
case Types::ErrorCode::NETWORK_ERROR:
    return "网络错误";

// 硬件错误
case Types::ErrorCode::HARDWARE_TIMEOUT:
    return "硬件响应超时";
case Types::ErrorCode::HARDWARE_NOT_RESPONDING:
    return "硬件无响应";
case Types::ErrorCode::HARDWARE_COMMAND_FAILED:
    return "硬件命令执行失败";

// 适配器错误
case Types::ErrorCode::ADAPTER_EXCEPTION:
    return "适配器异常";
case Types::ErrorCode::ADAPTER_UNKNOWN_EXCEPTION:
    return "适配器未知异常";
```

---

### Step 3: 替换 UNKNOWN_ERROR 使用处（示例）

**文件:** `src/application/usecases/DXFWebPlanningUseCase.cpp:56`

**修改前:**
```cpp
return Result<void>::Failure(Shared::Types::Error(
    Shared::Types::ErrorCode::UNKNOWN_ERROR, response.error_message));
```

**修改后:**
```cpp
// 根据错误类型选择具体的错误码
ErrorCode code = ErrorCode::NETWORK_ERROR;  // 或根据 response.error_code 确定
return Result<void>::Failure(Shared::Types::Error(code, response.error_message));
```

---

### Step 4: 编译验证

```bash
cmake --build build\stage4-all-modules-check --config Debug --target siligen_shared_kernel -j8
```

**预期输出:** 编译成功

---

### Step 5: 提交修复

```bash
git add src/shared/types/Error.h
git add src/shared/errors/ErrorDescriptions.h
git add src/application/usecases/DXFWebPlanningUseCase.cpp
git commit -m "refactor(error): 拆分 UNKNOWN_ERROR 为细粒度错误码

- 添加文件系统、网络、硬件、适配器分类
- 错误码按类别分组 (9001-9999)
- UNKNOWN_ERROR 保留为兜底，应避免使用

Ref: docs/04-development/semantic-audit-report.md H2"
```

---

## 验证与测试

### 验证步骤

1. **全量编译**
   ```bash
   cmake --build build\stage4-all-modules-check --config Debug -j8
   ```
   预期：编译成功，无警告

2. **运行硬件测试**
   ```bash
   build/tests/hardware/test_y2_connection
   build/tests/hardware/test_y2_dispensing_workflow
   ```
   预期：测试通过

3. **检查 stderr 日志**
   确认异常被正确记录到 stderr

4. **代码审查**
   - 确认所有 `-9999` 已移除
   - 确认所有空 catch 块已修复
   - 确认 Domain 层无 PULSES_PER_MM

---

## 完成标准

- [ ] ValveAdapter 无 `-9999` 返回值
- [ ] Logger 无空 catch 块
- [ ] CONNECTION_TIMEOUT 重命名完成
- [ ] UnitConverter 创建并使用
- [ ] Domain 层 PULSES_PER_MM 移除
- [ ] 细粒度错误码定义完成
- [ ] 全量编译通过
- [ ] 硬件测试通过

---

**文档版本:** 1.0
**最后更新:** 2026-01-06
**参考文档:** `docs/04-development/semantic-audit-report.md`
