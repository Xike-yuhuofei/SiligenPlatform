# schemas

owner 与 canonical：

- `recipes/default.json`
  owner: `control-core` 配方域
  canonical: `data/schemas/recipes/default.json`
- `engineering/dxf/v1/dxf_primitives.proto`
  owner: `packages/engineering-contracts`
  canonical: `data/schemas/engineering/dxf/v1/dxf_primitives.proto`
- `engineering/dxf/v1/*.schema.json`
  owner: `packages/engineering-contracts`
  canonical: `data/schemas/engineering/dxf/v1/*.schema.json`

兼容说明：

- `control-core/src/infrastructure/resources/config/files/recipes/schemas/` 仅作为 fallback
- `packages/engineering-contracts/proto/` 与 `packages/engineering-contracts/schemas/` 仍保留 owner 视角下的实现副本，根级 `data/schemas/` 作为工作区公开 canonical 落点
