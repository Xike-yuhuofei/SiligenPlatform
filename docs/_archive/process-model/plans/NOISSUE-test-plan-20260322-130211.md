# NOISSUE 测试计划 - 整仓目录重构前置准备（适配版候选骨架）

- 状态：已采集基线
- 日期：2026-03-22
- 分支：feat/dispense/NOISSUE-e2e-flow-spec-alignment
- 关联范围文档：NOISSUE-scope-20260322-130211.md
- 关联架构文档：NOISSUE-arch-plan-20260322-130211.md

## 1. 关键路径

1. 关键路径 A：当前 canonical 入口基线冻结
   - 目标：在任何目录迁移前，根级 build/test/CI、app dry-run 和默认运行资产路径都有一份明确基线
2. 关键路径 B：路径依赖盘点
   - 目标：确认 CMake、脚本、CI、文档、运行时代码对现有路径的硬绑定范围
3. 关键路径 C：阻塞项冻结
   - 目标：确认 `control-core` residual dependency、`third_party` owner、过渡 `packages/*` 和 `tests/` 的保留理由
4. 关键路径 D：回滚单元冻结
   - 目标：确认后续每个波次都能按“目录 + gate + 文档 + 入口”成组回退

## 2. 边界场景

1. 工作树存在未跟踪文件时，准备工件仍需只覆盖本任务，不污染其他任务文档。
2. `NOISSUE` 下已有多个历史 scope/arch/test 文档时，新文档必须明确声明只服务当前目录重构准备任务。
3. `control-core` 仍是 source-root owner 时，不能误判为“目录重构可直接切断旧结构”。
4. `packages/engineering-data/scripts/dxf_to_pb.py` 等路径仍为运行链默认值时，不能把 `packages/*` 误判为可立即删除。
5. `config/`、`data/`、`examples/` 已承担运行资产责任时，不能回退到候选 `samples/` 语义。

## 3. 回归基线

1. 入口基线
   - `apps/control-runtime/run.ps1 -DryRun`
   - `apps/control-tcp-server/run.ps1 -DryRun`
   - `apps/control-cli/run.ps1 -DryRun`
2. 构建/测试基线
   - `build.ps1`
   - `test.ps1`
   - `ci.ps1`
   - `legacy-exit-check.ps1`
3. 运行资产基线
   - `config/machine/machine_config.ini`
   - `data/recipes/`
   - `data/schemas/recipes/`
   - `examples/`
4. 文档/报告基线
   - `integration/reports/*`
   - `docs/architecture/*`
   - `docs/runtime/*`

## 4. 测试层级与命令（优先根级命令）

### 4.1 基线冻结命令与结果

1. 记录当前工作树状态
   - 命令：`git status --short`
   - 预期：输出被归档到本次准备证据中；后续目录重构不与无关脏变更混淆
   - 结果：已执行；工作树中存在既有无关修改 `examples/dxf/rect_diag_glue_points.csv`，未在本轮基线采集中改动

2. 记录工作流上下文
   - 命令：`.\tools\scripts\resolve-workflow-context.ps1`
   - 预期：输出当前 `ticket / branch / timestamp`
   - 结果：已执行；当前上下文为 `feat/dispense/NOISSUE-e2e-flow-spec-alignment`，`timestamp=20260322-134203`

3. 冻结 app dry-run 基线
   - 命令：
     - `apps/control-runtime/run.ps1 -DryRun`
     - `apps/control-tcp-server/run.ps1 -DryRun`
     - `apps/control-cli/run.ps1 -DryRun`
   - 预期：
     - 输出 `source: canonical`
     - 不再探测 legacy fallback
   - 结果：已执行且全部通过；三者均输出 `source: canonical`

4. 采集基线报告
   - 结果目录：`integration/reports/verify/dir-refactor-prep/*`
   - 结果：`apps`、`packages`、`integration`、`protocol`、`legacy-exit`、`simulation` 均已生成对应报告，未出现 `new failure`

### 4.2 根级门禁命令

1. Legacy 回流门禁
   - 命令：`.\legacy-exit-check.ps1 -Profile CI -ReportDir integration\reports\verify\dir-refactor-prep\legacy-exit`
   - 预期：无新增失败；报告生成 `legacy-exit-checks.md/json`
   - 结果：已执行且通过；`passed_rules=18`，`failed_rules=0`

2. Apps 套件
   - 命令：`.\build.ps1 -Profile CI -Suite apps`
   - 预期：control apps 产物校验通过
   - 命令：`.\test.ps1 -Profile CI -Suite apps -ReportDir integration\reports\verify\dir-refactor-prep\apps -FailOnKnownFailure`
   - 预期：apps suite 通过，已知失败保持既有口径，不新增未知失败
   - 结果：已执行且通过；`passed=16 failed=0 known_failure=0`

