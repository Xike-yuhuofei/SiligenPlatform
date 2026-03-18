# MC_PrfTrap 函数教程

## 函数原型

```c
int MC_PrfTrap(short nAxis)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nAxis | short | 轴号，取值范围：1-16 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本梯形速度曲线运动
```cpp
// 设置轴为梯形速度曲线模式并执行点位运动
void CDemoVCDlg::OnBnClickedButton6()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号
    UpdateData(true);  // 更新界面数据

    // 配置梯形速度曲线参数
    TTrapPrm TrapPrm;
    TrapPrm.acc = 0.1;        // 加速度
    TrapPrm.dec = 0.1;        // 减速度
    TrapPrm.smoothTime = 0;   // 平滑时间
    TrapPrm.velStart = 0;     // 起始速度

    // 设置梯形速度曲线模式
    iRes = g_MultiCard.MC_PrfTrap(iAxisNum);

    // 设置梯形运动参数
    iRes += g_MultiCard.MC_SetTrapPrm(iAxisNum, &TrapPrm);

    // 设置目标位置
    iRes += g_MultiCard.MC_SetPos(iAxisNum, m_lTargetPos);

    // 设置运动速度
    iRes = g_MultiCard.MC_SetVel(iAxisNum, 5);

    // 更新轴参数，启动运动
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
}
```

### 示例2：高精度点位运动
```cpp
// 执行高精度的点位定位运动
bool ExecutePrecisionTrapMove(int axis, long targetPos, double vel, double acc)
{
    int iRes = 0;

    // 设置梯形速度曲线模式
    iRes = g_MultiCard.MC_PrfTrap(axis);
    if(iRes != 0)
    {
        TRACE("设置轴%d梯形模式失败\n", axis);
        return false;
    }

    // 配置梯形参数
    TTrapPrm trapPrm;
    trapPrm.acc = acc;           // 设置加速度
    trapPrm.dec = acc;           // 减速度等于加速度
    trapPrm.smoothTime = 0;      // 无平滑时间，保证精度
    trapPrm.velStart = 0;        // 从静止开始

    iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);
    if(iRes != 0)
    {
        TRACE("设置轴%d梯形参数失败\n", axis);
        return false;
    }

    // 设置运动参数
    g_MultiCard.MC_SetPos(axis, targetPos);
    g_MultiCard.MC_SetVel(axis, vel);

    // 启动运动
    iRes = g_MultiCard.MC_Update(0X0001 << (axis-1));
    if(iRes == 0)
    {
        TRACE("轴%d梯形运动启动成功，目标位置: %ld\n", axis, targetPos);
        return true;
    }

    return false;
}
```

### 示例3：批量轴点位运动
```cpp
// 多轴同时执行梯形速度曲线点位运动
void ExecuteMultiAxisTrapMove()
{
    int axes[] = {1, 2, 3};  // 要运动的轴号
    long positions[] = {10000, 20000, 15000};  // 各轴目标位置
    int count = sizeof(axes) / sizeof(axes[0]);

    int totalResult = 0;
    int axisMask = 0;

    // 为每个轴配置梯形模式
    for(int i = 0; i < count; i++)
    {
        int axis = axes[i];

        // 设置梯形速度曲线模式
        totalResult += g_MultiCard.MC_PrfTrap(axis);

        // 配置梯形参数
        TTrapPrm trapPrm;
        trapPrm.acc = 0.2;
        trapPrm.dec = 0.2;
        trapPrm.smoothTime = 0.01;  // 小量平滑
        trapPrm.velStart = 0;

        totalResult += g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);

        // 设置目标位置和速度
        g_MultiCard.MC_SetPos(axis, positions[i]);
        g_MultiCard.MC_SetVel(axis, 10.0);

        // 计算轴掩码
        axisMask |= (0X0001 << (axis - 1));
    }

    // 同时启动所有轴的运动
    totalResult += g_MultiCard.MC_Update(axisMask);

    if(totalResult == 0)
    {
        TRACE("多轴梯形运动启动成功\n");
    }
    else
    {
        TRACE("多轴梯形运动启动失败\n");
    }
}
```

## 参数映射表

| 应用场景 | nAxis | 说明 |
|----------|-------|------|
| 单轴运动 | m_ComboBoxAxis.GetCurSel()+1 | 界面选择的轴 |
| 精确定位 | 指定轴号 | 高精度定位应用 |
| 批量运动 | 数组元素 | 多轴同步运动 |
| 固定轴 | 常量 | 程序中固定使用的轴 |

## 关键说明

1. **梯形速度曲线特点**：
   - 加速阶段：按设定的加速度从起始速度加速到最大速度
   - 匀速阶段：以设定的最大速度匀速运动
   - 减速阶段：按设定的减速度减速到停止速度
   - 适合点位定位和精确控制

2. **使用步骤**：
   - **第一步**：调用MC_PrfTrap()设置梯形模式
   - **第二步**：调用MC_SetTrapPrm()设置运动参数
   - **第三步**：调用MC_SetPos()设置目标位置
   - **第四步**：调用MC_SetVel()设置最大速度
   - **第五步**：调用MC_Update()启动运动

3. **应用场景**：
   - **精确定位**：需要准确到达目标位置
   - **点位运动**：点对点的位置控制
   - **自动装配**：精密的装配操作
   - **质量检测**：检测位置的精确定位

4. **注意事项**：
   - 需要先配置梯形参数才能启动运动
   - 运动精度取决于加速度和速度参数
   - 平滑时间会影响运动的平滑性
   - 适合中低速的高精度应用

## 函数区别

- **MC_PrfTrap() vs MC_PrfJog()**: MC_PrfTrap()是梯形点位运动，MC_PrfJog()是连续运动
- **MC_PrfTrap() vs MC_Update()**: MC_PrfTrap()设置运动模式，MC_Update()启动运动
- **MC_PrfTrap() vs 插补运动**: MC_PrfTrap()是单轴运动，插补是多轴协调运动

---

**使用建议**: 梯形速度曲线模式最适合需要精确定位的应用，如装配、检测、定位等场景。通过调整加速度和速度参数可以优化运动效率和精度。