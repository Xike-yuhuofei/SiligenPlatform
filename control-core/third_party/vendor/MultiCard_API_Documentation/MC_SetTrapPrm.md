# MC_SetTrapPrm 函数教程

## 函数原型

```c
int MC_SetTrapPrm(short nAxis, TTrapPrm* pTrapPrm)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nAxis | short | 轴号，取值范围：1-16 |
| pTrapPrm | TTrapPrm* | 梯形运动参数结构体指针 |

### TTrapPrm 结构体
```c
typedef struct {
    double acc;        // 加速度 (脉冲/毫秒²)
    double dec;        // 减速度 (脉冲/毫秒²)
    double smoothTime; // 平滑时间 (毫秒)
    double velStart;   // 起始速度 (脉冲/毫秒)
} TTrapPrm;
```

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本梯形参数配置
```cpp
// 配置标准的梯形运动参数
void ConfigureBasicTrapParameters(int axis)
{
    int iRes = 0;
    TTrapPrm TrapPrm;

    // 设置梯形参数
    TrapPrm.acc = 0.1;        // 加速度：0.1 脉冲/毫秒²
    TrapPrm.dec = 0.1;        // 减速度：0.1 脉冲/毫秒²
    TrapPrm.smoothTime = 0;   // 平滑时间：0毫秒（无平滑）
    TrapPrm.velStart = 0;     // 起始速度：0 脉冲/毫秒

    // 设置梯形运动参数
    iRes = g_MultiCard.MC_SetTrapPrm(axis, &TrapPrm);

    if(iRes == 0)
    {
        TRACE("轴%d梯形参数设置成功\n", axis);
    }
    else
    {
        TRACE("轴%d梯形参数设置失败\n", axis);
    }
}
```

### 示例2：高精度应用参数配置
```cpp
// 为高精度应用配置优化的梯形参数
bool ConfigureHighPrecisionTrapParams(int axis)
{
    TTrapPrm trapPrm;

    // 高精度配置：较小的加速度，保证稳定性
    trapPrm.acc = 0.05;       // 较小的加速度，减少振动
    trapPrm.dec = 0.05;       // 较小的减速度，提高定位精度
    trapPrm.smoothTime = 10;  // 10ms平滑时间，减少冲击
    trapPrm.velStart = 0;     // 从静止开始

    int iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);
    if(iRes == 0)
    {
        TRACE("轴%d高精度梯形参数配置成功\n", axis);
        return true;
    }

    TRACE("轴%d高精度梯形参数配置失败\n", axis);
    return false;
}
```

### 示例3：快速应用参数配置
```cpp
// 为快速应用配置高效率的梯形参数
bool ConfigureFastTrapParams(int axis)
{
    TTrapPrm trapPrm;

    // 快速配置：较大的加速度，提高效率
    trapPrm.acc = 0.5;        // 较大的加速度，快速启动
    trapPrm.dec = 0.5;        // 较大的减速度，快速停止
    trapPrm.smoothTime = 0;   // 无平滑，保证响应速度
    trapPrm.velStart = 0;     // 从静止开始

    int iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);
    if(iRes == 0)
    {
        TRACE("轴%d快速梯形参数配置成功\n", axis);
        return true;
    }

    TRACE("轴%d快速梯形参数配置失败\n", axis);
    return false;
}
```

### 示例4：软性启动参数配置
```cpp
// 配置软性启动参数，减少机械冲击
void ConfigureSoftStartTrapParams(int axis)
{
    TTrapPrm trapPrm;

    // 软性启动配置：较小的加速度，适当的平滑时间
    trapPrm.acc = 0.02;       // 非常小的加速度，软性启动
    trapPrm.dec = 0.1;        // 正常减速度，及时停止
    trapPrm.smoothTime = 50;  // 50ms平滑时间，减少冲击
    trapPrm.velStart = 0;     // 从静止开始

    int iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);

    if(iRes == 0)
    {
        TRACE("轴%d软性启动参数配置成功\n", axis);
    }
    else
    {
        TRACE("轴%d软性启动参数配置失败\n", axis);
    }
}
```

## 参数映射表

| 应用类型 | acc | dec | smoothTime | velStart | 说明 |
|----------|-----|-----|------------|----------|------|
| 标准应用 | 0.1 | 0.1 | 0 | 0 | 平衡效率和精度 |
| 高精度 | 0.05 | 0.05 | 10 | 0 | 优先考虑精度和稳定性 |
| 快速应用 | 0.5 | 0.5 | 0 | 0 | 优先考虑响应速度 |
| 软性启动 | 0.02 | 0.1 | 50 | 0 | 减少机械冲击和振动 |

## 关键说明

1. **加速度 (acc)**：
   - 决定从起始速度加速到目标速度的快慢
   - 单位：脉冲/毫秒²
   - 较大值：快速启动，但可能增加振动
   - 较小值：平缓启动，提高精度

2. **减速度 (dec)**：
   - 决定从运动速度减速到停止的快慢
   - 单位：脉冲/毫秒²
   - 较大值：快速停止，但可能过冲
   - 较小值：平缓停止，提高定位精度

3. **平滑时间 (smoothTime)**：
   - 在加速度变化时的平滑过渡时间
   - 单位：毫秒
   - 用于减少机械冲击和振动
   - 过大会影响响应速度

4. **起始速度 (velStart)**：
   - 运动开始时的初始速度
   - 单位：脉冲/毫秒
   - 通常设为0，从静止开始运动
   - 可用于连续运动的平滑过渡

5. **参数选择原则**：
   - **负载重量**：重负载需要较小的加速度
   - **机械刚度**：刚度差的系统需要平滑时间
   - **定位精度**：高精度应用需要较小的加减速度
   - **运动速度**：高速运动需要适当增加加减速度

## 函数区别

- **MC_SetTrapPrm() vs MC_SetJogPrm()**: 分别用于梯形和JOG模式
- **MC_SetTrapPrm() vs MC_SetPos()**: 前者设置运动曲线，后者设置目标位置
- **acc vs dec**: 加速度影响启动，减速度影响停止精度

---

**调试建议**: 根据实际机械特性和应用需求调整参数。建议从较小值开始，逐步调整到最佳效果。