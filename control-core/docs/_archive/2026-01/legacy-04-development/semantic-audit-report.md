# 语义不严谨审查报告

**审查日期**: 2026-01-06
**审查范围**: 当前分支 (`feature/test-and-adapter-updates`) 相对 `010-cmp-phase1-integration` 的变更
**审查重点**: 错误码占位符、单位混淆、返回语义、魔法数

---

## 结论摘要

本次审查发现 **8 个高风险点** 和 **12 个中风险点**：

1. **错误码占位符泛滥**：`-9999` 作为"异常返回值"在 ValveAdapter 中被滥用，与 MultiCard SDK 原生错误码混在一起
2. **UNKNOWN_ERROR = 9999**：定义为"兜底错误码"，但实际使用中被随意填充，丢失真实错误语义
3. **Domain 层单位泄露**：`TrajectoryExecutor.cpp:11` 硬编码 `PULSES_PER_MM = 1000.0`，违反架构边界
4. **时间单位混乱**：同一参数用 `ms`/`Ms`/`_ms` 混合命名，SDK 返回的是 `pulse/ms` 但注释未明确
5. **空 catch 吞错**：ValveAdapter 多处 `catch(...) { return -9999; }` 吞掉异常细节
6. **魔法数散落**：`1000.0`（脉冲换算）、`0.001`（时间换算）散落在各适配器，未集中管理
7. **配置缺少单位品牌类型**：`HardwareConfiguration.h` 虽然用了 `_mm`/`_ms` 后缀，但仍是裸 `float32`/`int32`
8. **参数单位不清**：`timeout_ms` 但实际传值可能用 `std::chrono` 或裸 int，无强制约束

---

## 发现清单

### 🔴 High Severity

#### H1: 错误码占位符 `-9999` 滥用

**位置**: `src/infrastructure/adapters/hardware/ValveAdapter.cpp`
```cpp
// 行 626, 647, 664, 724, 750
catch (...) {
    return -9999;  // 异常情况返回特殊错误码
}
```

**问题**:
- `-9999` 与 MultiCard SDK 的原生错误码（`-1`, `-6`, `-7` 等）混在一起，调用方无法区分是 SDK 错误还是适配器内部异常
- 错误码 `-9999` 在 `CreateErrorMessage` 中被硬编码解释为"调用异常"，但原始异常信息完全丢失
- 返回值类型是 `int`，但语义是"可能是脉冲数、可能是错误码"，调用方需先检查是否 `< 0`

**风险场景**:
```cpp
int result = CallMC_CmpPluse(...);
if (result < 0) {
    // 无法区分是 SDK 返回 -1 还是适配器返回 -9999
    // 日志会显示"调用异常"，但真实原因可能是空指针、std::bad_alloc 等
}
```

**推荐修复**:
```cpp
// 方案1：使用 Result<T> 而非裸 int
Result<int> ValveAdapter::CallMC_CmpPluse(...) noexcept {
    try {
        int ret = hardware_->MC_CmpPluse(...);
        if (ret < 0) {
            return Result<int>::Failure(Error(ErrorCode::HARDWARE_COMMAND_FAILED,
                CreateErrorMessage(ret, "CallMC_CmpPluse")));
        }
        return Result<int>::Success(ret);
    } catch (const std::exception& e) {
        return Result<int>::Failure(Error(ErrorCode::ADAPTER_EXCEPTION,
            std::string("CallMC_CmpPluse exception: ") + e.what()));
    } catch (...) {
        return Result<int>::Failure(Error(ErrorCode::ADAPTER_UNKNOWN_EXCEPTION,
            "CallMC_CmpPluse: unknown exception"));
    }
}

// 方案2：如果必须用 int，定义专用常量
namespace ValveAdapterErrorCodes {
    constexpr int ADAPTER_EXCEPTION = -10000;  // 与 SDK 错误码（-1 ~ -99）区分开
}
```

---

#### H2: ErrorCode::UNKNOWN_ERROR = 9999 语义模糊

**位置**: `src/shared/types/Error.h:80`
```cpp
enum class ErrorCode : int32 {
    // ...
    UNKNOWN_ERROR = 9999
};
```

**问题**:
- 多处代码在无法确定具体错误时直接填充 `UNKNOWN_ERROR`，丢失真实失败原因
- 调用栈中被层层传递，最终日志显示"未知错误"，无法调试
- 与硬件错误码混淆：ValveAdapter 中 SDK 返回 `-1`、`-7` 等负数错误码，而 `UNKNOWN_ERROR` 是正数 `9999`

