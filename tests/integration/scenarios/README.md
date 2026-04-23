# integration scenarios

这里承载 `L3` 集成测试场景和共享样本复放。

当前正式入口：

- `run_engineering_regression.py`
- `run_preview_flow_regression.py`
- `first-layer/run_tcp_precondition_matrix.py`
- `apps/hmi-app/scripts/online-smoke.ps1`

这里的目标是离线固定主链，不是替代 `L4` simulated-line 或 `L5` HIL。

这些 runner 现在都必须产出结构化证据：

- `*-summary.json`
- `*-summary.md`
- `case-index.json`
- `validation-evidence-bundle.json`
- `evidence-links.md`

其中：

- `run_engineering_regression.py` 固化 `DXF -> PB -> simulation-input -> preview` 工程数据链。
- `run_preview_flow_regression.py` 固化 `DXF 导入 -> preview snapshot -> HMI 状态推进`。
- `first-layer/run_tcp_precondition_matrix.py` 固化 `preview confirm / execution preflight` 阻断矩阵；`dxf.plan.prepare` 固定参数 owner 已正式收敛为 runtime 返回的 `production_baseline`，不再接受 `--recipe-id/--version-id`。
