# Job-Ingest Upload Semantic Freeze

更新时间：`2026-04-25`

## 1. 决策结论

- `modules/job-ingest/` 继续冻结为 upload-centric owner；本阶段不把 `M1` 重新解释为泛化 `JobDefinition` owner。
- `module.yaml` 中的 `owner_artifact` 固定为 `SourceDrawing`。它表示 M1 已完成 source drawing 接入、归档与向下游交接后对外公开的 authority fact。
- `UploadRequest` 是 ingress 输入 DTO，不是 authority artifact；它只承载上传入口所需的最小输入字段。
- live contracts 固定为 `UploadRequest`、`SourceDrawing`、`IUploadFilePort`，命名空间固定为 `Siligen::JobIngest::Contracts`。
- live application seam 固定为 `UploadFileUseCase` 与 `IUploadStoragePort`，authoritative include path 固定为 `job_ingest/application/...`。
- M1 不拥有 `.pb` preparation、cleanup、canonical geometry 生成或 production gate。`job-ingest` 对外只负责 source drawing archive 与 `validation_report(S1)`。
- foreign storage owner 不迁回 `job-ingest`。M1 只通过 `IUploadStoragePort` 观察文件校验、落盘与删除能力；storage seam 继续停留在 `runtime_process_bootstrap/storage/ports/IFileStoragePort.h` app-local quarantine surface。
- `tests/regression/` 与 `domain/`、`services/`、`adapters/`、`examples/` 当前都属于 canonical skeleton，不是 live implementation / live test lane。

## 2. 最终处置结果

- `frozen`
  - `modules/job-ingest/module.yaml`
  - `modules/job-ingest/contracts/include/job_ingest/contracts/dispensing/UploadContracts.h`
  - `modules/job-ingest/application/include/job_ingest/application/ports/dispensing/UploadPorts.h`
  - `modules/job-ingest/application/include/job_ingest/application/usecases/dispensing/UploadFileUseCase.h`
- `deleted`
  - `modules/job-ingest/application/include/application/usecases/dispensing/UploadFileUseCase.h`
  - `modules/job-ingest/contracts/include/job_ingest/contracts/storage/IFileStoragePort.h`
- `deferred-outside-M1`
  - runtime-service 组合根中的 `RuntimeUploadStorageAdapter`
  - planning/runtime 内部的 `.pb` preparation、cleanup 与 canonical geometry 产物
  - 若后续要把 app-local storage seam 再上提到更稳定 contract，需要单独立题

## 3. Consumer 收口证据

- `apps/runtime-service/container/ApplicationContainer.Dispensing.cpp`
- `modules/runtime-execution/application/include/runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h`
- `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h`
- `apps/planner-cli/CommandHandlers.Dxf.cpp`

以上 direct consumers / composition root 已统一收口到 `Siligen::JobIngest::Contracts::*` 与 `job_ingest/application/...`。

## 4. 验证焦点

- `owner_artifact: SourceDrawing` 与 `allowed_dependencies: [shared]` 不得漂移。
- `UploadContracts.h` 必须导出 `SourceDrawing`，且不得回退到 `Siligen::Application::UseCases::Dispensing` 命名空间。
- `UploadFileUseCase` public header 不得重新暴露 `IFileStoragePort`、`IConfigurationPort`、`DxfPbPreparationService` 或任何 preparation seam。
- runtime-service 组合根只能适配 storage seam；不得重新把 `.pb` preparation 适配回 `UploadFileUseCase`。
- direct consumers 不得重新使用 legacy upload namespace / include seam。
- `tests/regression/` 继续保持 skeleton-only；不得隐式注册 live regression target。

## 5. 禁止回退

- 禁止把 `owner_artifact` 改回 `UploadResponse`、`JobDefinition` 或其它新的公开命名，除非单独立题做跨 consumer 迁移。
- 禁止恢复 `modules/job-ingest/application/include/application/usecases/dispensing/UploadFileUseCase.h` legacy wrapper。
- 禁止恢复 `modules/job-ingest/contracts/include/job_ingest/contracts/storage/IFileStoragePort.h` 或在 `job-ingest` contracts 内重新维护第二套 storage seam。
- 禁止恢复 `IUploadPreparationPort` 或任何等价 preparation 旁路。
- 禁止把 `prepared_filepath`、`.pb` cleanup 或 foreign planning/geometry concrete 类型重新暴露到 `UploadFileUseCase` public surface。
- 禁止 direct consumers 回退到 `Siligen::Application::UseCases::Dispensing::{UploadRequest, UploadResponse, IUploadFilePort}`。
