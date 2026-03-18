# 阀门协调配置参数参考

## 概述

本文档详细说明DXF阀门协调功能的所有配置参数。配置文件位于:
```
src/infrastructure/configs/machine_config.ini
```

配置文件使用INI格式,包含 `[ValveCoordination]` 节,定义软件时间触发点胶的所有时序参数。

**代码实现**: 参考 `src/shared/types/ConfigTypes.h:395-498` (ValveCoordinationConfig)

---

## 点胶电磁阀硬件规格

### 型号信息

本系统使用 **FESTO MHE3 系列微型电磁阀**:

| 参数 | 值 | 说明 |
|------|-----|------|
| **型号** | MHE3-MS1H-3/2G-1/8-K | FESTO 微型电磁阀 |
| **物料号** | 525149 | FESTO 产品编号 |
| **电压** | 24V DC ±10% | 直流电源 |
| **功率** | 6.5W | 线圈功率 |
| **启动电流** | 1A | Ion |
| **防护等级** | IP65 | 防尘防水 |
| **工作压力** | -0.09 ~ 0.8 MPa | (-0.9 ~ 8 bar, -13.05 ~ 116 psi) |
| **阀门类型** | 3/2G | 3位2通,单电控 |
| **接口尺寸** | G1/8" | 螺纹接口 |
| **产地** | 匈牙利 | Made in Hungary |

### 响应时间参数

MHE3 系列是 FESTO 的高速微型阀,专为快速切换应用设计:

| 时间参数 | 典型值 | 说明 |
|---------|--------|------|
| **开启时间 (Opening time)** | 1.5 ~ 3 ms | 从通电到阀芯完全打开 |
| **关闭时间 (Closing time)** | 1 ~ 2 ms | 从断电到阀芯完全关闭 |
| **总响应时间** | 2.5 ~ 5 ms | 开启 + 关闭 |
| **最小脉冲宽度** | ~5 ms | 保证阀芯完全动作的最小时间 |

### 最小点胶时间计算依据

系统配置的 `min_duration_ms = 10 ms` 基于以下计算:

```
最小点胶时间 = 阀门开启时间 + 稳定出胶时间 + 安全裕量
            = 3 ms + 2 ms + 5 ms
            = 10 ms
```

| 组成部分 | 时间 | 原因 |
|---------|------|------|
| **阀门开启时间** | ~3 ms | 电磁铁吸合 + 阀芯位移 |
| **稳定出胶时间** | ~2 ms | 气路建压 + 胶水流动稳定 |
| **安全裕量** | ~5 ms | 考虑电压波动、温度变化、机械磨损 |
| **合计** | **10 ms** | 保守设计值 (约 2 倍安全裕量) |

### 为什么不能更短?

1. **阀芯机械惯性**: 即使电信号瞬间切换,阀芯需要物理位移时间
2. **气路响应延迟**: 从阀门打开到针头出胶存在气路传输延迟
3. **胶水流变特性**: 胶水从静止到稳定流动需要时间
4. **可靠性考虑**: 过短的脉冲可能导致阀芯未完全打开就关闭,出胶量不稳定

### 运动中点胶的位移影响

即使使用最小点胶时间 (10ms),在不同速度下仍会产生相对位移:

| 点胶速度 (mm/s) | 相对位移 (mm) | 影响程度 |
|----------------|--------------|---------|
| 10 | 0.1 | 可接受 |
| 30 | 0.3 | 轻微拉伸 |
| 50 | 0.5 | 明显拉伸 |
| 100 | 1.0 | 严重拉伸 |

**计算公式**: `相对位移 = 点胶速度 × 点胶时间 / 1000`

**结论**: 如果点胶速度 > 30 mm/s,即使使用最小脉宽,仍需要**点动停胶**或**速度补偿**策略来保证圆形胶点。

---

## 配置文件结构

### [ValveCoordination] 节概览

