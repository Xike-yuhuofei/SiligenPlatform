# 梯形速度曲线下的均匀触发算法分析报告

> **日期**: 2026-01-26
> **类型**: 算法咨询报告
> **状态**: 完成

## 一、问题背景

### 1.1 系统架构

- **硬件架构**: Windows 上位机 + 运动控制卡（无位置反馈/编码器反馈关闭）+ 点胶阀
- **控制模式**: 开环控制
- **触发方式**: 硬件仅支持 `MC_CmpPluse(total_count, interval_ms, pulse_width)` API
  - 该指令下发后，固件会严格按照 `interval_ms` 的时间间隔，连续输出 `total_count` 个脉冲
  - 这是一个纯时间域的触发，完全不感知电机的实际运动速度

### 1.2 核心痛点

当前代码简单粗暴地假设电机全程做匀速运动，因此计算出的 `interval_ms` 是一个固定值：

$$T_{interval} = \frac{S_{target}}{V_{cruise}}$$

然而，实际运动遵循梯形速度曲线 (Trapezoidal Velocity Profile)，包含：
1. **加速段 (Accel Ramp)**: 速度从 0 线性增加到 $V_{cruise}$
2. **匀速段 (Cruise)**: 速度维持 $V_{cruise}$
3. **减速段 (Decel Ramp)**: 速度从 $V_{cruise}$ 线性减小到 0

### 1.3 后果

由于触发间隔是按最大速度设定的固定值：
- 在加速段和减速段，电机实际速度 $v(t) < V_{cruise}$，但脉冲依然飞快地发
- 导致单位距离内的胶点数量激增（Over-dispensing），在起笔和收笔处出现严重的堆胶/大头现象

---

## 二、数学分析

### 2.1 梯形速度曲线模型

```
速度 v(t)
    ^
    |        _______________
    |       /               \
V_c |------/                 \------
    |     /                   \
    |    /                     \
    |   /                       \
    +--+--+--------+--------+--+----> 时间 t
       0  T_a      T_a+T_c   T_total

       加速段    匀速段    减速段
```

**参数定义**:
- $V_c$ = 巡航速度 (cruise velocity)
- $a$ = 加速度 (acceleration)
- $T_a$ = 加速时间 = $V_c / a$
- $T_d$ = 减速时间 = $V_c / a$（假设对称）
- $S_{target}$ = 目标胶点间距

### 2.2 固定频率触发的问题推导

**当前错误做法**: 使用固定间隔 $T_{interval} = S_{target} / V_c$

**加速段内的实际间距**:

在加速段，速度 $v(t) = a \cdot t$

第 $n$ 次触发时刻: $t_n = n \cdot T_{interval}$

第 $n$ 到第 $n+1$ 次触发之间的**实际位移**:

$$S_n = \int_{t_n}^{t_{n+1}} v(t) \, dt = \int_{t_n}^{t_{n+1}} a \cdot t \, dt = \frac{a}{2}(t_{n+1}^2 - t_n^2)$$

$$S_n = \frac{a}{2} \cdot T_{interval}^2 \cdot (2n + 1)$$

**关键发现**: 实际间距 $S_n$ 与触发序号 $n$ 成线性关系!

| 触发序号 n | 实际间距 $S_n$ | 相对于目标的比例 |
|-----------|---------------|-----------------|
| 0 (起点)  | $\frac{a \cdot T_{interval}^2}{2}$ | 很小 |
| 1         | $\frac{3a \cdot T_{interval}^2}{2}$ | 较小 |
| ... | ... | ... |
| $N_{accel}$ (加速结束) | $\approx S_{target}$ | 100% |

**数值示例**:
- $V_c = 100$ mm/s, $a = 500$ mm/s^2, $S_{target} = 1$ mm
- $T_{interval} = 1/100 = 10$ ms
- $T_a = 100/500 = 0.2$ s = 200 ms
- 加速段触发次数 = 20 次

第 0 次间距: $S_0 = 0.5 \times 500 \times 0.01^2 \times 1 = 0.025$ mm（仅为目标的 2.5%!）
第 1 次间距: $S_1 = 0.5 \times 500 \times 0.01^2 \times 3 = 0.075$ mm

---

## 三、解决方案

### 方案 A: 分段平均速度法（简单但有误差）

**思路**: 将加速段视为整体，使用平均速度计算间隔

```
加速段平均速度: V_avg = V_c / 2
加速段触发间隔: T_accel = S_target / V_avg = 2 * S_target / V_c
```

**优点**: 实现简单，只需调用 3 次 `MC_CmpPluse`
**缺点**: 加速段内部仍有密度不均，只是宏观平均正确

