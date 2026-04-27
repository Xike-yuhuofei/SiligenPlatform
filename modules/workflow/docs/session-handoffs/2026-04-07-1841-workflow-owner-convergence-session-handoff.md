# 新窗口接续摘要

## 1. 任务本质
- 本会话围绕 `D:/Projects/SiligenSuite/modules/workflow` 的架构收敛工作展开。
- 原始任务从“重构不彻底风险审查”转为“按既定方案彻底清除架构重构遗留问题”。
- 按会话中已有计划，目标是把 `workflow` 收敛为真正的 `M0 orchestration owner`，并消除 compatibility shim、legacy fallback、混合 owner 边界和命名残留。

## 2. 当前阶段
- 按当前会话压缩摘要记录：原计划分为 6 个阶段。
- 压缩摘要明确记录：Phase 1、Phase 2、Phase 3、Phase 4 已完成，且 Phase 4 做过真实构建验证。
- 本次窗口后半段实际完成了 Phase 5（DXF legacy fallback 硬删除与最小验证）。
- Phase 6 尚未开始实施；在开始前，用户已明确要求“停止继续实施”，改为只生成交接文档。

## 3. 当前目标
- 停止前的直接目标是进入 Phase 6：收尾 `workflow/tests` canonical 命名和相关文档口径。
- 该目标在会话中被明确表述为：去掉 `tests/process-runtime-core` 这套 canonical 命名，并同步 README / CMake / 测试入口与当前 owner / build graph 一致。
- 当前窗口的新任务不是继续实施，而是生成并保存本交接文档。

## 4. 已确认事实
- 用户在本会话中已明确同意从审查转向直接整改，并通过 `/plan` 要求按报告清除遗留问题。
- 用户在本会话中锁定了 4 项不可协商决策：
  1. `siligen_process_runtime_core_*` 本轮必须删除。
  2. workflow 中仍编译的 motion execution concrete 本轮必须迁出 workflow。
  3. preview snapshot DTO + assembly 必须迁到 M8（`dispense-packaging`）。
  4. `SILIGEN_DXF_AUTOPATH_LEGACY` 及相关 fallback 本轮必须硬删除。
- 计划文件在会话中已给出：`C:/Users/Xike/.claude/plans/nested-snacking-moon.md`。
- 会话后半段已经实际修改并验证了 DXF 相关收口：
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/AutoPathSourceAdapter.cpp`
  - `modules/workflow/adapters/include/infrastructure/adapters/planning/dxf/AutoPathSourceAdapter.h`
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/CMakeLists.txt`
  - `modules/workflow/tests/process-runtime-core/CMakeLists.txt`
  - `modules/workflow/tests/process-runtime-core/unit/infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp`
  - `modules/workflow/application/usecases/dispensing/README.md`
- 会话中已物理删除以下 workflow DXF 迁移残留文件：
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/DXFMigrationConfig.cpp`
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/DXFMigrationConfig.h`
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/DXFMigrationManager.cpp`
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/DXFMigrationManager.h`
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/DXFMigrationValidator.cpp`
  - `modules/workflow/adapters/infrastructure/adapters/planning/dxf/DXFMigrationValidator.h`
- 会话中已同步修改以下文档：
  - `D:/Projects/SiligenSuite/docs/decisions/process-path-legacy-compat-freeze.md`
  - `D:/Projects/SiligenSuite/docs/architecture/history/audits/architecture-audits/DXF_full_analysis_compare.md`
  - `D:/Projects/SiligenSuite/modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
- 为了让 tests-on 构建通过，本会话还修正了 redundancy 相关 include：
  - `modules/workflow/application/services/redundancy/CandidateScoringService.cpp`
  - `modules/workflow/application/usecases/redundancy/RedundancyGovernanceUseCases.h`
  - `modules/workflow/application/usecases/redundancy/RedundancyGovernanceUseCases.cpp`
- 已进行真实构建/测试验证：
  - `build-phase5-verify` 成功 configure。
  - `build-phase5-tests` 以 `-DSILIGEN_BUILD_TESTS=ON` 成功 configure。
  - `cmake --build ... --target siligen_parsing_adapter` 成功。
  - `cmake --build ... --target siligen_unit_tests` 成功。
  - `cmake --build ... --target siligen_dispensing_semantics_tests` 成功。
  - `ctest --test-dir "D:/Projects/SiligenSuite/build-phase5-tests" -C Debug --output-on-failure -R "siligen_unit_tests|siligen_dispensing_semantics_tests"` 结果为 2/2 通过。
