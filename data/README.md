# data

根级 `data/` 现在承接工作区级 canonical 数据资产。

当前约定：

- `data/recipes/`：配方主数据、版本、模板，以及必要的审计 seed
- `data/schemas/`：配方 schema 与工程契约 schema/proto
- `data/baselines/`：deprecated redirect，仅保留迁移说明，不再承载 canonical baseline/fixture 真值

边界说明：

- `uploads/`、`build/`、`tmp/`、`logs/` 不属于 `data/`
- 运行时生成但需要受控保留的结构化资产，必须单独标注为“运行时业务产物”或“基线”
- 手工维护源文件与生成产物不可混放在同一 canonical 子目录