**风险场景**:
```cpp
// DXFWebPlanningUseCase.cpp:56
return Result<void>::Failure(Shared::Types::Error(
    Shared::Types::ErrorCode::UNKNOWN_ERROR, response.error_message));
// ↑ response.error_message 才是真实错误，但错误码却写死了 UNKNOWN_ERROR
```

**推荐修复**:
```cpp
// 1. 拆分 UNKNOWN_ERROR 为更细粒度的错误码
enum class ErrorCode : int32 {
    // 保留 UNKNOWN_ERROR 作为"不应该发生"的断言失败
    UNKNOWN_ERROR = 9999,  // ⚠️ 使用时必须添加日志

    // 新增细粒度错误码
    FILE_NOT_FOUND = 9001,
    FILE_PARSE_ERROR = 9002,
    NETWORK_ERROR = 9003,
    HARDWARE_TIMEOUT = 9004,
    // ...
};

// 2. 添加构造函数，禁止直接用 UNKNOWN_ERROR + 自定义消息
class Error {
public:
    // 删除这个构造函数，强制调用方选择正确的 ErrorCode
    static Error Unknown(const std::string& message) {
        LogWarning("Using UNKNOWN_ERROR - this should be avoided");
        return Error(ErrorCode::UNKNOWN_ERROR, message);
    }
};
```

---

#### H3: Domain 层硬编码硬件单位转换常数

**位置**: `src/domain/motion/TrajectoryExecutor.cpp:11`
```cpp
// 硬件转换系数：每毫米的脉冲数
constexpr double PULSES_PER_MM = 1000.0;
```

**问题**:
- **违反架构边界**：Domain 层不应该知道"脉冲"这个硬件单位
- 魔法数 `1000.0` 散落多处（HardwareTestAdapter.cpp:791, 1320, 1368 等）
- 如果更换电机或驱动器（`PULSES_PER_MM` 变为 `500` 或 `2000`），需要修改 Domain 层代码

**风险场景**:
- 不同轴的分辨率可能不同（X 轴 1000 pulse/mm，Z 轴 500 pulse/mm）
- Domain 层无法处理这种差异，必须由 Adapter 层转换

**推荐修复**:
```cpp
// 1. 删除 TrajectoryExecutor.cpp 中的 PULSES_PER_MM
// 2. 在 Adapter 层实现单位转换
namespace Siligen::Infrastructure::Adapters {

class UnitConverter {
private:
    const std::array<double, 8> pulses_per_mm_;  // 每轴独立配置

public:
    explicit UnitConverter(const AxisConfiguration& config)
        : pulses_per_mm_{config.x.pulse_per_mm, config.y.pulse_per_mm, ...} {}

    long PositionToPulses(Axis axis, double position_mm) const {
        return static_cast<long>(position_mm * pulses_per_mm_[static_cast<size_t>(axis)]);
    }

    double PulsesToPosition(Axis axis, long pulses) const {
        return static_cast<double>(pulses) / pulses_per_mm_[static_cast<size_t>(axis)];
    }
};

// Domain 层的 TrajectoryExecutor 只处理 mm 单位
Result<void> TrajectoryExecutor::ExecuteLine(const Point2D& end_mm) {
    // ...
    hardware_controller_->MoveLine(end_mm.x, end_mm.y);  // 传递 mm 单位
}

// Adapter 层的 HardwareController 实现负责转换
Result<void> MultiCardMotionAdapter::MoveLine(double x_mm, double y_mm) override {
    long x_pulse = unit_converter_.PositionToPulses(Axis::X, x_mm);
    long y_pulse = unit_converter_.PositionToPulses(Axis::Y, y_mm);
    hardware_->MC_LnXY(crd, x_pulse, y_pulse, ...);
}
}
```

---

#### H4: 空 catch 块吞掉异常

**位置**: `src/infrastructure/adapters/hardware/ValveAdapter.cpp:49` 等
```cpp
} catch (...) {
    // 析构函数中捕获所有异常
}
```

**位置**: `src/infrastructure/logging/Logger.cpp:170, 181, 198`
```cpp
} catch (...) {}
```

**问题**:
- 异常被静默吞掉，程序状态可能已损坏但继续运行
- Logger 捕获异常但不记录，导致调试时完全无法追踪问题根源

