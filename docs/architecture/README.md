# Architecture

本目录是 `SiligenSuite` 工作区级架构文档的正式入口。

对 `Codex` 的默认读取顺序：

1. 根级锚点文档
2. `dsp-e2e-spec/` 正式冻结文档集
3. `topics/`、`validation/`、`governance/` 专题目录
4. `modules/` 模块映射入口
5. 只有在需要追溯专项审计、阶段性评审或收口过程时，才进入 `history/`

## 1. 根级锚点文档

### 工作区规则

- `workspace-baseline.md`
- `canonical-paths.md`
- `directory-responsibilities.md`
- `dependency-rules.md`
- `config-layout.md`
- `data-asset-layout.md`

### 系统入口与当前状态

- `dsp-e2e-spec/`
- `modules-owner-boundary.md`
- `legacy-cutover-status.md`

- `system-acceptance-report.md`

### 测试体系与运行门禁

- `build-and-test.md`

### 模块入口

- `modules/README.md`

## 2. 正式专题目录

### 冻结规格

- `dsp-e2e-spec/`：唯一正式冻结文档集
- `modules/`：模块 owner 与实施映射入口

### 主题专题

- `topics/dxf/`：DXF authority、几何语义与 motion 执行契约
- `topics/runtime-hmi/`：runtime supervision 与 HMI online 启动链
- `topics/sim-observer/`：sim observer 数据与交付说明
- `topics/dispense-preview/`：点胶轨迹预览专题

### 验证与基线

- `validation/`：测试体系、门禁、覆盖矩阵、性能基线

### 治理与迁移

- `governance/migration/`：legacy、cutover、migration、bridge exit 治理
- `governance/refactor-guard/`：refactor guard 基线、规则和 runbook
- `governance/boundaries/`：device/shared/third-party 边界专题

## 3. 历史文档区

历史材料统一放在 [history/](./history/README.md)，当前已分为：

- `history/module-boundary/`：模块边界评审、置信度审计、阶段性 owner 审查
- `history/audits/`：专项审计与盘点报告
- `history/baselines/`：历史基线与旧阶段指标快照
- `history/progress/`：strangler/progress 过程记录
- `history/closeouts/`：阶段性 closeout 记录

这些内容保留追溯价值，但不应作为当前正式架构规则的第一入口。

## 4. 使用规则

1. 当 `Codex` 需要理解当前系统结构、边界和依赖时，优先读本目录正式区
2. review、audit、progress、closeout 只用于解释“如何收敛到当前状态”，不直接承担当前真值
3. 若历史材料中的结论仍然有效，应在正式区形成稳定文档，而不是继续依赖历史稿
4. 根级只保留高频锚点文档；单一专题的设计、契约、状态机和交付说明统一进入对应子目录
