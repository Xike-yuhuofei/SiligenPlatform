# M2 实施进展记录（2026-03-21）

## 完成项

1. 已新增冗余治理核心类型与端口：
   - `domain/system/redundancy/RedundancyTypes.h`
   - `domain/system/redundancy/ports/IRedundancyRepositoryPort.h`
2. 已新增 JSON 持久化适配器并接入构建：
   - `infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapter.*`
   - `infrastructure/adapters/redundancy/CMakeLists.txt`
3. 已新增候选评分服务与治理用例：
   - `application/services/redundancy/CandidateScoringService.*`
   - `application/usecases/redundancy/RedundancyGovernanceUseCases.*`
4. 已完成 CMake 接线：
   - `packages/process-runtime-core/CMakeLists.txt`
   - `src/application/CMakeLists.txt`
5. 已新增并通过冗余治理专项单测：
   - `tests/unit/infrastructure/adapters/redundancy/JsonRedundancyRepositoryAdapterTest.cpp`
   - `tests/unit/redundancy/CandidateScoringServiceTest.cpp`
   - `tests/unit/redundancy/RedundancyGovernanceUseCasesTest.cpp`

## 验证记录

1. 编译验证：
   - `cmake --build build --target siligen_application_redundancy --config Debug --parallel 4`
   - `cmake --build build/control-apps-root --target siligen_unit_tests --config Debug --parallel 4`
2. 测试验证：
   - `build/control-apps-root/bin/Debug/siligen_unit_tests.exe --gtest_filter='CandidateScoringServiceTest.*:JsonRedundancyRepositoryAdapterTest.*:RedundancyGovernanceUseCasesTest.*'`
   - 结果：`5 passed / 0 failed`

## 当前边界

1. 当前实现仅在 `process-runtime-core` 包内可用，未对接 transport-gateway/HMI。
2. 持久化后端为 JSON 文件目录：`packages/process-runtime-core/tmp/redundancy-governance`（可在适配器构造时覆盖）。
3. 状态迁移规则已在用例层执行，且迁移与决策日志写入保持绑定。
