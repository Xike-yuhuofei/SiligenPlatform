# Doc Sync Report

- Branch: feat/dispense/NOISSUE-e2e-flow-spec-alignment
- BranchSafe: feat-dispense-NOISSUE-e2e-flow-spec-alignment
- Ticket: NOISSUE
- Timestamp: 20260322-134203
- Scope: 目录重构准备阶段的基线固化、阻塞清单纠偏、Wave 1 shared 边界冻结

## 文档变更列表

1. `docs/process-model/plans/NOISSUE-test-plan-20260322-130211.md`
   - 状态由准备态更新为“已采集基线”
   - 回写本轮 `apps/packages/integration/protocol-compatibility/simulation/legacy-exit` 的实际结果

2. `docs/process-model/plans/NOISSUE-baseline-status-20260322-134203.md`
   - 新增基线状态文档
   - 固化命令、结果、报告目录与阶段判断

3. `docs/process-model/plans/NOISSUE-path-inventory-20260322-130211.md`
   - 修正执行硬绑定口径
   - 明确 `integration/reports` 的治理权重高于粗量命中
   - 下调 `data/recipes` 的执行绑定强度，避免把说明文本和历史记录当 runtime 依赖

4. `docs/process-model/plans/NOISSUE-blocker-register-20260322-134203.md`
   - 新增 blocker register
   - 按 `Wave 1 / Wave 2A / Wave 2B` 分类写死 blocker、owner、解除条件和是否阻断物理迁移

5. `docs/architecture/shared-boundary-freeze-wave1.md`
   - 新增 Wave 1 shared 目标边界冻结文档
   - 明确 `Wave 1` 为 Go，shared 物理迁移为 No-Go

6. `docs/architecture/dependency-rules.md`
   - 新增 `Wave 1 shared target boundary（目标态，不代表已迁移）`
   - 不改写当前 canonical 依赖事实，只补目标边界说明

7. `docs/process-model/plans/NOISSUE-stage-plan-20260322-134203.md`
   - 新增下一阶段执行版阶段计划
   - 把允许事项、禁止事项、子任务和阶段完成条件写死

8. `packages/test-kit/src/test_kit/workspace_validation.py`
   - 将 Wave 1 轻量门禁接入默认 `packages` suite
   - 让 shared 冻结校验随 workspace validation 一起执行

9. `tools/migration/validate_shared_freeze.py`
   - 新增 shared 冻结校验脚本
   - 只负责边界扩张和越界引用检查，不改 canonical 事实

10. `shared/README.md`
   - 新增 shared 目标骨架说明
   - 明确 `shared/*` 当前只承载边界说明、占位文件和迁移锚点

11. `tools/migration/shared_freeze_manifest.json`
   - 新增 `packages/shared-kernel` 与 `packages/test-kit` 的冻结快照
   - 为 Wave 1 轻量门禁提供精确比对基线

12. `docs/process-model/plans/NOISSUE-wave1-exit-checklist-20260322-140814.md`
   - 新增 Wave 1 正式出口清单
   - 明确 `Wave 1 Exit = Pass` 的含义与 No-Go 边界

13. `docs/process-model/plans/NOISSUE-wave2a-path-bridge-20260322-140814.md`
   - 新增 Wave 2A path bridge 设计
   - 把 `packages/engineering-data` 的 3 个默认入口替换目标写死为 bridge 方案

## 非文档工件

1. 新增占位目录：
   - `shared/contracts/application/.gitkeep`
   - `shared/contracts/device/.gitkeep`
   - `shared/contracts/engineering/.gitkeep`
   - `shared/kernel/.gitkeep`
   - `shared/testing/.gitkeep`
   - `shared/logging/.gitkeep`
2. 这些文件只用于让 Wave 1 目标骨架进入版本控制，不表示实现已迁移。

## 关键差异摘要

1. 当前阶段已经从“只做准备性判断”进入“治理落地”，证据链已固化到 `integration/reports/verify/dir-refactor-prep/*`。
2. `Wave 1` 已被正式限定为 shared 边界冻结阶段，而不是 shared 源码迁移阶段。
3. `Wave 1` 轻量门禁已经接入默认 workspace validation 的 `packages` suite，用于防止 shared 目标边界继续被实现扩张。
4. `Wave 2A` 与 `Wave 2B` 的物理迁移阻塞项已明确登记，当前没有被弱化或隐藏。
5. `Wave 1` 已具备正式出口条件，下一步应进入 `Wave 2A` 的 bridge 准备，而不是直接启动物理迁移。

## 刻意未改动的文档

1. `README.md`
   - 当前没有对外运行方式或交付行为变化，不应为治理冻结补写新行为

2. `docs/runtime/*`
   - 当前未修改 runtime 默认路径、启动命令或排障入口，不应伪造行为变化

3. `docs/architecture/build-and-test.md`
   - 本轮只采集并引用现有基线，没有改变根级 build/test/CI 接口

## 待确认项

1. 候选规格目录 `docs/architecture/点胶机端到端流程规格说明/` 当前仍未纳入版本控制，继续只作为参考输入，不作为门禁基线。
