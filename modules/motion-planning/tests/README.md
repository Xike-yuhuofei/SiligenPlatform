# tests

motion-planning 的模块级验证入口收敛到此目录。

- `siligen_motion_planning_unit_tests`：M7 owner closeout 的主证据，只覆盖规划 owner 与冻结 public surface。
- `siligen_motion_planning_architecture_tests`：跨模块 owner / compat / build truth 审计，不再与 unit gate 混在同一个 target。
- `siligen_motion_planning_integration_tests`：保留 facade 与主线场景的集成验证，不再伪装成纯单测。
- `python/test_trajectory_owner_lane.py`：离线 trajectory Python owner 的模块内验证，覆盖 owner 算法、`-m motion_planning.trajectory_generation --help` 与稳定 JSON 输出。
