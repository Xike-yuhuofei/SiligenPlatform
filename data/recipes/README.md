# recipes

owner: `packages/process-runtime-core` 配方域 + `packages/runtime-host` 配方仓储适配器

canonical 说明：

- `recipes/`：手工维护的配方主记录
- `versions/`：版本化配方记录
- `templates/`：按模板 ID 拆分后的模板源文件
- `audit/`：运行时业务审计 seed；可保留、不可当作手工编辑主源

兼容说明：

- legacy `templates.json` 已拆分为 `templates/*.json`
- 运行时代码默认只读取根级 `data/recipes/`
- `control-core/data/recipes` 已退出默认 fallback 链路，只保留历史残留价值，不再作为 source of truth
