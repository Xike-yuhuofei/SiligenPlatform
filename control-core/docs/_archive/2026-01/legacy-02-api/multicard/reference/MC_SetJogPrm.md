# MC_SetJogPrm 函数教程

## 函数原型

```c
int MC_SetJogPrm(short nAxis, TJogPrm* pJogPrm)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nAxis | short | 轴号，取值范围：1-16 |
| pJogPrm | TJogPrm* | JOG运动参数结构体指针 |

### TJogPrm 结构体
```c
typedef struct {
    double dAcc;    // 加速度 (脉冲/毫秒²)
    double dDec;    // 减速度 (脉冲/毫秒²)
    double dSmooth; // 平滑系数 (0-1)
} TJogPrm;
```

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本JOG参数配置
```cpp
// 配置标准的JOG运动参数
void CDemoVCDlg::OnBnClickedButton5()
{
    int iRes = 0;
    TJogPrm m_JogPrm;
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号

    // 配置JOG运动参数
    m_JogPrm.dAcc = 0.1;    // 加速度：0.1 脉冲/毫秒²
    m_JogPrm.dDec = 0.1;    // 减速度：0.1 脉冲/毫秒²
    m_JogPrm.dSmooth = 0;   // 平滑系数：0（无平滑）

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

### 示例2：负向JOG运动参数配置
```cpp
// 在按钮按下时配置负向JOG运动
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
        iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

        if(0 == iRes)
        {
            TRACE("负向连续移动......\r\n");
        }
    }
    break;
```

### 示例3：正向JOG运动参数配置
```cpp
// 在按钮按下时配置正向JOG运动
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
        iRes += g_MultiCard.MC_SetVel(iAxisNum, 20);   // 正向速度
        iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

        if(0 == iRes)
        {
            TRACE("正向连续移动......\r\n");
        }
    }
    break;
```

### 示例4：平滑JOG运动参数配置
```cpp
// 配置平滑的JOG运动参数，减少机械冲击
bool ConfigureSmoothJogParams(int axis)
{
    int iRes = 0;
    TJogPrm jogPrm;

    // 平滑JOG配置：适当的加减速和较大的平滑系数
    jogPrm.dAcc = 0.05;     // 较小的加速度，软性启动
    jogPrm.dDec = 0.05;     // 较小的减速度，软性停止
    jogPrm.dSmooth = 0.5;   // 较大的平滑系数，减少冲击

    // 首先设置JOG模式
    iRes = g_MultiCard.MC_PrfJog(axis);
    if(iRes != 0)
    {
        TRACE("设置轴%dJOG模式失败\n", axis);
        return false;
    }

    // 设置平滑JOG参数
    iRes = g_MultiCard.MC_SetJogPrm(axis, &jogPrm);
    if(iRes == 0)
    {
        TRACE("轴%d平滑JOG参数配置成功\n", axis);
        return true;
    }
    else
    {
        TRACE("轴%d平滑JOG参数配置失败\n", axis);
        return false;
    }
}
```

### 示例5：快速响应JOG参数配置
```cpp
// 配置快速响应的JOG运动参数，提高响应速度
bool ConfigureFastJogParams(int axis)
{
    int iRes = 0;
    TJogPrm jogPrm;

    // 快速JOG配置：较大的加减速，无平滑
    jogPrm.dAcc = 0.5;      // 较大的加速度，快速启动
    jogPrm.dDec = 0.5;      // 较大的减速度，快速停止
    jogPrm.dSmooth = 0;     // 无平滑，保证响应速度

    // 设置JOG模式
    iRes = g_MultiCard.MC_PrfJog(axis);
    if(iRes != 0)
    {
        TRACE("设置轴%dJOG模式失败\n", axis);
        return false;
    }

    // 设置快速JOG参数
    iRes = g_MultiCard.MC_SetJogPrm(axis, &jogPrm);
    if(iRes == 0)
    {
        TRACE("轴%d快速JOG参数配置成功\n", axis);
        return true;
    }
    else
    {
        TRACE("轴%d快速JOG参数配置失败\n", axis);
        return false;
    }
}
```

## 参数映射表

| 应用类型 | dAcc | dDec | dSmooth | 说明 |
|----------|------|------|---------|------|
| 标准应用 | 0.1 | 0.1 | 0 | 平衡响应性和稳定性 |
| 平滑运动 | 0.05 | 0.05 | 0.5 | 减少机械冲击，适合精密设备 |
| 快速响应 | 0.5 | 0.5 | 0 | 提高响应速度，适合调试 |
| 软性操作 | 0.02 | 0.02 | 0.8 | 极软的运动，保护设备 |

## 关键说明

1. **加速度 (dAcc)**：
   - 决定JOG运动的启动快慢
   - 单位：脉冲/毫秒²
   - 较大值：快速响应，但可能产生冲击
   - 较小值：平缓启动，保护机械设备

2. **减速度 (dDec)**：
   - 决定JOG运动的停止快慢
   - 单位：脉冲/毫秒²
   - 较大值：快速停止，响应及时
   - 较小值：平缓停止，减少过冲

3. **平滑系数 (dSmooth)**：
   - 控制运动曲线的平滑程度
   - 取值范围：0-1
   - 0：无平滑，最快响应
   - 1：最大平滑，最柔和运动

4. **参数选择原则**：
   - **设备类型**：精密设备需要较小的加减速度
   - **负载情况**：重负载需要更软的参数
   - **应用场景**：调试需要快速响应，生产需要稳定
   - **机械特性**：刚度差的设备需要更多平滑

5. **使用注意事项**：
   - 必须先调用MC_PrfJog()设置JOG模式
   - 参数设置后需要调用MC_Update()生效
   - 平滑运动会增加响应延迟
   - 需要根据实际效果调整参数

## 函数区别

- **MC_SetJogPrm() vs MC_SetTrapPrm()**: 分别用于JOG和梯形运动模式
- **MC_SetJogPrm() vs MC_SetVel()**: 前者设置加减速参数，后者设置运动速度
- **dSmooth vs 0**: 有平滑时响应较慢但运动更柔和

---

**调试建议**: 根据实际机械特性调整参数。建议从较小的加减速度开始，逐步调整到最佳效果。平滑系数可以根据实际需要调整，在响应速度和运动平滑性之间找到平衡。