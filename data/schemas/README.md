# schemas

owner 与 canonical：

- `recipes/default.json`
  owner: `modules/workflow` 配方域
  canonical: `data/schemas/recipes/default.json`
- `engineering/dxf/v1/dxf_primitives.proto`
  owner: `shared/contracts/engineering`
  canonical: `data/schemas/engineering/dxf/v1/dxf_primitives.proto`
- `engineering/dxf/v1/*.schema.json`
  owner: `shared/contracts/engineering`
  canonical: `data/schemas/engineering/dxf/v1/*.schema.json`

兼容说明：

- 运行时代码默认只读取 `data/schemas/recipes/`
- `control-core/src/infrastructure/resources/config/files/recipes/schemas/` 已退出默认 fallback 链路
- `shared/contracts/engineering/` 与根级 `data/schemas/` 共同构成当前工程契约的 canonical 落点
