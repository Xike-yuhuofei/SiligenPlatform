# control-cli

## 负责什么

- 作为根级 canonical CLI 入口
- 承接 bootstrap 检查与 recipe 子命令的真实源码承载面
- 为剩余未迁移 CLI 能力提供明确阻塞提示，而不是继续假装 wrapper 已完成 canonical 化

## 真实入口

- 源码入口：`apps/control-cli/main.cpp`
- 构建入口：`apps/control-cli/CMakeLists.txt`
- 统一启动入口：`apps/control-cli/run.ps1`
- 根级 build 入口：`cmake -S control-core -B <CONTROL_APPS_BUILD_ROOT>`，由 `control-core/apps/CMakeLists.txt` 注册本目录

## 真实产物

- 目标名：`siligen_cli`
- 构建根解析顺序：`SILIGEN_CONTROL_APPS_BUILD_ROOT` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build` -> `D:\Projects\SiligenSuite\build\control-apps`
- 真实 exe 路径模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_cli.exe`
- 当前默认路径示例：`%LOCALAPPDATA%\SiligenSuite\control-apps-build\bin\Debug\siligen_cli.exe`

## 当前承载面

- `bootstrap-check`
- `recipe create`
- `recipe list`
- `recipe get`
- `recipe versions`
- `recipe audit`
- `recipe export`
- `recipe import`

## 剩余阻塞

- 运动、点胶、DXF、连接调试命令尚未迁入 canonical CLI。
- 旧仓 `D:\Projects\Backend_CPP\src\adapters\cli` 仍是这些未迁移命令的可信迁移源。
- 如确需调用 legacy CLI，必须显式执行 `run.ps1 -UseLegacyFallback`，该回退仅用于过渡期审计。
- 对未迁移命令，canonical `siligen_cli.exe` 会直接返回阻塞提示，不再继续伪装 wrapper 已经完成。
