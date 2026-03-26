# scenarios

这里放跨 package 的业务场景回归。

当前已接入：

- `run_engineering_regression.py`

它验证：

- `engineering-data` 从 DXF 导出 `.pb`
- `.pb` 导出 canonical `simulation-input.json`
- 导出结果与 `engineering-contracts` fixture 一致
