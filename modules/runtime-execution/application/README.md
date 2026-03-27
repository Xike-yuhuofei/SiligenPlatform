# application

runtime-execution 的 facade、handler、query/command 入口应收敛到此目录。

当前规则：

- app / host consumer 统一从 `application/include/runtime_execution/application/**` 引入 M9 执行链 public headers。
- `usecases/dispensing/DispensingExecutionUseCase*.cpp` 的真实编译 owner 位于 `modules/runtime-execution/application/`，不得再由 `workflow/application` 回编。
- 上传归 M1、规划与编排归 M0；这些 owner 头必须分别从 `job_ingest/application/**`、`workflow/application/**` 引入。
- runtime-workflow compat 只允许保留 `domain` 级 residual bridge，禁止恢复 broad runtime compat target。
