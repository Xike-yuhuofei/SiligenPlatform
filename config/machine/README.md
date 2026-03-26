# machine

owner: `modules/runtime-execution/runtime/host` + `apps/runtime-service` / `apps/runtime-gateway` 消费侧

canonical：

- `config/machine/machine_config.ini`

注意：

- 当前 INI 仍含工作站相关绝对路径，后续应继续模板化。
- 构建期会把 canonical 配置复制到 `build/config/machine/machine_config.ini`。
- 历史机器配置副本已退出默认解析链路，不再作为入口。

真实硬件 bring-up 关键配置面：

- `[Hardware] mode`：当前为 `Real`，运行 `runtime-service` / `runtime-gateway` 前会触发 MultiCard vendor 资产预检查。
- `[Network] control_card_ip`、`local_ip`、`timeout_ms`：现场必须与工控机网卡和板卡网络实际配置一致。
- `[Machine] pulse_per_mm` 与 `[Hardware] pulse_per_mm`：必须保持一致，避免脉冲当量失配。
- `[Homing_Axis1]` ~ `[Homing_Axis4]`：回零方向、搜索距离、输入极性需要按现场轴卡接线确认。
- `[Interlock] emergency_stop_input`、`emergency_stop_active_low`、`safety_door_input`：真实机型互锁极性和输入位必须现场复核。
- `[VelocityTrace] output_path`：当前是工作站绝对路径 `D:\Projects\SiligenSuite\logs\velocity_trace`，bring-up 前需要确认目标主机可写。

运行入口约束：

- `apps/runtime-service/run.ps1` 与 `apps/runtime-gateway/run.ps1` 默认总是转发 `config/machine/machine_config.ini`。
- 如需临时覆盖，只允许通过脚本参数 `-ConfigPath` 指向替代 INI；canonical 入口仍保持 `config/machine/machine_config.ini`。
