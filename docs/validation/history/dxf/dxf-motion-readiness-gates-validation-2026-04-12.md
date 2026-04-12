# DXF Motion Readiness Gates Validation 2026-04-12

更新时间：`2026-04-13`

## 1. 范围

- 分支：`fix/runtime/NOISSUE-motion-readiness-gates`
- 目标：收敛 DXF 停止/取消过渡态、运动稳定门禁，以及 HMI 对后端过渡结果的透传语义。
- 本轮覆盖：
  - `dxf.job.stop` / `dxf.job.cancel` 返回结构化 transition result
  - DXF 任务停止后等待运动真正稳定，再提交最终状态
  - `move` / `home` / `go home` 在运动未稳定或任务仍处于活跃过渡态时阻断
  - 控制卡 mock/runtime 状态映射与 `MC_StopEx` 行为对齐

## 2. 代码边界

- `apps/hmi-app`
- `apps/runtime-gateway/transport-gateway`
- `apps/runtime-service/container`
- `modules/runtime-execution`

## 3. 执行验证

1. HMI 单元测试
- 命令：
  - `python -m pytest .\apps\hmi-app\tests\unit\test_main_window.py -q`
  - `python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q`
- 结果：
  - `79 passed`
  - `45 passed`

2. C++ 目标重新生成
- 命令：`cmake -S . -B .\build`
- 结果：通过

3. C++ 定向构建
- 命令：
  - `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe .\build\tests\runtime-execution-unit\siligen_runtime_execution_unit_tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nologo`
  - `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe .\build\rtx-host-unit\siligen_runtime_host_unit_tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nologo`
  - `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe .\build\apps\runtime-gateway\transport-gateway\siligen_transport_gateway.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nologo`
- 结果：通过

4. C++ 定向测试
- 命令：
  - `.\build\bin\Debug\siligen_runtime_execution_unit_tests.exe --gtest_filter=EnsureAxesReadyZeroUseCaseTest.*:DispensingExecutionUseCaseInternalTest.*`
  - `.\build\bin\Debug\siligen_runtime_host_unit_tests.exe --gtest_filter=MockMultiCardCharacterizationTest.*:MultiCardMotionAdapterTest.*:DispensingExecutionUseCaseInternalTest.*`
  - `.\build\bin\Debug\runtime_execution_motion_runtime_assembly_test.exe`
  - `.\build\bin\Debug\siligen_runtime_service_unit_tests.exe`
- 结果：
  - `33 tests from 2 test suites ... PASSED`
  - `24 tests from 3 test suites ... PASSED`
  - 退出码 `0`
  - `43 tests from 8 test suites ... PASSED`

## 3.1 补充说明

- `MotionReadinessService` 的直测当前收敛在 `EnsureAxesReadyZeroUseCaseTest.cpp` 中，不存在单独的 `MotionReadinessServiceTest.*` suite。
- 本轮收尾已按真实 test filter 重新验证，避免把“未被构建系统接纳的独立 suite”误记为已执行。

## 3.2 2026-04-13 收尾补充验证

1. transport-gateway 协议契约校验
- 命令：`python -m pytest .\apps\runtime-gateway\transport-gateway\tests\test_transport_gateway_compatibility.py -q`
- 结果：`22 passed`
- 备注：补齐 `shared/contracts/application/commands/dxf.command-set.json` 中缺失的 `dxf.job.cancel` authority 项，并同步 stop/cancel 的 structured transition result 字段。

2. HMI DXF 过渡结果契约回归
- 命令：`python -m pytest .\apps\hmi-app\tests\unit\test_protocol_preview_gate_contract.py -q`
- 结果：`45 passed`

3. runtime-execution 定向重建
- 命令：`C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe .\build\tests\runtime-execution-unit\siligen_runtime_execution_unit_tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nologo`
- 结果：通过

4. runtime-execution 定向单测
- 命令：`.\build\bin\Debug\siligen_runtime_execution_unit_tests.exe --gtest_filter=EnsureAxesReadyZeroUseCaseTest.*:DispensingExecutionUseCaseInternalTest.*`
- 结果：`34 tests from 2 test suites ... PASSED`

5. transport-gateway 临时构建验证
- 命令：
  - `cmake -S . -B %TEMP%\siligen-closeout-transport-build`
  - `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe %TEMP%\siligen-closeout-transport-build\apps\runtime-gateway\transport-gateway\siligen_transport_gateway.vcxproj /p:Configuration=Debug /p:Platform=x64 /m:1 /nologo`
- 结果：通过
- 备注：当前工作树自带的 `.\build` 目录在构建 `third_party/protobuf/libprotoc` 时持续出现 `CL.read.1.tlog` 文件占用；该问题属于构建树状态污染，不是本批次源码编译错误，因此改用全新临时构建目录获取 `transport-gateway` 编译证据。

## 4. 结果结论

- HMI 已能识别 `dxf.job.stop` / `dxf.job.cancel` 的结构化过渡结果。
- `runtime-gateway` 已在 `move/home/go home` 入口前统一执行 motion readiness 评估。
- `runtime-execution` 已在 DXF 停止后的最终状态提交前等待运动稳定。
- mock/runtime 适配层已补齐 `MC_StopEx` 与轴状态映射语义，避免“看起来已 homed、实际上仍在 running”的误判。

## 5. 已知限制

- 根脚本 `build.ps1` 当前仍会因 `scripts/build/build-validation.ps1` 不接受 `-EnableCppCoverage` 而失败；这不是本批次代码差异直接引入的问题，本轮以定向构建与定向测试作为收尾验证证据。
- 当前工作树自带 `.\build` 目录在 `libprotoc` 阶段存在 tlog 锁竞争；如需复现 `transport-gateway` 编译，请优先使用新的构建目录或先清理该构建树状态。
