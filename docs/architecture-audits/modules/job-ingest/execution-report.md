# job-ingest Execution Report

> Current truth note (2026-04-27): this report is historical phase-3 evidence. The active `ARCH-215` owner truth has superseded `UploadResponse` / `IUploadPreparationPort`: `job-ingest` now owns `SourceDrawing` only, while `.pb` preparation and input-quality projection belong to the planning owner.

## 1. Execution Scope
- 本轮处理的模块：`modules/job-ingest/`
- 读取的台账路径：`docs/architecture-audits/modules/job-ingest/residue-ledger.md`
- 本轮 batch 范围：`P3-A contracts namespace freeze`、`P3-B application surface shrink`、`P3-C consumer retarget`、`P3-D build/doc truth reconciliation`、`P3-E closeout evidence tests`

## 2. Execution Oracle

| batch | 本轮冻结范围 | acceptance criteria |
| --- | --- | --- |
| `P3-A` | `UploadContracts.h` namespace 统一到 `Siligen::JobIngest::Contracts` | `UploadRequest` / `UploadResponse` / `IUploadFilePort` 不再继续挂在 workflow-style namespace 下 |
| `P3-B` | 新增 module-owned upload ports，收缩 `UploadFileUseCase` public ctor | public header 不再暴露 foreign storage/config/PB types；use case 只接受 `IUploadStoragePort` / `IUploadPreparationPort` |
| `P3-C` | retarget `runtime-service`、`workflow`、`runtime-gateway`、`planner-cli` | direct consumer 改用 `Siligen::JobIngest::Contracts::*`，runtime-service 只在组合根适配 foreign deps |
| `P3-D` | 对齐 `module.yaml`、`README`、`application/README`、`contracts/README`、`tests/README`、`CMakeLists.txt` | 文档、命名、build truth 不再声称 producer 仍合理依赖 foreign owner |
| `P3-E` | 运行 closeout 主证据测试 | `job-ingest` 四个测试 target 构建通过并实际执行；若 consumer build 被仓库外部问题阻塞，需明确记录 |

- 本轮 no-change zone：`modules/process-planning/**` 实现、`modules/dxf-geometry/**` 实现、与本任务无关的脏改动
- 本轮测试集合：
- `ctest --test-dir build-job-ingest-phase3 -C Debug --output-on-failure -R "siligen_job_ingest_(contract|unit|golden|integration)_tests"`
- `python apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py`
- 定向构建尝试：`siligen_dispensing_semantics_tests`、`siligen_runtime_service`、`siligen_runtime_gateway`、`siligen_planner_cli`

## 3. Change Log

