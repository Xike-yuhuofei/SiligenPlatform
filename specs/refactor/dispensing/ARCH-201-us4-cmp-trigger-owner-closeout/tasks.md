# ARCH-201 US4 CMP Trigger Owner Closeout Tasks

- [x] Create dedicated US4 evidence bundle and boundary notes.
- [x] Freeze `M8 dispense-packaging` as the sole trigger/CMP business owner in code comments, README/contracts touchpoints, and validation rules.
- [x] Remove or demote live trigger/CMP owner logic from `modules/workflow/domain/domain/dispensing` so workflow only consumes `M8` authority artifacts.
- [x] Demote `modules/motion-planning` trigger/CMP code to math/helper-only seams or redirect consumers to `M8` owner artifacts.
- [x] Ensure preview authority and execution-preparation paths share one `M8` truth source.
- [x] Add scoped bridge checks and targeted regressions for forbidden trigger/CMP owner leaks.
- [x] Run targeted validation and record pass/fail evidence with explicit out-of-scope failures, if any.

## Task Breakdown

### Task 1

- 名称：冻结 US4 owner 边界
- 目标：把 `M8 owner / M7 helper / workflow consumer` 的边界写入验证与文档锚点
- 输入：US3 spec bundle、ADR-005、motion-planning 架构审查、当前 README 与 owner 入口
- 输出：可执行的边界说明和禁止项列表
- 依赖：无
- 验证方式：spec/plan/tasks 与边界规则一致，未把 ADR-005 或 US3 问题带入
- 停止条件：若发现 authority artifact 不是 `M8`，先暂停实现并重做契约冻结

### Task 2

- 名称：收掉 workflow 重复 owner
- 目标：让 workflow 不再保留 live trigger/CMP 规划 owner 实现
- 输入：`modules/workflow/domain/domain/dispensing/*`
- 输出：consumer-only workflow 路径或 shim residue
- 依赖：Task 1
- 验证方式：workflow 相关 live owner 源文件不再承载 trigger/CMP 业务真值；相关用例仍能消费 `M8` 结果
- 停止条件：若需要新增 workflow 业务真值对象才能继续，停止并回到边界评估

### Task 3

- 名称：收掉 M7 trigger/CMP 业务 owner
- 目标：让 `M7` 只保留轨迹/数学 helper，不再定义 trigger/CMP 业务语义
- 输入：`TriggerCalculator`、`CMPCoordinatedInterpolator` 及其消费者
- 输出：helper-only seam、迁移后的调用链或兼容转发
- 依赖：Task 1
- 验证方式：`M7` 不再是 preview/execution trigger truth 来源；consumer 转向 `M8`
- 停止条件：若改动会连带重写运动规划主契约，则停止并拆出后续批次

### Task 4

- 名称：统一 preview 与 execution truth
- 目标：确保 preview glue points、authority trigger points 与 execution-preparation 来自同一 `M8` 权威工件
- 输入：`DispensePlanningFacade`、workflow planning/usecase 链、planner CLI preview 入口
- 输出：单一 truth-source 消费链
- 依赖：Task 2、Task 3
- 验证方式：相关单测和 use-case 测试通过，且 `preview_authority_ready` 链路不再走本地重建
- 停止条件：若需要新增协议字段或改变外部 CLI/UI 行为，停止并留到新批次

### Task 5

- 名称：补边界门禁与验证证据
- 目标：防止 trigger/CMP owner 回流
- 输入：US4 实际落地代码
- 输出：bridge checks、测试结果、US4 evidence
- 依赖：Task 2、Task 3、Task 4
- 验证方式：目标测试和边界检查执行完成
- 停止条件：若仅剩已知批次外失败，明确记录并停止，不扩大修复范围

## 串并行关系

- 串行：Task 1 -> Task 4 -> Task 5
- 并行：Task 2 与 Task 3 可在 Task 1 后并行推进，但都必须在 Task 4 前收敛

## Highest Risk

- workflow 仍保留可编译的 trigger/CMP owner concrete，导致迁移后出现双真值
- `M7` 中残留的 CMP/trigger helper 与业务 owner 语义边界切分不清，只换目录不换真值
