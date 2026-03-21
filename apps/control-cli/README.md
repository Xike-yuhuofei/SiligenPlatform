# control-cli

## 负责什么

- 作为根级 canonical CLI 唯一默认入口
- 承接连接调试、运动、点胶、DXF、recipe 的真实命令实现
- 对外暴露独立的 `siligen_cli.exe`

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

## 当前 canonical 命令面

- `bootstrap-check`
- `connect`
- `disconnect`
- `status`
- `home`
- `jog`
- `move`
- `stop-all`
- `estop`
- `dispenser start|purge|stop|pause|resume`
- `supply open|close`
- `dxf-plan`
- `dxf-dispense`
- `dxf-preview`
- `dxf-augment`
- `recipe create|update|draft|draft-update|publish|list|get|versions|archive|version-create|compare|rollback|activate|audit|export|import`

## 默认行为

- `run.ps1` 默认且只解析 canonical `siligen_cli.exe`
- `run.ps1 -DryRun` 找不到 canonical 产物时会非零退出
- 不再支持 `-UseLegacyFallback`
- `control-core/build/bin/**/siligen_cli.exe` 不再是默认或显式入口

## 与 legacy 的关系

- `control-core` 不再承载新的 CLI 命令实现
- 若短期需要保留旧名称，只允许在 canonical 路径上做 alias；真实实现必须继续留在 `apps/control-cli`
- 当前 CLI cutover 不再依赖外部 `Backend_CPP\src\adapters\cli`
- `dxf-augment` 已位于 canonical CLI，但当前本地 build 若关闭 `SILIGEN_ENABLE_CGAL`，会返回 `NOT_IMPLEMENTED`
- `--preview-max-points` 已迁移到 `dxf-preview` 命令；在 `dxf-plan/dxf-dispense/dxf-augment` 中仅告警且不生效
- `dxf-preview` 通过参数化进程调用 `engineering-data` 预览脚本，不再通过 shell 拼接命令执行
- 预览 Python 解释器环境变量优先级：`SILIGEN_ENGINEERING_DATA_PYTHON` > `SILIGEN_DXF_PREVIEW_PYTHON`（后者保留兼容但已弃用）
- DXF 真机点胶主链的 CMP 运行时配置入口是 `config/machine/machine_config.ini` 的 `[ValveDispenser]` 段；CLI 不再暴露 `--cmp-channel` 这类会造成主链配置错觉的残留参数
- CLI 已移除一组“可解析但无消费点”的死参数：`--verbose`、`--disable-safety-checks`、`--no-auto-enable`、`--show-status`、`--dispensing-time`、`--pulse-width`、`--segments`
