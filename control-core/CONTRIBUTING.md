# 贡献指南

## 提交前检查

1. 先阅读 `PROJECT_BOUNDARY_RULES.md`、`docs/DEVELOPMENT_CONVENTIONS.md`、`docs/TESTING_CONVENTIONS.md`。
2. 确认变更落在正确层级：`shared` / `domain` / `application` / `infrastructure` / `adapters`。
3. 如涉及协议、错误码、状态字段或配置项，先补文档再补实现。
4. 运行最小验证：
   - `./scripts/build.ps1`
   - `./scripts/test.ps1`

## 开发流程

1. 先明确问题归属与影响范围。
2. 优先从小改动开始，避免跨层大范围耦合修改。
3. 涉及跨项目协作时，只能通过公开契约推进。
4. 提交说明必须能回答“改了什么、为什么改、如何验证”。

## 代码评审关注点

- 是否破坏了层间边界。
- 是否把硬件/运行时细节泄漏到领域层或应用层。
- 是否补齐了最小测试与文档。
- 是否影响 HMI、DXF pipeline 等外部契约。
