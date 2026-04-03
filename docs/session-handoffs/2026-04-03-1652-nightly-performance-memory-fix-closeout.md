# DXF Long-Run Memory Fix Closeout

## 1. 任务结论

- 本轮围绕 `tests/performance/collect_dxf_preview_profiles.py` 驱动的 DXF preview / execution performance gate 收敛 `nightly-performance` 中的 `long_run` 资源增长异常。
- 当前正式证据以 `tests/reports/performance/dxf-preview-profiles/20260403T083352Z/` 为准。
- 当前正式 `Threshold Gate` 结果为 `passed`。
- 本阶段到此为止，不继续追 `large.long_run` 的进一步压降，只保留为后续候选优化项。

## 2. 已固化的实现结果

- `long_run` 采集逻辑已切换为：
  - 单次 `plan.prepare -> preview.snapshot` 建立 `preview_setup`
  - 循环内仅执行 `preview.confirm -> dxf.job.start -> dxf.job.status -> dxf.job.stop`
- `long_run` 结果已显式输出 `preview_setup`。
- `DispensingExecutionUseCase.Async.cpp` 已在调度器提交前调用 `task_scheduler_port_->CleanupExpiredTasks()`。
- `collect_dxf_preview_profiles.py` 中 `control_cycle.summary.count` 与 `long_run.summary.count` 现在固定表示轮次/迭代数。
- `collect_dxf_preview_profiles.py` 中新增 `control_cycle.summary.sample_count` 与 `long_run.summary.sample_count`，用于表达资源采样数，避免覆盖顶层 `count` 语义。

## 3. 正式证据

- 正式报告目录：`tests/reports/performance/dxf-preview-profiles/20260403T083352Z/`
- 正式 Markdown 报告：`tests/reports/performance/dxf-preview-profiles/20260403T083352Z/report.md`
- 当前正式 `long_run` 结果：
  - `small.long_run.working_set_mb.delta_max = 11.461`
  - `small.long_run.private_memory_mb.delta_max = 10.047`
  - `medium.long_run.working_set_mb.delta_max = 19.855`
  - `medium.long_run.private_memory_mb.delta_max = 18.406`
  - `large.long_run.working_set_mb.delta_max = 66.429`
  - `large.long_run.private_memory_mb.delta_max = 65.832`

## 4. 本轮最小验证面

- `python -m py_compile .\tests\performance\collect_dxf_preview_profiles.py .\tests\contracts\test_performance_threshold_gate_contract.py`
- `python -m pytest .\tests\contracts\test_performance_threshold_gate_contract.py -q`
- `D:\Projects\ss-t006\build\bin\Debug\siligen_runtime_execution_unit_tests.exe --gtest_filter=DispensingExecutionUseCaseInternalTest.*`

## 5. 文档与基线状态

- 保留 `tests/baselines/performance/dxf-preview-profile-thresholds.json` 中的当前 evidence 指针更新。
- 保留 `tests/performance/README.md` 中关于 `long_run` 语义、正式 gate 结果和证据入口的同步说明。
- 旧 handoff 文档仅保留历史状态，不回写。

## 6. 未纳入本阶段的事项

- 未继续优化 `large.long_run` 的资源增长。
- 若后续需要继续压降，应基于当前正式证据继续分析，不从头重跑定位。
- 若后续需要额外联机或更大范围回归，应继续使用独立端口，避免与 `9527` 上的外部 gateway 冲突。