```cpp
void executeWithAverageVelocity(double S_target, double V_cruise, double accel) {
    double T_accel = V_cruise / accel;  // 加速时间
    double S_accel = 0.5 * accel * T_accel * T_accel;  // 加速段距离

    // 加速段: 使用平均速度
    double V_avg_accel = V_cruise / 2.0;
    double interval_accel_ms = (S_target / V_avg_accel) * 1000.0;
    int count_accel = static_cast<int>(S_accel / S_target);

    MC_CmpPluse(count_accel, interval_accel_ms, pulse_width);
    Sleep(T_accel * 1000);  // 等待加速段完成

    // 匀速段
    double interval_cruise_ms = (S_target / V_cruise) * 1000.0;
    MC_CmpPluse(count_cruise, interval_cruise_ms, pulse_width);
    // ... 减速段类似
}
```

---

### 方案 B: 等距触发时刻预计算法（推荐）

**核心思想**: 反向求解——给定目标间距，计算每个触发应该发生的**精确时刻**

#### 数学推导

**目标**: 找到时刻序列 $\{t_0, t_1, t_2, ...\}$，使得相邻触发之间的位移恒为 $S_{target}$

**加速段** ($v(t) = a \cdot t$):

位置函数: $x(t) = \frac{1}{2} a t^2$

第 $k$ 个触发点的位置: $x_k = k \cdot S_{target}$

反解时刻:
$$t_k = \sqrt{\frac{2 \cdot k \cdot S_{target}}{a}}$$

**触发间隔**（非均匀!）:
$$\Delta t_k = t_k - t_{k-1} = \sqrt{\frac{2 S_{target}}{a}} \left( \sqrt{k} - \sqrt{k-1} \right)$$

**匀速段** ($v = V_c$):

$$\Delta t = \frac{S_{target}}{V_c}$$ （均匀间隔）

**减速段** ($v(t) = V_c - a \cdot (t - t_{decel\_start})$):

设减速段起始时刻为 $t_0'$，则 $v(t) = V_c - a(t - t_0')$

位置: $x(t) = V_c(t-t_0') - \frac{1}{2}a(t-t_0')^2$

令 $\tau = t - t_0'$，则 $x(\tau) = V_c \tau - \frac{1}{2}a\tau^2$

对于第 $k$ 个触发点（从减速段开始计数）:
$$k \cdot S_{target} = V_c \tau_k - \frac{1}{2}a\tau_k^2$$

这是一元二次方程，解为:
$$\tau_k = \frac{V_c - \sqrt{V_c^2 - 2 a \cdot k \cdot S_{target}}}{a}$$

---

### 方案 C: 软件精确定时触发（Windows 实现）

由于 `MC_CmpPluse` 只支持固定间隔，我们需要绕过它，直接用软件控制 IO 触发。

#### Windows 高精度定时的可行性分析

| 方法 | 精度 | 抖动 | 适用场景 |
|------|------|------|----------|
| `Sleep()` | ~15ms | 很大 | 不适用 |
| `timeBeginPeriod(1)` + `Sleep()` | ~1ms | 中等 | 勉强可用 |
| `QueryPerformanceCounter` 忙等待 | <1us | 小 | 推荐 |
| Multimedia Timer | ~1ms | 中等 | 可用 |
| Waitable Timer | ~1ms | 中等 | 可用 |

---

### 方案 D: 混合策略（工程最优解）

**核心思想**: 匀速段用硬件触发（稳定），加减速段用软件触发（灵活）

```
时间线:
|<-- 软件触发 -->|<-- MC_CmpPluse -->|<-- 软件触发 -->|
     加速段            匀速段              减速段
```

---

## 四、完整实现代码

### 4.1 触发时刻预计算

