# Release Process

更新时间：`2026-03-23`

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

发布前默认门禁入口（本地）：

```powershell
Set-Location <repo-root>
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\scripts\run-local-validation-gate.ps1
```

门禁脚本会串行执行：

1. `python .\tools\migration\validate_workspace_layout.py`
2. `.\test.ps1 -Profile CI -Suite packages -FailOnKnownFailure`
3. `.\tools\build\build-validation.ps1 -Profile Local -Suite packages`
4. `.\tools\build\build-validation.ps1 -Profile CI -Suite packages`

并输出证据到 `integration\reports\local-validation-gate\<timestamp>\`。

在本地门禁通过后，再执行 release 检查：

发布前必须在根仓执行：

```powershell
Set-Location <repo-root>
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

说明：

- 仓库已弃用 GitHub Actions 作为默认门禁，不再要求云端 checks 才能给出发布结论。
- 验收以本地门禁实跑证据为准。

## 3. DXF 编辑发布口径

- 内建 DXF 编辑器已删除，不再作为发布项。
- `notify / CLI` 编辑器协作协议已废弃，不再作为 release gate。
- 若交付说明涉及 DXF 编辑，统一引用 `docs/runtime/external-dxf-editing.md`。

## 4. 当前已知阻塞

1. 仓内 release gate 只能证明 canonical 工作区当前通过，不能直接证明仓外迁移已完成。
2. 仓外已部署环境、发布包和现场脚本是否仍缓存 legacy DXF 入口，仍需按观察期清单单独确认。
3. `hardware smoke` 已具备最小 canonical 启动闭环，但尚未替代长稳或现场验收 gate。

## 5. 当前已完成的切换

- `control-runtime`、`control-tcp-server`、`control-cli` 的默认可执行产物都已切到 canonical control-apps build root。
- `config\machine\machine_config.ini` 与 `data\recipes\` 已是默认配置/数据来源。
- HIL 默认入口已经切到 `integration\hardware-in-loop\run_hardware_smoke.py` + canonical `siligen_tcp_server.exe`。
- legacy gateway/tcp alias 已删除；兼容测试现在只验证 canonical target 仍存在且 alias 不得回流。
- `dxf-pipeline` compat shell、legacy CLI alias 与 root config alias 已从仓内退出；若仓外交付物仍出现这些入口，应直接判为迁移阻断而不是恢复兼容壳。