```ini
[ValveCoordination]
# 软件时间触发参数
DispensingIntervalMs=1000        ; 点胶间隔(毫秒) default: 1000
DispensingDurationMs=100         ; 点胶持续时间(毫秒) default: 100
ValveOpenDelayMs=50              ; 阀门开启延迟(毫秒) default: 50
SafetyTimeoutMs=5000             ; 安全超时(毫秒) default: 5000

# 多阀门配置
EnabledValves=2                  ; 启用阀门数量(1-4) default: 2
Valve1.Channel=0                 ; 阀门1 CMP通道号
Valve2.Channel=1                 ; 阀门2 CMP通道号
Valve3.Channel=2                 ; 阀门3 CMP通道号(可选)
Valve4.Channel=3                 ; 阀门4 CMP通道号(可选)

# ARC段处理参数
ArcSegmentationMaxDegree=30      ; ARC拆分最大角度(度) default: 30
ArcChordToleranceMm=0.1          ; 弦高容差(毫米) default: 0.1
```

---

## 参数详细说明

### 1. DispensingIntervalMs

**类型**: 整数 (int32)
**范围**: 10 - 60000
**单位**: 毫秒
**默认值**: 1000

**参数说明**:
两个连续触发点之间的时间间隔。此参数控制点胶的密度,值越小点胶越密集。

**时序图**:
```
时间轴: 0ms -----> 1000ms -----> 2000ms -----> 3000ms
触发:   ✓ 点胶1      ✓ 点胶2       ✓ 点胶3
阀门:   [开100ms]    [开100ms]    [开100ms]
```

**配置示例**:
```ini
# 高频点胶(密集)
DispensingIntervalMs=100   # 每100ms触发一次

# 标准点胶
DispensingIntervalMs=1000  # 每1秒触发一次

# 低频点胶(稀疏)
DispensingIntervalMs=5000  # 每5秒触发一次
```

**验证规则** (参考 ConfigTypes.h):
```cpp
if (dispensing_interval_ms < 10 || dispensing_interval_ms > 60000) {
    return false;  // 超出范围
}
```

**性能影响**:
- 间隔越小 → 触发点越多 → 内存占用越大
- 建议范围: 500-2000ms(平衡精度与性能)

---

### 2. DispensingDurationMs

**类型**: 整数 (int32)
**范围**: 10 - 5000
**单位**: 毫秒
**默认值**: 100

**参数说明**:
单次点胶时阀门保持开启的时间。此参数直接控制胶量,时间越长胶量越多。

**时序图**:
```
DispensingIntervalMs=1000, DispensingDurationMs=200

时间轴: 0ms -----> 200ms -----> 1000ms -----> 1200ms
触发:   ✓ 点胶1      | 关闭        ✓ 点胶2      | 关闭
阀门:   [开启 200ms] [关闭 800ms] [开启 200ms]
状态:   点胶中       等待         点胶中
```

**配置示例**:
```ini
# 小胶量(精密点胶)
DispensingDurationMs=50   # 50ms

# 标准胶量
DispensingDurationMs=100  # 100ms

# 大胶量
DispensingDurationMs=500  # 500ms
```

**胶量计算公式**:
```
胶量 ∝ DispensingDurationMs × 阀门流量系数
```

**注意事项**:
- `DispensingDurationMs` 必须 < `DispensingIntervalMs`
- 如果持续时间 ≥ 间隔,阀门将无法关闭
- 系统验证: `DispensingDurationMs < DispensingIntervalMs`

---

### 3. ValveOpenDelayMs

**类型**: 整数 (int32)
**范围**: 0 - 1000
**单位**: 毫秒
**默认值**: 50

**参数说明**:
阀门打开延迟补偿。用于消除阀门机械响应滞后,确保在到达触发点时阀门已完全打开。

**时序图**:
```
到达触发点: 0ms -----> 50ms -----> 100ms
指令:       ✓ 发送打开指令  ✓ 阀门完全打开
延迟:       ValveOpenDelayMs=50
实际点胶:                    [开始点胶]
```

