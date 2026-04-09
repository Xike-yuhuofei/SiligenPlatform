# application

motion-planning 的 facade、handler、query/command 入口应收敛到此目录。

- `services/motion_planning/` 继续承载当前冻结的 C++ facade public surface。
- `motion_planning/trajectory_generation.py` 是离线 trajectory Python owner，实现由 `scripts/engineering-data/path_to_trajectory.py` 调用。
- 该 Python owner 只负责轨迹生成语义，不定义新的 shared schema，也不替代 `motion_planning/contracts/*` 的 C++ authority。
