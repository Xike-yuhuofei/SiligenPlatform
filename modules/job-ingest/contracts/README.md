# job-ingest contracts

`modules/job-ingest/contracts/` 承载 `M1 job-ingest` 的模块 owner 专属契约。

## 契约范围

- 任务接入阶段的输入对象约束
- ingest 请求与上传结果的稳定公开面
- 对 workflow 交接文件事实所需的最小字段口径
- authority artifact 固定为 `SourceDrawing`；`UploadRequest` 仅是 ingress 输入 DTO

## 边界约束

- 当前 source-bearing contract 仅包括 `UploadRequest`、`SourceDrawing` 与 `IUploadFilePort`。
- 不在本模块 contracts 中继续维护第二套 `IFileStoragePort` / `FileData` seam。
- 跨模块可复用且长期稳定的应用侧契约应维护在 `shared/contracts/application/`。
- 当前 contracts namespace 已统一为 `Siligen::JobIngest::Contracts`。
- 不在 contracts 中重新声明 storage / planning / preparation owner；相关 foreign concrete 只允许通过 application ports 被适配。

## 当前对照面

- `modules/runtime-execution/application/include/runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h`
- `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h`
- `apps/planner-cli/CommandHandlers.Dxf.cpp`
