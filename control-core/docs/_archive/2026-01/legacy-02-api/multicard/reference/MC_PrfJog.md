# MC_PrfJog 函数教程

## 函数原型

```c
int MC_PrfJog(short nAxis)
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

### 示例1：正向连续运动

```cpp
// 设置轴为JOG模式并执行正向连续运动
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

    // 设置运动速度（正向）
    iRes += g_MultiCard.MC_SetVel(iAxisNum, 50);

    // 更新轴参数，启动连续运动
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

    if(0 == iRes)
    {
        TRACE("正向连续移动2......\r\n");
    }
}
```

### 示例2：负向连续运动（按钮按下）
```cpp
// 在按钮按下时启动负向JOG运动
switch(pMsg->message)
{
case WM_LBUTTONDOWN:
    UpdateData(TRUE);

    // 负向运动
    if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd)
    {
        int iRes = 0;
        TJogPrm m_JogPrm;
        int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

        // 配置JOG参数
        m_JogPrm.dAcc = 0.1;
        m_JogPrm.dDec = 0.1;
        m_JogPrm.dSmooth = 0;

        // 设置JOG模式
        iRes = g_MultiCard.MC_PrfJog(iAxisNum);
        iRes += g_MultiCard.MC_SetJogPrm(iAxisNum, &m_JogPrm);
        iRes += g_MultiCard.MC_SetVel(iAxisNum, -20);  // 负向速度
        iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

        if(0 == iRes)
        {
            TRACE("负向连续移动......\r\n");
        }
    }
    break;
}
```

### 示例3：正向连续运动（按钮按下）
```cpp
// 在按钮按下时启动正向JOG运动
switch(pMsg->message)
{
case WM_LBUTTONDOWN:
    UpdateData(TRUE);

    // 正向运动
    if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_POS)->m_hWnd)
    {
        int iRes = 0;
        TJogPrm m_JogPrm;
        int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

        // 配置JOG参数
        m_JogPrm.dAcc = 0.1;
        m_JogPrm.dDec = 0.1;
        m_JogPrm.dSmooth = 0;

        // 设置JOG模式
        iRes = g_MultiCard.MC_PrfJog(iAxisNum);
        iRes += g_MultiCard.MC_SetJogPrm(iAxisNum, &m_JogPrm);
        iRes += g_MultiCard.MC_SetVel(iAxisNum, 20);   // 正向速度
        iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

        if(0 == iRes)
        {
            TRACE("正向连续移动......\r\n");
        }
    }
    break;
}
```


## 参数映射表

| 应用场景 | nAxis | 说明 |
|----------|-------|------|
| 界面控制 | m_ComboBoxAxis.GetCurSel()+1 | 用户选择的轴 |
| 正向运动 | 指定轴号 | 正向连续运动 |
| 负向运动 | 指定轴号 | 负向连续运动 |
| 按钮控制 | 指定轴号 | 通过按钮按下/松开控制运动 |

## 关键说明

1. **JOG运动特点**：
   - **连续运动**：按下按钮开始运动，松开停止
   - **实时控制**：可以随时改变速度和方向
   - **无固定目标**：运动到用户停止为止
   - **适合调试**：手动控制轴位置和速度

2. **使用步骤**：
   - **第一步**：调用MC_PrfJog()设置JOG模式
   - **第二步**：调用MC_SetJogPrm()设置运动参数
   - **第三步**：调用MC_SetVel()设置速度和方向
   - **第四步**：调用MC_Update()启动运动

3. **速度控制**：
   - **正向运动**：速度值为正数
   - **负向运动**：速度值为负数
   - **速度范围**：根据系统限制设置
   - **动态调整**：可以实时改变速度值

4. **应用场景**：
   - **手动调试**：手动调整轴位置
   - **设备对准**：精确对准操作
   - **速度测试**：测试不同速度下的性能
   - **教学演示**：演示运动控制效果

5. **注意事项**：
   - 需要配合停止指令（MC_Stop）
   - 注意限位保护和安全距离
   - 避免高速JOG运动
   - 确保有人工监控

## 函数区别

- **MC_PrfJog() vs MC_PrfTrap()**: MC_PrfJog()是连续运动，MC_PrfTrap()是点位运动
- **MC_PrfJog() vs MC_Update()**: MC_PrfJog()设置运动模式，MC_Update()启动运动
- **连续 vs 点位**: JOG适合手动控制，Trap适合自动定位

---

**安全建议**: JOG运动主要用于调试和手动操作，生产环境中建议使用自动化的点位运动模式。执行JOG运动时需要人工监控，注意安全和限位保护。