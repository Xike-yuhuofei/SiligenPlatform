# dxf-geometry Execution Report

## 1. Execution Scope
- 本轮处理模块：`modules/dxf-geometry`
- 读取的台账路径：`docs/architecture-audits/modules/dxf-geometry/residue-ledger.md`
- 本轮 batch 范围：
  - `R4 DxfPbPreparationService split`
  - `R5 engineering_data residual quarantine`
  - `R6 python owner gate realignment`

## 2. Execution Oracle
- 本轮冻结的 batch 列表
  - `R4 DxfPbPreparationService split`
    - acceptance criteria：`DxfPbPreparationService` public API 保持不变；环境变量优先级、默认脚本发现、PB 输出校验、现有 C++ tests 行为不变；launcher / command / process 职责不再堆在单一实现文件。
  - `R5 engineering_data residual quarantine`
    - acceptance criteria：`trajectory/offline_path_to_trajectory.py`、`contracts/simulation_input.py`、`preview/html_preview.py` 退化成最薄 public 入口；厚逻辑迁入 `application/engineering_data/residual/`；不新增新的 façade / bridge / public package 名。
  - `R6 python owner gate realignment`
    - acceptance criteria：新增 `modules/dxf-geometry/tests/python/`；覆盖 simulation input、trajectory、3 个 dxf-geometry CLI `--help` smoke；README / contracts / tests 文档明确 `residual/**` 是 quarantine 而不是 canonical closeout。
- 本轮 no-change zone
  - `shared/contracts/engineering/**`
  - `apps/runtime-service/**`
  - `modules/job-ingest/**`
  - `modules/workflow/**`
  - `modules/process-planning/**`
  - `modules/dxf-geometry/adapters/**`
  - 既有仓库级 consumer wrapper 与 console script 名称
- 本轮测试集合
  - `python -m pytest modules/dxf-geometry/tests/python -q`
  - `cmake -S D:\Projects\SiligenSuite -B D:\Projects\SiligenSuite\build-dxf-geometry-residue -G Ninja -DSILIGEN_ENABLE_PROTOBUF=ON -DSILIGEN_BUILD_TESTS=ON`
  - `cmake --build D:\Projects\SiligenSuite\build-dxf-geometry-residue --target siligen_dxf_geometry_unit_tests siligen_dxf_geometry_contract_tests siligen_dxf_geometry_golden_tests siligen_dxf_geometry_integration_tests dxf_geometry_pb_path_source_adapter_contract_test`
  - `ctest --test-dir D:\Projects\SiligenSuite\build-dxf-geometry-residue -C Debug --output-on-failure -R "siligen_dxf_geometry_(unit|contract|golden|integration)_tests|PbPathSourceAdapterContractTest"`
  - `python -m pytest tests/contracts/test_engineering_contracts.py -q`
  - `python -m pytest tests/contracts/test_engineering_data_compatibility.py -q`
  - `$env:PYTHONPATH='D:\Projects\SiligenSuite\modules\dxf-geometry\application'; powershell -NoProfile -ExecutionPolicy Bypass -File scripts/validation/verify-engineering-data-cli.ps1`

## 3. Change Log
- `modules/dxf-geometry/application/services/dxf/PbCommandResolver.h`
  - 新增内部命令解析入口。
  - 负责 `SILIGEN_DXF_PB_COMMAND`、`SILIGEN_DXF_PROJECT_ROOT`、`SILIGEN_DXF_PB_SCRIPT` / workspace 默认脚本，以及 `SILIGEN_ENGINEERING_DATA_PYTHON` / `SILIGEN_DXF_PB_PYTHON` 优先级。
  - 对应消除 residual：`DG-R004`
- `modules/dxf-geometry/application/services/dxf/PbCommandResolver.cpp`
  - 从 `DxfPbPreparationService.cpp` 下沉 command tokenization、脚本定位、外部 root 校验、DXF preprocess 参数拼装。
  - 保持原错误码与优先级不变。
  - 对应消除 residual：`DG-R004`
- `modules/dxf-geometry/application/services/dxf/PbProcessRunner.h`
  - 新增内部进程执行入口。
  - 对应消除 residual：`DG-R004`
- `modules/dxf-geometry/application/services/dxf/PbProcessRunner.cpp`
  - 从 `DxfPbPreparationService.cpp` 下沉跨平台进程启动、旧 `.pb` 删除、输出校验与命令摘要日志。
  - 对应消除 residual：`DG-R004`