| 文件 | 改了什么 | 为什么这样改 | 消除的 residual |
| --- | --- | --- | --- |
| `modules/job-ingest/contracts/include/job_ingest/contracts/dispensing/UploadContracts.h` | namespace 切到 `Siligen::JobIngest::Contracts` | 让 contracts owner 与模块 owner 一致 | `JI-R004` |
| `modules/job-ingest/application/include/job_ingest/application/ports/dispensing/UploadPorts.h` | 新增 `IUploadStoragePort`、`IUploadPreparationPort` | 建立 module-owned application seam，承接 foreign concrete 的外部适配 | `JI-R006`、`JI-R007`、`JI-R009` |
| `modules/job-ingest/application/include/job_ingest/application/usecases/dispensing/UploadFileUseCase.h` | ctor 改成只接受 module-owned ports；namespace 改到 `Siligen::JobIngest::Application::UseCases::Dispensing` | 收紧 public seam，去掉 foreign type leakage | `JI-R006` |
| `modules/job-ingest/application/usecases/dispensing/UploadFileUseCase.cpp` | 去掉 direct include foreign storage/PB headers；改为通过 storage/preparation port 做 validate/store/prepare/cleanup | use case 只保留 owner 编排，不再直连 foreign concrete | `JI-R007`、`JI-R009` |
| `modules/job-ingest/application/include/application/usecases/dispensing/UploadFileUseCase.h` | 删除 legacy wrapper | phase 3 目标是不再保留旧 surface 伪装成合法入口 | `JI-R005` closeout |
| `modules/job-ingest/CMakeLists.txt` | 删除 producer 对 `process-planning/domain/configuration` 和 `dxf-geometry/application` 的 add/link | producer target 回到 shared-only truth | `JI-R002` |
| `modules/job-ingest/tests/support/UploadFileUseCaseTestSupport.h` | 用 `TestUploadStoragePort` / `FakeUploadPreparationPort` 替换 foreign storage double | 测试不再依赖 process-planning storage seam | `JI-R006`、`JI-R010` |
| `modules/job-ingest/tests/unit/dispensing/UploadFileUseCaseTest.cpp` | unit 改用 module-owned doubles，覆盖 prepare failure cleanup | closeout 主证据回到 module seam | `JI-R007`、`JI-R009` |
| `modules/job-ingest/tests/golden/UploadResponseGoldenTest.cpp` | golden 改用 fake preparation port | golden lane 不再站在 real PB service 上 | `JI-R010` |
| `modules/job-ingest/tests/integration/UploadFileUseCaseIntegrationTest.cpp` | 本地定义 `RealUploadPreparationPort` 适配 `DxfPbPreparationService` | real PB command 证据留在 integration lane，不污染 producer public seam | `JI-R007`、`JI-R009` |
| `modules/job-ingest/tests/contract/UploadContractsTest.cpp` | 合同测试 using 改到 `Siligen::JobIngest::Contracts` | contract lane 与真实 public namespace 对齐 | `JI-R004` |
| `modules/job-ingest/tests/CMakeLists.txt` | unit/golden 去掉 process-planning link；integration 私有补链 `siligen_dxf_geometry_application_public` | 测试依赖按 lane 真相收敛 | `JI-R002`、`JI-R010` |
| `apps/runtime-service/container/ApplicationContainer.Dispensing.cpp` | 在匿名 namespace 新增 `RuntimeUploadStorageAdapter` / `DxfPreparationAdapter`；实例化新 `UploadFileUseCase`；Resolve 改到 `JobIngest::Contracts::IUploadFilePort` | foreign deps 只允许留在 composition root | `JI-R002`、`JI-R006`、`JI-R007`、`JI-R009` |
| `apps/runtime-service/container/ApplicationContainer.h` / `ApplicationContainerFwd.h` | template specialization / forward decl 改到新 job-ingest types | container truth 与新 stable seam 对齐 | `JI-R004`、`JI-R006` |
| `modules/workflow/application/include/application/phase-control/DispensingWorkflowUseCase.h` | 引入 `using Siligen::JobIngest::Contracts::*` | workflow 消费 module-owned contracts，而不再消费旧 namespace | `JI-R004` |
| `modules/workflow/tests/integration/DispensingWorkflowUseCaseTest.cpp` | using 改到 `Siligen::JobIngest::Contracts::IUploadFilePort` | 测试证据随 consumer truth 对齐 | `JI-R004` |
| `apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h/.cpp` | 上传相关参数/返回值/成员改到 `JobIngest::Contracts::*` | TCP facade 消费真实 contracts seam | `JI-R004` |
| `apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h` | resolver 改成 `Resolve<Siligen::JobIngest::Contracts::IUploadFilePort>()` | builder 与 container public port 对齐 | `JI-R004` |
| `apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp` | artifact create / load 场景改用 `JobIngest::Contracts::UploadRequest` | dispatcher 不再实例化旧 namespace DTO | `JI-R004` |
| `apps/planner-cli/CommandHandlers.Dxf.cpp` | preview snapshot 流程改用 `JobIngest::Contracts::UploadRequest` | CLI 上传请求真值改到 module contracts | `JI-R004` |
| `modules/job-ingest/module.yaml` | `allowed_dependencies` 收缩到 `shared`；notes 更新为 phase 3 真相 | metadata 与 live build 对齐 | `JI-R002`、`JI-R013` |
| `modules/job-ingest/README.md` / `application/README.md` / `contracts/README.md` / `tests/README.md` | 同步 namespace、stable seam、build truth、测试 lane 真相 | 文档不再与当前代码相互打架 | `JI-R013` |

## 4. Residual Disposition Table

| residue_id | file_or_target | disposition | resulting_location | reason |
| --- | --- | --- | --- | --- |
| `JI-R004` | `UploadContracts.h` + direct consumers | `migrated` | `Siligen::JobIngest::Contracts` | contracts namespace 已统一，consumer 已 retarget |
| `JI-R006` | `UploadFileUseCase` public ctor | `demoted` | `UploadPorts.h` + runtime-service composition root | foreign storage/config/PB types 已从 public header 移除 |
| `JI-R007` | PB prepare 行为落在 use case | `split` | `IUploadPreparationPort` + runtime-service/test-local adapters | 准备行为保留为 owner 编排调用，但 foreign concrete 已外提 |
| `JI-R009` | `.pb` cleanup 直落 use case | `demoted` | `IUploadPreparationPort::CleanupPreparedInput` | use case 不再直连文件系统清理 `.pb` |
| `JI-R002` | producer target 透传 foreign deps | `migrated` | `job-ingest/CMakeLists.txt` + runtime-service composition root | producer 已 shared-only；foreign concrete 只留在组合根 / integration |
| `JI-R005` | old legacy wrapper include | `deleted` | 无 | 旧 `application/.../UploadFileUseCase.h` 已删除 |
| `JI-R010` | tests owner seam 偏移 | `migrated` | module-owned doubles + integration-local real adapter | unit/golden 已回到 module ports，integration 才接 real PB service |

