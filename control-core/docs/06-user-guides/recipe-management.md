# Recipe Management (TCP/HMI)

## Overview
This guide is retained as a recipe capability reference. The human-facing CLI client has been removed, and recipe operations should now be driven through HMI and the TCP backend.

## Prerequisites
- `siligen_tcp_server` running and reachable from HMI
- Write access to `data/recipes/`
- Parameter schema JSON files available under `data/recipes/schemas/`

## Recommended operation flow

### Create and publish a recipe
1. 在 HMI 中创建配方基础信息（名称、描述、标签、模板）。
2. 通过 HMI 表单或 TCP 请求创建草稿版本。
3. 更新参数并执行校验。
4. 发布版本并将其激活为生产版本。

对应的程序化能力边界如下：

- `createRecipe`
- `createDraftVersion`
- `updateDraftVersion`
- `publishVersion`

### List, inspect, and archive

- 在 HMI 中查看配方列表、详情、版本树和状态过滤结果。
- 如需程序化访问，调用稳定 TCP / facade 接口：
  - `listRecipes`
  - `getRecipe`
  - `getRecipeVersions`
  - `archiveRecipe`

### Versioning and rollback

- 从已发布版本创建新的草稿版本。
- 对比两个版本的参数与元数据差异。
- 将某个已发布版本重新激活为当前生产版本。

对应能力：

- `createVersionFromPublished`
- `compareRecipeVersions`
- `rollbackRecipeVersion`

### Audit

- HMI 应提供配方级、版本级审计记录查看能力。
- 程序接口对应能力：
  - `getRecipeAuditTrail`
  - `getRecipeVersionAuditTrail`

### Import and export

- 导出：将配方及其依赖模板打包到 `exports/recipe_bundle.json`
- 导入：支持预校验（dry-run）与冲突处理策略（如 rename）

对应能力：

- `exportRecipeBundle`
- `importRecipeBundle`

## Parameter format
通过 HMI 或 TCP 更新草稿参数时，底层仍遵循 `key[:type]=value` 的能力模型。支持类型：`int`、`float`、`bool`、`string`、`enum`。

Examples:
- `dispense_speed:float=120.5`
- `enable_vacuum:bool=true`
- `material_type:enum=epoxy`

## Notes
- Only published versions can be activated for production.
- Draft versions are editable; published versions are immutable.
- Parameter schemas are loaded from `data/recipes/schemas/` with fallback to
  `src/infrastructure/resources/config/files/recipes/schemas/`.