- `modules/dxf-geometry/application/services/dxf/DxfPbPreparationService.cpp`
  - 改为只保留输入扩展名判定、缓存/时间戳判断、命令解析 orchestration、进程执行 orchestration。
  - 结果：service 不再直接承载 env compat、workspace script discovery、subprocess 细节。
  - 对应消除 residual：`DG-R004`
- `modules/dxf-geometry/CMakeLists.txt`
  - 将 `PbCommandResolver.cpp`、`PbProcessRunner.cpp` 纳入 `siligen_dxf_geometry_application`。
  - 对应消除 residual：`DG-R004`
- `modules/dxf-geometry/application/engineering_data/residual/__init__.py`
  - 新增 quarantine package 根，显式标记 residual owner。
  - 对应消除 residual：`DG-R001`、`DG-R002`、`DG-R003`
- `modules/dxf-geometry/application/engineering_data/residual/trajectory_generation.py`
  - 承接原 `trajectory/offline_path_to_trajectory.py` 的轨迹生成与报告逻辑，并补充 quarantine 说明。
  - 对应消除 residual：`DG-R001`
- `modules/dxf-geometry/application/engineering_data/residual/simulation_runtime_export.py`
  - 曾承接原 `contracts/simulation_input.py` 的 runtime simulation payload、trigger 投影、export notes interim quarantine；后续 `R9` 已删除。
  - 对应消除 residual：`DG-R002`
- `modules/dxf-geometry/application/engineering_data/residual/html_preview_renderer.py`
  - 承接原 `preview/html_preview.py` 的 HTML renderer 逻辑，并补充 quarantine 说明。
  - 对应消除 residual：`DG-R003`
- `modules/dxf-geometry/application/engineering_data/trajectory/offline_path_to_trajectory.py`
  - 收缩为 residual delegator，只保留 public import surface。
  - 对应消除 residual：`DG-R001`
- `modules/dxf-geometry/application/engineering_data/contracts/simulation_input.py`
  - 当前已收敛为 stable compat shell，只保留当前 consumer 仍依赖的 public symbol surface，并在内部拼接 `runtime_execution` owner 与 geometry helper。
  - 对应消除 residual：`DG-R002`
- `modules/dxf-geometry/application/engineering_data/preview/html_preview.py`
  - 收缩为 residual delegator，只保留 `generate_preview` public surface。
  - 对应消除 residual：`DG-R003`
- `modules/dxf-geometry/tests/python/support.py`
  - 新增模块内 Python lane 支撑文件，固定 `PACKAGE_ROOT`、fixture root 与 `PYTHONPATH` 注入。
  - 对应消除 residual：`DG-R011`
- `modules/dxf-geometry/tests/python/test_engineering_data_residual_quarantine.py`
  - 新增 owner closeout 主证据，覆盖：
    - public surface 绑定到 `runtime_execution` owner + geometry helper
    - preview deterministic contract
    - simulation payload trigger projection / notes
    - trajectory artifact 基本行为与错误分支
  - 对应消除 residual：`DG-R011`
- `modules/dxf-geometry/tests/python/test_engineering_data_cli_lane.py`
  - 新增 module + workspace 共 4 个 engineering-data CLI `--help` smoke。
  - 对应消除 residual：`DG-R011`
- `modules/dxf-geometry/README.md`
  - 明确 `application/engineering_data/residual/` 是 quarantine；`tests/python/` 是模块内 Python owner gate。
  - 对应消除 residual：`DG-R011`
- `modules/dxf-geometry/contracts/README.md`
  - 明确 `residual/**` 只是当前隔离容器，不等于 canonical contract closeout。
  - 对应消除 residual：`DG-R012`
- `modules/dxf-geometry/tests/README.md`
  - 增补模块内 Python lane 的入口和边界说明。
  - 对应消除 residual：`DG-R011`
- `modules/dxf-geometry/domain/README.md`
  - 明确 `domain/` 当前是 shell-only canonical skeleton，不参与 live build。
  - 对应消除 residual：`DG-R013`
- `modules/dxf-geometry/services/README.md`
  - 明确 `services/` 当前是 shell-only canonical skeleton，不参与 live build。
  - 对应消除 residual：`DG-R013`
- `modules/dxf-geometry/examples/README.md`
  - 明确 `examples/` 当前是 shell-only canonical skeleton，不参与 live build。
  - 对应消除 residual：`DG-R013`
