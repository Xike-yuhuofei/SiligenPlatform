# 轴号约定重构计划

> **Created**: 2026-01-23
> **Status**: completed
> **Owner**: @claude
> **Related**: [风险分析报告](../_archive/2026-01/analysis/2026-01-23-axis-numbering-convention-risk-analysis.md)

---

## 实施完成总结

**完成日期**: 2026-01-23

### 已完成的修改

| Phase | 状态 | 修改内容 |
|-------|------|----------|
| Phase 1 | DONE | 创建 `AxisTypes.h`，定义 `LogicalAxisId`、`SdkAxisId` 和转换函数 |
| Phase 2 | DONE | `MultiCardAdapter` 使用类型安全转换 |
| Phase 3 | DONE | Domain层Port接口更新为0-based轴号约定 |
| Phase 4 | DONE | `ArcTriggerPointCalculator`、`DXFDispensingPlanner` 使用0-based |
| Phase 5 | DONE | `ValveAdapter` 添加SDK轴号转换，CLI/TCP适配器验证 |
| Phase 6 | DONE | `SafetyLimitsValidator`、`IValvePort.h` 更新为0-based |

### 关键修复

1. **ValveAdapter.cpp** - 添加了从0-based到1-based的SDK轴号转换
   ```cpp
   short sdk_axis = ToSdkShort(ToSdkAxis(FromIndex(params.axis)));
   ```

2. **IValvePort.h** - `PositionTriggeredDispenserParams.axis` 默认值从1改为0，验证范围从1-16改为0-15

3. **SafetyLimitsValidator.cpp** - `ValidateAxisPosition` 从1-based改为0-based

4. **ArcTriggerPointCalculator.cpp** - 所有轴号引用从1-based改为0-based

---

## 一、背景与目标

### 1.1 问题背景

项目中存在三种不同的轴号约定，导致：
- DXF执行时X轴不响应（Y轴正常）
- 跨模块调用时轴号混淆
- 维护困难，容易引入新bug

### 1.2 重构目标

1. **类型安全**: 通过强类型防止编译期轴号混淆
2. **单一约定**: 内部统一使用0-based，SDK边界统一转换
3. **架构合规**: 符合六边形架构和`.claude/rules`约束
4. **渐进迁移**: 支持逐步重构，不破坏现有功能

---

## 二、架构约束分析

### 2.1 相关规则

| 规则文件 | 规则名称 | 约束内容 | 对重构的影响 |
|----------|----------|----------|--------------|
| `HEXAGONAL.md` | HEXAGONAL_DEPENDENCY_DIRECTION | infrastructure → domain | 类型定义必须在Shared层 |
| `DOMAIN.md` | DOMAIN_NO_DYNAMIC_MEMORY | 禁止new/delete/malloc | 轴号类型必须是值类型 |
| `DOMAIN.md` | DOMAIN_PUBLIC_API_NOEXCEPT | 公共API必须noexcept | 转换函数必须noexcept |
| `NAMESPACE.md` | NAMESPACE_SHARED_ISOLATION | Shared层零依赖 | 轴号类型不能依赖其他层 |
| `PORTS_ADAPTERS.md` | PORT_INTERFACE_PURITY | Port禁止HAL类型 | Port使用LogicalAxis |
| `PORTS_ADAPTERS.md` | ADAPTER_SINGLE_PORT | Adapter实现单一Port | 转换逻辑在Adapter内部 |

### 2.2 现有类型分析

`src/shared/types/Types.h` 已定义：
```cpp
enum class Axis { X, Y, Z, A, B, C, U, V };
```

**问题**: 这是语义枚举，不是数值索引类型，无法直接用于数组索引或SDK调用。

---

## 三、类型安全设计

### 3.1 新增类型定义

**文件位置**: `src/shared/types/AxisTypes.h`

**命名空间**: `Siligen::Shared::Types`

