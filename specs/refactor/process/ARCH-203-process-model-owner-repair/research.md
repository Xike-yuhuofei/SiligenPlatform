# Research: ARCH-203 Process Model Owner Boundary Repair

## Decision 1: 先把 2026-03-31 评审包纳入当前 worktree，再开始实现

- Decision: 将 9 份模块评审和 1 份复核报告视为 `Wave 0` 的必备输入；在当前 clean worktree 缺失这些文档时，不进入 implementation。
- Rationale: 本特性的所有修复决策都要求回链到这批评审资产；如果实现发生在没有基线文档的 clean worktree 中，任务、代码和证据将无法自洽。
- Alternatives considered:
  - 只依赖 `spec.md` 继续实现：会丢失逐模块事实和证据索引，无法完成细粒度追溯
  - 在 sibling worktree 中继续实现：会破坏 clean worktree 的隔离价值，重新回到脏工作树风险

## Decision 2: 使用复核报告修正后的结论作为唯一事实口径

- Decision: 任何后续任务、plan、contracts 和 evidence 都必须以复核报告纠偏后的结论为准，尤其是 `dispense-packaging` 对 `M9` 依赖归因的修正。
- Rationale: 复核报告已经确认原评审里存在 1 个实质性事实错误；如果不统一口径，后续整改方向会继续跑偏。
- Alternatives considered:
  - 同时保留原评审与复核表述：会制造双轨结论，无法形成唯一实施方向
  - 忽略复核报告：会让已知事实错误继续污染后续任务分解

## Decision 3: 以 owner seam 划分整改波次

- Decision: 按 seam 风险排序拆分为 `Wave 0 baseline import`、`Wave 1 M0/M9 seam`、`Wave 2 M4-M8 主链交接`、`Wave 3 M3/M11 + consumer cutover`、`Wave 4 文档/构建/测试/库存同步`。
- Rationale: `workflow/runtime-execution` 的双向耦合和主链交接错位是其它模块问题的上游约束；先切这里，后续模块才能在稳定边界上收口。
- Alternatives considered:
  - 按模块数量平均分波次：不能反映依赖图，容易先做下游再返工
  - 一次性全量收口九模块：风险过大，难以做独立验证

## Decision 4: 兼容壳只能是显式、限时、禁止新逻辑的过渡层

- Decision: 所有 legacy header、compat facade、shadow implementation、duplicate contract tree 都必须登记为 `只读兼容壳`、`临时 bridge` 或 `移除目标`，并带退出条件。
- Rationale: 很多模块问题不是“没有 owner”，而是“旧 owner 没退出”；没有退出约束的兼容壳会再次变成新的 live owner。
- Alternatives considered:
  - 保留兼容壳但不写退出条件：风险不可控，容易形成长期并存
  - 一步删除全部旧壳：对当前多消费者仓库风险过高，且缺乏波次缓冲

## Decision 5: 宿主和消费者必须只依赖 canonical public surface

- Decision: 把 `apps/hmi-app`、`apps/planner-cli`、`apps/runtime-gateway`、`apps/runtime-service`、`workflow` 以及验证脚本都纳入消费者契约，显式列出 allowed/forbidden surfaces。
- Rationale: 当前边界漂移大多发生在“模块外的消费者直接连内部实现”；如果不把消费者纳入 contract，模块 owner 迁移会持续被反向侵入。
- Alternatives considered:
  - 只定义模块侧 owner，不定义消费者规则：无法堵住反向依赖回流
  - 靠 code review 约束消费者：不可自动复查，也缺少明确判定基线

## Decision 6: 规划阶段只冻结验证策略，不提前宣称通过

- Decision: planning 阶段明确根级入口与定向验证策略，但不在 plan 里伪报任何 build/test 已通过的状态。
- Rationale: 当前 new worktree 连 baseline review pack 都不完整，提前写“已通过”会直接违反事实导向原则。
- Alternatives considered:
  - 沿用其它 feature 的已验证文案模板：与当前真实状态不符
  - 完全不写验证策略：后续 tasks 缺少可执行 gate
