# 数据资产布局

更新时间：`2026-03-19`

## 1. 目标

把配方、schema、示例、回归基线收口到工作区根级 `data/` 与 `examples/`，并让运行时代码只认这些 canonical 路径。

## 2. 分类规则

| 分类 | 定义 | canonical 落位 | 备注 |
|---|---|---|---|
| 源码资产 | 人工维护、被业务逻辑直接消费的正式数据 | `data/recipes/`、`data/schemas/`、`examples/` | 必须有 owner 和 canonical 路径 |
| 运行时业务产物 | 运行时产生，但仍属于业务记录的一部分 | `data/recipes/audit/` | 可保留 seed，不应被误判为日志 |
| 回归基线/派生夹具 | 用于契约比对、仿真回归的稳定样本 | `data/baselines/`、`examples/replay-data/` | 必须明确“可比对”或“演示”用途 |

## 3. Canonical 目录与 owner

| 资产 | owner | canonical 路径 | 当前说明 |
|---|---|---|---|
| 配方主记录 / 版本 / 模板 / 审计 seed | `packages/process-runtime-core` 配方域 + `packages/runtime-host` | `data/recipes/` | runtime-host 默认只读取这里 |
| 配方参数 schema | `packages/process-runtime-core` 配方域 | `data/schemas/recipes/default.json` | runtime-host 默认只读取这里 |
| DXF proto / JSON schema | `packages/engineering-contracts` | `data/schemas/engineering/dxf/v1/` | 工作区公开 canonical，owner 副本仍保留在包内 |
| 工程二进制夹具 | `packages/engineering-contracts` | `data/baselines/engineering/` | 用于协议和导出链路回归 |
| 仿真回归基线 | `packages/simulation-engine` + `integration` | `data/baselines/simulation/` | 用于仿真结果稳定性比对 |
| DXF / 仿真 / 配方演示输入 | 各对应 owner | `examples/` | 共享给文档、仿真与演示脚本 |

## 4. 默认消费规则

- `packages/runtime-host` 默认只解析：
  - `data/recipes/`
  - `data/schemas/recipes/`
- `control-core/data/recipes/` 与 `control-core/src/infrastructure/resources/config/files/recipes/schemas/` 已退出默认 fallback 链路。
- `packages/engineering-contracts/proto/` 与 `packages/engineering-contracts/schemas/` 仍保留 owner 副本，但不是工作区公开入口。

## 5. 风险说明

1. 根级 canonical 与 package owner 副本同时存在，短期内有内容漂移风险；迁移期必须以根级文档为准。
2. `data/recipes/audit/` 属于运行时业务产物，后续仍建议引入 seed 与真实运行数据分层策略。
3. `control-core/data/recipes/*` 当前已没有默认运行价值，但物理目录仍作为历史残留存在，删除前仍需完成仓内 provenance 审计。
