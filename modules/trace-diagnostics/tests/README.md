# tests

trace-diagnostics 的模块级验证入口应收敛到此目录。

当前最小正式测试面：

- `contract/`
  - `ValidationEvidenceBundle` 的最小字段 contract
- `evidence/`
  - 最小 evidence bundle baseline 快照
- `observability/`
  - `CreateLoggingService` bootstrap 与可观测日志回归

当前未补：

- trace/query/archive owner 契约
- `apps/trace-viewer` 邻接集成回归
- 真实 report directory 输出与 schema 校验
