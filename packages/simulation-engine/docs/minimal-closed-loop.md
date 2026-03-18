# 最小闭环

## 1. 目标

方案 C 的第一个可交付闭环，不追求完整机台覆盖，只要求以下链路稳定：

1. 输入：canonical `simulation-input.json`
2. 驱动：固定步长 `VirtualClock`
3. 执行：`RuntimeBridge` 调用真实控制/执行语义
4. 反馈：`VirtualAxisGroup` / `VirtualIo`
5. 输出：`motion_profile`、`timeline`、`summary`

## 2. 闭环结构

```text
simulation-input.json
    -> application/session
    -> runtime-bridge
    -> virtual-time
    -> virtual-devices
    -> recording
    -> result.json
```

## 3. 首版范围

- 轴组：`X`、`Y`、可选 `Z`
- 时钟：固定步长，默认 `1 ms`
- 动作：home、move、execute path、stop
- 反馈：position、velocity、home、soft-limit、running、done
- 记录：时间线、最终位置、总时长、关键状态变化

## 4. 暂不进入首版的内容

- 胶量/胶宽/材料工艺
- 电机、电流、振动、背隙、摩擦等伺服物理
- UI、TCP、runtime-host 装配
- 真实硬件驱动

## 5. 实现顺序

1. 复用现有 compatibility timing engine 保住示例和基线
2. 增加 `VirtualClock`
3. 增加 `VirtualAxisGroup` 与基础反馈
4. 增加 `RuntimeBridge`，把真实执行语义接入虚拟端口
5. 增加 `Recorder`，统一导出结果
6. 用 `integration/simulated-line` 做最小回归
