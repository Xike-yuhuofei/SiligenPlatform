# process-path contracts

`modules/process-path/contracts/` 承载 `M6 process-path` 的模块 owner 专属契约。

## 正式职责

- 定义工艺路径生成请求与输入约束。
- 固化路径段、路径序列和路径校验结果的模块内语义。
- 作为 `M6 -> M7` 交接的最小字段边界说明。

## 边界约束

- 仅放置 `M6 process-path` owner 专属契约，不放跨模块稳定公共契约。
- `shared/contracts/engineering/` 承载长期稳定的工程公共 schema，本目录只保留 `M6` 特有语义。
- `modules/process-path/` 与 `shared/contracts/engineering/` 构成当前 canonical 契约承载面。