- `modules/dxf-geometry/tests/regression/README.md`
  - 用显式 skeleton 说明替代纯 `.gitkeep` 占位，冻结 `tests/regression/` 当前不承载 source-bearing 测试、也不注册 regression target。
  - 对应消除 residual：`DG-R013`
- `modules/dxf-geometry/README.md`
  - 增补统一骨架状态，明确 source-bearing roots 与 shell-only canonical skeleton 的边界。
  - 对应消除 residual：`DG-R013`
- `modules/dxf-geometry/tests/README.md`
  - 增补 `tests/regression/` 当前不进入 live suite 的冻结说明。
  - 对应消除 residual：`DG-R013`
- `docs/architecture-audits/modules/dxf-geometry/residue-ledger.md`
  - 将 `DG-R013` 从待删除 placeholder 调整为 canonical skeleton reclassified closeout，并补齐 workspace layout gate 证据。
  - 对应消除 residual：`DG-R013`

## 4. Residual Disposition Table
| residue_id | file_or_target | disposition | resulting_location | reason |
|---|---|---|---|---|
| DG-R004 | `application/services/dxf/DxfPbPreparationService.cpp` | split | `application/services/dxf/{PbCommandResolver,PbProcessRunner}.*` + 变薄后的 `DxfPbPreparationService.cpp` | public API 保持不变，但 command / process / validation 细节已从单一 service 中剥离。 |
| DG-R001 | `application/engineering_data/trajectory/offline_path_to_trajectory.py` | migrated | `modules/motion-planning/application/motion_planning/trajectory_generation.py` | public 入口保留，但 trajectory 实现 owner 已迁到 `motion-planning`，`dxf-geometry` 只剩 compat wrapper。 |
| DG-R002 | `application/engineering_data/contracts/simulation_input.py` | migrated | `modules/runtime-execution/application/runtime_execution/` + `application/engineering_data/processing/simulation_geometry.py` + stable compat shell | public contract import 不变，但 runtime defaults / trigger projection 已迁出 `dxf-geometry`，不再伪装为 canonical contract 实现。 |
| DG-R003 | historical `application/engineering_data/preview/html_preview.py` | migrated | `apps/planner-cli/tools/planner_cli_preview/` | workspace preview 入口保留，但本地 HTML renderer 已切到 `apps/planner-cli` owner。 |
| DG-R011 | `modules/dxf-geometry/tests/python/**` | migrated | 模块内 Python lane 已建立 | owner closeout 不再只依赖仓库级 consumer tests 与 validation script。 |
| DG-R005 | `module.yaml` / `adapters` target 命名 / 跨模块依赖 | aligned | `module.yaml` + current CMake graph | `allowed_dependencies`、module root target 与 parser seam 命名已对齐当前 live build truth。 |
| DG-R006 | `siligen_parsing_adapter` | deleted | `siligen_dxf_geometry_pb_path_source_adapter` | generic parser target 已退出 live graph，模块与 consumer 只承认 owner-scoped parser seam。 |
| DG-R007 | `DXFAdapterFactory*` / `AutoPathSourceAdapter*` | deleted | retired | 旧 selector / auto adapter seam 未被恢复；当前 `dxf-geometry` live tree 只保留 `PbPathSourceAdapter`。 |
| DG-R013 | `domain/README.md` / `services/README.md` / `examples/README.md` / `tests/regression/README.md` | reclassified | canonical skeleton（shell-only / non-live root） | workspace layout gate 要求这些目录物理存在；当前已明确它们不是 live build root，也不是 live regression suite。 |

## 5. Dependency Reconciliation Result
- `module.yaml / CMake / target_link_libraries / public headers` 当前已对齐 parser target 命名与最小依赖集合，没有重新拉偏。
- 本轮新增的 C++ 内部 helper 仅挂到 `siligen_dxf_geometry_application`，没有对外扩张 public surface。
- 当前仍由 `dxf-geometry` 承担的 Python public import 只剩：
  - `engineering_data.contracts.simulation_input`
  - `engineering_data.cli.export_simulation_input`
  - `engineering_data.cli.dxf_to_pb`
- trajectory workspace 入口保持为 `scripts/engineering-data/path_to_trajectory.py`。
  - 实现 owner：`modules/motion-planning/application/motion_planning/`
  - `dxf-geometry` 只保留 `engineering_data.trajectory.offline_path_to_trajectory` compat wrapper 与 `-m engineering_data.cli.path_to_trajectory` 兼容入口
