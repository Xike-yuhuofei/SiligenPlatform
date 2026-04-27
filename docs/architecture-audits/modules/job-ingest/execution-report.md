# job-ingest Execution Report

更新时间：`2026-04-25`

## 1. Execution Scope

- 本轮处理的模块：`modules/job-ingest/`
- 读取的台账路径：`docs/architecture-audits/modules/job-ingest/residue-ledger.md`
- 本轮 batch 范围：
  - `S4-A` 语义冻结真值重写
  - `S4-B` 仓库级合同守护重写
  - `S4-C` golden 命名 residue 清退
  - `S4-D` audit / docs truth reconciliation

## 2. Execution Oracle

| batch | 本轮冻结范围 | acceptance criteria |
| --- | --- | --- |
| `S4-A` | `module.yaml`、README、decision record | 当前真值统一为 `SourceDrawing` + `IUploadStoragePort`，不再保留 `UploadResponse` / `IUploadPreparationPort` |
| `S4-B` | `tests/contracts/test_job_ingest_semantic_freeze_contract.py` | 合同测试只验证当前 live truth：`runtime-execution` consumer、storage-only composition root、无 preparation seam |
| `S4-C` | `modules/job-ingest/tests/CMakeLists.txt` + golden 工件 | `UploadResponseGoldenTest.cpp` 与 `upload_response.summary.txt` 不再作为 current truth 名称存在 |
| `S4-D` | `execution-report.md`、`residue-ledger.md` | audit 文件不再把过期 freeze 当作当前真值源 |

## 3. Current Frozen Truth

- `owner_artifact`：`SourceDrawing`
- live contracts：`UploadRequest`、`SourceDrawing`、`IUploadFilePort`
- live application seam：`UploadFileUseCase`、`IUploadStoragePort`
- `UploadFileUseCase` public surface 只负责 source drawing 校验、归档、`source_hash` 生成与 `validation_report(S1)` 输出
- `.pb` preparation、cleanup、canonical geometry 生成与 production gate 不属于 `job-ingest` current owner truth
- direct consumers：
  - `apps/runtime-service/`
  - `apps/runtime-gateway/`
  - `apps/planner-cli/`
  - `modules/runtime-execution/`

## 4. Validation Anchors

- `python -m pytest tests/contracts/test_job_ingest_semantic_freeze_contract.py -q`
- `ctest --test-dir build/ca -C Debug --output-on-failure -R "siligen_job_ingest_(contract|unit|golden|integration)_tests"`

## 5. Final Verdict

- 当前 `job-ingest` semantic freeze 真值已经正式收口为 `SourceDrawing`，旧 `UploadResponse` / `IUploadPreparationPort` 口径不再属于 current truth。
- 本文件现在只保留 current truth；任何历史阶段若仍引用旧 freeze，必须视为已过期证据，而不是 live authority。
