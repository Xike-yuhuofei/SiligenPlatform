# MC_Update 函数教程

## 函数原型

```c
int MC_Update(long lAxisMask)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| lAxisMask | long | 轴掩码。bit0对应轴1，bit1对应轴2，以此类推。对应的bit位为1表示更新该轴 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：单轴运动更新
```cpp
// 更新单个轴的运动参数并启动运动
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
    iRes += g_MultiCard.MC_SetVel(iAxisNum, 5);

    // 更新轴参数，启动运动（单轴掩码）
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
}
```

### 示例2：JOG运动更新
```cpp
// 更新轴的JOG运动参数并启动连续运动
void CDemoVCDlg::OnBnClickedButton5()
{
    int iRes = 0;
    TJogPrm m_JogPrm;
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号

    // 配置JOG运动参数
    m_JogPrm.dAcc = 0.1;    // 加速度
    m_JogPrm.dDec = 0.1;    // 减速度
    m_JogPrm.dSmooth = 0;   // 平滑系数

    // 设置JOG运动模式
    iRes = g_MultiCard.MC_PrfJog(iAxisNum);

    // 设置JOG运动参数
    iRes += g_MultiCard.MC_SetJogPrm(iAxisNum, &m_JogPrm);

    // 设置运动速度
    iRes += g_MultiCard.MC_SetVel(iAxisNum, 50);

    // 更新轴参数，启动连续运动
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

    if(0 == iRes)
    {
        TRACE("正向连续移动2......\r\n");
    }
}
```

### 示例3：负向JOG运动更新
```cpp
// 更新轴的负向JOG运动参数
case WM_LBUTTONDOWN:
    UpdateData(TRUE);

    // 负向运动
    if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd)
    {
        int iRes = 0;
        TJogPrm m_JogPrm;
        int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

        // 配置JOG参数
        m_JogPrm.dAcc = 0.1;    // 加速度
        m_JogPrm.dDec = 0.1;    // 减速度
        m_JogPrm.dSmooth = 0;   // 平滑系数

        // 设置JOG模式
        iRes = g_MultiCard.MC_PrfJog(iAxisNum);
        iRes += g_MultiCard.MC_SetJogPrm(iAxisNum, &m_JogPrm);
        iRes += g_MultiCard.MC_SetVel(iAxisNum, -20);  // 负向速度

        // 更新轴参数，启动运动
        iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

        if(0 == iRes)
        {
            TRACE("负向连续移动......\r\n");
        }
    }
    break;
```

### 示例4：多轴同步更新
```cpp
// 同时更新多个轴的运动参数并启动同步运动
bool ExecuteMultiAxisSyncMove()
{
    int axes[] = {1, 2, 3};           // 要同步运动的轴号
    long positions[] = {10000, 20000, 15000};  // 各轴目标位置
    double velocities[] = {10.0, 15.0, 12.0};   // 各轴速度
    int count = sizeof(axes) / sizeof(axes[0]);

    int totalResult = 0;
    int axisMask = 0;

    // 为每个轴配置运动参数
    for(int i = 0; i < count; i++)
    {
        int axis = axes[i];

        // 设置梯形运动模式
        totalResult += g_MultiCard.MC_PrfTrap(axis);

        // 配置梯形参数
        TTrapPrm trapPrm = {0.2, 0.2, 0, 0};
        totalResult += g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);

        // 设置目标位置和速度
        g_MultiCard.MC_SetPos(axis, positions[i]);
        g_MultiCard.MC_SetVel(axis, velocities[i]);

        // 计算轴掩码
        axisMask |= (0X0001 << (axis - 1));
    }

    // 一次性更新所有轴，实现同步启动
    totalResult += g_MultiCard.MC_Update(axisMask);

    if(totalResult == 0)
    {
        TRACE("多轴同步运动启动成功，轴掩码: 0x%X\n", axisMask);
        return true;
    }
    else
    {
        TRACE("多轴同步运动启动失败\n");
        return false;
    }
}
```

### 示例5：选择性轴更新
```cpp
// 根据条件选择性地更新特定轴
void ConditionalAxisUpdate()
{
    bool axisEnable[] = {true, false, true, false};  // 各轴的使能状态
    int axisMask = 0;

    // 计算要更新的轴掩码
    for(int i = 0; i < 4; i++)
    {
        if(axisEnable[i])
        {
            // 为使能的轴设置运动参数
            g_MultiCard.MC_PrfTrap(i+1);

            TTrapPrm trapPrm = {0.1, 0.1, 0, 0};
            g_MultiCard.MC_SetTrapPrm(i+1, &trapPrm);

            g_MultiCard.MC_SetPos(i+1, 5000 * (i+1));
            g_MultiCard.MC_SetVel(i+1, 5.0);

            // 设置对应的掩码位
            axisMask |= (0X0001 << i);
        }
    }

    // 更新使能的轴
    int iRes = g_MultiCard.MC_Update(axisMask);
    if(iRes == 0)
    {
        TRACE("选择性轴更新完成，轴掩码: 0x%X\n", axisMask);
    }
}
```

### 示例6：轴掩码计算辅助函数
```cpp
// 辅助函数：计算轴掩码
int CalculateAxisMask(int* axes, int count)
{
    int mask = 0;
    for(int i = 0; i < count; i++)
    {
        mask |= (0X0001 << (axes[i] - 1));
    }
    return mask;
}