- 在 `modules/workflow` 范围内，针对以下模式的 live 残留搜索结果为无匹配：
  - `SILIGEN_DXF_AUTOPATH_LEGACY`
  - `siligen_workflow_dxf_legacy_compat`
  - `SILIGEN_BUILD_WORKFLOW_DXF_LEGACY_COMPAT`
  - `DXFMigration(Config|Validator|Manager)`
- `build-phase5-verify` 中未生成 workflow 测试工程的直接原因已确认：`CMakeCache.txt` 显示 `SILIGEN_BUILD_TESTS:BOOL=OFF`。

## 5. 关键假设与约束
- 本摘要只基于当前会话中已明确出现的信息，包括压缩摘要、显式读到的文件内容、实际命令输出和当前轮对话指令。
- 不把会话外信息写入事实；对未在会话中明确确认的内容，统一放到“推断”或“未知”。
- 用户已明确要求停止继续实施；新窗口不应默认继续分析、设计、编码或调试。
- 用户级 `CLAUDE.md` 已明确：始终使用中文对话。
- 本会话中，多次出现“用户要求在需要摘要时使用纯文本且带 `<analysis>` / `<summary>`”的约束；这是会话内已出现的用户偏好。
- 本会话已出现“编辑器/clang 诊断与真实构建结果不完全一致”的情况；后续如需验证，应优先参考真实 CMake/MSBuild/ctest 结果。

## 6. 已完成事项
- 会话压缩摘要已记录：
  - Phase 1：workflow 构建与 public surface 收口完成。
  - Phase 2：PlanningUseCase 混合装配清理完成。
  - Phase 3：preview snapshot owner 迁移完成。
  - Phase 4：bridge 清理与 execution concrete 迁移完成，并做过真实构建验证。
- 本窗口后半段完成的事项：
  - 移除了 workflow DXF legacy fallback 的 live 代码路径。
  - 移除了 workflow DXF legacy compat target 的 live 构建残留。
  - 将 `AutoPathSourceAdapter` 收口为 canonical `.pb` 读取边界，对 direct `.dxf` 输入执行 hard-fail。
  - 更新 DXF 相关测试语义，使其断言 direct DXF 被拒绝、而不是断言旧 env 开关行为。
  - 删除了 workflow DXF 迁移壳代码文件（`DXFMigration*`）。
  - 收口了与 DXF 相关的文档和文件树说明。
  - 修复了一个与 DXF 主任务无关但阻塞 tests-on 构建的 redundancy include 问题。
  - 完成了最小真实验证：`siligen_parsing_adapter`、`siligen_unit_tests`、`siligen_dispensing_semantics_tests` 编译通过，相关 `ctest` 通过。

## 7. 未完成事项
- Phase 6 尚未开始实施。
- `workflow/tests` canonical 命名仍存在 `process-runtime-core` 目录与文档口径残留；本窗口未开始改名或改文档。
- 会话内没有完成以下更广范围验证：
  - workflow 全量测试矩阵
  - process-path / motion-planning / runtime-execution / dispense-packaging 的更广联动回归
- repo 范围内仍可搜到一些旧术语/旧路径引用，但这些匹配发生在以下位置，未在本窗口被清理：
  - `build-phase4-verify/CMakeCache.txt`
  - `docs/process-model/plans/*.md`
- `workflow_pb_path_source_adapter_test` 这个特定 target 在 `build-phase5-verify` 中未生成；原因是 tests-off 构建。当前会话未进一步确认它在 tests-on 构建中的状态。

## 8. 关键文件 / 文档 / 路径 / 命令
### 关键文件
- 计划文件：`C:/Users/Xike/.claude/plans/nested-snacking-moon.md`
- DXF 收口核心：
  - `D:/Projects/SiligenSuite/modules/workflow/adapters/infrastructure/adapters/planning/dxf/AutoPathSourceAdapter.cpp`
  - `D:/Projects/SiligenSuite/modules/workflow/adapters/include/infrastructure/adapters/planning/dxf/AutoPathSourceAdapter.h`
  - `D:/Projects/SiligenSuite/modules/workflow/adapters/infrastructure/adapters/planning/dxf/CMakeLists.txt`
  - `D:/Projects/SiligenSuite/modules/workflow/tests/process-runtime-core/CMakeLists.txt`
  - `D:/Projects/SiligenSuite/modules/workflow/tests/process-runtime-core/unit/infrastructure/adapters/planning/dxf/DXFAdapterFactoryTest.cpp`
