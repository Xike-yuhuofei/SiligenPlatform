# recipes

owner: `control-core` 配方域 + `packages/runtime-host` 配方仓储适配器

canonical 说明：

- `recipes/`：手工维护的配方主记录
- `versions/`：版本化配方记录
- `templates/`：按模板 ID 拆分后的模板源文件
- `audit/`：运行时业务审计 seed；可保留、不可当作手工编辑主源

兼容说明：

- legacy `templates.json` 已拆分为 `templates/*.json`
- 旧代码如仍从 `control-core/data/recipes` 读取，当前通过运行时桥接继续可用