**配置示例**:
```ini
# 快速阀门(响应快)
ValveOpenDelayMs=10   # 10ms延迟

# 标准阀门
ValveOpenDelayMs=50   # 50ms延迟

# 慢速阀门(响应慢)
ValveOpenDelayMs=100  # 100ms延迟
```

**延迟测量方法**:
1. 记录发送打开指令时间
2. 使用高速摄像机观察阀门实际打开时间
3. 差值即为 `ValveOpenDelayMs`

**过度补偿风险**:
- 如果延迟设置过大,点胶会提前开始
- 建议: 通过实际测试确定最佳值

---

### 4. SafetyTimeoutMs

**类型**: 整数 (int32)
**范围**: 100 - 60000
**单位**: 毫秒
**默认值**: 5000

**参数说明**:
阀门操作超时时间。如果阀门操作(开启/关闭)超过此时间,系统触发故障安全机制,强制关闭所有阀门。

**故障安全时序**:
```
0ms: 发送打开指令
5000ms: 阀门仍未确认打开
       → 触发故障安全
       → 强制关闭供胶阀
       → 停止运动
       → 记录错误日志
```

**配置示例**:
```ini
# 快速故障检测
SafetyTimeoutMs=1000   # 1秒超时

# 标准超时
SafetyTimeoutMs=5000   # 5秒超时

# 宽松超时(慢速阀门)
SafetyTimeoutMs=10000  # 10秒超时
```

**安全建议**:
- 快速响应阀门: 1000-3000ms
- 标准阀门: 5000ms
- 慢速阀门: 8000-10000ms

**故障安全实现**:
```cpp
// 参考: src/application/usecases/ValveCoordinationUseCase.cpp
if (elapsed_time > config.safety_timeout_ms) {
    // 故障安全: 强制关闭阀门
    hardware_controller_->EmergencyStop();
    hardware_controller_->CloseSupplyValve();
    return Result::Failure(Error::TIMEOUT, "Valve operation timeout");
}
```

---

### 5. EnabledValves

**类型**: 整数 (int32)
**范围**: 1 - 4
**默认值**: 2

**参数说明**:
启用的阀门数量,用于多阀门协同点胶。所有启用的阀门将同时触发(并行控制)。

**阀门编号**:
- 阀门1: 第一个启用的阀门
- 阀门2: 第二个启用的阀门
- 阀门3: 第三个启用的阀门
- 阀门4: 第四个启用的阀门

**配置示例**:
```ini
# 单阀门模式
EnabledValves=1
Valve1.Channel=0

# 双阀门模式(默认)
EnabledValves=2
Valve1.Channel=0
Valve2.Channel=1

# 四阀门模式
EnabledValves=4
Valve1.Channel=0
Valve2.Channel=1
Valve3.Channel=2
Valve4.Channel=3
```

**并行触发时序**:
```
EnabledValves=2

时间轴: 0ms -----> 100ms -----> 1000ms
触发:   ✓ 点胶1
阀门1:  [开100ms] [关闭]
阀门2:  [开100ms] [关闭]
        ↑ 同时触发
```

**硬件连接**:
```
软件CMP通道 → MultiCard硬件输出
Valve1.Channel=0 → DO0 (物理输出0)
Valve2.Channel=1 → DO1 (物理输出1)
Valve3.Channel=2 → DO2 (物理输出2)
Valve4.Channel=3 → DO3 (物理输出3)
```

---

### 6. Valve[N].Channel (N=1-4)

**类型**: 整数 (int32)
**范围**: 0 - 7
**默认值**: 0, 1, 2, 3
**单位**: CMP通道号

**参数说明**:
每个阀门对应的CMP(比较输出)通道号。CMP通道是MultiCard硬件的数字输出,用于控制阀门开关。

**通道映射**:
```
CMP通道0 → DO0 (Digital Output 0)
CMP通道1 → DO1
CMP通道2 → DO2
...
CMP通道7 → DO7
```

**配置示例**:
```ini
EnabledValves=2
Valve1.Channel=0  # 阀门1使用CMP通道0
Valve2.Channel=3  # 阀门2使用CMP通道3
```

