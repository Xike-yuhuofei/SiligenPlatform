# tests

job-ingest 的模块级验证入口应收敛到此目录。

## 当前最小正式矩阵

- `unit/dispensing/UploadFileUseCaseTest.cpp`
  - 使用 module-owned upload doubles 冻结 source drawing 归档、`source_hash` 与 `validation_report(S1)` 的 owner 行为。
- `contract/UploadContractsTest.cpp`
  - 冻结 `UploadRequest`、`SourceDrawing` 与 `IUploadFilePort` 的最小公开面。
- `golden/SourceDrawingGoldenTest.cpp`
  - 冻结脱敏后的 source drawing 摘要，避免把动态 UUID / 时间戳写成假真值。
- `integration/UploadFileUseCaseIntegrationTest.cpp`
  - 覆盖真实文件系统 roundtrip、legacy PB command override 忽略路径，以及 `validation_report(S1)` 形状保持稳定。

## 当前 live target

- `siligen_job_ingest_contract_tests`
- `siligen_job_ingest_unit_tests`
- `siligen_job_ingest_golden_tests`
- `siligen_job_ingest_integration_tests`

## 仓库级语义守护

- `tests/contracts/test_job_ingest_semantic_freeze_contract.py`
  - 冻结 `owner_artifact: SourceDrawing`、shared-only build truth、无 `IUploadPreparationPort`、legacy seam 删除状态，以及 direct consumers 持续指向 `Siligen::JobIngest::Contracts`。

## 当前不进入 live suite

- `regression/`
  - 仅为 workspace layout 要求的 canonical skeleton；当前不承载 source-bearing 测试，也不注册 regression target。

## 边界

- 模块内 `tests/` 是 `job-ingest` owner 级真值证明，不用仓库级 `tests/e2e` 代替 contract。
- `golden` 只冻结稳定摘要，不冻结运行时生成的动态文件名细节。
- `unit` / `golden` / `integration` 都不得把 `.pb` preparation 重新带回 M1 owner surface；`job-ingest` 只对 source drawing archive 负责。