```cpp
#include <vector>
#include <cmath>

struct TriggerSchedule {
    std::vector<double> trigger_times_ms;  // 每个触发的绝对时刻（毫秒）
    int accel_count;
    int cruise_count;
    int decel_count;
};

// 预计算所有触发时刻
TriggerSchedule computeTriggerSchedule(
    double total_distance_mm,
    double V_cruise_mm_s,
    double accel_mm_s2,
    double spacing_mm
) {
    TriggerSchedule schedule;

    // 1. 计算运动学参数
    double T_accel = V_cruise_mm_s / accel_mm_s2;  // 加速时间 (s)
    double S_accel = 0.5 * accel_mm_s2 * T_accel * T_accel;  // 加速段距离
    double S_decel = S_accel;  // 对称减速
    double S_cruise = total_distance_mm - S_accel - S_decel;

    // 处理短距离情况（无法达到巡航速度）
    if (S_cruise < 0) {
        // 三角形速度曲线，需要重新计算
        S_accel = total_distance_mm / 2.0;
        S_decel = total_distance_mm / 2.0;
        S_cruise = 0;
        T_accel = std::sqrt(2.0 * S_accel / accel_mm_s2);
        V_cruise_mm_s = accel_mm_s2 * T_accel;  // 实际达到的最大速度
    }

    double T_cruise = S_cruise / V_cruise_mm_s;
    double T_decel = T_accel;

    double current_pos = 0.0;
    int trigger_index = 0;

    // 2. 加速段触发时刻计算
    schedule.accel_count = 0;
    while (current_pos < S_accel - spacing_mm * 0.5) {
        // 下一个触发点的位置
        double next_pos = (trigger_index + 1) * spacing_mm;
        if (next_pos > S_accel) break;

        // 反解时刻: x = 0.5 * a * t^2  =>  t = sqrt(2x/a)
        double t_trigger = std::sqrt(2.0 * next_pos / accel_mm_s2);
        schedule.trigger_times_ms.push_back(t_trigger * 1000.0);

        current_pos = next_pos;
        trigger_index++;
        schedule.accel_count++;
    }

    // 3. 匀速段触发时刻计算
    double cruise_start_time = T_accel;
    double cruise_start_pos = S_accel;
    schedule.cruise_count = 0;

    while (current_pos < S_accel + S_cruise - spacing_mm * 0.5) {
        double next_pos = (trigger_index + 1) * spacing_mm;
        if (next_pos > S_accel + S_cruise) break;

        // 匀速段: x = S_accel + V_c * (t - T_accel)
        // => t = T_accel + (x - S_accel) / V_c
        double t_trigger = cruise_start_time + (next_pos - cruise_start_pos) / V_cruise_mm_s;
        schedule.trigger_times_ms.push_back(t_trigger * 1000.0);

        current_pos = next_pos;
        trigger_index++;
        schedule.cruise_count++;
    }

    // 4. 减速段触发时刻计算
    double decel_start_time = T_accel + T_cruise;
    double decel_start_pos = S_accel + S_cruise;
    schedule.decel_count = 0;

    while (current_pos < total_distance_mm - spacing_mm * 0.5) {
        double next_pos = (trigger_index + 1) * spacing_mm;
        if (next_pos > total_distance_mm) break;

        // 减速段位移（相对于减速起点）
        double delta_x = next_pos - decel_start_pos;

        // x = V_c * t - 0.5 * a * t^2
        // 解一元二次方程: 0.5*a*t^2 - V_c*t + delta_x = 0
        double discriminant = V_cruise_mm_s * V_cruise_mm_s - 2.0 * accel_mm_s2 * delta_x;
        if (discriminant < 0) break;  // 无解，超出减速段范围

        double tau = (V_cruise_mm_s - std::sqrt(discriminant)) / accel_mm_s2;
        double t_trigger = decel_start_time + tau;
        schedule.trigger_times_ms.push_back(t_trigger * 1000.0);

        current_pos = next_pos;
        trigger_index++;
        schedule.decel_count++;
    }

    return schedule;
}
```

### 4.2 高精度定时触发器

```cpp
#include <windows.h>
#include <vector>
#include <functional>
#include <intrin.h>

class HighPrecisionTrigger {
private:
    LARGE_INTEGER frequency_;
    LARGE_INTEGER start_time_;

    // 获取当前时间（微秒）
    double getCurrentTimeMicros() {
        LARGE_INTEGER now;
        QueryPerformanceCounter(&now);
        return static_cast<double>(now.QuadPart - start_time_.QuadPart)
               * 1000000.0 / frequency_.QuadPart;
    }

public:
    HighPrecisionTrigger() {
        QueryPerformanceFrequency(&frequency_);
    }

    // 执行预计算的触发序列
    void executeTriggerSequence(
        const std::vector<double>& trigger_times_ms,
        std::function<void()> triggerCallback,
        std::function<void()> startMotionCallback
    ) {
        if (trigger_times_ms.empty()) return;

        // 提升线程优先级
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

        // 设置系统定时器精度
        timeBeginPeriod(1);

        // 记录起始时间
        QueryPerformanceCounter(&start_time_);

        // 启动运动
        startMotionCallback();

        size_t trigger_index = 0;

        while (trigger_index < trigger_times_ms.size()) {
            double target_time_us = trigger_times_ms[trigger_index] * 1000.0;
            double current_time_us = getCurrentTimeMicros();
            double remaining_us = target_time_us - current_time_us;

            // 粗等待: 如果剩余时间 > 2ms，使用 Sleep 节省 CPU
            if (remaining_us > 2000) {
                Sleep(static_cast<DWORD>((remaining_us - 1500) / 1000));
            }

            // 精等待: 忙等待到精确时刻
            while (getCurrentTimeMicros() < target_time_us) {
                // 忙等待（可以加 _mm_pause() 减少功耗）
                _mm_pause();
            }

            // 触发!
            triggerCallback();
            trigger_index++;
        }

        // 恢复设置
        timeEndPeriod(1);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
    }
};
```

