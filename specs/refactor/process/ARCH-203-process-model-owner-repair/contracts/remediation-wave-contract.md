# Remediation Wave Contract

## 1. 目的

将 `ARCH-203` 的边界修复分解为可独立验收的波次，确保每一波都有明确的进入条件、退出条件和阻断项。

## 2. 波次定义

| Wave | Focus | Entry Criteria | Exit Criteria | Blocked By |
| --- | --- | --- | --- | --- |
| `Wave 0` | baseline import + evidence freeze | 当前分支与 spec/plan 已存在 | 10 份 `2026-03-31` 基线文档已进入当前 worktree 并可被 tasks/evidence 直接引用；4 处可追溯性补强要求已登记到 closeout 清单 | 当前分支缺失 baseline review pack，或 review pack 未被当前 closeout 账本引用 |
| `Wave 1` | `workflow <-> runtime-execution` seam + build spine retirement rules | `Wave 0` 完成 | `M0` 只保留编排/compat；`M9` owner seam、public surface 和双向耦合处置路径冻结；旧 build spine/helper target 的退场或限用规则明确 | 基线文档缺失；M9 owner 仍无唯一落点；旧 build spine 仍被默认视为 canonical |
| `Wave 2` | `M4 -> M8` 主链交接 + external input consumers | `Wave 1` 的 execution seam 已冻结 | `ProcessPlan`、对齐、路径、运动规划、packaging 的 canonical handoff 明确，`job-ingest/dxf-geometry` 的 reverse concrete dependency 与 duplicate adapter 处置就位 | 上游 handoff 仍依赖 workflow 隐含上下文；外部输入消费者仍直连 concrete |
| `Wave 3` | `M3` / `M11` + consumer cutover | `Wave 1` 和 `Wave 2` 的公共交接面已明确 | `topology-feature`、`hmi-application` 与主要宿主消费者的接入规则冻结；app 侧 compat re-export、owner 单测、无关链接依赖已处置 | canonical public surface 仍不稳定；宿主仍保留 owner 级入口 |
| `Wave 4` | docs/build/tests/inventory sync + review traceability补强 | 前三波已形成稳定 owner 图 | 文档、构建、测试、模块库存与 evidence 全部回写一致；复核报告要求的哈希/diff/精确索引已补齐到对应评审文档或附录 | 任意模块仍处于“文档已改、代码未收口”状态；评审证据仍不可复查；任一根级入口或 local gate 未通过 |

## 3. 波次规则

1. 不允许跳过 `Wave 0` 直接进入代码实现。
2. `Wave 2` 不得在 `Wave 1` 未明确 `M0/M9` seam 前开始。
3. `Wave 4` 只能在前三波完成后执行，因为它负责收尾一致性，不负责替代 owner 修复本身。
4. 复核报告指出的可追溯性补强，只能在 `Wave 4` 完成时一并关闭，不能默认为可延期项。

## 4. 统一完成标准

每一波完成都必须至少提供：

1. 一份需求到实施点的追踪矩阵或等价 evidence。
2. 一组 boundary script / build/test / doc review 证据。
3. 一份明确列出 remaining blockers 的状态说明。
