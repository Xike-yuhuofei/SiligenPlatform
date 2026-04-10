# ADR-005 CMP Runtime Config Boundary

- 状态：`Accepted`
- 日期：`2026-03-21`

## 决策

当前 DXF 真机点胶主链中的 CMP 运行时配置边界固定如下：

1. 运行时权威入口只允许使用 `config/machine/machine_config.ini` 中的 `[ValveDispenser]` 段。
2. `[CMP]` 全段在当前主链中降级为历史/兼容信息，不再作为运行时权威配置入口。
3. CLI 不再提供 `--cmp-channel` 这类会制造“命令行可覆盖主链配置”错觉的参数。
4. `MC_CmpBufData.nCmpEncodeNum` 在当前主链中的真实语义固定为“触发轴映射后的 SDK 轴号”，不再按 `encoder_num` 的历史命名理解。
5. 当前机型必须显式配置 `[ValveDispenser].cmp_axis_mask`，不再从 `[CMP].cmp_axis_mask` 回退。

## 背景

本次 DXF 真机点胶链路收口过程中，仓库内同时存在三类容易混淆的历史语义：

- 旧配置把 `[CMP]` 视为 CMP 触发的主入口。
- 部分代码和共享类型仍沿用 `encoder_num` 这类历史命名，但实际语义已经变成“位置比较源轴号”。
- CLI 曾暴露 `--cmp-channel` 等无消费点或无主链效果的参数，容易让现场误判配置生效路径。

这些混淆在真机诊断时会直接放大排障成本，甚至让现场修改无效配置。

## 依据

- 真机 DXF 点胶主链已经按 `[ValveDispenser]` 完成上传、规划、执行和真实点胶回归。
- 运行时配置加载已删除 `[CMP].cmp_axis_mask` 回退，并对遗留 `[CMP]` 字段输出兼容告警。
- `control-cli` 已删除 `--cmp-channel` 及一批无消费点的死参数。
- 机型文档、runbook 和迁入的 `Backend_CPP` 副本文档都已补充“当前 canonical 以 `[ValveDispenser]` 为准”的说明。

## 结果

- 当前主链中，修改 `[CMP]` 不应再改变 DXF 真机点胶运行时行为。
- 任何新的 CMP 运行时参数都必须先进入 `[ValveDispenser]`，再决定是否需要历史兼容映射。
- 任何新的 CLI 参数若不能形成真实消费链路，不得暴露到公共命令面。
- 历史根级 alias `config/machine_config.ini` 不再属于当前默认解析链路，只允许作为兼容背景或审计对象被提及。

## 后果

- 现场排障和配置变更时，首先检查 `[ValveDispenser]`，不再把 `[CMP]` 当作首选入口。
- 历史工具、外部文档或旧截图若仍引用 `[CMP]`，应按“历史背景”解释，而不是按当前主链事实执行。
- 后续若计划彻底删除 `[CMP]`，必须先确认不存在外部兼容依赖，再单独立项。
