# control-runtime

## 负责什么

- 作为根级 canonical 运行时宿主进程入口
- 承接 `packages/runtime-host` 的真实 bootstrap 与生命周期持有
- 对外暴露独立的 `siligen_control_runtime.exe`

## 真实入口

- 源码入口：`apps/control-runtime/main.cpp`
- 构建入口：`apps/control-runtime/CMakeLists.txt`
- 统一启动入口：`apps/control-runtime/run.ps1`
- 根级 build 入口：`cmake -S control-core -B <CONTROL_APPS_BUILD_ROOT>`，由 `control-core/apps/CMakeLists.txt` 注册本目录

## 真实产物

- 目标名：`siligen_control_runtime`
- 构建根解析顺序：`SILIGEN_CONTROL_APPS_BUILD_ROOT` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build` -> `D:\Projects\SiligenSuite\build\control-apps`
- 真实 exe 路径模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_control_runtime.exe`
- 当前默认路径示例：`%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\Debug\siligen_control_runtime.exe`

## 与 legacy 的关系

- `control-core/apps/control-runtime` 兼容壳目录已删除；根级 `apps/control-runtime` 是当前唯一 app 入口。
- `control-core/build/bin/**/siligen_control_runtime.exe` 不再被默认启动链路使用。
- `run.ps1` 已移除 legacy fallback；若传入 `-UseLegacyFallback` 会直接报错，要求先修复 canonical 产物。
- `run.ps1 -DryRun` 在 canonical 产物缺失时会以非零退出，作为真实可执行物验证的一部分。
