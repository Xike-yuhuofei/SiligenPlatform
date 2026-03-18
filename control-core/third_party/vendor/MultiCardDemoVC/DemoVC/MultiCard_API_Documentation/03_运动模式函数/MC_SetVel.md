# MC_SetVel 函数教程

## 函数原型

```c
int MC_SetVel(short nAxis, double dVel)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nAxis | short | 轴号，取值范围：1-16 |
| dVel | double | 速度值（脉冲/毫秒）。正值为正向，负值为负向 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：梯形运动速度设置
```cpp
// 为梯形运动设置中低速
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

    // 设置运动速度（中低速，精确定位）
    iRes = g_MultiCard.MC_SetVel(iAxisNum, 5);

    // 更新轴参数，启动运动
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
}
```

### 示例2：JOG正向高速运动
```cpp
// 为JOG运动设置较高的正向速度
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

    // 设置运动速度（正向高速）
    iRes += g_MultiCard.MC_SetVel(iAxisNum, 50);

    // 更新轴参数，启动连续运动
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

    if(0 == iRes)
    {
        TRACE("正向连续移动2......\r\n");
    }
}
```

### 示例3：JOG负向中速运动
```cpp
// 在按钮按下时启动负向中速JOG运动
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
        iRes += g_MultiCard.MC_SetVel(iAxisNum, -20);  // 负向中速

        iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

        if(0 == iRes)
        {
            TRACE("负向连续移动......\r\n");
        }
    }
    break;
```

### 示例4：JOG正向中速运动
```cpp
// 在按钮按下时启动正向中速JOG运动
case WM_LBUTTONDOWN:
    UpdateData(TRUE);

    // 正向运动
    if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_POS)->m_hWnd)
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
        iRes += g_MultiCard.MC_SetVel(iAxisNum, 20);   // 正向中速

        iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

        if(0 == iRes)
        {
            TRACE("正向连续移动......\r\n");
        }
    }
    break;