**通道冲突检查**:
- 系统会自动检查通道冲突
- 如果两个阀门使用相同通道,会报错

**硬件限制**:
- 单个MultiCard卡最多8个CMP通道(0-7)
- 如果需要更多阀门,需要多卡协同

---

### 7. ArcSegmentationMaxDegree

**类型**: 浮点数 (float32)
**范围**: 1.0 - 90.0
**单位**: 度 (°)
**默认值**: 30.0

**参数说明**:
ARC段拆分为直线插补段的最大圆心角。值越小,拆分越细,精度越高,但触发点数越多。

**ARC拆分原理**:
```
ARC段: 0° → 90° (90度圆弧)
ArcSegmentationMaxDegree=30.0

拆分结果:
0° → 30° → 60° → 90°
3段,4个触发点(起点+2个中间点+终点)
```

**配置示例**:
```ini
# 高精度(精细拆分)
ArcSegmentationMaxDegree=5.0   # 每5度一段

# 标准精度
ArcSegmentationMaxDegree=30.0  # 每30度一段

# 低精度(粗略拆分)
ArcSegmentationMaxDegree=60.0  # 每60度一段
```

**精度对比**:
```
ARC: 90度圆弧,半径10mm

ArcSegmentationMaxDegree=5.0:
  → 18段,19个触发点
  → 弦高误差: < 0.01mm
  → 处理时间: 慢

ArcSegmentationMaxDegree=30.0:
  → 3段,4个触发点
  → 弦高误差: < 0.1mm
  → 处理时间: 快

ArcSegmentationMaxDegree=60.0:
  → 2段,3个触发点
  → 弦高误差: < 0.5mm
  → 处理时间: 最快
```

**弦高误差公式**:
```
弦高误差 ≈ 半径 × (1 - cos(角度/2))

例如: 半径10mm,角度30°
弦高误差 ≈ 10 × (1 - cos(15°)) ≈ 0.34mm
```

**性能权衡**:
```
精度 ↑ → ArcSegmentationMaxDegree ↓ → 触发点数 ↑ → 处理时间 ↑ → 内存占用 ↑
```

---

### 8. ArcChordToleranceMm

**类型**: 浮点数 (float32)
**范围**: 0.001 - 1.0
**单位**: 毫米 (mm)
**默认值**: 0.1

**参数说明**:
ARC段拟合的弦高容差。控制直线段拟合圆弧的精度,值越小拟合越精确。

**弦高容差定义**:
```
圆弧拟合示意图:

        实际圆弧
       /-------\
      /         \
     /           \
    /-------------\  直线拟合段
          ↑
      弦高误差
```

**配置示例**:
```ini
# 严格容差(高精度)
ArcChordToleranceMm=0.01   # 0.01mm

# 标准容差
ArcChordToleranceMm=0.1    # 0.1mm

# 宽松容差(低精度)
ArcChordToleranceMm=0.5    # 0.5mm
```

**与 ArcSegmentationMaxDegree 的关系**:

两个参数共同控制ARC拆分精度:
1. **角度约束**: 每段最大圆心角
2. **容差约束**: 弦高误差上限

**拆分算法**:
```cpp
// 伪代码: ARC拆分逻辑
function SegmentArc(arc, maxDegree, chordTolerance) {
    segments = [];
    currentAngle = arc.startAngle;

    while (currentAngle < arc.endAngle) {
        // 尝试创建 maxDegree 的段
        nextAngle = min(currentAngle + maxDegree, arc.endAngle);
        segment = CreateSegment(currentAngle, nextAngle);

        // 检查弦高容差
        if (ChordHeight(segment) > chordTolerance) {
            // 容差超限,减小角度
            nextAngle = currentAngle + CalculateSafeAngle(chordTolerance);
            segment = CreateSegment(currentAngle, nextAngle);
        }

        segments.push(segment);
        currentAngle = nextAngle;
    }

    return segments;
}
```

**推荐配置组合**:

```ini
# 高精度模式
ArcSegmentationMaxDegree=15.0
ArcChordToleranceMm=0.05
# 特点: 精度高,点数多,适合精密点胶

# 标准模式
ArcSegmentationMaxDegree=30.0
ArcChordToleranceMm=0.1
# 特点: 平衡精度与性能,适合一般应用

# 快速模式
ArcSegmentationMaxDegree=45.0
ArcChordToleranceMm=0.2
# 特点: 精度低,点数少,适合快速生产
```

---

## 参数验证规则

### 验证逻辑 (ConfigTypes.h)

```cpp
bool ValveCoordinationConfig::Validate() const {
    // 1. 点胶间隔验证
    if (dispensing_interval_ms < 10 || dispensing_interval_ms > 60000) {
        return false;
    }

    // 2. 点胶持续时间验证
    if (dispensing_duration_ms < 10 || dispensing_duration_ms > 5000) {
        return false;
    }

    // 3. 阀门开启延迟验证
    if (valve_open_delay_ms < 0 || valve_open_delay_ms > 1000) {
        return false;
    }

    // 4. 安全超时验证
    if (safety_timeout_ms < 100 || safety_timeout_ms > 60000) {
        return false;
    }

    // 5. 启用阀门数量验证
    if (enabled_valves < 1 || enabled_valves > 4) {
        return false;
    }

    // 6. ARC拆分角度验证
    if (arc_segmentation_max_degree < 1.0f || arc_segmentation_max_degree > 90.0f) {
        return false;
    }

    // 7. 弦高容差验证
    if (arc_chord_tolerance_mm < 0.001f || arc_chord_tolerance_mm > 1.0f) {
        return false;
    }

    return true;
}
```

**验证失败处理**:
```cpp
auto config_result = config_adapter->GetValveCoordinationConfig();
if (!config_result.IsSuccess()) {
    // 配置验证失败
    std::cerr << "配置错误: " << config_result.Error().Message() << std::endl;
    // 使用默认配置或提示用户修正
}
```

---

## 配置示例库

### 示例1: 标准配置(默认)

```ini
[ValveCoordination]
DispensingIntervalMs=1000
DispensingDurationMs=100
ValveOpenDelayMs=50
SafetyTimeoutMs=5000
EnabledValves=2
Valve1.Channel=0
Valve2.Channel=1
ArcSegmentationMaxDegree=30.0
ArcChordToleranceMm=0.1
```

**适用场景**: 一般点胶任务,精度要求适中

---

### 示例2: 高精度点胶

```ini
[ValveCoordination]
DispensingIntervalMs=500
DispensingDurationMs=80
ValveOpenDelayMs=30
SafetyTimeoutMs=3000
EnabledValves=1
Valve1.Channel=0
ArcSegmentationMaxDegree=15.0
ArcChordToleranceMm=0.05
```

**特点**:
- 密集点胶(500ms间隔)
- 精细ARC拆分(15°)
- 严格弦高容差(0.05mm)
- 单阀门精密控制

**适用场景**: 精密电子元件点胶,高精度要求

---

### 示例3: 快速生产模式

```ini
[ValveCoordination]
DispensingIntervalMs=2000
DispensingDurationMs=150
ValveOpenDelayMs=50
SafetyTimeoutMs=5000
EnabledValves=4
Valve1.Channel=0
Valve2.Channel=1
Valve3.Channel=2
Valve4.Channel=3
ArcSegmentationMaxDegree=45.0
ArcChordToleranceMm=0.2
```

**特点**:
- 稀疏点胶(2秒间隔)
- 粗略ARC拆分(45°)
- 宽松弦高容差(0.2mm)
- 4阀门并行,提高产量

**适用场景**: 大批量生产,效率优先

---

### 示例4: 慢速阀门补偿

```ini
[ValveCoordination]
DispensingIntervalMs=1500
DispensingDurationMs=200
ValveOpenDelayMs=100
SafetyTimeoutMs=8000
EnabledValves=2
Valve1.Channel=0
Valve2.Channel=1
ArcSegmentationMaxDegree=30.0
ArcChordToleranceMm=0.1
```