## 5. Dependency Reconciliation Result
- 已对齐：
- `module.yaml` 现在只声明 `shared`
- `modules/job-ingest/CMakeLists.txt` 中 `siligen_job_ingest_application` 不再链接 `siligen_dxf_geometry_application_public` 或 `siligen_process_planning_domain_configuration`
- `modules/job-ingest/tests/CMakeLists.txt` 中 unit/golden 不再链接 process-planning；integration 私有链接 dxf-geometry
- `runtime-service` composition root 负责把 runtime bootstrap storage seam / `DxfPbPreparationService` 适配到 module-owned ports
- 复跑结论：
- 交接文档记录的 `MotionControlUseCase.h -> runtime_execution/contracts/motion/HomingProcess.h` 缺失头问题，在当前工作树状态下未再复现
- 当前实际 consumer build blocker 是 `modules/runtime-execution/adapters/device/include/siligen/device/adapters/motion/MultiCardMotionAdapter.h` 通过 `../internal/UnitConverter.h` 引用不存在的 public 兼容头
- 已补齐 `modules/runtime-execution/adapters/device/include/siligen/device/adapters/internal/UnitConverter.h` 兼容入口，consumer build 已恢复
- `siligen_dispensing_semantics_tests`、`siligen_runtime_service`、`siligen_runtime_gateway`、`siligen_planner_cli` 已完成成功构建验证

## 6. Surface Reconciliation Result
- stable seam 已统一为：
- contracts：`job_ingest/contracts/dispensing/UploadContracts.h`
- application ports：`job_ingest/application/ports/dispensing/UploadPorts.h`
- use case：`job_ingest/application/usecases/dispensing/UploadFileUseCase.h`
- 不再存在旧 `application/...` wrapper
- 不再存在 façade/provider/bridge 多套并存的上传 contracts 命名
- `UploadFileUseCase` public surface 不再泄漏 foreign storage/config/PB 类型
- 内部 stage 类型没有新增对外泄漏；外部只看到 request/response/port seam

## 7. Test Result

| 测试/命令 | 结果 | 备注 |
| --- | --- | --- |
| `cmake -S D:/Projects/SiligenSuite -B D:/Projects/SiligenSuite/build-job-ingest-phase3 -DSILIGEN_BUILD_TESTS=ON` | 通过 | 专用 build 目录配置成功 |
| `cmake --build ... --target siligen_job_ingest_contract_tests siligen_job_ingest_unit_tests siligen_job_ingest_golden_tests siligen_job_ingest_integration_tests` | 通过 | 4 个模块测试 target 全部构建成功 |
| `ctest --test-dir D:/Projects/SiligenSuite/build-job-ingest-phase3 -C Debug --output-on-failure -R "siligen_job_ingest_(contract|unit|golden|integration)_tests"` | 通过 | 4/4 测试通过 |
| `python D:/Projects/SiligenSuite/apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py` | 通过 | gateway source-level compatibility 检查通过 |
| `cmake --build D:/Projects/SiligenSuite/build-job-ingest-phase3 --config Debug --target siligen_dispensing_semantics_tests siligen_runtime_service siligen_runtime_gateway siligen_planner_cli -- /m:4 /clp:ErrorsOnly /nologo` | 通过 | 复跑时先暴露 `MultiCardMotionAdapter.h -> ../internal/UnitConverter.h` 缺口；补齐兼容头后四个 consumer target 全部构建成功 |

- 本轮 closeout 主证据测试已实际执行并通过
- consumer build 证据现已补齐；本轮 `job-ingest` closeout 不再被仓库外构建 blocker 卡住

## 8. Remaining Blockers

### 架构 blocker
- 无新的 `job-ingest` owner blocker；本轮计划中的 producer closeout 已完成

### 构建 blocker
- 无本轮 closeout blocker；consumer build 已补齐成功构建证据

### 测试 blocker
- 无新增 blocker；本轮保留原有 `job-ingest` 测试执行证据，并补齐 consumer build 证据

### 文档 / 命名 blocker
- 已在本轮补记 `execution-report.md` 并同步 `residue-ledger.md` 到 phase 3 后状态

## 9. Final Verdict
- `modules/job-ingest/` 本轮后已明显更接近 canonical owner：contracts owner 统一、application public seam 收缩、producer build 回到 shared-only、consumer truth 改到 module-owned contracts
- 本轮 batch acceptance criteria 已达成：`P3-A`、`P3-B`、`P3-C`、`P3-D`、`P3-E` 完成
- phase 3 closeout 所需主证据与 consumer build 证据现已齐备；若继续下一批次，应针对 residue ledger 中尚未关闭的历史残项单独立题，而不是继续以本轮 closeout 名义扩散修改
