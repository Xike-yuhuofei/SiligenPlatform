# hmi-application contracts

`modules/hmi-application/contracts/` 承载 `M11 hmi-application` 的模块 owner 专属契约。

## 契约范围

- HMI 任务接入、审批、查询与展示状态的模块语义约束。
- 宿主入口与 `M11` owner 之间的命令/查询最小输入输出口径。
- 交互态异常分类、可视化状态与回链标识字段约定。

## 边界约束

- 仅放置 `M11` owner 专属契约，不放跨模块长期稳定公共契约。
- 跨模块稳定应用契约应维护在 `shared/contracts/application/`。
- 禁止在模块外再维护第二套 `M11` owner 专属契约事实。

## 当前对照面

- `shared/contracts/application/commands/`
- `shared/contracts/application/queries/`
- `shared/contracts/application/models/`
