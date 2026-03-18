# Release Process

更新时间：`2026-03-18`

## 1. 适用范围

本文档定义 `SiligenSuite` 根仓的正式版本治理与人工发布流程。

当前 release 检查覆盖：

- `apps/`
- `packages/`
- `integration/`
- 必要的 legacy 兼容产物

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
   - `apps\hmi-app\run.ps1 -DryRun`
   - `apps\control-tcp-server\run.ps1 -DryRun`
   - `apps\control-cli\run.ps1 -DryRun`
   - `apps\control-runtime\run.ps1 -DryRun`

## 3. DXF 编辑发布口径

- 内建 DXF 编辑器已删除，不再作为发布项。
- `notify / CLI` 编辑器协作协议已废弃，不再作为 release gate。
- 若交付说明涉及 DXF 编辑，统一引用 `docs/runtime/external-dxf-editing.md`。

## 4. 当前已知阻塞

1. 根仓当前 `No commits yet`，还没有可打 tag 的正式提交锚点
2. `apps\control-runtime` 仍无独立可执行文件，dry-run 当前返回 `BLOCKED`
3. `control-tcp-server` 与 `control-cli` 仍依赖 `control-core\build\bin\**` 兼容产物
4. 现场部署仍依赖手工拷贝 `control-core` 产物与 `dxf-pipeline` sibling 目录
5. `hardware smoke` 当前不能作为可信 release gate
