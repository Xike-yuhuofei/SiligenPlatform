# 新窗口接续摘要

## 1. 任务本质
- 用户最初要求对 `modules/dispense-packaging` 相关架构重构完成度做严格事实审计，并基于真实代码、真实构建图、真实运行调用链、真实测试结果判断 owner 边界是否真正收敛。
- 随后任务从审计转为实施清理：持续收缩 `workflow` 侧遗留 bridge / compat / residual，并让 live build 与目标 owner 边界一致。
- 当前窗口结束前，用户明确要求：停止继续实施，只生成本交接文档。

## 2. 当前阶段
- 会话内已有 summary 明确提到：清理已进入 Phase 4/5 区域。
- 本窗口最新实际落点是两类工作：
  1. 收缩 `modules/workflow/application/CMakeLists.txt` 中 `siligen_application_dispensing` 与 `siligen_application_system` 的宽聚合依赖。
  2. 修复 workflow 阀门残留实现未进入 live build 导致的链接缺口。
- 当前窗口未继续推进到 `siligen_application_motion` 的实际修改。

## 3. 当前目标
- 从会话最后一次实现状态看，当前主线目标仍是：继续缩小 `workflow/application` 中剩余宽聚合依赖面，同时保持当前仍保留的 workflow valve/purge residual 链路可构建、可验证。
- 在本窗口结束前，下一步已明确指向：审查并收缩 `siligen_application_motion` 的宽聚合依赖；但该步骤尚未开始实施。

## 4. 已确认事实
- `modules/workflow/application/CMakeLists.txt` 中，`siligen_application_dispensing` 已移除对 `siligen_domain`、`siligen_domain_services` 的直接链接，只保留更窄的真实依赖。
- `siligen_application_dispensing` 变更后，以下验证已通过：
  - `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_application_dispensing --parallel`
  - `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_dispensing_semantics_tests --parallel`
  - `ctest --test-dir "D:/Projects/SiligenSuite/build" -C Debug -R "^siligen_dispensing_semantics_tests$" --output-on-failure`
- `modules/workflow/application/CMakeLists.txt` 中，`siligen_application_system` 已移除对 `siligen_domain`、`siligen_domain_services` 的直接链接，改为依赖 `siligen_safety_domain_services`、`siligen_dispense_packaging_domain_dispensing` 及原有基础依赖。
- `siligen_application_system` 变更后，以下构建已通过：
  - `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_application_system --parallel`
- 之后暴露并确认了一个真实链接缺口：workflow 阀门残留实现未进入当前 live build，导致 `siligen_unit_tests` 与 `siligen_planner_cli` 链接失败，缺失符号包括 `PurgeDispenserProcess` 与 `ValveCoordinationService` 的构造/方法实现。
- 为修复该缺口，`modules/workflow/domain/domain/CMakeLists.txt` 中已恢复 `siligen_dispensing_execution_services` target，源文件为：
  - `dispensing/domain-services/PurgeDispenserProcess.cpp`
  - `dispensing/domain-services/SupplyStabilizationPolicy.cpp`
  - `dispensing/domain-services/ValveCoordinationService.cpp`
- `modules/workflow/application/usecases/dispensing/valve/CMakeLists.txt` 中，`siligen_valve_core` 已改为显式链接：
  - `siligen_dispensing_execution_services`
  - `siligen_process_planning_contracts_public`
- 修复后，以下构建/测试已通过：
  - `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_unit_tests --parallel`
  - `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_planner_cli --parallel`
  - `"D:/Projects/SiligenSuite/build/bin/Debug/siligen_unit_tests.exe" --gtest_filter="PurgeDispenserProcessTest.*:ValveCoordinationServiceTest.*:InitializeSystemUseCaseTest.*"`
  - `ctest --test-dir "D:/Projects/SiligenSuite/build" -C Debug -R "^siligen_dispensing_semantics_tests$" --output-on-failure`
- 会话内已明确观察到：`apps/runtime-service/container/ApplicationContainer.Dispensing.cpp` 仍直接构造 `Domain::Dispensing::DomainServices::ValveCoordinationService`。
- 会话内已有 summary 明确记录：M9 执行 owner 切换已完成到 `CreateDispensingProcessPort(...)`；workflow 侧 valve/purge residual 仍保留并仍被下游使用。

## 5. 关键假设与约束
- 约束（用户明确给出或会话环境明确给出）：
  - 不要回滚 intentional changes。
  - 尤其不要撤销 intentional CMake changes。
  - 继续实施时直接做，不要反复询问。
  - 本项目对话使用中文。
  - 本窗口最后一条用户指令：停止继续实施，只生成交接文档。
- 假设（仅作为交接说明，不当作事实）：
  - 本文中的“当前阶段已进入 Phase 4/5 区域”来自会话内现成 summary/plan 信息，而不是本窗口重新判定。
  - 本次保存路径 `docs/session-handoffs/` 按当前工作目录 `D:/Projects/SiligenSuite/modules/dispense-packaging` 解析。