3. Packages 套件
   - 命令：`.\test.ps1 -Profile CI -Suite packages -ReportDir integration\reports\verify\dir-refactor-prep\packages -FailOnKnownFailure`
   - 预期：`application-contracts`、`engineering-contracts`、`engineering-data`、`transport-gateway` 相关验证持续通过
   - 结果：已执行且通过；`passed=5 failed=0 known_failure=0`

4. Integration 套件
   - 命令：`.\test.ps1 -Profile CI -Suite integration -ReportDir integration\reports\verify\dir-refactor-prep\integration -FailOnKnownFailure`
   - 预期：工程回归、HIL smoke 默认链路不回归
   - 结果：已执行且通过；`passed=1 failed=0 known_failure=0`

5. Protocol compatibility
   - 命令：`.\test.ps1 -Profile CI -Suite protocol-compatibility -ReportDir integration\reports\verify\dir-refactor-prep\protocol -FailOnKnownFailure`
   - 预期：协议/契约验证持续通过
   - 结果：已执行且通过；`passed=1 failed=0 known_failure=0`

6. Simulation
   - 命令：`.\build.ps1 -Profile CI -Suite simulation`
   - 预期：仿真 build 通过
   - 命令：`.\test.ps1 -Profile CI -Suite simulation -ReportDir integration\reports\verify\dir-refactor-prep\simulation -FailOnKnownFailure`
   - 预期：仿真 suite 通过
   - 结果：已执行且通过；`passed=5 failed=0 known_failure=0`

### 4.3 路径依赖盘点命令

1. 运行资产硬绑定盘点
   - 命令：
     - `rg -n "config/machine/machine_config.ini" .`
     - `rg -n "data/recipes|data/schemas/recipes" .`
   - 预期：形成运行资产引用清单，供后续 cutover 使用

2. 离线脚本硬绑定盘点
   - 命令：`rg -n "packages/engineering-data/scripts/dxf_to_pb.py" .`
   - 预期：形成 DXF 离线链默认路径引用清单

3. 过渡 owner 盘点
   - 命令：
     - `rg -n "packages/process-runtime-core" .`
     - `rg -n "packages/runtime-host" .`
     - `rg -n "packages/transport-gateway" .`
   - 预期：形成 `packages/*` 仍被直接引用的证据，作为 phase 0-2 保留依据

4. `control-core` 阻塞项盘点
   - 命令：
     - `rg -n "control-core/src/shared|control-core/src/infrastructure|control-core/modules/device-hal|control-core/third_party" .`
   - 预期：形成 residual dependency 证据
   - 结果：已按路径清单与架构审计完成归档，仍作为 wave 2 阻塞依据

## 5. 通过 / 阻断标准

1. 以下任一项不满足，则禁止进入物理目录迁移：
   - 未生成本任务专用 `scope / arch-plan / test-plan`
   - `legacy-exit` 出现新增失败
   - 任一根级 suite 相比当前基线新增未知失败
   - app dry-run 不再输出 canonical 来源
   - 当前 `config/`、`data/`、`examples/` 默认路径无法被证据链证明仍为 canonical
   - `control-core` residual dependency 未被显式列出
2. 通过条件：
   - 工件齐备
   - 基线命令可执行
   - 报告目录与预期结果明确
   - 目录映射与波次切换逻辑已冻结
   - 本次基线已全部采集完成，且未出现 `new failure`

## 6. 证据产物要求

1. 准备阶段至少保留以下证据：
   - `git status --short` 摘要
   - `resolve-workflow-context.ps1` 输出
   - `legacy-exit` 报告路径
   - 各 suite 报告路径
   - 三个 app 的 dry-run 结果摘要
   - 路径依赖 `rg` 清单摘要
2. 报告目录统一放到：
   - `integration/reports/verify/dir-refactor-prep/*`
3. 本次采集结果结论：
   - `apps`、`packages`、`integration`、`protocol-compatibility`、`simulation`、`legacy-exit` 均已完成采集
   - 所有命令结果均为通过，当前可进入下一阶段治理收口，但不进入物理迁移

## 7. 风险跟踪

1. 若 `packages/*`、`modules/*`、`shared/*` 的目标归宿在准备阶段仍有歧义，必须先补架构决策，不得先动目录。
2. 若 `control-core` source-root owner 与 `third_party` 迁出策略未锁定，禁止进入 wave 2 以后阶段。
3. 若运行资产路径计划被改写为候选 `samples/` / `shared/config` 语义，视为阻断，不得推进。
