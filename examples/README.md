# examples

根级 `examples/` 现在用于工作区共享样例。

当前约定：

- `examples/dxf/`：DXF 输入样例
- `examples/recipes/`：导入/导出 bundle、模板包等演示输入
- `examples/simulation/`：仿真输入样例
- `examples/replay-data/`：演示性质的派生输出或回放片段
- `examples/task-plans/`：保留给后续任务计划样例

边界说明：

- `examples/` 中可以存放演示性的生成结果，但必须明确标注为“派生样例”，不能冒充源码资产
- `tests/fixtures/imported/uploads-*`、`tmp/`、`build/` 不自动视为样例
