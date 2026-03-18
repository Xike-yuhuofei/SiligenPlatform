# Validation

工作区级验证入口已经统一到根级脚本：

- [build.ps1](/D:/Projects/SiligenSuite/build.ps1)
- [test.ps1](/D:/Projects/SiligenSuite/test.ps1)
- [ci.ps1](/D:/Projects/SiligenSuite/ci.ps1)
- [legacy-exit-check.ps1](/D:/Projects/SiligenSuite/legacy-exit-check.ps1)

## 分层位置

- legacy 删除 / 冻结门禁：`.\legacy-exit-check.ps1`
- control apps 可执行产物：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_control_runtime.exe`、`siligen_tcp_server.exe`、`siligen_cli.exe`
- `process-runtime-core` 单测产物：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_unit_tests.exe`、`siligen_pr1_tests.exe`
  其中测试源码 canonical 位置已经收口到 `packages/process-runtime-core/tests/`，`control-core/tests/CMakeLists.txt` 当前仅保留兼容转发入口，产物已经随 canonical control-apps build root 输出
- 仿真单测：`packages/simulation-engine/build/Debug/*.exe`
- 集成：`integration/protocol-compatibility/`、`integration/scenarios/`
- 仿真/HIL：`integration/simulated-line/`、`integration/hardware-in-loop/`

`CONTROL_APPS_BUILD_ROOT` 解析顺序：

- `SILIGEN_CONTROL_APPS_BUILD_ROOT`
- `%LOCALAPPDATA%\SiligenSuite\control-apps-build`
- `D:\Projects\SiligenSuite\build\control-apps`

## 报告

根级验证报告输出到 `integration/reports/`。

- `legacy-exit-check.ps1` 输出到 `integration/reports/legacy-exit/`
- `ci.ps1` 会在执行 build/test 前先生成 `integration/reports/ci/legacy-exit/`