**风险场景**:
```cpp
// Logger.cpp:181
} catch (...) {}
// ↑ 如果日志写入失败（磁盘满、权限问题），问题被隐藏
//   调用方认为日志已写入，继续执行，丢失关键错误信息
```

**推荐修复**:
```cpp
// Logger.cpp - 至少记录到 stderr
} catch (const std::exception& e) {
    std::fprintf(stderr, "[Logger] Exception caught: %s\n", e.what());
} catch (...) {
    std::fprintf(stderr, "[Logger] Unknown exception caught\n");
}

// ValveAdapter.cpp - 析构函数中确保资源释放
ValveAdapter::~ValveAdapter() {
    try {
        StopDispenser();
        CloseSupply();
    } catch (const std::exception& e) {
        // 析构函数中不能抛异常，但应该记录
        std::fprintf(stderr, "[ValveAdapter] Destructor exception: %s\n", e.what());
    } catch (...) {
        std::fprintf(stderr, "[ValveAdapter] Destructor unknown exception\n");
    }
}
```

---

#### H5: 时间单位混合使用（ms / us / s / pulse/ms）

**位置**: 多处
```cpp
// DXFDispensingExecutionUseCase.h:30-31
static constexpr uint32 DISPENSER_INTERVAL_MS = 1000;    // 触发间隔: 1000 ms
static constexpr uint32 DISPENSER_DURATION_MS = 100;      // 单次持续时间: 100 ms

// CMPTypes.h:157
int32 pulse_width_us = 2000;              // 脉冲宽度 (微秒)

// MultiCardMotionAdapter.cpp:59
double vel_pulse_ms = vel_pulses / 1000.0;  // 转换为 pulse/ms
```

**问题**:
- 同一概念用 `Ms`/`_ms`/`_us` 混合命名，容易写错
- MultiCard SDK 期望 `pulse/ms` 但 Domain 层传递 `mm/s`，转换散落在各处
- 没有强类型保证，传错单位时编译器无法检测

**风险场景**:
```cpp
// 错误示例：把 ms 当 us 传
MC_CmpRpt(..., 1000, ...);  // 想传 1000us，实际传 1000ms
```

**推荐修复**:
```cpp
// 1. 定义强类型时间包装器
namespace Siligen::Shared {

template<typename Rep, typename Ratio>
class Duration {
    Rep count_;
public:
    explicit Duration(Rep count) : count_(count) {}
    Rep count() const { return count_; }

    // 禁止隐式转换
    template<typename R2>
    Duration(Duration<Rep, R2>) = delete;
};

using Milliseconds = Duration<int64, std::milli>;
using Microseconds = Duration<int64, std::micro>;
using Seconds = Duration<double, std::ratio<1>>;

// 2. 硬件 API 包装器使用强类型
class CMPConfig {
public:
    Microseconds pulse_width;    // 强制使用微秒
    Milliseconds interval;       // 强制使用毫秒
};

// 3. 转换函数集中在 UnitConverter
class UnitConverter {
public:
    double VelocityToPulsePerMs(double mm_per_s) const {
        return (mm_per_s * pulses_per_mm_) / 1000.0;
    }
};
}
```

---

### 🟡 Medium Severity

#### M1: Domain 层包含硬件相关枚举

**位置**: `src/domain/dispensing/ports/ITriggerControllerPort.h:23`
```cpp
enum class SignalType {
    PULSE,          // 脉冲信号
    LEVEL           // 电平信号
};
```

**问题**: Domain 层的端口接口包含"脉冲"这个硬件术语，违反了领域纯净性

**推荐修复**: 改为 `DISCRETE` / `CONTINUOUS` 或 `ONE_SHOT` / `SUSTAINED`

---

#### M2: 配置缺少单位后缀或单位后缀不一致

**位置**: `src/shared/types/HardwareConfiguration.h`
```cpp
int32 response_timeout_ms = 5000;         // 有 _ms 后缀
float32 max_velocity_mm_s = 1000.0f;      // 有 _mm_s 后缀
float32 position_tolerance_mm = 0.1f;     // 有 _mm 后缀
int32 num_axes = 4;                       // 无单位（确实不需要）
```

**部分问题**:
```cpp
// Types.h:33 - 缺少单位后缀
constexpr int32 CONNECTION_TIMEOUT = 5000;  // 应为 CONNECTION_TIMEOUT_MS
```

