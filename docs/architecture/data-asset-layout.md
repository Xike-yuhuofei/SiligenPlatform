# 数据资产布局

更新时间：`2026-04-23`

## 1. 目标

把工程 schema、稳定样本与回放输入收口到工作区根级 `data/`、`samples/` 与 `shared/contracts/engineering/fixtures/`，并让运行时代码只认这些 canonical 路径。

## 2. 分类规则

| 分类 | 定义 | canonical 落位 | 备注 |
|---|---|---|---|
| 源码资产 | 人工维护、被业务逻辑直接消费的正式数据 | `data/schemas/`、`samples/` | 必须有 owner 和 canonical 路径 |
| 契约 fixture / 稳定样本 | 用于契约比对、仿真回归与演示输入的稳定样本 | `shared/contracts/engineering/fixtures/`、`samples/` | 必须明确 authority 或 consumer |
| 已降级 baseline redirect | 历史基线路径，占位用于防回流或 provenance 审计 | `data/baselines/` | 不是 live source of truth |

## 3. Canonical 目录与 owner

| 资产 | owner | canonical 路径 | 当前说明 |
|---|---|---|---|
| DXF proto / JSON schema | `shared/contracts/engineering/` | `data/schemas/engineering/dxf/v1/` | 工作区公开 schema 根；authority 由 engineering contracts 维护 |
| 工程契约 fixture | `shared/contracts/engineering/fixtures/` | `shared/contracts/engineering/fixtures/cases/` | 用于 `.pb`、preview artifact、simulation input 的契约回归 |
| 仿真稳定样本 / 回放输入 | 对应 consumer owner + `shared/testing/` 目录约束 | `samples/simulation/`、`samples/replay-data/` | 用于仿真与回放链路的稳定输入 |
| DXF 演示输入 | 各对应 owner | `samples/` | 共享给文档、仿真与演示脚本 |

## 4. 默认消费规则

- runtime consumer 默认只解析：
  - `data/schemas/engineering/dxf/v1/`
- `data/schemas/engineering/dxf/v1/` 是工作区公开 schema 根；长期 authority 由 `shared/contracts/engineering/` 维护。
- `shared/contracts/engineering/fixtures/` 是工程契约 fixture 的正式 authority；`data/baselines/**` 不再承担 live fixture 职责。
- `data/recipes/**` 与 `data/schemas/recipes/**` 已随 recipe management 退场而删除，不再是任何 runtime 或测试链路的 canonical 输入。

## 5. 风险说明

1. `data/`、`samples/` 与 `shared/contracts/engineering/fixtures/` 同时承载不同类型资产，后续仍需持续避免 authority 漂移。
2. `data/baselines/**` 当前已降级为 deprecated redirect；若重新承载 live 资产，应由 workspace gate 直接阻断。
