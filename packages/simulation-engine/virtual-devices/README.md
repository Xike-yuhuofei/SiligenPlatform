# virtual-devices

这里承接虚拟硬件。

职责：

- 虚拟轴组
- 虚拟编码器与反馈
- home / soft-limit / limit 状态
- 虚拟 IO 与触发反馈

规则：

- 只实现设备能力，不吸入业务状态机
- 只依赖 contracts，不依赖真实 device-adapters

## 可选运动真实性增强

当前已实现的可选增强包：

- 跟随误差近似
- 编码器量化误差

模型目的：

- 让 `VirtualAxisGroup` 在保持 deterministic tick 驱动的前提下，提供“理想命令位置”和“反馈读数”之间的可观测差异
- 让 runtime bridge、recording、summary 能看到更接近真实控制系统的轴反馈行为

近似方法：

- 先按原有确定性路径执行得到理想命令位置 `command_position_mm`
- 再对反馈位置应用一阶滞后近似，滞后强度由 `motion_realism.servo_lag_seconds` 控制
- 最后对反馈位置做固定步长量化，量化步长由 `motion_realism.encoder_quantization_mm` 控制
- `following_error_mm` 定义为 `command_position_mm - position_mm`
- `following_error_active` 依据显式阈值或自动阈值判定，并进入 `trace` / `timeline`

明确不模拟：

- 完整伺服控制器参数、PID 回路、刚体/柔性动力学
- 背隙、摩擦、软限位边缘、报警恢复脚本
- 非确定性线程调度或 wall-clock 采样抖动

如何验证：

- 包级：
  `simulation_engine_scheme_c_virtual_devices_test`
  `simulation_engine_scheme_c_runtime_bridge_shim_test`
  `simulation_engine_scheme_c_application_runner_test`
- 集成级：
  `python D:\Projects\SiligenSuite\integration\simulated-line\run_simulated_line.py`
  其中 `following_error_quantized.simulation-input.json` 专门覆盖该增强包
