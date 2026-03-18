# hardware-in-loop

这里统一放硬件冒烟和机台联调入口。

当前入口：

- `run_hardware_smoke.py`

当前策略：

- 默认验证 canonical `siligen_tcp_server.exe` 的最小启动闭环
- 默认产物根按 `SILIGEN_CONTROL_APPS_BUILD_ROOT` -> `%LOCALAPPDATA%\SiligenSuite\control-apps-build` -> `D:\Projects\SiligenSuite\build\control-apps` 解析
- 默认目标路径模式：`<CONTROL_APPS_BUILD_ROOT>\bin\<Config>\siligen_tcp_server.exe`
- 可通过 `SILIGEN_HIL_GATEWAY_EXE` 显式覆盖可执行文件
- 进程 `cwd` 使用仓库根，而不是 `control-core`
- 若启动失败并命中当前已知模式，会归类为 `known_failure`
- 若未来接入真实机台，可通过环境变量替换目标可执行程序

推荐命令：

```powershell
python .\integration\hardware-in-loop\run_hardware_smoke.py
```
