# 开环点胶系统在梯形速度曲线下的均匀触发问题分析报告

**日期**: 2026-01-26
**状态**: 待解决
**标签**: Algorithm, Motion-Control, Dispensing

## 1. 问题背景与现状

### 1.1 系统架构
*   **控制模式**: 开环控制 (Open-Loop Control)。
*   **硬件能力**: 
    *   无位置反馈（编码器反馈关闭）。
    *   **触发 API**: `MC_CmpPluse(count, interval_ms, pulse_width)`。
    *   **特性**: 固件级定时器，一旦启动，严格按固定时间间隔 `interval_ms` 输出脉冲，**不随电机速度变化而自适应**。

### 1.2 核心痛点
当前算法 (`DXFDispensingPlanner`) 假设电机全程做**理想匀速运动**，导致触发间隔 $T_{interval}$ 为常数：
$$
 T_{interval} = \frac{\text{Target\_Spacing}}{\text{Cruise\_Velocity}}
$$

然而，实际物理运动遵循**梯形速度曲线 (Trapezoidal Velocity Profile)**：
1.  **加速段**: $v(t) = a \cdot t$
2.  **匀速段**: $v(t) = V_{cruise}$
3.  **减速段**: $v(t) = V_{cruise} - a \cdot t$

### 1.3 现象与后果
由于触发频率 $f = 1/T_{interval}$ 是按**最高速度**设定的：
*   在加/减速段，实际速度 $v(t) < V_{cruise}$。
*   导致空间点密度 $\rho = f / v(t)$ 显著增大。
*   **结果**: 起笔和收笔处胶点严重堆积（“大头”现象），甚至连成一片。

---

## 2. 待咨询的算法挑战 (Prompt)

以下描述用于咨询外部专家或 AI 模型：

> **核心问题**: 如何在仅支持“定频脉冲”指令的硬件上，实现变速度下的均匀空间布点？

### 2.1 约束条件
1.  **硬件黑盒**: 无法修改固件，无法读取实时位置。
2.  **指令限制**: 可能不支持指令队列（即不能预先塞入一堆不同频率的指令自动执行）。
3.  **OS 环境**: Windows 上位机，定时精度受限（±10ms Jitter）。

### 2.2 拟探讨的解决方案

#### 方案 A: 三段式分频 (Segmented Frequency)
将一条轨迹拆解为 3 次独立的运动指令和触发指令：
1.  **加速段**: 计算平均速度 $\bar{v}_{acc} = V_{cruise} / 2$，设置触发频率 $f_{acc} = f_{cruise} / 2$。
2.  **匀速段**: 维持 $f_{cruise}$。
3.  **减速段**: 同加速段。
*   *疑问*: 这种粗粒度的平均，在加速段内部（速度从 0 到 V）是否依然会导致分布严重不均？

#### 方案 B: 动态软件时间片 (Software Time-Slicing)
放弃 `MC_CmpPluse`，改用上位机高频定时器手动 `SetDO`。
*   *算法*: $P_{next} = P_{curr} + \text{Spacing}$，根据 $S(t)$ 曲线反解出 $t_{next}$，设置定时器等待。
*   *疑问*: Windows 的定时精度能否支撑？

---

## 3. 下一步行动建议

1.  使用本文档内容咨询 ChatGPT/Claude，获取数学推导和分段策略的误差分析。
2.  在 `DXFDispensingPlanner` 中实现“三段式”拆分逻辑（MVP 验证）。
3.  评估迁移 Linux RT 的必要性（作为终极手段）。