## 6. 已完成事项
1. 审查并收缩了 `siligen_application_dispensing` 的宽聚合链接依赖。
2. 对 `siligen_application_dispensing` 做了最小构建与语义测试验证，结果通过。
3. 审查并收缩了 `siligen_application_system` 的宽聚合链接依赖。
4. 对 `siligen_application_system` 做了目标级构建验证，结果通过。
5. 定位到 workflow 阀门残留实现 target 缺失导致的真实链接问题。
6. 在 `workflow/domain/domain/CMakeLists.txt` 恢复了窄 residual target：`siligen_dispensing_execution_services`。
7. 将 `siligen_valve_core` 重新接回该 residual target 的显式链接。
8. 重新验证了 `siligen_unit_tests`、`siligen_planner_cli`、focused gtest 与 `siligen_dispensing_semantics_tests`，结果通过。

## 7. 未完成事项
- 尚未开始对 `siligen_application_motion` 做同类宽聚合依赖收缩。
- 尚未开始对 `siligen_application_redundancy` 做同类宽聚合依赖收缩。
- 尚未继续处理 `siligen_workflow_dispensing_planning_compat` 的去留。
- 尚未重新评估 workflow valve/purge residual 的最终 owner 去向；当前只完成了“让它重新进入 live build 并恢复链接”。
- 尚未在本窗口内执行更大范围的全链路回归（例如 runtime-service 全量测试/更多下游目标）。

## 8. 关键文件 / 文档 / 路径 / 命令
### 关键文件
- `D:/Projects/SiligenSuite/modules/workflow/application/CMakeLists.txt`
- `D:/Projects/SiligenSuite/modules/workflow/application/usecases/dispensing/valve/CMakeLists.txt`
- `D:/Projects/SiligenSuite/modules/workflow/domain/domain/CMakeLists.txt`
- `D:/Projects/SiligenSuite/modules/workflow/application/include/application/usecases/system/InitializeSystemUseCase.h`
- `D:/Projects/SiligenSuite/modules/workflow/application/usecases/system/InitializeSystemUseCase.cpp`
- `D:/Projects/SiligenSuite/modules/workflow/application/include/application/usecases/system/EmergencyStopUseCase.h`
- `D:/Projects/SiligenSuite/modules/workflow/application/usecases/system/EmergencyStopUseCase.cpp`
- `D:/Projects/SiligenSuite/modules/workflow/domain/domain/dispensing/domain-services/PurgeDispenserProcess.cpp`
- `D:/Projects/SiligenSuite/modules/workflow/domain/domain/dispensing/domain-services/ValveCoordinationService.cpp`
- `D:/Projects/SiligenSuite/modules/workflow/domain/domain/dispensing/domain-services/SupplyStabilizationPolicy.cpp`
- `D:/Projects/SiligenSuite/modules/workflow/tests/process-runtime-core/CMakeLists.txt`
- `D:/Projects/SiligenSuite/modules/workflow/tests/process-runtime-core/unit/system/InitializeSystemUseCaseTest.cpp`
- `D:/Projects/SiligenSuite/apps/runtime-service/container/ApplicationContainer.Dispensing.cpp`
- `C:/Users/Xike/.claude/plans/declarative-nibbling-manatee.md`

### 关键路径
- 构建目录：`D:/Projects/SiligenSuite/build`
- 当前工作目录：`D:/Projects/SiligenSuite/modules/dispense-packaging`

### 关键命令
- `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_application_dispensing --parallel`
- `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_dispensing_semantics_tests --parallel`
- `ctest --test-dir "D:/Projects/SiligenSuite/build" -C Debug -R "^siligen_dispensing_semantics_tests$" --output-on-failure`
- `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_application_system --parallel`
- `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_unit_tests --parallel`
- `cmake --build "D:/Projects/SiligenSuite/build" --config Debug --target siligen_planner_cli --parallel`
- `"D:/Projects/SiligenSuite/build/bin/Debug/siligen_unit_tests.exe" --gtest_filter="PurgeDispenserProcessTest.*:ValveCoordinationServiceTest.*:InitializeSystemUseCaseTest.*"`

## 9. 当前实现状态 / 观察结果
- 当前已确认：
  - `siligen_application_dispensing` 已不再直接依赖 `siligen_domain` / `siligen_domain_services`。
  - `siligen_application_system` 已不再直接依赖 `siligen_domain` / `siligen_domain_services`。
- 当前仍可从会话已读内容确认：
  - `siligen_application_motion` 仍直接链接 `siligen_domain`、`siligen_domain_services`。
  - `siligen_application_redundancy` 仍直接链接 `siligen_domain`、`siligen_domain_services`。
