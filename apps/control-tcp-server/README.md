# control-tcp-server

## 负责什么

- 作为根级 canonical TCP/JSON 服务进程入口
- 承接 `packages/transport-gateway` + `packages/runtime-host` 的真实装配
- 对外暴露独立的 `siligen_tcp_server.exe`

## 真实入口

- 源码入口：`apps/control-tcp-server/main.cpp`
- 构建入口：`apps/control-tcp-server/CMakeLists.txt`
- 统一启动入口：`apps/control-tcp-server/run.ps1`
- 根级 build 入口：`cmake -S control-core -B <CONTROL_APPS_BUILD_ROOT>`，由 `control-core/apps/CMakeLists.txt` 注册本目录

## 真实产物

- 目标名：`siligen_tcp_server`
- 构建根解析顺序：`SILIGEN_CONTROL_APPS_BUILD_ROOT` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build` -> `D:\Projects\SiligenSuite\build\control-apps`
- 真实 exe 路径模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe`
- 当前默认路径示例：`%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\Debug\siligen_tcp_server.exe`

## 与 legacy 的关系

- `control-core/apps/control-tcp-server` 不再是默认源码 owner。
- `control-core/build/bin/**/siligen_tcp_server.exe` 不再是默认启动目标。
- `run.ps1 -UseLegacyFallback` 仍保留临时、显式、可审计的回退通道。
- `integration/hardware-in-loop/run_hardware_smoke.py` 默认也已切到 canonical TCP server，而不是 `control-core/build/bin/**`。
