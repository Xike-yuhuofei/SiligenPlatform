# DXF点胶单轴响应问题分析报告

- **日期**: 2026-01-25
- **问题来源**: `docs/debug/dxf_dispense_axis_response_issue.md`
- **分析方法**: UltraThink 多代理协同分析
- **报告状态**: ⚠️ **假设阶段 - 待验证**

---

## 重要声明

> **本报告提出的"根因"是未经验证的假设集合，不能认定为根本原因。**
>
> 报告缺乏���要的技术证据链（API返回码、坐标系状态、硬限位/报警状态、段切换日志），
> 且部分推断与接口语义不一致。需要进一步收集证据才能确定真正��根因。

---

## 1. 问题概述

### 1.1 问题描述

- **核心问题**: DXF点胶执行仅响应单轴且只执行首段，无法进入第二段
- **上下文环境**: CLI客户端执行DXF点胶；点动控制X/Y轴正常

### 1.2 问题现象

| DXF文件 | 响应轴 | 执行段数 |
|---------|--------|----------|
| demo_100x100_rect_diag.dxf | 仅X轴 | 首段 |
| I_LOVE_YOU.dxf | 仅Y轴 | 首段 |

---

## 2. 几何事实澄清

### 2.1 DXF文件几何分析

**demo_100x100_rect_diag.dxf**:
| 段号 | 起点 | 终点 | 几何特征 |
|------|------|------|----------|
| 1 | (0,0) | (100,0) | **纯水平线** (dx=100, dy=0) |
| 2 | (100,0) | (100,100) | 纯垂直线 |
| 3 | (100,100) | (0,100) | 纯水平线 |
| 4 | (0,100) | (0,0) | 纯垂直线 |
| 5 | (0,0) | (100,100) | 对角线 |

**I_LOVE_YOU.dxf**:
| 段号 | 起点 | 终点 | 几何特征 |
|------|------|------|----------|
| 1 | (10,20) | (10,80) | **纯垂直线** (dx=0, dy=60) - 字母"I" |
| 2+ | ... | ... | 其他字母笔画 |

### 2.2 "单轴响应"的正确解释

**首段本身就是单轴运动**：
- `demo_100x100_rect_diag.dxf` 的首段是纯水平线，只有X轴变化
- `I_LOVE_YOU.dxf` 的首段是纯垂直线，只有Y轴变化

**这是几何事实，不是"CMP触发轴导致单轴运动"**。

运动通过 `MC_LnXY` 驱动，该函数本身支持双轴联动，但首段的几何特征决定了它表现为单轴运动。

---

## 3. 初步假设（待验证）

以下是代码分析中发现的潜在问题点，但**缺乏证据证明它们是根因**：

### 3.1 假设1：每段清空缓冲区

**位置**: `DXFDispensingExecutionUseCase.Segments.cpp:46, 199`

**观察**: 每段执行前都调用 `ClearInterpolationBuffer`

**逻辑漏洞**:
- 如果这会阻断执行，首段也应受影响，但首段能执行
- 它最多影响"批量预先入队"的能力，而非"能否进入下一段"的必要条件

**结论**: ❌ 不构成根因证据

### 3.2 假设2：缺少 segment_id 参数

**位置**: `InterpolationAdapter.cpp:150, 170`

**观察**: `MC_LnXY`/`MC_ArcXYC` 的段号使用默认值0

**逻辑漏洞**:
- 段号通常是标识用途
- 当前流程是一段一启动（非批量入队），重复段号难以解释"首段之后无法继续"
- 报告未给出任何 API 返回码或硬件状态佐证

**结论**: ❌ 弱证据

### 3.3 假设3：MC_CrdStart 的 mask 参数

**位置**: `InterpolationAdapter.cpp:261`

**观察**: `MC_CrdStart(coord_sys, 0)` 传入 mask=0

**API签名分析** (`MultiCardCPP.h`):
```cpp
int MC_CrdStart(short mask, short option)
```

**逻辑漏洞**:
- 第一个参数更倾向"坐标系掩码"，不是"轴掩码"
- 报告建议的轴掩码 0x03 与该语义不一致
- 目前调用路径在坐标系1场景下传入 1 与掩码等价
- 难以解释"仅首段执行"

**结论**: ❌ 与接口语义不一致

---

## 4. 需要收集的证据

要确定真正的根因，需要收集以下信息：

### 4.1 API返回码

