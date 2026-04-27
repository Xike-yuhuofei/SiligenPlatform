# job-ingest Residue Ledger

更新时间：`2026-04-25`

## 1. Executive Verdict

- 当前 `job-ingest` 的 live owner / build / test blocker 已收口；本轮关闭的是 semantic freeze 真值与当前实现不一致的 residue。
- 当前真值来源固定为：
  - `docs/decisions/job-ingest-upload-semantic-freeze.md`
  - `docs/architecture-audits/modules/job-ingest/execution-report.md`
  - `modules/job-ingest/module.yaml`
- 旧 `UploadResponse` / `IUploadPreparationPort` 冻结口径不再属于 current truth。

## 2. Frozen Baseline

- `owner_artifact`：`SourceDrawing`
- live contracts：`UploadRequest`、`SourceDrawing`、`IUploadFilePort`
- contracts namespace：`Siligen::JobIngest::Contracts`
- authoritative application include path：`job_ingest/application/...`
- live application seam：`UploadFileUseCase`、`IUploadStoragePort`
- allowed dependencies：`shared`
- direct consumers：
  - `apps/runtime-service/`
  - `apps/runtime-gateway/`
  - `apps/planner-cli/`
  - `modules/runtime-execution/`
- shell-only / skeleton-only roots：
  - `domain/`
  - `services/`
  - `adapters/`
  - `examples/`
  - `tests/regression/`

## 3. Historical Residue Disposition

| residue_id | 当前状态 | 结果落点 |
| --- | --- | --- |
| `JI-R001` | `closed` | `module.yaml` 已冻结为 `owner_artifact: SourceDrawing` |
| `JI-R002` | `closed` | `modules/job-ingest/CMakeLists.txt` 已回到 shared-only truth |
| `JI-R004` | `closed` | `UploadContracts.h` 已统一到 `Siligen::JobIngest::Contracts` |
| `JI-R005` | `closed` | legacy wrapper `application/include/application/usecases/dispensing/UploadFileUseCase.h` 已删除 |
| `JI-R006` | `closed` | `UploadFileUseCase` public seam 已收缩到 `IUploadStoragePort` |
| `JI-R007` | `closed` | `.pb` preparation 已被移出 producer public seam，不再属于当前 M1 freeze truth |
| `JI-R008` | `closed` | `job_ingest/contracts/storage/IFileStoragePort.h` 已不存在，`job-ingest` contracts 不再维护重复 storage seam |
| `JI-R009` | `closed` | `.pb` cleanup 不再属于 `job-ingest` owner truth |
| `JI-R010` | `closed` | `contract` / `unit` / `golden` / `integration` 已拆成独立 live target，golden 命名已改到 `SourceDrawing` |
| `JI-R012` | `reclassified` | `tests/regression/` 明确定义为 canonical skeleton，而不是 live residue |
| `JI-R013` | `closed` | `README.md`、`module.yaml`、decision/audit docs 已对齐真实 consumer map |

## 4. Cross-Module Follow-Up Status

- runtime-service 组合根通过本地 `RuntimeUploadStorageAdapter` 把 `runtime_process_bootstrap/storage/ports/IFileStoragePort.h` 适配到 `IUploadStoragePort`。
- `.pb` preparation / cleanup / canonical geometry 继续停留在 planning/runtime 内部 owner，不再属于 `job-ingest` 当前 freeze scope。
- 后续如需继续治理，只剩 storage seam 是否要再从 app-local quarantine 上提到更稳定 cross-app contract 的独立议题。

## 5. Validation Anchors

- 仓库级语义守护：
  - `tests/contracts/test_job_ingest_semantic_freeze_contract.py`
- 模块主证据：
  - `ctest --test-dir build/ca -C Debug --output-on-failure -R "siligen_job_ingest_(contract|unit|golden|integration)_tests"`

## 6. No-Change Zone

- `modules/process-planning/**` 其余实现
- `modules/dxf-geometry/**` 其余实现
- 与本轮无关的脏工作区改动

以上对象只作为 foreign seam / foreign concrete 证据，不在本轮 `job-ingest` semantic freeze 中扩散修改。
