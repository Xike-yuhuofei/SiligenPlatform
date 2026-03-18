# 开环点胶系统梯形速度曲线下的均匀触发方案报告

**日期**: 2026-01-26
**状态**: 可落地方案
**标签**: Algorithm, Motion-Control, Dispensing, Open-Loop

## 1. 目标与约束

**目标**: 在仅支持 `MC_CmpPluse(total_count, interval_ms, pulse_width)` 的硬件上，
尽可能让胶点在空间上接近等距，降低起笔/收笔堆胶。

**约束**:
- 开环控制，无位置反馈。
- 控制卡一次只能执行一条 `MC_CmpPluse`，无法队列排程。
- 触发是纯时间域，不能感知实际速度。
- 上位机为 Windows，软件定时存在抖动。

---

## 2. 方案总览

### 方案 A: 三段 + 子段分频（优先推荐）
- 将整段运动拆成：加速段、匀速段、减速段。
- 加/减速段进一步细分为 N 个子段，每个子段下发一条 `MC_CmpPluse`。
- 每个子段用该段的**平均速度**计算固定 interval。
- 优点：不依赖高精度软件定时；工程落地快。

### 方案 B: 软件定时逐点触发（高精度但有抖动风险）
- 放弃 `MC_CmpPluse`，用高分辨率计时器手动触发 IO。
- 使用解析公式计算每个脉冲的触发时刻。
- 优点：理论上最接近等距；缺点：Windows 抖动限制精度。

---

## 3. 运动参数与分段计算

### 3.1 基本参数
- 目标点间距: `S`
- 匀速速度: `V`
- 加速度: `a`
- 减速度: `d`
- 总长度: `L_total`

### 3.2 梯形/三角形判定
- 加速时间: `T_a = V / a`
- 减速时间: `T_d = V / d`
- 加速距离: `L_a = V^2 / (2a)`
- 减速距离: `L_d = V^2 / (2d)`

若 `L_total >= L_a + L_d`，为梯形速度曲线：
- 匀速距离 `L_c = L_total - L_a - L_d`
- 匀速时间 `T_c = L_c / V`

若 `L_total < L_a + L_d`，为三角速度曲线：
- 峰值速度:
  - 对称加减速时: `V_peak = sqrt(a * L_total)`
  - 非对称时: `V_peak = sqrt(2 * a * d * L_total / (a + d))`
- 匀速段消失，替换为加速到 `V_peak` 再减速。

---

## 4. 分段分频算法（方案 A）

### 4.1 加速段子段计算
将加速段分成 N 个子段，每段时间 `dt = T_a / N`。

第 i 段 (i = 1..N):
- 中点时刻: `t_mid = (i - 0.5) * dt`
- 段平均速度（线性加速时等于中点速度）:
  `v_i = a * t_mid`
- 固定间隔:
  `interval_i = S / v_i`
- 触发次数:
  `count_i = floor(dt / interval_i)`

若 `count_i <= 0`，可跳过该段或与相邻段合并。

### 4.2 匀速段
- `interval_c = S / V`
- `count_c = floor(T_c / interval_c)`

### 4.3 减速段子段计算
与加速段对称：
- `t_mid = (i - 0.5) * dt`
- `v_i = V - d * t_mid`
- `interval_i = S / v_i`
- `count_i = floor(dt / interval_i)`

### 4.4 误差与残差处理（重要）
- `interval_ms` 多数情况下量化到 1ms，需处理量化误差。
- 建议维护残差距离 `err_dist`:
  - 每段理想距离 `L_seg` 可用运动学计算。
  - `count_i = floor((L_seg + err_dist) / S)`
  - `err_dist = (L_seg + err_dist) - count_i * S`
- 量化后的 `interval_ms` 变化应尽量小，避免机械震荡。

---

## 5. 软件定时逐点触发（方案 B）

### 5.1 加速段逐点触发公式
速度: `v(t) = a t`

要求相邻触发的空间间距为 `S`:

```
0.5 * a * (t_{k+1}^2 - t_k^2) = S
=> t_{k+1} = sqrt(t_k^2 + 2S/a)
```

### 5.2 减速段逐点触发公式
速度: `v(t) = V - d t` (t 从减速段起点计时)

位置: `x(t) = V t - 0.5 d t^2`

```
V (t_{k+1} - t_k) - 0.5 d (t_{k+1}^2 - t_k^2) = S
=> t_{k+1} = (V - sqrt((V - d t_k)^2 - 2 d S)) / d
```

### 5.3 Windows 可行性评估
- `QueryPerformanceCounter` 精度高，但线程调度带来 0.1~1ms 级别抖动。
- 若 `interval < 2ms`，抖动可能导致明显误差。
- 若 `interval >= 5ms`，宏观效果通常可接受。

---

## 6. 固定频率触发导致点距变化公式

加速段中固定周期 `T` 触发：

- 第 k 个脉冲发生在 `t_k = kT`
- 点距:

```
Δx_k = ∫(t_k 到 t_{k+1}) a t dt
     = 0.5 * a * ( (k+1)^2 - k^2 ) * T^2
     = 0.5 * a * (2k + 1) * T^2
```

结论：点距随 k 线性增大，起笔处最密，末端最稀。

---

## 7. 工程落地伪代码

```cpp
struct Segment {
    int count;
    double interval_ms;
};

std::vector<Segment> buildRamp(double accel, double v_start, double v_end,
                               double S, int N, double duration_sec) {
    std::vector<Segment> segs;
    double dt = duration_sec / N;
    for (int i = 1; i <= N; ++i) {
        double t_mid = (i - 0.5) * dt;
        double v = v_start + accel * t_mid;
        if (v <= 1e-6) {
            continue;
        }
        double interval = S / v;      // sec
        int count = (int)floor(dt / interval);
        if (count > 0) {
            segs.push_back({count, interval * 1000.0});
        }
    }
    return segs;
}

void dispatchSegments(const std::vector<Segment>& segs, int pulse_width_us) {
    for (const auto& s : segs) {
        MC_CmpPluse(s.count, s.interval_ms, pulse_width_us);
        // 等待该段结束再下发下一段
        sleep_ms((int)ceil(s.count * s.interval_ms));
    }
}
```

---

## 8. 实施建议

- **MVP**: 先做三段 + 8~16 子段分频，观察起笔/收笔堆胶是否显著改善。
- **参数策略**:
  - N 过小：改善有限
  - N 过大：指令切换频繁，可能受通讯时序影响
- **验收指标**:
  - 起笔/收笔处的点距变化是否明显减小
  - 全程胶量是否均匀

如需进一步精度，可评估方案 B 或迁移实时系统。