```

### 示例5：可变速度运动
```cpp
// 根据应用需求设置不同的运动速度
bool ExecuteVariableSpeedMove(int axis, long targetPos, double speed)
{
    int iRes = 0;

    // 速度安全检查
    const double MAX_SAFE_SPEED = 100.0;
    if(fabs(speed) > MAX_SAFE_SPEED)
    {
        TRACE("速度%.2f超出安全范围%.2f\n", speed, MAX_SAFE_SPEED);
        return false;
    }

    // 设置梯形运动模式
    iRes = g_MultiCard.MC_PrfTrap(axis);
    if(iRes != 0)
    {
        TRACE("设置轴%d梯形模式失败\n", axis);
        return false;
    }

    // 根据速度配置运动参数
    TTrapPrm trapPrm;
    if(fabs(speed) > 50.0)        // 高速运动
    {
        trapPrm.acc = 0.5;     // 大加速度
        trapPrm.dec = 0.5;     // 大减速度
        trapPrm.smoothTime = 0;
        trapPrm.velStart = 0;
    }
    else if(fabs(speed) > 20.0) // 中速运动
    {
        trapPrm.acc = 0.2;     // 中等加速度
        trapPrm.dec = 0.2;     // 中等减速度
        trapPrm.smoothTime = 5;
        trapPrm.velStart = 0;
    }
    else                        // 低速运动
    {
        trapPrm.acc = 0.1;     // 小加速度
        trapPrm.dec = 0.1;     // 小减速度
        trapPrm.smoothTime = 10;
        trapPrm.velStart = 0;
    }

    iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);
    if(iRes != 0)
    {
        TRACE("设置轴%d运动参数失败\n", axis);
        return false;
    }

    // 设置目标位置和速度
    g_MultiCard.MC_SetPos(axis, targetPos);
    g_MultiCard.MC_SetVel(axis, speed);

    // 启动运动
    iRes = g_MultiCard.MC_Update(0X0001 << (axis-1));
    if(iRes == 0)
    {
        TRACE("轴%d开始运动，速度: %.2f, 目标: %ld\n", axis, speed, targetPos);
        return true;
    }

    return false;
}
```

### 示例6：分段速度控制
```cpp
// 在长距离运动中使用分段速度控制
bool ExecuteSegmentedSpeedMove(int axis, long startPos, long endPos)
{
    long totalDistance = abs(endPos - startPos);
    int iRes = 0;

    // 设置梯形运动模式
    iRes = g_MultiCard.MC_PrfTrap(axis);
    if(iRes != 0) return false;

    // 配置运动参数
    TTrapPrm trapPrm = {0.2, 0.2, 0, 0};
    iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);
    if(iRes != 0) return false;

    // 根据距离选择速度
    double speed;
    if(totalDistance > 50000)       // 长距离：高速
    {
        speed = (endPos > startPos) ? 80.0 : -80.0;
    }
    else if(totalDistance > 10000)  // 中距离：中速
    {
        speed = (endPos > startPos) ? 30.0 : -30.0;
    }
    else                            // 短距离：低速
    {
        speed = (endPos > startPos) ? 10.0 : -10.0;
    }

    // 执行运动
    g_MultiCard.MC_SetPos(axis, endPos);
    g_MultiCard.MC_SetVel(axis, speed);
    iRes = g_MultiCard.MC_Update(0X0001 << (axis-1));

    if(iRes == 0)
    {
        TRACE("轴%d分段速度运动，距离: %ld, 速度: %.2f\n",
              axis, totalDistance, speed);
        return true;
    }

    return false;
}
```

## 参数映射表

| 应用场景 | nAxis | dVel | 说明 |
|----------|-------|------|------|
| 精确定位 | m_ComboBoxAxis.GetCurSel()+1 | 5 | 低速确保精度 |
| 快速JOG | 指定轴号 | 50 | 高速连续运动 |
| 中速JOG | 指定轴号 | 20/-20 | 中速正负向运动 |
| 高速运动 | 指定轴号 | 80 | 长距离高速运动 |
| 微调运动 | 指定轴号 | 1-3 | 精细位置调整 |

## 关键说明

1. **速度单位**：
   - 单位：脉冲/毫秒
   - 1脉冲/毫秒 = 1000脉冲/秒
   - 可根据机械传动比换算为实际速度

2. **方向控制**：
   - **正值**：正向运动
   - **负值**：负向运动
   - **零值**：停止运动（但不停止已启动的运动）

3. **速度选择原则**：
   - **负载重量**：重负载需要较低速度
   - **运动距离**：长距离可以使用较高速度
   - **定位精度**：高精度应用需要较低速度
   - **机械特性**：刚度差的系统需要较低速度

4. **与运动模式的关系**：
   - **梯形模式**：速度是最大速度，系统会自动加减速
   - **JOG模式**：速度是连续运动速度
   - 需要先调用MC_PrfTrap()或MC_PrfJog()

5. **注意事项**：
   - 速度设置需要调用MC_Update()才能生效
   - 过高速度可能导致振动或失步
   - 变速时需要先停止当前运动
   - 考虑加速度和减速度的配合

6. **安全考虑**：
   - 设置合理的速度上限
   - 考虑限位开关的响应时间
   - 高速运动时加强监控
   - 避免频繁的速度变化

## 函数区别

- **MC_SetVel() vs MC_SetPos()**: MC_SetVel()设置运动速度，MC_SetPos()设置目标位置
- **MC_SetVel() vs MC_SetTrapPrm()**: MC_SetVel()设置速度，MC_SetTrapPrm()设置加减速参数
- **正速度 vs 负速度**: 正值正向运动，负值负向运动

---

**使用建议**: 根据具体应用选择合适的速度。精确定位使用低速，长距离移动可使用较高速度。建议在实际应用中测试不同速度的效果，找到最佳的速度参数配置。