### 4.3 混合策略控制器

```cpp
class HybridDispensingController {
public:
    void execute(
        double total_distance,
        double V_cruise,
        double accel,
        double spacing
    ) {
        auto schedule = computeTriggerSchedule(total_distance, V_cruise, accel, spacing);

        // 计算时间节点
        double T_accel = V_cruise / accel;
        double S_accel = 0.5 * accel * T_accel * T_accel;
        double S_cruise = total_distance - 2 * S_accel;
        double T_cruise = S_cruise / V_cruise;

        // 提取各段的触发时刻
        std::vector<double> accel_triggers;

        for (size_t i = 0; i < schedule.accel_count; i++) {
            accel_triggers.push_back(schedule.trigger_times_ms[i]);
        }

        // 匀速段可以用硬件触发
        double cruise_interval_ms = (spacing / V_cruise) * 1000.0;

        // === 执行流程 ===

        // 1. 启动运动
        startLinearMotion(total_distance, V_cruise, accel);

        // 2. 加速段: 软件精确定时触发
        HighPrecisionTrigger softTrigger;
        softTrigger.executeTriggerSequence(accel_triggers, triggerPulse, [](){});

        // 3. 匀速段: 切换到硬件触发
        MC_CmpPluse(schedule.cruise_count, cruise_interval_ms, PULSE_WIDTH);

        // 等待匀速段完成
        Sleep(static_cast<DWORD>(T_cruise * 1000));

        // 4. 减速段: 软件精确定时触发
        // 注意: 需要调整时间基准
        std::vector<double> decel_triggers_adjusted;
        double decel_start_time = T_accel + T_cruise;
        for (size_t i = schedule.accel_count + schedule.cruise_count;
             i < schedule.trigger_times_ms.size(); i++) {
            decel_triggers_adjusted.push_back(
                schedule.trigger_times_ms[i] - decel_start_time * 1000.0
            );
        }

        softTrigger.executeTriggerSequence(decel_triggers_adjusted, triggerPulse, [](){});
    }

private:
    void startLinearMotion(double distance, double velocity, double accel) {
        // 调用运动控制卡的线性运动指令
    }

    static void triggerPulse() {
        // 调用运动控制卡的 IO 输出函数
    }
};
```

---

## 五、方案对比

| 方案 | 实现复杂度 | 精度 | CPU 占用 | 适用场景 |
|------|-----------|------|----------|----------|
| A. 分段平均速度 | 低 | 中等 | 低 | 精度要求不高 |
| B. 预计算时刻 | 中 | 高 | - | 算法基础 |
| C. 纯软件定时 | 高 | 高 | 高 | 触发频率低 |
| D. 混合策略 | 较高 | 最高 | 中等 | **推荐** |

---

## 六、误差分析与边界处理

### 6.1 Windows 定时抖动的影响

假设定时抖动 $\sigma_t = 0.5$ ms，在加速段:

位置误差 = $v(t) \times \sigma_t = a \cdot t \times \sigma_t$

在加速段末尾（$t = T_a$）:
$$\Delta x_{max} = V_c \times 0.5\text{ms} = 100 \times 0.0005 = 0.05 \text{mm}$$

对于 1mm 间距，这是 5% 的误差，通常可接受。

### 6.2 短线段处理（三角形速度曲线）

当线段太短，无法达到巡航速度时:

```cpp
if (total_distance < 2 * S_accel) {
    // 三角形速度曲线
    double V_peak = std::sqrt(accel * total_distance);
    double T_accel_actual = V_peak / accel;
    // 重新计算触发时刻...
}
```

---

## 七、关键洞察

1. **开环系统的本质限制**: 没有位置反馈意味着必须依赖精确的运动学模型，任何模型误差（摩擦、惯性、负载变化）都会累积

2. **时间域 vs 空间域**: 硬件 `MC_CmpPluse` 是时间域触发，而点胶需求是空间域均匀——这个 mismatch 是问题根源

3. **Windows 实时性**: 通过 `QueryPerformanceCounter` + 忙等待 + 线程优先级提升，可以在 Windows 上实现亚毫秒级定时，但要注意 CPU 占用和系统负载的影响

---

## 八、后续建议

1. **短期**: 实现方案 A（分段平均速度），快速验证效果
2. **中期**: 实现方案 D（混合策略），获得最佳精度
3. **长期**: 考虑升级硬件，使用支持位置触发的运动控制卡

---

*报告生成时间: 2026-01-26*
