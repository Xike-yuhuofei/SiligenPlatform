# job-ingest

`modules/job-ingest/` 是 `M1 job-ingest` 的 canonical owner 入口。

## Owner 范围

- 任务接入与输入归档（ingest）语义
- 输入校验与接入侧基础编排
- 面向上游静态工程链的输入事实边界

## 边界约束

- `apps/` 入口层只保留宿主与装配职责，不承载 `M1` 终态 owner 事实。
- 跨模块稳定应用契约归位到 `shared/contracts/application/`。
- `job-ingest` 专属契约归位到 `modules/job-ingest/contracts/`。

## 当前协作面

- `apps/hmi-app/`
- `shared/contracts/application/`
- `modules/job-ingest/contracts/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- 终态实现必须只落在该模块 canonical 骨架内，不再引入桥接目录或旁路 owner 面。

## 测试基线

- `modules/job-ingest/tests/` 负责 `job-ingest` owner 级 `unit + contracts + golden + integration` 证明。
- 当前最小正式矩阵聚焦 `UploadFileUseCase` 的上传校验、PB 生成闭环与失败清理。
- 仓库级 `tests/` 只消费跨 owner 场景，不替代 `job-ingest` 模块内 contract。
