# tests

job-ingest 的模块级验证入口应收敛到此目录。

## 当前最小正式矩阵

- `unit/dispensing/UploadFileUseCaseTest.cpp`
  - 冻结上传成功生成 PB 与失败清理产物的 owner 行为。
- `contract/UploadContractsTest.cpp`
  - 冻结 `UploadRequest`、`UploadResponse`、`IUploadFilePort` 与 `IFileStoragePort` 的最小公开面。
- `golden/UploadResponseGoldenTest.cpp`
  - 冻结脱敏后的上传响应摘要，避免把动态 UUID / 时间戳写成假真值。
- `integration/UploadFileUseCaseIntegrationTest.cpp`
  - 覆盖 `UploadFileUseCase -> DxfPbPreparationService -> 外部命令` 的最小成功/失败闭环。

## 边界

- 模块内 `tests/` 是 `job-ingest` owner 级真值证明，不用仓库级 `tests/e2e` 代替 contract。
- `golden` 只冻结稳定摘要，不冻结运行时生成的动态文件名细节。
