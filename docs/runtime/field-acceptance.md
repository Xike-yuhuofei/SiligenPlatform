# Field Acceptance

更新时间：`2026-03-26`

## 1. 结论摘要

现场验收以单轨入口为准，覆盖以下层级：

- mock：HMI client -> TCP mock server -> mock core 行为
- simulation：工程数据导出与仿真回归
- protocol compatibility：application / engineering / transport 契约兼容
- HIL：`tests/e2e/hardware-in-loop/run_hil_controlled_test.ps1`（60s 快速门禁 + 1800s 长稳）
- real hardware：真机执行结果（按现场记录归档）

当前验收口径：

- `mock`、`simulation`、`protocol compatibility` 应保持通过。
- `first-layer`、`mock`、`simulation` 只能证明协议、状态和最小链路口径，不等同设备动作验收。
- `HIL` 长稳以 `pause/resume=3` 为默认门槛，且 `failed=0`、`known_failure=0`、`skipped=0`。
- `real hardware` 结论用于现场可运行性确认，不等同工艺质量签收。
- `hardware smoke` 只能证明最小启动闭环，不构成正式发布的现场放行依据。
- 正式发布必须满足 [docs/runtime/release-readiness-standard.md](/D:/Projects/SiligenSuite/docs/runtime/release-readiness-standard.md) 中 `G8` 的要求。

## 2. 证据位置

- HIL 报告根：`tests/reports/hil-controlled-test/`
- 时间戳批次：`tests/reports/hil-controlled-test/<timestamp>/`
- 统一验证报告：`tests/reports/workspace-validation.md`
- 真机回归历史记录：见 [docs/validation/history/dxf/README.md](/D:/Projects/SiligenSuite/docs/validation/history/dxf/README.md)

## 3. 关键链路检查

- HMI -> Gateway -> Runtime 链路可启动、可执行、可结束。
- DXF 加载/执行在 simulation 与 HIL 均有通过证据。
- pause/resume 状态转换满足门槛。
- 报警/异常恢复必须有专项场景证据，不能只作为建议项保留。

## 4. 当前验收结论模板

- `mock/simulation 替代验收`：通过/阻断
- `HIL 受控闭环长稳`：通过/阻断
- `real hardware 可运行性`：通过/阻断
- `工艺质量签收`：独立流程，不由本页替代
- `正式发布现场门禁`：只有 `HIL` 与 `real hardware` 均满足最低要求时，才可判定为通过
