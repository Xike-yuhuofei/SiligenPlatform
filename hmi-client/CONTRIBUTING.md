# 贡献指南

## 提交前检查

1. 先阅读 `PROJECT_BOUNDARY_RULES.md`、`docs/DEVELOPMENT_CONVENTIONS.md`、`docs/TESTING_CONVENTIONS.md`。
2. 确认 UI、ViewModel、client、integrations 的职责边界没有被打破。
3. 如涉及后端协议或 DXF editor 契约，先更新文档模板再改代码。
4. 运行最小验证：
   - `./scripts/run.ps1`
   - `./scripts/test.ps1`

## 开发流程

1. 先定义用户流程与状态变化。
2. 将协议调用封装到 `client` 或 `integrations`。
3. 将界面逻辑沉淀到 `viewmodels` 或 `workflows`。
4. 避免把复杂流程直接塞进窗口或面板类。

## 代码评审关注点

- 是否在 UI 层直接拼接协议或操作外部进程。
- 是否影响用户流程一致性。
- 是否对错误提示、状态更新做了统一处理。
- 是否补了 mock 集成测试或最小单元测试。
