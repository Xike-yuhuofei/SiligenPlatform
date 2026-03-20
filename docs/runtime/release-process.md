# Release Process

更新时间：`2026-03-19`

## 1. 适用范围

本文档定义 `SiligenSuite` 根仓的正式版本治理与人工发布流程。

当前 release 检查覆盖：

- `apps/`
- `packages/`
- `integration/`
- `legacy-exit` 门禁
- 必要的兼容产物说明

不再覆盖：

- `apps/dxf-editor-app`
- `dxf-editor`
- `packages/editor-contracts`

## 2. 发布前检查

发布前必须在根仓执行：

```powershell
Set-Location D:\Projects\SiligenSuite
.\release-check.ps1 -Version <version>
```

`release-check.ps1` 会执行：

1. 检查根仓是否已有可打 tag 的提交
2. 检查 `CHANGELOG.md` 是否已有目标版本条目
3. 执行根级 `build.ps1 -Profile CI -Suite all`
4. 执行根级 `test.ps1 -Profile CI -Suite all -FailOnKnownFailure`
5. 生成 release evidence 到 `integration\reports\releases\<version>\`
6. 执行以下 app dry-run 并固化输出：
   - `apps\hmi-app\run.ps1 -DryRun -DisableGatewayAutostart`
   - `apps\control-tcp-server\run.ps1 -DryRun`
   - `apps\control-cli\run.ps1 -DryRun`
   - `apps\control-runtime\run.ps1 -DryRun`

## 3. DXF 编辑发布口径

- 内建 DXF 编辑器已删除，不再作为发布项。
- `notify / CLI` 编辑器协作协议已废弃，不再作为 release gate。
- 若交付说明涉及 DXF 编辑，统一引用 `docs/runtime/external-dxf-editing.md`。

## 4. 当前已知阻塞

1. 根仓当前 `No commits yet`，还没有可打 tag 的正式提交锚点。
2. `control-core` 仍承载真实 C++ 库图、`third_party`、shared compat include root 与未迁完的 infrastructure / device-hal 实现，因此还不能进入物理删除。
3. `hardware smoke` 已具备最小 canonical 启动闭环，但尚未升级为长稳或现场验收 gate。

## 5. 当前已完成的切换

- `control-runtime`、`control-tcp-server`、`control-cli` 的默认可执行产物都已切到 canonical control-apps build root。
- `config\machine\machine_config.ini` 与 `data\recipes\` 已是默认配置/数据来源。
- HIL 默认入口已经切到 `integration\hardware-in-loop\run_hardware_smoke.py` + canonical `siligen_tcp_server.exe`。
- legacy gateway/tcp alias 已删除；兼容测试现在只验证 canonical target 仍存在且 alias 不得回流。
