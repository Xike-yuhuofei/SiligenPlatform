---
Owner: @codex
Status: active
Last reviewed: 2026-03-17
Scope: Recipe management (HMI/TCP)
---

# Recipe Management Guide

## Purpose
提供一套面向当前架构的配方管理操作说明：人通过 HMI 操作，程序通过稳定 TCP / facade 接口访问，不再依赖面向人的 CLI 客户端。

## Prerequisites
- `siligen_tcp_server` 已构建并运行
- HMI 可连接到 TCP 服务
- 本地对 `data/recipes/` 具有读写权限
- `data/recipes/schemas/` 下存在参数 schema JSON
- `data/recipes/templates/` 下存在模板 JSON，或通过 bundle 导入模板

## Recommended workflow
1. 在 HMI 中创建配方：
   - 录入名称、描述、标签、模板来源和操作者信息
2. 创建草稿版本：
   - 基于模板或已发布版本生成草稿
3. 更新草稿参数：
   - 通过 HMI 表单或 TCP 请求提交参数变更
4. 执行校验并发布：
   - 通过校验后发布版本，同时激活为当前生产版本
5. 浏览与筛选：
   - 在 HMI 查看配方列表、状态、标签和版本历史
6. 比较与回滚：
   - 对比两个版本差异，必要时回滚到某个已发布版本
7. 审计追踪：
   - 查看配方级、版本级审计记录
8. 导入导出：
   - 导出 bundle，或导入 bundle 并处理命名冲突

## Stable programmatic capabilities

推荐将下列能力作为 TCP / application facade 的稳定契约，而不是重新引入 CLI 入口：

- `createRecipe`
- `createDraftVersion`
- `updateDraftVersion`
- `publishVersion`
- `listRecipes`
- `getRecipe`
- `getRecipeVersions`
- `archiveRecipe`
- `createVersionFromPublished`
- `compareRecipeVersions`
- `rollbackRecipeVersion`
- `getRecipeAuditTrail`
- `exportRecipeBundle`
- `importRecipeBundle`

## Parameter update model

底层参数模型仍使用 `key[:type]=value` 形式，便于 HMI、TCP 和内部 service API 共用同一套校验逻辑。

示例：

- `dispense_speed:float=120.5`
- `flow:float=0.35`
- `enable_vacuum:bool=true`
- `material_type:enum=epoxy`

## Common Errors
- `RECIPE_NOT_FOUND`: 检查配方 ID / 版本 ID 是否存在
- `RECIPE_ALREADY_EXISTS`: 使用唯一名称，或在导入时使用 rename 策略
- `RECIPE_VALIDATION_FAILED`: 检查必填参数、范围和 schema 约束
- `RECIPE_INVALID_STATE`: 确认目标版本已发布，且配方未归档
- `PARAMETER_SCHEMA_NOT_FOUND`: 确认 schema 文件存在且可读
