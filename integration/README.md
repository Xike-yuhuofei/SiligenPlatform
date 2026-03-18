# integration

根级 `integration/` 现在承接工作区级跨包验证，不再只是分类占位。

## 目录职责

- `protocol-compatibility/`：契约兼容性入口
- `scenarios/`：工程数据到仿真输入的场景回归
- `simulated-line/`：packages/simulation-engine 仿真回归
- `hardware-in-loop/`：硬件冒烟入口
- `regression-baselines/`：回归基线
- `reports/`：根级验证报告输出

## 根级入口

```powershell
D:\Projects\SiligenSuite\test.ps1
```

执行后会生成：

- `D:\Projects\SiligenSuite\integration\reports\workspace-validation.json`
- `D:\Projects\SiligenSuite\integration\reports\workspace-validation.md`