**特点**:
- 增大阀门开启延迟(100ms)补偿慢速响应
- 延长安全超时(8秒)避免误触发故障安全
- 延长点胶持续时间(200ms)确保充足胶量

**适用场景**: 使用慢速响应阀门

---

### 示例5: 高频软件触发

```ini
[ValveCoordination]
DispensingIntervalMs=50
DispensingDurationMs=20
ValveOpenDelayMs=10
SafetyTimeoutMs=2000
EnabledValves=2
Valve1.Channel=0
Valve2.Channel=1
ArcSegmentationMaxDegree=30.0
ArcChordToleranceMm=0.1
```

**特点**:
- 极密集点胶(50ms间隔 = 20Hz)
- 短持续时间(20ms)
- 快速阀门响应(10ms延迟)
- 短超时(2秒)

**适用场景**: 高频点胶,软件触发极限(±10ms精度)

**注意**: 软件触发受操作系统调度影响,如需更高精度请使用硬件CMP触发

---

### 示例6: 大文件优化

```ini
[ValveCoordination]
DispensingIntervalMs=1000
DispensingDurationMs=100
ValveOpenDelayMs=50
SafetyTimeoutMs=5000
EnabledValves=2
Valve1.Channel=0
Valve2.Channel=1
ArcSegmentationMaxDegree=60.0
ArcChordToleranceMm=0.3
```

**特点**:
- 粗略ARC拆分(60°),减少触发点数
- 宽松弦高容差(0.3mm),进一步减少点数
- 其他参数保持标准值

**适用场景**: DXF文件 > 1000段,避免触发点超限(50000)

**性能优化**:
```
标准配置: 1000段 → 约15000触发点 → 内存~30MB
优化配置: 1000段 → 约8000触发点  → 内存~15MB
```

---

## 参数调整指南

### 场景1: 点胶位置不准

**症状**: 实际点胶位置偏离预期

**调整策略**:
1. **减小 `ValveOpenDelayMs`** (如果点胶提前)
   ```ini
   ValveOpenDelayMs=30  # 从50减少到30
   ```

2. **增大 `ValveOpenDelayMs`** (如果点胶滞后)
   ```ini
   ValveOpenDelayMs=80  # 从50增大到80
   ```

3. **提高ARC拆分精度**
   ```ini
   ArcSegmentationMaxDegree=15.0  # 从30减小到15
   ArcChordToleranceMm=0.05       # 从0.1减小到0.05
   ```

---

### 场景2: 触发点数超限

**症状**: 警告 "触发点数超过上限(50000)"

**调整策略**:
1. **增大点胶间隔**
   ```ini
   DispensingIntervalMs=2000  # 从1000增大到2000
   ```

2. **粗略化ARC拆分**
   ```ini
   ArcSegmentationMaxDegree=45.0  # 从30增大到45
   ArcChordToleranceMm=0.2        # 从0.1增大到0.2
   ```

3. **拆分DXF文件** (最后手段)
   - 将复杂路径拆分为多个小文件
   - 分别执行

---

### 场景3: 胶量不一致

**症状**: 胶点大小不均匀

**可能原因**:
- `DispensingDurationMs` 不稳定
- 阀门响应不一致
- 压力波动

**调整策略**:
1. **延长点胶持续时间**
   ```ini
   DispensingDurationMs=150  # 从100增大到150
   ```

2. **增大阀门开启延迟**
   ```ini
   ValveOpenDelayMs=80  # 确保阀门完全打开
   ```

3. **检查供胶系统**
   - 确认供胶压力稳定
   - 检查胶水粘度

---

### 场景4: 作业速度过慢

**症状**: 生产效率低

**调整策略**:
1. **增大点胶间隔**
   ```ini
   DispensingIntervalMs=2000  # 减少点数,加快速度
   ```

2. **使用多阀门并行**
   ```ini
   EnabledValves=4  # 从2增大到4
   Valve3.Channel=2
   Valve4.Channel=3
   ```