- preview workspace 入口保持为 `scripts/engineering-data/generate_preview.py`。
  - 实现 owner：`apps/planner-cli/tools/planner_cli_preview/`
  - 已退出的旧 seam：`engineering_data.contracts.preview`、`engineering_data.preview.html_preview.generate_preview`、`engineering_data.cli.generate_preview`
- `domain/`、`services/`、`examples/` 与 `tests/regression/` 已冻结为 workspace layout 要求的 canonical skeleton。
  - `CMakeLists.txt` 当前只把 `application/`、`adapters/`、`tests/` 计入 implementation roots，没有把这些 shell-only roots 拉回 live build。

## 6. Surface Reconciliation Result
- stable seam 保护结果
  - `dxf_geometry/application/services/dxf/DxfPbPreparationService.h` 未变
  - `DxfPbPreparationService::EnsurePbReady(const std::string&) -> Result<std::string>` 未变
  - `engineering_data` 既有 public import / CLI 模块名未变
- façade / provider / bridge 多套并存
  - 本轮未新增任何新的 façade / provider / bridge / compat 壳层
  - C++ 侧 `DxfPbPreparationService` 明显变薄
  - Python 侧 `simulation_input` public modules 已收敛为最薄 compat shell
- 内部 stage 类型泄漏
  - 本轮未新增新的内部类型泄漏到 public surface
  - `simulation_input` runtime concrete 已迁出；当前不再存在对应 residual 泄漏

## 7. Test Result
- 实际运行的测试
  - `python -m pytest modules/dxf-geometry/tests/python -q`
  - `python -m pytest apps/planner-cli/tests/python -q`
  - `python -m pytest modules/motion-planning/tests/python -q`
  - `python -m pytest tests/contracts/test_dxf_geometry_parser_target_closeout.py -q`
  - `python scripts/migration/validate_workspace_layout.py --wave "dxf-geometry DG-R013 closeout"`
  - `cmake -S D:\Projects\SiligenSuite -B D:\Projects\SiligenSuite\build-dxf-geometry-residue -G Ninja -DSILIGEN_ENABLE_PROTOBUF=ON -DSILIGEN_BUILD_TESTS=ON`
  - `cmake --build D:\Projects\SiligenSuite\build-dxf-geometry-residue --target siligen_dxf_geometry_unit_tests siligen_dxf_geometry_contract_tests siligen_dxf_geometry_golden_tests siligen_dxf_geometry_integration_tests dxf_geometry_pb_path_source_adapter_contract_test`
  - `ctest --test-dir D:\Projects\SiligenSuite\build-dxf-geometry-residue -C Debug --output-on-failure -R "siligen_dxf_geometry_(unit|contract|golden|integration)_tests|PbPathSourceAdapterContractTest"`
  - `python -m pytest tests/contracts/test_engineering_contracts.py -q`
  - `python -m pytest tests/contracts/test_engineering_data_compatibility.py -q`
  - `python -m pytest tests/contracts/test_process_path_legacy_compat_contract.py -q`
  - `$env:PYTHONPATH='D:\Projects\SiligenSuite\modules\dxf-geometry\application'; powershell -NoProfile -ExecutionPolicy Bypass -File scripts/validation/verify-engineering-data-cli.ps1`
- 通过
  - 模块内 Python lane：`8 passed`
  - planner-cli preview owner lane：`3 passed`
  - motion-planning trajectory owner lane：`3 passed`
  - parser target closeout contract：`4 passed`
  - workspace layout gate：`status=passed`
  - C++ closeout 主证据：`5/5 tests passed`
  - 仓库级 engineering-data contracts：`8 passed, 1 skipped`
  - 仓库级 engineering-data compatibility：`5 passed`
  - 仓库级 process-path legacy compat contract：`6 passed`
  - CLI validation script：`status=passed`
- 失败
  - 本轮实际执行集合内无失败
- 追加重验结论
  - `2026-04-09` 已补跑 `tests/contracts/test_process_path_legacy_compat_contract.py`
  - 结果：仓库级 process-path legacy compat contract 已与当前删除态收敛；无需为该测试恢复已删除 seam

## 8. Remaining Blockers
- none

## 9. Final Verdict
- `modules/dxf-geometry` 本轮后更接近 canonical owner。
- 本轮 batch acceptance criteria 已达成：
  - `R4` 达成：`DxfPbPreparationService` 已拆薄且 C++ 行为未回归
  - `R5` 达成：Python public surface 先完成 quarantine 隔离，且后续 `R9` 已完成最终 owner 迁移
  - `R6` 达成：模块内 Python owner gate 已建立并实际执行