```cpp
#pragma once

#include <cstdint>
#include <type_traits>

namespace Siligen::Shared::Types {

/// @brief 逻辑轴号 (0-based, 内部使用)
/// @details X=0, Y=1, Z=2, U=3
/// @note 用于数组索引、内部接口、Port定义
enum class LogicalAxisId : int16_t {
    X = 0,
    Y = 1,
    Z = 2,
    U = 3,
    INVALID = -1
};

/// @brief SDK轴号 (1-based, MultiCard SDK使用)
/// @details X=1, Y=2, Z=3, U=4
/// @note 仅在MultiCardAdapter内部使用
enum class SdkAxisId : int16_t {
    X = 1,
    Y = 2,
    Z = 3,
    U = 4,
    INVALID = 0
};

/// @brief 轴号转换: LogicalAxisId -> SdkAxisId
/// @param axis 逻辑轴号 (0-based)
/// @return SDK轴号 (1-based)
[[nodiscard]] constexpr SdkAxisId ToSdkAxis(LogicalAxisId axis) noexcept {
    if (axis == LogicalAxisId::INVALID) {
        return SdkAxisId::INVALID;
    }
    return static_cast<SdkAxisId>(static_cast<int16_t>(axis) + 1);
}

/// @brief 轴号转换: SdkAxisId -> LogicalAxisId
/// @param axis SDK轴号 (1-based)
/// @return 逻辑轴号 (0-based)
[[nodiscard]] constexpr LogicalAxisId ToLogicalAxis(SdkAxisId axis) noexcept {
    if (axis == SdkAxisId::INVALID) {
        return LogicalAxisId::INVALID;
    }
    return static_cast<LogicalAxisId>(static_cast<int16_t>(axis) - 1);
}

/// @brief 获取轴号的数组索引值
/// @param axis 逻辑轴号
/// @return 数组索引 (0-based)
[[nodiscard]] constexpr int16_t ToIndex(LogicalAxisId axis) noexcept {
    return static_cast<int16_t>(axis);
}

/// @brief 从数组索引创建逻辑轴号
/// @param index 数组索引 (0-based)
/// @return 逻辑轴号
[[nodiscard]] constexpr LogicalAxisId FromIndex(int16_t index) noexcept {
    if (index < 0 || index > 3) {
        return LogicalAxisId::INVALID;
    }
    return static_cast<LogicalAxisId>(index);
}

/// @brief 验证逻辑轴号是否有效
/// @param axis 逻辑轴号
/// @return true=有效, false=无效
[[nodiscard]] constexpr bool IsValid(LogicalAxisId axis) noexcept {
    return axis != LogicalAxisId::INVALID &&
           static_cast<int16_t>(axis) >= 0 &&
           static_cast<int16_t>(axis) <= 3;
}

/// @brief 验证SDK轴号是否有效
/// @param axis SDK轴号
/// @return true=有效, false=无效
[[nodiscard]] constexpr bool IsValid(SdkAxisId axis) noexcept {
    return axis != SdkAxisId::INVALID &&
           static_cast<int16_t>(axis) >= 1 &&
           static_cast<int16_t>(axis) <= 4;
}

/// @brief 轴号名称
/// @param axis 逻辑轴号
/// @return 轴名称字符串
[[nodiscard]] constexpr const char* AxisName(LogicalAxisId axis) noexcept {
    switch (axis) {
        case LogicalAxisId::X: return "X";
        case LogicalAxisId::Y: return "Y";
        case LogicalAxisId::Z: return "Z";
        case LogicalAxisId::U: return "U";
        default: return "INVALID";
    }
}

}  // namespace Siligen::Shared::Types
```

### 3.2 设计决策说明

| 决策 | 理由 |
|------|------|
| 使用`enum class`而非`class` | 符合DOMAIN_NO_DYNAMIC_MEMORY，零开销抽象 |
| 底层类型`int16_t` | 与现有`short`兼容，便于渐进迁移 |
| 所有函数`constexpr noexcept` | 符合DOMAIN_PUBLIC_API_NOEXCEPT，编译期计算 |
| 分离`LogicalAxisId`和`SdkAxisId` | 类型系统强制区分，防止混用 |
| 放置在Shared层 | 符合NAMESPACE_SHARED_ISOLATION，零依赖 |

---

## 四、分层重构策略

### 4.1 层次职责划分