- 文档：
  - `D:/Projects/SiligenSuite/modules/workflow/application/usecases/dispensing/README.md`
  - `D:/Projects/SiligenSuite/docs/decisions/process-path-legacy-compat-freeze.md`
  - `D:/Projects/SiligenSuite/docs/architecture/history/audits/architecture-audits/DXF_full_analysis_compare.md`
  - `D:/Projects/SiligenSuite/modules/MODULES_BUSINESS_FILE_TREE_AND_TABLES.md`
- 构建阻塞修复：
  - `D:/Projects/SiligenSuite/modules/workflow/application/services/redundancy/CandidateScoringService.cpp`
  - `D:/Projects/SiligenSuite/modules/workflow/application/usecases/redundancy/RedundancyGovernanceUseCases.h`
  - `D:/Projects/SiligenSuite/modules/workflow/application/usecases/redundancy/RedundancyGovernanceUseCases.cpp`
- 下一阶段直接相关但尚未处理的 Phase 6 文件（来自计划与会话）：
  - `D:/Projects/SiligenSuite/modules/workflow/tests/README.md`
  - `D:/Projects/SiligenSuite/modules/workflow/tests/CMakeLists.txt`
  - `D:/Projects/SiligenSuite/modules/workflow/tests/process-runtime-core/CMakeLists.txt`
  - `D:/Projects/SiligenSuite/modules/workflow/tests/integration/README.md`
  - `D:/Projects/SiligenSuite/modules/workflow/tests/regression/README.md`
  - `D:/Projects/SiligenSuite/modules/workflow/README.md`

### 关键路径
- 当前模块根：`D:/Projects/SiligenSuite/modules/workflow`
- tests-off 验证目录：`D:/Projects/SiligenSuite/build-phase5-verify`
- tests-on 验证目录：`D:/Projects/SiligenSuite/build-phase5-tests`

### 关键命令（本会话已执行）
- `cmake -S "D:/Projects/SiligenSuite" -B "D:/Projects/SiligenSuite/build-phase5-verify" -G "Visual Studio 17 2022" -A x64`
- `cmake --build "D:/Projects/SiligenSuite/build-phase5-verify" --config Debug --target siligen_parsing_adapter`
- `cmake -S "D:/Projects/SiligenSuite" -B "D:/Projects/SiligenSuite/build-phase5-tests" -G "Visual Studio 17 2022" -A x64 -DSILIGEN_BUILD_TESTS=ON`
- `cmake --build "D:/Projects/SiligenSuite/build-phase5-tests" --config Debug --target siligen_unit_tests`
- `cmake --build "D:/Projects/SiligenSuite/build-phase5-tests" --config Debug --target siligen_dispensing_semantics_tests`
- `ctest --test-dir "D:/Projects/SiligenSuite/build-phase5-tests" -C Debug --output-on-failure -R "siligen_unit_tests|siligen_dispensing_semantics_tests"`

## 9. 当前实现状态 / 观察结果
- 当前会话明确显示：workflow 内 DXF live 运行边界已收口到 canonical `.pb` 消费路径。
- 当前会话读取到的 `DXFAdapterFactoryTest.cpp` 断言：local adapter 对 direct `.dxf` 输入返回 `ErrorCode::FILE_FORMAT_INVALID`。
- 当前会话中，`siligen_parsing_adapter` 能成功编译。
- 当前会话中，`siligen_unit_tests` 与 `siligen_dispensing_semantics_tests` 能成功编译并通过对应 `ctest`。
- 当前会话中，`modules/workflow` 范围内对 DXF legacy 关键模式的搜索结果为无 live 残留。
- 当前会话也显示：repo 范围内仍存在部分历史性文本残留，主要位于旧 build cache 和 `docs/process-model/plans` 下的计划文档中。
- 当前会话显示：为让 tests-on 编译继续进行，已经修复了一处 redundancy include 路径问题；该问题不属于 DXF legacy 主线，但会阻塞 tests-on 构建。

