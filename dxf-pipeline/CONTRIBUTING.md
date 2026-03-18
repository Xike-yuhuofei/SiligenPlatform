# 贡献指南

## 提交前检查

1. 先阅读 `PROJECT_BOUNDARY_RULES.md`、`docs/DEVELOPMENT_CONVENTIONS.md`、`docs/TESTING_CONVENTIONS.md`。
2. 确认变更属于 `geometry`、`path`、`trajectory`、`contracts` 或 `adapters` 的正确层级。
3. 如涉及 proto/json 输出结构，先更新契约文档与兼容策略，再改实现。
4. 运行最小验证：
   - `./scripts/run.ps1`
   - `./scripts/test.ps1`

## 开发流程

1. 先判断问题是几何、路径、轨迹还是契约问题。
2. 算法逻辑与 CLI 壳层保持分离。
3. 任何输出格式变更都要补 fixtures 与回归测试。
4. 性能优化变更建议更新 benchmark 记录。

## 代码评审关注点

- 是否破坏契约兼容性。
- 是否将实时控制逻辑错误地下沉到 pipeline。
- 是否引入难以维护的隐式输出格式。
- 是否补了 proto 兼容与样例回归测试。
