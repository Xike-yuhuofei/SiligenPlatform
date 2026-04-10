# External DXF Editing

更新时间：`2026-03-18`

## 1. 当前结论

工作区已不再提供内建 DXF 编辑器。

`apps/dxf-editor-app`、legacy `dxf-editor`、`packages/editor-contracts` 以及旧 notify / CLI 编辑器协作协议均已退出工作区默认链路。

## 2. 推荐工作方式

1. 在系统中生成或导出需要处理的 DXF 文件。
2. 使用 AutoCAD 或其他外部 DXF 编辑器手工打开并编辑该文件。
3. 编辑完成后，回到工作区或业务系统执行人工导入、刷新、预览或后续处理。

## 3. 当前不再支持的能力

- 系统自动拉起内建 DXF 编辑器
- 编辑器 `notify` 文件回传
- 编辑器 CLI 参数协作
- 编辑完成后的自动保存通知或自动刷新
- 只读锁、保存状态、错误码等编辑器进程级语义

## 4. 文档口径

默认引用以下路径：

- 运行与部署：`docs/runtime/deployment.md`
- 新人接手：`docs/onboarding/workspace-onboarding.md`
- 日常开发：`docs/onboarding/developer-workflow.md`
- 排障：`docs/troubleshooting/canonical-runbook.md`

禁止再把以下路径当成默认入口：

- `apps/dxf-editor-app`
- `dxf-editor`
- `packages/editor-contracts`
