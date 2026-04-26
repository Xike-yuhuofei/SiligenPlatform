# Self-hosted Build Runner Runbook

更新时间：`2026-04-25`

本文档定义 `Strict Native Gate` 使用的 self-hosted Windows build runner 运维契约。该 runner 是 PR 合并门禁的一部分，不是本地开发环境。

## 1. Runner 身份

- GitHub runner name：`XIKE-build`
- GitHub repository：`Xike-yuhuofei/SiligenPlatform`
- Windows Service：`actions.runner.Xike-yuhuofei-SiligenPlatform.XIKE-build`
- active runner root：`D:\r`
- active workspace：`D:\r\_work\SiligenPlatform\SiligenPlatform`
- legacy fallback root：`D:\GitHubRunners\siligen-build`

旧目录只允许短期回退留存，不得同时作为生产 runner 运行。

## 2. Required Labels

runner 必须同时具备：

- `self-hosted`
- `Windows`
- `X64`
- `build`

`Strict Native Gate` 只允许调度到该 build runner lane。HIL runner 必须使用独立 `hil` label，不得与 build runner 合并职责。

## 3. Service Account Contract

Windows Service 运行账号必须满足：

1. 可访问 GitHub。
2. 可执行 `git fetch` / `git ls-remote`。
3. 可读写 `D:\r`。
4. 可执行 PowerShell 7、Python、CMake、CTest、Visual Studio Build Tools / MSBuild。
5. 不依赖交互式桌面会话。

如果 `NT AUTHORITY\NETWORK SERVICE` 无法稳定执行 Git checkout，必须切换为专用本机构建账号。不得改回前台 `run.cmd` 作为长期方案。

## 4. Baseline Checks

以 runner service 的实际运行身份验证以下命令：

```powershell
git --version
git ls-remote https://github.com/Xike-yuhuofei/SiligenPlatform HEAD
pwsh -NoProfile -Command '$PSVersionTable.PSVersion'
python --version
cmake --version
ctest --version
& "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" `
  -latest `
  -products * `
  -requires Microsoft.Component.MSBuild `
  -property installationPath
```

验证失败时，`Strict Native Gate` 必须保持失败或 pending，不允许静默降级。

## 5. Path Budget

active workspace root 长度不得超过 45 个字符：

```text
D:\r\_work\SiligenPlatform\SiligenPlatform
```

该约束用于避免 Visual Studio / MSBuild 生成 `.tlog` 文件时触发 260 字符路径限制。不得通过重命名 GitHub repository 规避本地 runner 路径问题。

## 6. Failure Semantics

以下情况均为阻断：

- runner offline。
- runner busy 且超过 SLA。
- checkout 失败。
- GitHub DNS / TLS / Git fetch 失败。
- native build/test 失败。
- `tests\reports\github-actions\strict-native-gate` 缺失。
- artifact 上传失败。

错误可以在报告中区分为环境故障或代码故障，但 required check 层面均不得通过。
