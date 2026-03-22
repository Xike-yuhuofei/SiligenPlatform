# NOISSUE Wave 4E Scope - External Intake Preparation and Observation Rerun Landing Zone

- 状态：Prepared
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 工作流上下文：`ticket=NOISSUE`，`timestamp=20260322-215937`
- 上游收口：`docs/process-model/plans/NOISSUE-wave4d-closeout-20260322-212031.md`

## 1. 阶段目标

1. 在不重复 `Wave 4D` repo-side launcher payoff 的前提下，为下一次仓外观察建立独立的 evidence landing zone。
2. 把 `Wave 4D` intake 模板补成可直接执行的外部采集模板。
3. 把观察期文档从 `Wave 4C` 专属目录约束修正为“当前重跑阶段独立目录”约束。
4. 只做到“可采集、可重跑、可落证据”，不伪造任何仓外输入或观察结果。

## 2. in-scope

1. `docs/runtime/external-migration-observation.md`
2. `integration/reports/verify/wave4d-closeout/intake/*.md`
3. `integration/reports/verify/wave4e-rerun/`
4. `docs/process-model/plans/`、`docs/process-model/reviews/` 中本阶段新增工件

## 3. out-of-scope

1. 回写 `Wave 4C/4D` 历史结论
2. 重做 repo-side launcher 安装或验证
3. 伪造 field scripts / release package / rollback package 外部输入
4. 直接给出 `external migration complete declaration = Go`
5. 新的全量 build/test 或并行构建

## 4. 完成标准

1. 观察期文档明确要求把新一轮 intake/observation 证据落到当前阶段独立目录。
2. `Wave 4D` intake 模板包含可执行的采集命令和结果语义。
3. 仓内存在新的 `wave4e-rerun` 目录骨架，供外部输入到位后直接落证据。
4. 本阶段输出不把缺失输入写成 `Go`，也不把准备动作误写成 closeout。
