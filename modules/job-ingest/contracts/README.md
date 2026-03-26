# job-ingest contracts

`modules/job-ingest/contracts/` 承载 `M1 job-ingest` 的模块 owner 专属契约。

## 契约范围

- 任务接入阶段的输入对象约束
- ingest 请求与校验结果的模块内语义约定
- 对上游 `M1 -> M2` 交接所需的最小字段口径

## 边界约束

- 仅放置 `job-ingest` owner 专属契约，不放跨模块公共稳定契约。
- 跨模块可复用且长期稳定的应用侧契约应维护在 `shared/contracts/application/`。
- 禁止在模块外再维护第二套 `M1` owner 专属契约事实。

## 当前对照面

- `shared/contracts/application/commands/`
- `shared/contracts/application/queries/`
- `shared/contracts/application/models/`