- workflow valve/purge residual 当前状态：
  - 其实现文件仍位于 `modules/workflow/domain/domain/dispensing/domain-services/`。
  - 它们已重新被 `siligen_dispensing_execution_services` 纳入 live build。
  - `siligen_valve_core` 当前通过该 target 获取实现链接。
- 从当前窗口的构建结果看，恢复 residual target 后：
  - `siligen_unit_tests` 可构建。
  - `siligen_planner_cli` 可构建。
  - 选中的 focused gtests 可通过。
- 会话内既有 summary 明确指出：
  - DXF execution 的 canonical owner 已切到 M9/runtime-execution。
  - workflow 残留的主要 live 面已收缩到 valve/purge 一类残留逻辑，而非 DXF 执行主链。

## 10. 阻塞点 / 风险点
- workflow valve/purge residual 目前仍是 live 依赖面的一部分；如果再次从 CMake 中移除而未同步替换下游链接，已确认会导致 `siligen_unit_tests`、`siligen_planner_cli` 等目标链接失败。
- `siligen_application_motion` 与 `siligen_application_redundancy` 仍保留宽聚合依赖，后续继续收缩时仍可能暴露新的直接依赖缺口。
- `siligen_workflow_dispensing_planning_compat` 仍在 source CMake 中存在，后续若继续删 bridge/compat，需避免与当前 residual/测试 wiring 互相影响。
- build 目录中的已生成工程文件可能保留旧 target 线索；判断 live state 时应以 source CMake 与重新生成后的构建结果为准，不能只看 build 目录现状。

## 11. 待确认项
- workflow valve/purge residual 最终应该长期保留为一个窄 residual target，还是继续迁移到其他 owner，目前未在本窗口内定案。
- 下一步优先继续拆 `siligen_application_motion`，还是转向 `siligen_application_redundancy`，本窗口未重新征询用户。
- `siligen_workflow_dispensing_planning_compat` 是否已进入可删除窗口，本窗口未做新的事实核验。
- `runtime-service` 及其他更大范围下游目标是否还存在未暴露的 residual 依赖，本窗口未做全量验证。

## 12. 建议新窗口立刻执行的下一步
1. 先读本交接文档，再读以下两个文件的当前 live 内容：
   - `modules/workflow/application/CMakeLists.txt`
   - `modules/workflow/domain/domain/CMakeLists.txt`
2. 然后直接从 `siligen_application_motion` 开始，按与 `siligen_application_dispensing` / `siligen_application_system` 相同的方法：
   - 先读真实源码依赖；
   - 再收缩宽聚合链接；
   - 再做最小构建/测试验证。
3. 在继续删减 workflow residual 前，保留 `siligen_dispensing_execution_services`，除非新的 owner 替代链路已经完成并验证通过。

## 13. 给新窗口的执行约束
- 只基于 live 代码、live CMake、live 构建结果、live 测试结果推进。
- 不要把推断写成事实。
- 不要回滚 intentional changes，尤其不要回滚 intentional CMake changes。
- 延续用户已明确接受的工作风格：直接推进，不做无必要来回确认。
- 对总结/汇报类回复保持简短；如用户再次要求纯文本摘要，避免附带工具细节。
- 继续使用中文。

## 14. 事实 / 推断 / 未知 三分表
| 类型 | 内容 |
|---|---|
| 事实 | `siligen_application_dispensing` 已从 `siligen_domain` / `siligen_domain_services` 直接解耦，并通过构建与 `siligen_dispensing_semantics_tests` 验证。 |
| 事实 | `siligen_application_system` 已从 `siligen_domain` / `siligen_domain_services` 直接解耦，并通过目标级构建验证。 |
| 事实 | `siligen_dispensing_execution_services` 已在 `modules/workflow/domain/domain/CMakeLists.txt` 中恢复，并包含 `PurgeDispenserProcess.cpp`、`SupplyStabilizationPolicy.cpp`、`ValveCoordinationService.cpp`。 |
| 事实 | 恢复该 target 后，`siligen_unit_tests`、`siligen_planner_cli` 构建通过；focused gtests 通过。 |
| 事实 | `siligen_application_motion`、`siligen_application_redundancy` 在本窗口读取到的 CMake 内容中仍直接依赖 `siligen_domain` / `siligen_domain_services`。 |
| 推断 | 如果继续沿当前路线推进，`siligen_application_motion` 是最自然的下一个收缩对象。 |
| 推断 | workflow valve/purge residual 在短期内仍需保留，直到新的 owner 链路完成替代并验证。 |
| 未知 | workflow valve/purge residual 的最终长期 owner 归属。 |
| 未知 | `siligen_workflow_dispensing_planning_compat` 何时可以安全删除。 |
| 未知 | 更大范围下游目标在继续收缩后还会暴露哪些新的直接依赖缺口。 |