## 10. 阻塞点 / 风险点
- 直接阻塞：用户已经明确要求停止继续实施；新窗口如果要继续原任务，需要先得到新的用户授权。
- 技术风险：Phase 6 还未开始，`workflow/tests` canonical 命名和文档口径仍未收尾。
- 覆盖风险：本窗口只做了最小真实验证，未重新跑更大范围测试矩阵。
- 搜索噪音风险：repo 全局仍有历史计划文档和旧 build cache 出现旧术语；这些匹配不等于 live 代码仍依赖旧机制，但会影响后续检索判断。
- 诊断噪音风险：本会话中 clang/LSP 诊断与真实构建结果不完全一致；后续若继续，需要区分“编辑器诊断”与“真实构建失败”。

## 11. 待确认项
- 用户是否仍要求继续整个 6 阶段收尾，还是到 Phase 5 即可暂停。
- Phase 6 是否要物理重命名目录 `tests/process-runtime-core/`，还是先只改 target / README / 入口命名。
- `docs/process-model/plans/*.md` 里的旧术语是否需要一并清理，还是保留为历史计划文档。
- 是否需要在继续 Phase 6 前，先补更大范围的构建/测试矩阵验证。
- `workflow_pb_path_source_adapter_test` 在 tests-on 构建中的状态，当前会话未明确确认。

## 12. 建议新窗口立刻执行的下一步
- 先完整阅读本摘要与计划文件 `C:/Users/Xike/.claude/plans/nested-snacking-moon.md`。
- 然后先向用户确认：是否重新授权继续原任务，以及是否直接进入 Phase 6。
- 如果用户明确授权继续，建议从以下文件开始做 Phase 6 的最小范围盘点：
  - `modules/workflow/tests/README.md`
  - `modules/workflow/tests/CMakeLists.txt`
  - `modules/workflow/tests/process-runtime-core/CMakeLists.txt`
  - `modules/workflow/tests/integration/README.md`
  - `modules/workflow/tests/regression/README.md`
  - `modules/workflow/README.md`

## 13. 给新窗口的执行约束
- 不要把本摘要中的“推断”写成事实。
- 不要默认继续编码、调试或改文件；当前用户的最新明确指令是停止继续实施、只生成交接文档。
- 始终使用中文对话。
- 如果用户再次要求“总结/汇总”，优先遵守其本会话中已多次强调的摘要格式约束（纯文本、必要时使用 `<analysis>` / `<summary>`）。
- 如果后续继续验证，优先参考真实的 CMake / MSBuild / ctest 结果，而不是单独依赖 clang/LSP 诊断。
- 不要把 repo 全局搜到的历史文档或旧 build cache 命中，直接当作 live 依赖证据。

## 14. 事实 / 推断 / 未知 三分表
| 类别 | 内容 |
|---|---|
| 事实 | 当前会话压缩摘要记录：Phase 1-4 已完成，且 Phase 4 已做真实构建验证。 |
| 事实 | 本窗口后半段已完成 Phase 5 的 DXF legacy fallback 清理，并完成最小真实验证。 |
| 事实 | `build-phase5-tests` 以 `-DSILIGEN_BUILD_TESTS=ON` 成功 configure。 |
| 事实 | `siligen_parsing_adapter`、`siligen_unit_tests`、`siligen_dispensing_semantics_tests` 已在本会话中真实编译成功。 |
| 事实 | `ctest` 中 `siligen_unit_tests` 与 `siligen_dispensing_semantics_tests` 均通过。 |
| 事实 | 在 `modules/workflow` 范围内，对 DXF legacy 关键模式的搜索结果为无匹配。 |
| 事实 | 用户的最新明确指令是：停止继续实施，只生成交接文档。 |
| 推断 | 如果后续继续原任务，最自然的下一阶段是按既定计划进入 Phase 6 收尾 tests 命名与文档。 |
| 推断 | `docs/process-model/plans/*.md` 中的旧术语更像历史计划文档残留，而不是 live 代码依赖；但本会话未做最终定性。 |
| 推断 | `workflow_pb_path_source_adapter_test` 可能需要在 tests-on 构建中单独确认；本会话未覆盖。 |
| 未知 | 用户是否仍希望继续整个 6 阶段任务。 |
| 未知 | Phase 6 是否要求物理重命名 `tests/process-runtime-core/` 目录。 |
| 未知 | 是否需要在继续 Phase 6 前先补跑更大范围测试矩阵。 |
