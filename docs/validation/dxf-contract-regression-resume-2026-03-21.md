# DXF 契约修复断点续跑记录 2026-03-21

更新时间：`2026-03-21`

## 1. 背景

- 续跑触发：网络中断后恢复会话。
- 续跑目标：确认“预览确认门禁 + plan_fingerprint 强校验 + mock/真实链路契约对齐”改动在工作区回归通过。
- 分支：`feat/hmi/NOISSUE-dispense-trajectory-preview-v2`

## 2. 本轮执行与结果

1. HMI 单元测试
- 命令：`powershell -ExecutionPolicy Bypass -File apps/hmi-app/scripts/test.ps1`
- 结果：`Ran 33 tests ... OK`

2. 协议兼容套件
- 命令：`powershell -ExecutionPolicy Bypass -File test.ps1 -Suite protocol-compatibility`
- 结果：`passed=1 failed=0`

3. apps 套件（第一次）
- 命令：`powershell -ExecutionPolicy Bypass -File test.ps1 -Suite apps`
- 结果：`passed=13 failed=2`
- 失败原因：canonical `siligen_cli.exe` 产物未刷新，导致 `dxf-preview` 命令面未暴露且 `dxf-plan` 走到旧配置校验路径。

4. 刷新 canonical control-apps 产物
- 命令：`powershell -ExecutionPolicy Bypass -File tools/build/build-validation.ps1 -Suite apps`
- 结果：命令超时中断（15 分钟），但 `C:\Users\Xike\AppData\Local\SiligenSuite\control-apps-build\bin\Debug\siligen_cli.exe` 已更新（`2026/3/21 23:27:47`），`--help` 已包含 `dxf-preview`。

5. apps 套件（第二次）
- 命令：`powershell -ExecutionPolicy Bypass -File test.ps1 -Suite apps`
- 结果：`passed=15 failed=0`

6. C++ 侧关键单测（本次改动相关）
- 构建命令：`cmake --build build-tests --config Debug --target process_runtime_core_tests`
- 运行命令：`build-tests/bin/Debug/siligen_unit_tests.exe --gtest_filter="DispensingWorkflowUseCaseTest.*:DispensingExecutionUseCaseProgressTest.*:JsonRedundancyRepositoryAdapterTest.*:MotionMonitoringUseCaseTest.*:RedundancyGovernanceUseCasesTest.*"`
- 结果：`26 tests from 5 test suites ... PASSED`

7. 合并验收（apps + protocol-compatibility）
- 命令：`powershell -ExecutionPolicy Bypass -Command "& ./test.ps1 -Suite @('apps','protocol-compatibility')"`
- 结果：`passed=16 failed=0 known_failure=0 skipped=0`
- 报告：
  - `integration/reports/workspace-validation.json`
  - `integration/reports/workspace-validation.md`

## 3. 结论

- 本次“契约修复”主范围（接口语义、输入输出一致性、错误门禁、链路一致性）在续跑后已通过自动化回归。
- 首次 `apps` 失败属于构建产物陈旧，不是源码契约回归；刷新 canonical 产物后已恢复全绿。
- 当前可进入下一阶段：现场多批次真实链路验收（`prepare -> snapshot -> confirm -> start`）与工艺侧签收。
