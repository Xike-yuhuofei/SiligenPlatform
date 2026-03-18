# DXF Editor Removal Report

更新时间：`2026-03-18`

## 1. 当前结论

1. `apps/dxf-editor-app` 已删除。
2. legacy `dxf-editor` 已删除。
3. `packages/editor-contracts` 已删除。
4. 旧 notify / CLI 编辑器协作协议已废弃。
5. DXF 编辑流程统一改为外部编辑器人工处理。

## 2. 替代路径

当前唯一推荐说明入口：

- `D:\Projects\SiligenSuite\docs\runtime\external-dxf-editing.md`

当前工作方式：

- 系统负责 DXF 文件的生成、导入、预览或后续处理
- 用户使用 AutoCAD 等外部编辑器手工编辑 DXF
- 编辑完成后回到系统执行人工导入、刷新或后续流程

## 3. 已完成项

- 删除 DXF editor app 与 legacy 目录
- 删除 editor contracts 包
- 移除根级安装、测试、release gate 对 editor 的默认依赖
- 更新根级 README、架构文档、onboarding、部署与排障文档
- 停止把 notify / CLI 协议写成当前受支持能力

## 4. 删除后门禁

以下检查期望 `0` 行：

```powershell
Get-ChildItem .\apps,.\packages,.\integration,.\tools,.\docs -Recurse -File |
  Where-Object { $_.FullName -notmatch '\\build\\|\\reports\\|\\tmp\\|\\__pycache__\\' } |
  Select-String -Pattern 'apps\\dxf-editor-app|apps/dxf-editor-app|dxf-editor\\|dxf-editor/|packages\\editor-contracts|packages/editor-contracts|dxf_editor'
```