```
┌─────────────────────────────────────────────────────────────────┐
│                        Adapters Layer                           │
│  CLI/HMI/TCP: 用户输入1-based → FromIndex(input-1) → LogicalAxisId │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Application Layer                          │
│  UseCases: 使用 LogicalAxisId，不做转换                          │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                        Domain Layer                             │
│  Ports: 接口参数使用 LogicalAxisId                               │
│  Domain Services: 内部使用 LogicalAxisId                         │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Infrastructure Layer                         │
│  MultiCardAdapter: LogicalAxisId → ToSdkAxis() → SDK调用         │
│  (唯一的转换点)                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### 4.2 转换边界规则

| 边界 | 转换方向 | 转换函数 | 责任方 |
|------|----------|----------|--------|
| 用户输入 → Adapter | 1-based → LogicalAxisId | `FromIndex(input - 1)` | CLI/HMI/TCP Adapter |
| Adapter → UseCase | LogicalAxisId | 无转换 | - |
| UseCase → Port | LogicalAxisId | 无转换 | - |
| Port → MultiCardAdapter | LogicalAxisId → SdkAxisId | `ToSdkAxis()` | MultiCardAdapter |
| MultiCardAdapter → SDK | SdkAxisId → short | `static_cast<short>()` | MultiCardAdapter |

---

## 五、迁移步骤

### Phase 1: 基础设施 (无破坏性)

**目标**: 添加新类型，不修改现有代码

| 步骤 | 文件 | 操作 |
|------|------|------|
| 1.1 | `src/shared/types/AxisTypes.h` | 创建新文件，定义类型和转换函数 |
| 1.2 | `src/shared/types/Types.h` | 添加 `#include "AxisTypes.h"` |
| 1.3 | 编译验证 | 确保新类型不破坏现有编译 |

### Phase 2: Infrastructure层迁移

**目标**: 在SDK边界统一转换

| 步骤 | 文件 | 操作 |
|------|------|------|
| 2.1 | `MultiCardAdapter.cpp` | 内部使用`SdkAxisId`，入口接收`LogicalAxisId` |
| 2.2 | `MultiCardAdapter.h` | 更新接口签名 |
| 2.3 | 单元测试 | 验证转换正确性 |

### Phase 3: Domain层迁移

**目标**: Port接口使用强类型

| 步骤 | 文件 | 操作 |
|------|------|------|
| 3.1 | `IPositionControlPort.h` | 轴号参数改为`LogicalAxisId` |
| 3.2 | `IValvePort.h` | 轴号参数改为`LogicalAxisId` |
| 3.3 | `IHardwareTestPort.h` | 轴号参数改为`LogicalAxisId` |
| 3.4 | `SafetyLimitsValidator.cpp` | 使用`LogicalAxisId`，移除硬编码1/2 |
| 3.5 | `ArcTriggerPointCalculator.cpp` | 使用`LogicalAxisId` |

### Phase 4: Application层迁移

**目标**: UseCase使用强类型

| 步骤 | 文件 | 操作 |
|------|------|------|
| 4.1 | `ManualMotionControlUseCase.cpp` | 使用`LogicalAxisId` |
| 4.2 | `HomeAxesUseCase.cpp` | 使用`LogicalAxisId` |
| 4.3 | `DXFDispensingExecutionUseCase.cpp` | 使用`LogicalAxisId` |
| 4.4 | `DXFDispensingPlanner.cpp` | 使用`LogicalAxisId` |

### Phase 5: Adapters层迁移

**目标**: 入口点统一转换

| 步骤 | 文件 | 操作 |
|------|------|------|
| 5.1 | `CommandHandlers.cpp` | 用户输入转换为`LogicalAxisId` |
| 5.2 | `TcpCommandDispatcher.cpp` | TCP命令转换为`LogicalAxisId` |
| 5.3 | HMI适配器 | 用户输入转换为`LogicalAxisId` |

### Phase 6: 清理与验证

**目标**: 移除旧代码，全面验证

| 步骤 | 操作 |
|------|------|
| 6.1 | 移除所有`short axis`参数，改用`LogicalAxisId` |
| 6.2 | 更新所有注释和文档 |
| 6.3 | 运行全部单元测试 |
| 6.4 | 硬件集成测试：点动、回零、DXF执行 |

