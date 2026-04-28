# job-ingest Residue Ledger

> Current truth note (2026-04-27): this ledger is historical phase-3 evidence. The active `ARCH-215` owner truth has superseded `UploadResponse` / `IUploadPreparationPort`: `job-ingest` now owns `SourceDrawing` only, while `.pb` preparation and input-quality projection belong to the planning owner.

更新时间：`2026-04-09`

## 1. Executive Verdict

- `job-ingest` 的 phase 3 closeout 与后续语义冻结已完成；当前没有留在 `modules/job-ingest/` 内部的 active owner / build / test blocker。
- 当前真值来源固定为：
  - `docs/decisions/job-ingest-upload-semantic-freeze.md`
  - `docs/architecture-audits/modules/job-ingest/execution-report.md`
  - `modules/job-ingest/module.yaml`
- 原先遗留的 storage owner / PB preparation owner 历史债已在跨模块迁移批次中收敛：storage seam 已迁到 `runtime_process_bootstrap` app-local quarantine surface，PB cleanup owner 已收回 `dxf-geometry`；它们不再算作本模块 closeout residue。

## 2. Frozen Baseline

- `owner_artifact`：`UploadResponse`
- live contracts：`UploadRequest`、`UploadResponse`、`IUploadFilePort`
- contracts namespace：`Siligen::JobIngest::Contracts`
- authoritative application include path：`job_ingest/application/...`
- live application seam：`UploadFileUseCase`、`IUploadStoragePort`、`IUploadPreparationPort`
- allowed dependencies：`shared`
- direct consumers：
  - `apps/runtime-service/`
  - `apps/runtime-gateway/`
  - `apps/planner-cli/`
  - `modules/workflow/`
- shell-only / skeleton-only roots：
  - `domain/`
  - `services/`
  - `adapters/`
  - `examples/`
  - `tests/regression/`

## 3. Historical Residue Disposition

| residue_id | 当前状态 | 结果落点 |
| --- | --- | --- |
| `JI-R001` | `closed` | `module.yaml` 已冻结为 `owner_artifact: UploadResponse` |
| `JI-R002` | `closed` | `modules/job-ingest/CMakeLists.txt` 已回到 shared-only truth |
| `JI-R003` | `closed` | `siligen_job_ingest_application_headers` 只保留 canonical include dirs 与 `siligen_job_ingest_contracts` |
| `JI-R004` | `closed` | `UploadContracts.h` 已统一到 `Siligen::JobIngest::Contracts` |
| `JI-R005` | `closed` | legacy wrapper `application/include/application/usecases/dispensing/UploadFileUseCase.h` 已删除 |
| `JI-R006` | `closed` | `UploadFileUseCase` public seam 已收缩到 `IUploadStoragePort` / `IUploadPreparationPort` |
| `JI-R007` | `closed` | PB preparation 已通过 `IUploadPreparationPort` 下沉为 seam |
| `JI-R008` | `closed` | `job_ingest/contracts/storage/IFileStoragePort.h` 已不存在，`job-ingest` contracts 不再维护重复 storage seam |
| `JI-R009` | `closed` | 上传失败清理已通过 storage / preparation port 完成，不再直触文件系统 |
| `JI-R010` | `closed` | `contract` / `unit` / `golden` / `integration` 已拆成独立 live target |
| `JI-R011` | `closed` | shell-only roots 已从 `SILIGEN_TARGET_IMPLEMENTATION_ROOTS` 移出 |
| `JI-R012` | `reclassified` | `tests/regression/` 现在被明确定义为 canonical skeleton，而不是 live residue |
| `JI-R013` | `closed` | `README.md` 与 `module.yaml` 已对齐真实 consumer map |
| `JI-R014` | `closed` | `application/README.md` 已对齐当前 live application seam |

## 4. Cross-Module Follow-Up Status

- runtime-service 组合根当前通过本地 `RuntimeUploadStorageAdapter` 把 `runtime_process_bootstrap/storage/ports/IFileStoragePort.h` 适配到 `IUploadStoragePort`。
- runtime-service 组合根与 integration tests 当前继续通过本地 adapter / real service 适配 `DxfPbPreparationService` 到 `IUploadPreparationPort`，但 cleanup owner 已统一回到 `dxf-geometry` canonical service。
- 后续如需继续治理，只剩 storage seam 是否要再从 app-local quarantine 上提到更稳定 cross-app contract 的独立议题。

## 5. Validation Anchors

- 仓库级语义守护：
  - `tests/contracts/test_job_ingest_semantic_freeze_contract.py`
- 模块主证据：
  - `ctest --test-dir D:/Projects/SiligenSuite/build-job-ingest-phase3 -C Debug --output-on-failure -R "siligen_job_ingest_(contract|unit|golden|integration)_tests"`
- source compatibility：
  - `python D:/Projects/SiligenSuite/apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py`
- consumer build 证据：
  - `cmake --build D:/Projects/SiligenSuite/build-job-ingest-phase3 --config Debug --target siligen_dispensing_semantics_tests siligen_runtime_service siligen_runtime_gateway siligen_planner_cli -- /m:4 /clp:ErrorsOnly /nologo`

## 6. No-Change Zone

- `modules/process-planning/**` 其余实现
- 与 storage/PB owner migration 无关的 `modules/dxf-geometry/**` 其余实现
- 与本轮无关的脏工作区改动

以上对象只作为 foreign seam / foreign concrete 证据，不在本轮 `job-ingest` semantic freeze 中扩散修改。