| 调用点 | 需要记录的信息 |
|--------|----------------|
| `MC_SetCrdPrm` | 返回码 |
| `MC_CrdClear` | 返回码 |
| `MC_LnXY` / `MC_ArcXYC` | 返回码 |
| `MC_CrdData` | 返回码 |
| `MC_CrdStart` | 返回码 |

### 4.2 坐标系状态

| 时机 | 需要记录的信息 |
|------|----------------|
| 段1执行前 | `MC_CrdStatus` 返回的 status 和 segment |
| 段1执行后 | `MC_CrdStatus` 返回的 status 和 segment |
| 段2执行前 | `MC_CrdStatus` 返回的 status 和 segment |
| 段2启动后 | `MC_CrdStatus` 返回的 status 和 segment |

### 4.3 硬件状态

| 检查项 | 需要记录的信息 |
|--------|----------------|
| 硬限位状态 | 各轴正负限位是否触发 |
| 报警状态 | 是否有轴报警 |
| 急停状态 | 是否触发急停 |

### 4.4 段切换日志

需要在以下位置添加详细日志：
- `WaitForMotionComplete` 的退出条件
- 段循环的进入和退出
- 任何 `break` 或 `return` 语句

---

## 5. 建议的调试步骤

### 5.1 增强日志

在 `DXFDispensingExecutionUseCase.Segments.cpp` 中添加：

```cpp
// 段循环开始时
SILIGEN_LOG_INFO_FMT_HELPER("=== 开始执行段 %u/%u ===", i, result.total_segments);

// 每个关键API调用后
SILIGEN_LOG_DEBUG_FMT_HELPER("ClearInterpolationBuffer 返回: %s",
    clear_result.IsSuccess() ? "成功" : clear_result.GetError().GetMessage().c_str());

// WaitForMotionComplete 返回后
SILIGEN_LOG_DEBUG_FMT_HELPER("WaitForMotionComplete 返回: %s",
    wait_result.IsSuccess() ? "成功" : wait_result.GetError().GetMessage().c_str());
```

### 5.2 状态查询

在 `InterpolationAdapter.cpp` 的 `StartCoordinateSystemMotion` 中已有状态查询：

```cpp
// 启动前检查坐标系状态
short crd_status_before = 0;
long segment_before = 0;
int status_result = wrapper_->MC_CrdStatus(coord_sys, &crd_status_before, &segment_before, 0);
SILIGEN_LOG_DEBUG("Before CrdStart: status=" + std::to_string(crd_status_before) + ...);
```

需要确认这些日志是否被输出，以及输出的值是什么。

### 5.3 复现测试

1. 使用简化的2段DXF文件测试
2. 记录完整的执行日志
3. 对比段1和段2的API调用序列和返回值

---

## 6. 相关文件索引

| 功能 | 文件路径 |
|------|----------|
| 段执行逻辑 | `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.Segments.cpp` |
| 运动等待 | `src/application/usecases/dispensing/dxf/DXFDispensingExecutionUseCase.Control.cpp` |
| 插补适配器 | `src/infrastructure/adapters/motion/controller/interpolation/InterpolationAdapter.cpp` |
| MultiCard API | `src/infrastructure/drivers/multicard/MultiCardCPP.h` |
| DXF文件 | `uploads/dxf/demo_100x100_rect_diag.dxf`, `uploads/dxf/I_LOVE_YOU.dxf` |

---

## 7. 结论

### 7.1 当前状态

| 项目 | 状态 |
|------|------|
| "单轴响应"现象 | ✅ 已解释（首段几何特征） |
| "只执行首段"根因 | ❌ **未确定** |
| 证据链 | ❌ **缺失** |

### 7.2 下一步行动

1. **收集证据**: 按第4节要求收集API返回码、坐标系状态、硬件状态
2. **增强日志**: 按第5.1节添加详细日志
3. **复现测试**: 使用简化DXF文件复现问题并记录完整日志
4. **重新分析**: 基于收集的证据重新分析根因

---

## 附录：原始假设的逻辑漏洞总结

| 假设 | 逻辑漏洞 | 评估 |
|------|----------|------|
| 每段清空缓冲区 | 若阻断执行，首段也应受影响 | ❌ 不成立 |
| 缺少 segment_id | 一段一启动模式下，段号重复不影响执行 | ❌ 弱证据 |
| MC_CrdStart mask=0 | 参数语义是坐标系掩码，非轴掩码 | ❌ 语义不一致 |
| CMP触发轴选择 | 首段本身就是单轴几何 | ❌ 误解现象 |

---

*本报告承认分析的局限性，需要进一步收集证据才能确定真正的根因。*
