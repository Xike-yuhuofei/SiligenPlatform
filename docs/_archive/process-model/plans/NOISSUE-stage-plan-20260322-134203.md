# NOISSUE 阶段计划 - Wave 0 收口与 Wave 1 冻结执行版

- 状态：In Progress
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 关联范围文档：NOISSUE-scope-20260322-130211.md
- 关联架构文档：NOISSUE-arch-plan-20260322-130211.md
- 关联测试文档：NOISSUE-test-plan-20260322-130211.md

## 1. 阶段目标

本阶段只执行两类事情：

1. 把已确认的基线结果、阻塞项和 shared 目标边界固化成 authoritative 文档。
2. 把后续目录重构进入 Wave 2 前必须遵守的治理门禁写实。

本阶段不执行：

1. 业务源码物理迁移。
2. 根级 build/test/CI 图翻转。
3. `config/`、`data/`、`examples/`、`integration/` 的默认路径切换。
4. `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli`、`apps/hmi-app` 的外部命名调整。

## 2. 当前阶段结论

### 2.1 已满足的进入条件

1. `legacy-exit` 基线已采集，当前为全绿。
2. `apps`、`packages`、`integration`、`protocol-compatibility`、`simulation` 基线已采集，当前无 `new failure`。
3. `apps/control-runtime`、`apps/control-tcp-server`、`apps/control-cli` 的 `-DryRun` 仍指向 `source: canonical`。
4. 当前目录映射矩阵已经足以支撑 Wave 1 的 owner 冻结。

### 2.2 当前仍然阻塞 Wave 2 的事项

1. `Wave 2A` 仍被 `packages/engineering-data` 的默认脚本/目录入口阻塞：
   - `packages/engineering-data/scripts/dxf_to_pb.py`
   - `packages/engineering-data/scripts/path_to_trajectory.py`
   - `packages/engineering-data` 作为 `dxf_pipeline_path`
2. `Wave 2B` 仍被以下事项阻塞：
   - 根级 `CMakeLists.txt` / `tests/CMakeLists.txt` 仍把 `packages/process-runtime-core`、`packages/runtime-host`、`packages/transport-gateway` 当 canonical source root
   - runtime 默认资产路径仍固定在 `config/machine/machine_config.ini`、`data/recipes`、`data/schemas/recipes`
   - `process-runtime-core`、`device-adapters`、`device-contracts` 仍有 `control-core` residual dependency

### 2.3 对 Wave 1 的明确判断

1. `Wave 1`：Go
   - 允许做 shared 目标边界冻结、治理文档更新、轻量门禁落地。
2. `Wave 1` 物理迁移：No-Go
   - `shared-kernel` 仍混装 kernel/logging/testing/trace 与部分业务倾向类型
   - `application-contracts`、`engineering-contracts` 仍混有 fixtures/tests
   - `shared/logging` 的抽象与实现尚未解耦为可迁移状态

## 3. 执行子任务

### 3.1 子任务 A：基线结果固化

交付物：

1. `NOISSUE-test-plan-20260322-130211.md` 更新为“已采集基线”状态
2. 新增 baseline status 文档，统一记录本轮命令、结果与报告目录

完成条件：

1. 所有 suite 的结果都可从文档追溯到 `integration/reports/verify/dir-refactor-prep/*`
2. 文档明确区分 `passed / known-failure / new-failure`
3. 当前结论写死为“无 `new failure`”

### 3.2 子任务 B：路径清单纠偏与 blocker register

交付物：

1. `NOISSUE-path-inventory-20260322-130211.md` 修正为“执行硬绑定口径”
2. 新增 blocker register，按 `Wave 1 / Wave 2A / Wave 2B` 分类

完成条件：

1. `integration/reports` 的 live consumer 不再被低估
2. `data/recipes` 的执行硬绑定与文档/夹具引用明确分离
3. 每个 blocker 都有 owner、解除条件、影响范围与是否阻断物理迁移

### 3.3 子任务 C：shared 目标边界冻结

交付物：

1. 新增 Wave 1 shared boundary freeze 文档
2. 在治理文档中补一段明确的“目标态，不代表已迁移”说明
3. 将 Wave 1 轻量门禁接入默认 workspace validation 的 `packages` suite

完成条件：

1. `shared/contracts/application`
2. `shared/contracts/device`
3. `shared/contracts/engineering`
4. `shared/kernel`
5. `shared/testing`
6. `shared/logging`

以上 6 个目标边界均有明确职责、禁止承载内容和与现有 `packages/*` 的过渡关系，且轻量门禁已在默认验证链中生效。

### 3.4 子任务 D：阶段计划与文档同步收口

交付物：

1. 本阶段计划文档
2. doc sync report

完成条件：

1. 本轮新增/更新的工件列表完整
2. 明确哪些文档被修改、为什么修改、哪些文档刻意未改
3. 不把候选规格目录误写成正式门禁基线

### 3.5 子任务 E：Wave 1 出口与 Wave 2A bridge 准备

交付物：

1. `NOISSUE-wave1-exit-checklist-20260322-140814.md`
2. `NOISSUE-wave2a-path-bridge-20260322-140814.md`

完成条件：

1. `Wave 1 Exit = Pass` 的含义和边界被写死
2. `Wave 2A` 的 3 个默认入口绑定有统一 bridge 设计
3. 不把 `Wave 2A` 的设计准备误写成“允许物理迁移”

## 4. 执行门禁

1. 不得编辑 `examples/dxf/rect_diag_glue_points.csv` 的既有用户改动。
2. 不得修改根级 `build.ps1`、`test.ps1`、`ci.ps1` 的对外接口。
3. 不得改变 `integration/reports/verify/dir-refactor-prep/*` 的证据结果。
4. 不得把 `shared/*` 写成“已完成迁移”；只能写成“目标边界已冻结”。
5. 若发现任何基线报告与计划文档矛盾，以已落盘报告为准并回写文档。
6. 不得把 `packages` suite 的 shared 冻结门禁扩展为 runtime 行为或 build/test/CI 入口变更。

## 5. 阶段完成判定

本阶段完成时，必须同时满足：

1. 基线状态文档、路径清单、blocker register、shared boundary freeze、doc sync report 全部落盘。
2. `Wave 1` 的允许事项与禁止事项在文档中无冲突。
3. `Wave 2A` 与 `Wave 2B` 的阻塞项仍被明确保留，没有被提前删除或弱化。
4. 当前工作树除本轮文档工件外，不新增无关修改。
5. `Wave 1` 出口判断与 `Wave 2A` 设计边界在文档上无冲突。