**推荐修复**: 添加命名规范检查规则（见规则化建议）

---

#### M3: 魔法数 `1000.0`（脉冲/mm）散落多处

**位置**:
- `HardwareTestAdapter.cpp:791, 1320, 1321, 1368, 1369, 1404, 1405`
- `TrajectoryExecutor.cpp:11`（已列为 H3）

**问题**: 换算常数未集中管理

**推荐修复**: 同 H3，使用 `UnitConverter` 类集中管理

---

#### M4: 返回值语义混乱（int 既表示脉冲数又表示错误码）

**位置**: ValveAdapter 中所有 `CallMC_*` 方法
```cpp
int ValveAdapter::CallMC_CmpPluse(...);
// 返回值: <0 表示错误，>=0 表示"成功"（但成功时返回的是什么？）
```

**问题**: 调用方无法确定成功时的返回值含义

**推荐修复**: 同 H1，改用 `Result<int>` 或 `Result<void>`

---

#### M5: 配置验证范围硬编码

**位置**: `HardwareConfiguration.h:50-62`
```cpp
if (max_velocity_mm_s <= 0.0f || max_velocity_mm_s > 1000.0f) {
    return Result<void>::Failure(Error(..., "max_velocity_mm_s must be in range (0, 1000]"));
}
```

**问题**: `1000.0` 是硬件限制还是配置限制？如果更换硬件需要修改代码

**推荐修复**: 从配置文件读取硬件限制

---

#### M6: 日志级别未使用强类型

**位置**: `src/shared/types/LogTypes.h`

**问题**: 日志级别是普通枚举，传参时可能用错

---

#### M7-M12: 其他中等风险问题

- **M7**: `bool` 返回值语义不明确（`IsError()`/`IsSuccess()` 同时存在可能导致混淆）
- **M8**: `Result<T>::Value()` 调用前未检查 `IsError()`（未强制）
- **M9**: `EmergencyStopUseCase.h:36` 枚举值 `UNKNOWN` 作为有效状态
- **M10**: `StatusMonitor.cpp:135` 硬编码 `PULSES_PER_MM`（同 H3）
- **M11**: `CMPTypes.h:96` 默认参数 `pulse_width = 2000`（单位不清）
- **M12**: `ConnectionHandler.cpp:118` `timeoutMs` 默认值 5000 但类型 `int32_t`（应为 `Milliseconds`）

---

### 🟢 Low Severity

#### L1: 注释中的"TODO"未跟踪

**位置**: 约 50+ 处 `// TODO:` 注释

**推荐**: 使用 issue tracker 跟踪或定期审查

#### L2: 变量命名误导

```cpp
// HandlerUtils.h:90
static void SendSuccess(..., int status_code = 200);  // 实际可传入任意状态码
```

---

## 最小修复集（按优先级）

### 1. 立即修复（本周内）

1. **ValveAdapter catch 块**: 至少记录异常消息，不要只返回 `-9999`
2. **删除 Domain 层的 `PULSES_PER_MM`**: 将单位转换移到 Adapter 层
3. **时间单位后缀统一**: 将 `CONNECTION_TIMEOUT` 改为 `CONNECTION_TIMEOUT_MS`

### 2. 短期修复（本月内）

4. **ValveAdapter 改用 `Result<T>` 返回**: 替换所有 `int` 返回类型
5. **拆分 `UNKNOWN_ERROR`**: 添加细粒度错误码，禁止随意使用
6. **创建 `UnitConverter` 类**: 集中管理所有单位转换逻辑

### 3. 中期修复（本季度内）

7. **引入强类型 `Duration`**: 用 `Milliseconds`/`Microseconds` 替换 `int32 _ms`
8. **修复 Domain 层端口接口**: 将 `PULSE` 改为领域术语

---

## 规则化建议

### C++ 静态分析规则

```yaml
# .clang-tidy 新增规则
Checks: >
  -modernize-use-chrono,  # 禁止用 int 表示时间
  -bugprone-unhandled-exception,  # 禁止空 catch 块
  cppcoreguidelines-avoid-magic-numbers,  # 禁止魔法数
  cppcoreguidelines-prefer-enum-over-macros,  # 用枚举代替宏
```

### Cppcheck 规则

```ini
[custom]
# 自定义规则：禁止返回 -9999
rule=prohibit_9999_return
message=Return value -9999 is ambiguous - use Result<T> instead
pattern="return -9999"

# 自定义规则：空 catch 块
rule=prohibit_empty_catch
message=Empty catch block swallows exceptions
pattern="catch (...) \{ \s* \}"
```

