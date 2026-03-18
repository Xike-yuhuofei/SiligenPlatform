# Recipes 子域 - 配方管理

**职责**: 负责配方生命周期、版本管理、配方生效与合规性校验

## 业务范围

- 配方定义与版本管理
- 配方生效与切换
- 配方参数校验与约束
- 配方模板与参数 Schema 管理
- 审计与可追溯记录

## 目录结构

```
recipes/
├── aggregates/                 # 聚合根
│   ├── Recipe.*                # 配方聚合
│   └── RecipeVersion.*         # 配方版本
├── domain-services/            # 领域服务
│   ├── RecipeValidationService # 兼容包装，委托 process-core 校验
│   └── RecipeActivationService # 兼容包装，委托 process-core 生效
├── value-objects/              # 值对象
│   ├── RecipeTypes.h
│   ├── RecipeBundle.h
│   └── ParameterSchema.h
├── ports/                      # 端口接口
│   ├── IRecipeRepositoryPort
│   ├── ITemplateRepositoryPort
│   ├── IParameterSchemaPort
│   ├── IAuditRepositoryPort
│   └── IRecipeBundleSerializerPort
├── RecipeModule.h
└── CMakeLists.txt
```

## 命名空间

```cpp
namespace Siligen::Domain::Recipes {
    namespace Aggregates { ... }
    namespace ValueObjects { ... }
    namespace DomainServices { ... }
    namespace Ports { ... }
}
```

## 依赖关系

- ✅ 依赖: `shared/types`, `shared/utils`, `domain/_shared`
- ❌ 不依赖: `infrastructure`, `application`, 其他子域

## 配方校验/生效统一规范

- `process-core` 是配方校验与生效规则的主实现与规则来源。
- `Domain::Recipes::DomainServices::RecipeActivationService` / `RecipeValidationService` 为兼容包装层，保留给旧调用链与测试使用。
- 应用层仅负责发布/显式激活的流程编排，禁止直接修改 `Recipe.active_version_id` 或重复实现生效规则。
- 生效规则统一为：仅已发布版本可生效；配方归档后禁止生效；生效必须记录审计。
- 审计记录通过 `IAuditRepositoryPort` 写入，存储细节由基础设施层实现。
