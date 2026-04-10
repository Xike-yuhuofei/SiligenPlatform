---
Owner: @Xike
Status: active
Last reviewed: 2026-01-18
Scope: 回零/接近开关
---

# 回零开关配置与验证(当前无硬限位)

> 本文档描述当前项目的回零接近开关接线方式、信号极性与验证方法。
> 当前硬件无硬限位，HOME 仅用于回零。若与其他文档冲突，以 `docs/machine-model/multicard-io-and-wiring-reference.md` 与本目录的 [06_reference.md](./06_reference.md) 为准。

## 1) 当前硬件约定

- 仅 X/Y 两轴。
- 回零接近开关仅用于回零，接 HOME1/HOME2。
- 回零方向: X/Y 负向回零。
- LIM1-4+/LIM1-4- 未接线，当前无硬限位保护。

## 2) 接线与IO映射

### 2.1 接线方式
- 回零接近开关类型: NPN NO (常开)
- X 轴回零开关 -> 控制卡 HOME1
- Y 轴回零开关 -> 控制卡 HOME2

### 2.2 IO映射与位含义
- `MC_GetDiRaw(MC_HOME)` 返回值:
  - bit0: 轴1(X) HOME
  - bit1: 轴2(Y) HOME

> HOME 信号只用于回零，不作为硬限位。

## 3) 信号极性(必须显式)

- HOME 触发时为高电平有效(未触发为低电平)。
- 代码侧假设: `kHomeActiveLow = false`。
- 传感器型号为 NPN NO，但当前输入逻辑以高电平有效为准；变更需同步更新文档与代码。

若现场电平相反, 需要修改:
- `src/infrastructure/adapters/motion/controller/control/MultiCardMotionAdapter.cpp`
- `src/infrastructure/adapters/diagnostics/health/testing/HardwareTestAdapter.cpp`
- 并同步更新 [06_reference.md](./06_reference.md)

## 4) 运动行为与安全策略

- 不启用硬限位监控。
- 运动互锁仅依赖软限位配置。
- HOME 信号仅用于回零流程，不作为点动/点位的硬限位条件。

## 5) 测试与验证

### 5.1 快速验证程序
`limit_switch_test.exe` 示例已移除，不再提供构建与运行。

判定要点:
- 未触发: HOME bit=0 (低电平)
- 触发: HOME bit=1 (高电平)
- LIM+/LIM- 不变化是正常现象(当前未接线)

### 5.2 现场验收建议
- 手动触发/释放 HOME 开关，bit 应立即翻转。
- 回零流程中 HOME 变化应与回零过程一致。

## 6) 代码位置

- HOME 读取: `src/infrastructure/adapters/diagnostics/health/testing/HardwareTestAdapter.cpp`
- 运动互锁(已关闭 HOME 作为硬限位): `src/infrastructure/adapters/motion/controller/control/MultiCardMotionAdapter.cpp`
- 硬限位监控默认关闭: `src/application/usecases/system/InitializeSystemUseCase.h`