---

## 六、验证方案

### 6.1 编译期验证

```cpp
// 类型不匹配会导致编译错误
void MoveAxis(LogicalAxisId axis);  // Port接口

// 错误用法 - 编译失败
MoveAxis(1);                        // int不能隐式转换
MoveAxis(SdkAxisId::X);             // SdkAxisId不能隐式转换

// 正确用法
MoveAxis(LogicalAxisId::X);         // OK
MoveAxis(FromIndex(0));             // OK
```

### 6.2 运行时验证

```cpp
// 在关键边界添加断言
void MultiCardAdapter::MoveAxis(LogicalAxisId axis) {
    assert(IsValid(axis) && "Invalid axis ID");
    SdkAxisId sdk_axis = ToSdkAxis(axis);
    assert(IsValid(sdk_axis) && "SDK axis conversion failed");
    // ...
}
```

### 6.3 测试用例

| 测试场景 | 预期结果 |
|----------|----------|
| CLI输入轴号1 → X轴移动 | X轴正确移动 |
| CLI输入轴号2 → Y轴移动 | Y轴正确移动 |
| DXF执行 → X/Y轴联动 | 两轴同时正确移动 |
| TCP点动X轴 → X轴移动 | X轴正确移动 |
| 安全验证X轴位置 → 正确验证 | 使用X轴限位配置 |

---

## 七、风险与缓解

### 7.1 迁移风险

| 风险 | 概率 | 影响 | 缓解措施 |
|------|------|------|----------|
| 遗漏转换点 | 中 | 高 | 编译期类型检查，grep搜索`short axis` |
| 转换方向错误 | 低 | 高 | 单元测试覆盖所有转换函数 |
| 性能影响 | 低 | 低 | constexpr保证零开销 |
| 破坏现有功能 | 中 | 高 | 分阶段迁移，每阶段验证 |

### 7.2 回滚策略

每个Phase完成后创建Git标签：
- `axis-refactor-phase1`
- `axis-refactor-phase2`
- ...

如果某阶段出现问题，可回滚到上一阶段标签。

---

## 八、时间估算

| Phase | 预估工时 | 依赖 |
|-------|----------|------|
| Phase 1 | 2小时 | 无 |
| Phase 2 | 4小时 | Phase 1 |
| Phase 3 | 8小时 | Phase 2 |
| Phase 4 | 6小时 | Phase 3 |
| Phase 5 | 4小时 | Phase 4 |
| Phase 6 | 4小时 | Phase 5 |
| **总计** | **28小时** | - |

---

## 九、附录

### 9.1 受影响文件清单

**Shared层** (新增):
- `src/shared/types/AxisTypes.h`

**Domain层** (修改):
- `src/domain/motion/ports/IPositionControlPort.h`
- `src/domain/dispensing/ports/IValvePort.h`
- `src/domain/dispensing/domain-services/ArcTriggerPointCalculator.cpp`

**Application层** (修改):
- `src/application/usecases/motion/manual/ManualMotionControlUseCase.cpp`
- `src/application/usecases/motion/homing/HomeAxesUseCase.cpp`
- `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.cpp`
- `src/application/usecases/dispensing/dxf/DXFDispensingPlanner.cpp`

**Infrastructure层** (修改):
- `modules/device-hal/src/drivers/multicard/MultiCardAdapter.cpp`
- `modules/device-hal/src/drivers/multicard/MultiCardAdapter.h`
- `src/infrastructure/security/SafetyLimitsValidator.cpp`

**Adapters层** (修改):
- `src/adapters/cli/CommandHandlers.cpp`
- `src/adapters/tcp/TcpCommandDispatcher.cpp`
- `src/adapters/hmi/` (待确认具体文件)

### 9.2 相关规则引用

- `.claude/rules/HEXAGONAL.md`
- `.claude/rules/DOMAIN.md`
- `.claude/rules/NAMESPACE.md`
- `.claude/rules/PORTS_ADAPTERS.md`

---

## 十、审批

- [ ] 架构审查
- [ ] 代码审查
- [ ] 测试计划审查
- [ ] 实施批准
