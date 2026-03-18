# 2026-01-28 DXF点胶触发模式与间距均匀性讨论记录

## 背景与目标
- 目标：胶点之间的间距尽量均匀。
- 场景：DXF点胶执行；HMI(Python/PyQt5)空跑用于验证；速度可能在生产中调整。

## 关键约束
- 点距（空间间隔）不能改。
- 压力/流量软件不可调（无法用来补偿胶点直径）。
- 胶点直径需固定 ⇒ 点胶持续时间（duration）不能随意改变。
- 触发参数合法范围：interval_ms ≥ 10ms，duration_ms ≥ 10ms。

## 触发模式技术定性
- 当前实现为固件级定时脉冲（MC_CmpPluse），属于“开环时间同步触发”。
- 与运动位置零耦合；速度波动会导致点距不均。
- 位置触发（MC_CmpBufData）在代码层有实现，但需编码器/硬件支持。
- 用户确认当前硬件无法读取编码器 ⇒ 位置触发不可行。

## 速度采样与CSV记录
- 已实现速度采样CSV，默认由配置文件驱动并自动启用。
- 输出CSV字段：epoch_ms, elapsed_ms, segment_index, segment_type, dispense_enabled, is_moving, velocity_mm_s。
- 采样输出路径默认改为：`D:\Projects\Backend_CPP\logs\velocity_trace`。
- 发现CSV速度单位问题：`MC_GetCrdVel` 返回值为“脉冲/ms”，之前误写为mm/s。
- 修正：在 `InterpolationAdapter::GetCoordinateSystemStatus` 中换算为mm/s。
  公式：v_mm_s = v_pulse_ms * 1000 / pulse_per_mm。
- 空跑CSV显示速度约4（原单位），换算后约20mm/s，与 541mm/27.406s ≈ 19.74mm/s 一致。

## “看起来一直开”判断与安全边界
- 在时间触发模式下，空间间距由 speed * interval 决定。
- 视觉上“看起来一直开”的条件（经验阈值）：
  interval_ms <= duration_ms + valve_response_ms (+ margin)
- 示例：
  - spacing=2mm, speed=20mm/s ⇒ interval≈100ms
  - duration=100ms、valve_response≈5ms ⇒ 几乎无间隙，仍会“看起来一直开”。
- 若保持 spacing=2mm 与 duration=100ms：
  speed_max ≈ 1000*2/(100+5) ≈ 19.05mm/s
  速度高于该值会出现“看起来一直开”。

### 判据细化（视觉连续 vs 物理连续）
- 视觉连续（“看起来一直开”）：相邻点之间的“可见空隙时间”接近 0。
- 物理连续：考虑阀响应/尾料后，有效关断时间 ≤ 0。
- 可用统一表述：
  - 有效开时长：T_on = duration_ms + t_open_delay + t_close_tail
  - 有效关时长：T_off = interval_ms - T_on
  - 若 T_off <= 0 ⇒ 物理连续；若 0 < T_off <= t_visual ⇒ 视觉连续（仍像一条线）
- 实务上可把 t_open_delay+t_close_tail 记为 valve_response_ms；t_visual 为“视觉阈值”，需现场标定。

### 安全边界建议（分级）
- 硬边界（禁止执行）：interval_ms < duration_ms + valve_response_ms
  - 含义：有效关时长为负或近 0，几乎必然连成线。
- 软边界（警告/降速/缩短有效段）：duration_ms + valve_response_ms <= interval_ms < duration_ms + valve_response_ms + margin_ms
  - margin_ms 作为视觉与过程裕量（建议用实测确定）。
- 正常区间：interval_ms >= duration_ms + valve_response_ms + margin_ms

### margin_ms 的经验设定（需标定）
- 建议先做“阶梯扫描”：固定压力与针嘴，在匀速段逐步减小 interval，观察从“点”到“线”的转折。
- 记录转折点附近的 interval，即可估出 t_visual；margin_ms 取 t_visual 或更保守值。
- 若无法现场标定，可先取 margin_ms ≈ 10~20ms 作为保守起点，但需在量产前验证。

### 速度变化时的边界计算
- interval_ms = 1000 * spacing_mm / speed_mm_s
- 若速度在段内变化（加减速），应按“最大速度”计算最小 interval，作为安全校验基准。
- 若已决定“仅匀速段点胶”，则用匀速段速度计算即可；加减速段禁胶可避免边界穿越。

### 术语备注
- “看起来一直开”是视觉判据，不等同于“阀门一直开”或“压力连续出胶”。
- 判据用于运行安全与品质预警，不代表胶点直径/粘度等物性已达稳态。

## SEGMENTED vs SUBSEGMENTED 讨论
- SEGMENTED：加速/匀速/减速三段；调用少，稳定，但间距均匀性一般。
- SUBSEGMENTED：加减速细分多个子段，间距更均匀，但调用多、受调度影响。
- 若目标是“加减速区间距更均匀”，SUBSEGMENTED更优；
  但仍是时间域触发，无法真正保证等距。

## 最终方向（在无法位置触发的前提下）
- 以“点距均匀”为唯一目标：
  1) 仅在匀速段点胶（加减速段禁胶）。
  2) 通过引线/退线保证有效区域落在匀速段。
  3) 或对短段降速，使其满足时间触发的安全间隔。
- 若必须使用时间触发：固定速度 + 固定interval，并满足 interval > duration + 响应时间。
- SEGMENTED/SUBSEGMENTED 仅能改善加减速区间距，不可根治。

## 相关代码与配置变更点（本次对话涉及）
- 配置驱动速度采样：
  - `src/shared/types/ConfigTypes.h` 新增 VelocityTraceConfig
  - `src/infrastructure/adapters/system/configuration/ConfigFileAdapter.*` 加载 [VelocityTrace]
  - `src/infrastructure/resources/config/files/machine_config.ini` 新增 [VelocityTrace]
- 采样换算修正：
  - `src/infrastructure/adapters/motion/controller/interpolation/InterpolationAdapter.cpp`
- 采样默认输出路径调整：
  - `output_path=D:\Projects\Backend_CPP\logs\velocity_trace`

## 待决事项
- 是否实现“匀速段点胶 + 加减速禁胶”策略（需明确行为与段级控制方式）。
- 是否加入“严格模式”：一旦无法满足安全间隔则阻止执行并告警。
- 是否对短段/拐角自动降速以维持间距。

## 结论摘要
- 当前硬件不支持编码器 ⇒ 位置触发不可行。
- 时间触发下无法保证绝对等距，只能通过匀速点胶、限速或分段策略改善。
- 速度采样已修正为真实mm/s，方便后续验证。