- 后续补充 closeout 也已达成：
  - `R10` 达成：`DG-R013` 已重分类为 canonical skeleton，`domain/`、`services/`、`examples/` 与 `tests/regression/` 不再作为待删除 residue 挂起
- `siligen_parsing_adapter` closeout 已从立即 blocker 列表移出：
  - `python -m pytest tests/contracts/test_dxf_geometry_parser_target_closeout.py -q` 已重验通过
- `历史 compat 测试清理` 已从当前立即 blocker 列表移出：
  - `tests/contracts/test_process_path_legacy_compat_contract.py` 已重验通过

## 10. Later Status Update
- `2026-04-09` 已完成 `R7 local preview owner cutover`
  - workspace preview 入口保留为 `scripts/engineering-data/generate_preview.py`
  - 实现 owner 已切到 `apps/planner-cli/tools/planner_cli_preview/`
  - `modules/dxf-geometry/application/engineering_data/{preview,cli/contracts preview,residual html renderer}` 已退出 live owner 面
- `2026-04-09` 起的 preview 验证改由以下入口承担：
  - `python -m pytest apps/planner-cli/tests/python -q`
  - `python scripts/engineering-data/generate_preview.py --help`
  - `python -m pytest tests/contracts/test_engineering_contracts.py -q`
  - `python -m pytest tests/contracts/test_engineering_data_compatibility.py -q`
- `2026-04-09` 已补做 planner-cli binary smoke：
  - `cmake --build D:\Projects\SiligenSuite\build-dxf-geometry-residue --target siligen_planner_cli`
  - `D:\Projects\SiligenSuite\build-dxf-geometry-residue\bin\siligen_planner_cli.exe dxf-preview --file D:\Projects\SiligenSuite\shared\contracts\engineering\fixtures\cases\rect_diag\rect_diag.dxf --output-dir D:\Projects\SiligenSuite\tests\reports\dxf-preview-smoke --json`
  - 结果：`siligen_planner_cli` 已成功构建，`dxf-preview` 本地 preview JSON 与 HTML 生成链路可用。
- `2026-04-09` 已完成 `R8 trajectory owner migration`
  - workspace trajectory 入口保留为 `scripts/engineering-data/path_to_trajectory.py`
  - 实现 owner 已切到 `modules/motion-planning/application/motion_planning/`
  - `modules/dxf-geometry/application/engineering_data/trajectory/offline_path_to_trajectory.py` 已退化为 compat wrapper
  - `modules/dxf-geometry/application/engineering_data/residual/trajectory_generation.py` 已退出 live owner 面
  - `python -m pytest modules/motion-planning/tests/python -q` 已作为新的 owner gate 通过
- `2026-04-09` 已完成 `R9 simulation_input owner split`
  - stable surface 保留为 `engineering_data.contracts.simulation_input`、`engineering_data.cli.export_simulation_input` 与 `scripts/engineering-data/export_simulation_input.py`
  - runtime concrete owner 已切到 `modules/runtime-execution/application/runtime_execution/{contracts,simulation_input,export_simulation_input}.py`
  - `dxf-geometry` 只保留 `engineering_data.processing.simulation_geometry` 中的 geometry helper 与最薄 compat shell
  - `modules/dxf-geometry/application/engineering_data/residual/simulation_runtime_export.py` 已删除
  - `python -m pytest modules/runtime-execution/tests/python -q` 已作为新的 owner gate 纳入本轮验证集合
- `2026-04-09` 已补做 parser target closeout 重验
  - `python -m pytest tests/contracts/test_dxf_geometry_parser_target_closeout.py -q`
  - 结果：`4 passed`，`siligen_parsing_adapter` 不再出现在 `modules/dxf-geometry`、`apps/runtime-service`、`README`、`module.yaml` 的 live truth 中
- `2026-04-09` 已完成 `R10 placeholder skeleton closeout`
  - `domain/`、`services/`、`examples/` 现已明确为 shell-only canonical skeleton，不参与 live build。
  - `tests/regression/` 已改为带 README 的 canonical skeleton，不承载 source-bearing 测试，也不注册 regression target。
  - `scripts/migration/validate_workspace_layout.py` 继续要求这些目录物理存在；因此 `DG-R013` 以 reclassified closeout 收口，而不是删除目录。
- 本轮后剩余立即 blocker 收敛为：
  - 无
