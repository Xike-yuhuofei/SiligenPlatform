# tests

workflow 的模块级验证入口应收敛到此目录。

- `process-runtime-core` 历史测试已切换到 `tests/process-runtime-core/` 作为 canonical 承载面。
- `integration` 现作为 workflow 级集成烟测登记面，承载最小 assembly/path execution smoke。
- `regression` 现作为 workflow 级回归哨兵登记面，承载最小可重复执行的 smoke target。
- `process-runtime-core/tests/` 仅保留 bridge forwarder。