// 使用辅助函数的多轴更新
bool UpdateMultipleAxes(int* axes, int count)
{
    int axisMask = CalculateAxisMask(axes, count);
    int totalResult = 0;

    // 配置所有轴的运动参数
    for(int i = 0; i < count; i++)
    {
        int axis = axes[i];
        totalResult += g_MultiCard.MC_PrfTrap(axis);
        // ... 其他参数设置 ...
    }

    // 一次性更新所有轴
    totalResult += g_MultiCard.MC_Update(axisMask);

    return (totalResult == 0);
}
```

## 参数映射表

| 应用场景 | lAxisMask | 说明 |
|----------|-----------|------|
| 单轴更新 | 0X0001 << (iAxisNum-1) | 更新单个轴 |
| 轴1更新 | 0X0001 | 只更新轴1 |
| 轴2更新 | 0X0002 | 只更新轴2 |
| 轴1+2更新 | 0X0003 | 同时更新轴1和轴2 |
| 前4轴更新 | 0X000F | 同时更新前4个轴 |
| 全部轴更新 | 0XFFFF | 同时更新所有16个轴 |

## 关键说明

1. **轴掩码计算**：
   - **bit0**：对应轴1（0X0001）
   - **bit1**：对应轴2（0X0002）
   - **bit2**：对应轴3（0X0004）
   - **bit(n-1)**：对应轴n（0X0001 << (n-1)）

2. **功能作用**：
   - **启动运动**：将配置的运动参数下发到控制卡
   - **参数生效**：使之前设置的位置、速度等参数生效
   - **同步控制**：可以同时启动多个轴的运动
   - **状态更新**：更新控制卡的轴配置状态

3. **使用时机**：
   - 必须在设置运动模式（MC_PrfTrap/MC_PrfJog）之后
   - 必须在设置运动参数（MC_SetPos/MC_SetVel等）之后
   - 在调用MC_Stop()停止运动后需要重新调用MC_Update()
   - 可以重复调用来更新参数

4. **注意事项**：
   - 调用前必须完成所有必要的参数设置
   - 掩码中未包含的轴不会更新
   - 更新后参数立即生效
   - 运动中的轴也可以更新参数

5. **多轴同步**：
   - 使用单个MC_Update()调用可以同步启动多个轴
   - 确保所有轴的参数都设置完成后再调用MC_Update()
   - 适用于需要精确同步的场合

6. **错误处理**：
   - 检查返回值确认更新是否成功
   - 失败时需要重新设置参数
   - 记录错误信息便于调试

## 函数区别

- **MC_Update() vs MC_Stop()**: MC_Update()启动运动，MC_Stop()停止运动
- **MC_Update() vs MC_SetPos()**: MC_Update()使参数生效，MC_SetPos()设置参数
- **单轴 vs 多轴**: 通过不同的轴掩码实现单轴或多轴更新

---

**使用建议**: 在多轴同步应用中，建议先设置完所有轴的参数，然后使用单个MC_Update()调用同时启动所有轴，确保同步性。对于需要实时调整参数的应用，可以在运动过程中重复调用MC_Update()来更新参数。