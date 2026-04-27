# job-ingest

`modules/job-ingest/` 是 `M1 job-ingest` 的 canonical owner 入口。

## Owner 范围

- authority artifact：`SourceDrawing`
- 当前 live owner surface：`UploadRequest`、`SourceDrawing`、`IUploadFilePort`、`UploadFileUseCase`
- `UploadRequest` 是 ingress 输入 DTO，不是 authority artifact
- 任务接入与输入归档（ingest）语义
- 输入校验与接入侧最小编排
- 向下游 workflow 交接 DXF source drawing 事实（`source_drawing_ref`、`filepath`、`source_hash`、`validation_report`）
- contracts namespace：`Siligen::JobIngest::Contracts`
- application use case namespace：`Siligen::JobIngest::Application::UseCases::Dispensing`

## 边界约束

- `apps/` 入口层只保留宿主与装配职责，不承载 `M1` 终态 owner 事实。
- 跨模块稳定应用契约归位到 `shared/contracts/application/`。
- `job-ingest` 专属契约当前通过 `job_ingest/contracts/...` 暴露稳定 include path。
- `job_ingest/application/...` 是唯一 authoritative application include path。
- `UploadFileUseCase` public seam 只暴露 module-owned storage port，不再公开 foreign PB service / configuration types。
- PB preparation 与 cleanup 不属于 M1；planning owner 负责 `.pb` preparation 与 input-quality projection。
- foreign storage owner 保持在模块外；M1 只通过 `IUploadStoragePort` 感知存储能力。

## 当前协作面

- `apps/planner-cli/`
- `apps/runtime-service/`
- `apps/runtime-gateway/`
- `modules/workflow/`

## 统一骨架状态

- 当前 source-bearing roots：`contracts/`、`application/`、`tests/`
- 非 live 的顶层骨架目录仍保留，但当前不参与 implementation roots。
- `tests/regression/` 仅保留为 canonical skeleton，不承载 live target。
- producer live build 不再声明 `process-planning` / `dxf-geometry` 依赖；planning preparation concrete 仅允许在 planning owner 或组合根中装配。

## 测试基线

- `modules/job-ingest/tests/` 负责 `job-ingest` owner 级 `unit + contracts + golden + integration` 证明。
- 当前正式测试 target：
  - `siligen_job_ingest_contract_tests`
  - `siligen_job_ingest_unit_tests`
  - `siligen_job_ingest_golden_tests`
  - `siligen_job_ingest_integration_tests`
- 当前最小正式矩阵聚焦 `UploadFileUseCase` 的 source drawing 封存、source hash、文件级 DXF 校验与失败清理，以及 `UploadContracts` 的稳定输入/输出公开面。
- 仓库级 `tests/` 只消费跨 owner 场景，不替代 `job-ingest` 模块内 contract。
