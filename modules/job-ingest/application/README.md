# application

job-ingest 的 live application seam 只包含 `UploadFileUseCase` 与 module-owned upload ports。

## 当前真相

- authoritative include path：`job_ingest/application/usecases/dispensing/UploadFileUseCase.h`
- module-owned port seam：`job_ingest/application/ports/dispensing/UploadPorts.h`
- `UploadFileUseCase` namespace：`Siligen::JobIngest::Application::UseCases::Dispensing`
- `SourceDrawing` 是本模块 authority artifact；`UploadFileUseCase` 是生成该 authority fact 的 application seam
- public constructor 只接受 `IUploadStoragePort`
- application producer target 不再直接链接 `process-planning` 或 `dxf-geometry`
- foreign storage 适配只允许存在于 runtime-service composition root 或模块测试 doubles
- `job-ingest` 不拥有 `.pb` preparation、cleanup 或 canonical geometry 生成；M1 只保留 source archive seam，不吞回 foreign concrete owner 语义