### 单位后缀命名规范

```cpp
// 物理量变量必须包含单位后缀
struct NamingConvention {
    // 位置
    float32 position_mm;      // ✓
    float32 position_m;       // ✓
    float32 position;         // ✗ 缺少单位

    // 速度
    float32 velocity_mm_s;    // ✓
    float32 velocity_m_s;     // ✓
    float32 speed;            // ✗ 缺少单位

    // 时间
    int32 timeout_ms;         // ✓
    int32 interval_us;        // ✓
    int32 delay;              // ✗ 缺少单位
};
```

---

## 附录：补丁风格修改建议

### Patch 1: ValveAdapter catch 块修复

```diff
--- a/src/infrastructure/adapters/hardware/ValveAdapter.cpp
+++ b/src/infrastructure/adapters/hardware/ValveAdapter.cpp
@@ -623,8 +623,11 @@ int ValveAdapter::CallMC_CmpPluse(short channel, uint32 count, ...) {
         return lastResult;
     }
-    catch (...) {
-        return -9999;  // 异常情况返回特殊错误码
+    catch (const std::exception& e) {
+        std::fprintf(stderr, "[ValveAdapter::CallMC_CmpPluse] Exception: %s\n", e.what());
+        return -10000;  // 与 SDK 错误码区分（SDK 使用 -1 ~ -99）
+    } catch (...) {
+        std::fprintf(stderr, "[ValveAdapter::CallMC_CmpPluse] Unknown exception\n");
+        return -10000;
     }
 }
```

### Patch 2: 删除 Domain 层 PULSES_PER_MM

```diff
--- a/src/domain/motion/TrajectoryExecutor.cpp
+++ b/src/domain/motion/TrajectoryExecutor.cpp
@@ -8,9 +8,6 @@

 namespace Siligen::Domain::Motion {

-// 硬件转换系数：每毫米的脉冲数
-constexpr double PULSES_PER_MM = 1000.0;
-
 void TrajectoryExecutionStats::Reset() {
     total_points = points_executed = buffer_overflows = buffer_underflows = 0;
     execution_time_s = average_velocity = max_velocity_achieved = 0.0f;
```

### Patch 3: 时间单位后缀统一

```diff
--- a/src/shared/types/Types.h
+++ b/src/shared/types/Types.h
@@ -30,7 +30,7 @@ constexpr const char* LOCAL_IP = "192.168.0.200";
 constexpr uint16 CONTROL_CARD_PORT = 0, LOCAL_PORT = 0;
-constexpr int32 CONNECTION_TIMEOUT = 5000;
+constexpr int32 CONNECTION_TIMEOUT_MS = 5000;
```

---

## 总结

本次审查发现的**根本问题**是：

1. **错误模型不统一**：混用 `int` 错误码、`Result<T>`、`ErrorCode` 枚举
2. **单位转换散落**：`1000.0`、`/ 1000.0` 散落各处，未集中管理
3. **架构边界模糊**：Domain 层包含硬件单位知识，Adapter 层返回裸 int

**核心建议**：
- 短期：修复 `-9999` 问题，至少记录异常信息
- 中期：统一使用 `Result<T>`，集中单位转换逻辑
- 长期：引入强类型（品牌类型模式），让编译器帮助检查单位错误

---

## 修复记录

### 2026-01-07

#### H4: 空 catch 块吞掉异常 - Logger.cpp 修复完成

**已修复位置** (`src/infrastructure/logging/Logger.cpp`):

1. **InitializeLogFile** (第 170-174 行) - Task 2 已修复
2. **WriteToFile(LogEntry)** (第 185-189 行) - Task 2 已修复
3. **WriteToFile(string)** (第 206-210 行) - Task 2 已修复
4. **PerformRotation** (第 255-260 行) - 本任务修复 ✅

**修复内容**:
```cpp
// 修复前
} catch (...) { return false; }

// 修复后
} catch (const std::exception& e) {
    std::fprintf(stderr, "[Logger] Exception in PerformRotation: %s\n", e.what());
} catch (...) {
    std::fprintf(stderr, "[Logger] Unknown exception in PerformRotation\n");
}
return false;
```

**Commit**: `fix(Logger): 修复 PerformRotation 空 catch 块`

**状态**: ✅ 完成