3. **粗略化ARC拆分**
   ```ini
   ArcSegmentationMaxDegree=60.0
   ```

---

### 场景5: 内存占用过高

**症状**: 处理大文件时内存 > 100MB

**调整策略**:
1. **增大ARC拆分角度** (优先)
   ```ini
   ArcSegmentationMaxDegree=60.0  # 减少触发点数
   ```

2. **增大弦高容差**
   ```ini
   ArcChordToleranceMm=0.3
   ```

3. **启用异步处理** (如果支持)
   ```ini
   AsyncProcessing=true
   ```

---

## 配置文件调试

### 方法1: 单元测试验证

使用单元测试验证配置加载:
```bash
cd build
./tests/unit/test_ValveCoorduationConfig.exe
```

**预期输出**:
```
[==========] Running 7 tests from 1 test suite.
[----------] 7 tests from ValveCoordinationConfigTest
[ RUN      ] ValveCoordinationConfigTest.DefaultConfig_IsValid
[       OK ] ValveCoordinationConfigTest.DefaultConfig_IsValid
...
[  PASSED  ] 7 tests.
```

---

### 方法2: 配置文件日志检查

启用详细日志:
```cpp
// ConfigFileAdapter.cpp:LoadValveCoordinationSection
std::cout << "[ConfigFileAdapter] 阀门协调配置加载成功: "
          << "interval=" << config.dispensing_interval_ms << "ms, "
          << "duration=" << config.dispensing_duration_ms << "ms, "
          << "valves=" << config.enabled_valves << ", "
          << "arc_max_degree=" << config.arc_segmentation_max_degree << ", "
          << "arc_tolerance=" << config.arc_chord_tolerance_mm << "mm"
          << std::endl;
```

**日志输出示例**:
```
[ConfigFileAdapter] 阀门协调配置加载成功: interval=1000ms, duration=100ms, valves=2, arc_max_degree=30.000000, arc_tolerance=0.100000mm
```

---

### 方法3: 参数有效性检查

运行配置验证脚本:
```bash
./scripts/validate-config.sh
```

**检查项**:
- 参数范围有效性
- 参数类型正确性
- 通道冲突检查
- 逻辑一致性检查(如 `DispensingDurationMs < DispensingIntervalMs`)

---

## 相关文档

**用户指南**:
- [DXF文件规范](../user-guide/dxf-file-specification.md)
- [阀门协调操作指南](../user-guide/valve-coordination-operations.md)

**技术文档**:
- [配置类型定义](../../src/shared/types/ConfigTypes.h:395-498)
- [配置适配器实现](../../src/infrastructure/adapters/system/configuration/ConfigFileAdapter.h)
- [架构验证报告](../reports/architecture-validation-report-2026-01-08.md)

**测试文档**:
- [配置单元测试](../../tests/unit/config/test_ValveCoordinationConfig.cpp)
- [集成测试套件](../../tests/integration/parsing/test_DXFArcParsing.cpp)

---

## API参考

### C++ API

**加载配置**:
```cpp
#include "infrastructure/adapters/system/configuration/ConfigFileAdapter.h"

auto config_adapter = std::make_shared<ConfigFileAdapter>("path/to/machine_config.ini");
auto result = config_adapter->GetValveCoordinationConfig();

if (result.IsSuccess()) {
    const auto& config = result.Value();
    std::cout << "点胶间隔: " << config.dispensing_interval_ms << "ms" << std::endl;
} else {
    std::cerr << "配置错误: " << result.Error().Message() << std::endl;
}
```

**修改配置**:
```cpp
Shared::Types::ValveCoordinationConfig config;
config.dispensing_interval_ms = 500;
config.dispensing_duration_ms = 80;

if (!config.Validate()) {
    std::cerr << "配置验证失败: " << config.GetValidationError() << std::endl;
}
```

---

**文档版本**: 1.0.0
**最后更新**: 2026-01-08
**维护人员**: Backend Team
**适用版本**: siligen-motion-controller v0.5.0+

