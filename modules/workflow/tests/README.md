# tests

workflow 的模块级验证入口应收敛到此目录。

- `canonical` 是 workflow canonical 测试源码承载面，覆盖当前 owner graph 下的主体验证入口。
- `unit/` 不再允许承载 source-bearing 测试；历史 workflow `unit` 目录只保留空目录占位，避免 foreign owner 测试继续回流。
- `integration` 现作为 workflow 级集成烟测登记面，承载最小 assembly/path execution smoke。
- `regression` 现作为 workflow 级回归哨兵登记面，承载最小可重复执行的 smoke target。
- 历史 bridge 测试入口不再属于 canonical tests surface。
