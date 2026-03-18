# MC_GetAllSysStatusSX 函数教程

## 函数原型

```c
int MC_GetAllSysStatusSX(TAllSysStatusDataSX* pAllSysStatusData)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| pAllSysStatusData | TAllSysStatusDataSX* | 系统状态数据结构体指针 |

### TAllSysStatusDataSX 结构体主要成员
```c
typedef struct {
    long lAxisPrfPos[16];      // 各轴规划位置
    long lAxisEncPos[16];      // 各轴编码器位置
    long lAxisStatus[16];      // 各轴状态
    long lGpiRaw[16];          // 通用输入原始状态
    long lLimitNegRaw;         // 负限位原始状态
    long lLimitPosRaw;         // 正限位原始状态
    long lMPG;                 // 手轮状态
    // ... 其他状态数据
} TAllSysStatusDataSX;
```

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本系统状态读取
```cpp
// 在定时器中定期读取系统状态
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    int iRes = 0;
    unsigned long lValue = 0;
    CString strText;

    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;
    int iCardIndex = m_ComboBoxCardSel.GetCurSel();

    TAllSysStatusDataSX m_AllSysStatusDataTemp;

    // 读取板卡状态数据
    iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_AllSysStatusDataTemp);

    // 显示规划位置
    strText.Format("%d", m_AllSysStatusDataTemp.lAxisPrfPos[iAxisNum-1]);
    GetDlgItem(IDC_STATIC_PRF_POS)->SetWindowText(strText);

    // 显示编码器位置
    strText.Format("%d", m_AllSysStatusDataTemp.lAxisEncPos[iAxisNum-1]);
    GetDlgItem(IDC_STATIC_ENC_POS)->SetWindowText(strText);

    CDialogEx::OnTimer(nIDEvent);
}
```

### 示例2：轴状态监控
```cpp
// 监控轴的运行状态
void MonitorAxisStatus()
{
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if(iRes == 0)
    {
        // 检查所有轴的状态
        for(int i = 0; i < 16; i++)
        {
            long axisStatus = statusData.lAxisStatus[i];

            // 检查报警状态
            if(axisStatus & AXIS_STATUS_SV_ALARM)
            {
                TRACE("轴%d伺服报警！\n", i+1);
                HandleServoAlarm(i+1);
            }

            // 检查运行状态
            if(axisStatus & AXIS_STATUS_RUNNING)
            {
                TRACE("轴%d正在运行\n", i+1);
            }

            // 检查限位状态
            if(axisStatus & AXIS_STATUS_POS_HARD_LIMIT)
            {
                TRACE("轴%d正向硬限位触发\n", i+1);
            }

            if(axisStatus & AXIS_STATUS_NEG_HARD_LIMIT)
            {
                TRACE("轴%d负向硬限位触发\n", i+1);
            }

            // 检查回零状态
            if(axisStatus & AXIS_STATUS_HOME_RUNNING)
            {
                TRACE("轴%d正在回零\n", i+1);
            }
        }
    }
    else
    {
        TRACE("读取系统状态失败\n");
    }
}
```

### 示例3：通用输入状态处理
```cpp
// 处理通用输入状态并更新界面
void ProcessGeneralInputs()
{
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if(iRes == 0)
    {
        long gpiStatus = statusData.lGpiRaw[0];  // 获取第一组输入状态

        // 处理16个通用输入
        for(int i = 0; i < 16; i++)
        {
            bool inputState = (gpiStatus & (0X0001 << i)) != 0;

            // 更新对应的界面控件
            switch(i)
            {
            case 0:
                ((CButton*)GetDlgItem(IDC_RADIO_INPUT_1))->SetCheck(inputState);
                break;
            case 1:
                ((CButton*)GetDlgItem(IDC_RADIO_INPUT_2))->SetCheck(inputState);
                break;
            case 2:
                ((CButton*)GetDlgItem(IDC_RADIO_INPUT_3))->SetCheck(inputState);
                break;
            // ... 其他输入位
            case 15:
                ((CButton*)GetDlgItem(IDC_RADIO_INPUT_16))->SetCheck(inputState);
                break;
            }
        }
    }
}
```

### 示例4：限位状态监控
```cpp
// 监控限位状态并更新显示
void MonitorLimitStatus()
{
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if(iRes == 0)
    {
        // 处理负限位状态
        long negLimitStatus = statusData.lLimitNegRaw;
        for(int i = 0; i < 8; i++)  // 假设显示前8轴
        {
            bool limitTriggered = (negLimitStatus & (0X0001 << i)) != 0;
            ((CButton*)GetDlgItem(IDC_RADIO_LIMN_1 + i))->SetCheck(limitTriggered);
        }

        // 处理正限位状态
        long posLimitStatus = statusData.lLimitPosRaw;
        for(int i = 0; i < 8; i++)  // 假设显示前8轴
        {
            bool limitTriggered = (posLimitStatus & (0X0001 << i)) != 0;
            ((CButton*)GetDlgItem(IDC_RADIO_LIMP_1 + i))->SetCheck(limitTriggered);
        }
    }
}
```

### 示例5：手轮状态处理
```cpp
// 处理手轮输入状态
void ProcessHandwheelStatus()
{
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if(iRes == 0)
    {
        long mpgStatus = statusData.lMPG;

        // 处理轴选择信号
        if(mpgStatus & 0X0001)      // X轴选择
        {
            ((CButton*)GetDlgItem(IDC_RADIO_X))->SetCheck(true);
            HandleHandwheelAxisSelect(1);
        }
        else
        {
            ((CButton*)GetDlgItem(IDC_RADIO_X))->SetCheck(false);
        }

        if(mpgStatus & 0X0002)      // Y轴选择
        {
            ((CButton*)GetDlgItem(IDC_RADIO_Y))->SetCheck(true);
            HandleHandwheelAxisSelect(2);
        }
        else
        {
            ((CButton*)GetDlgItem(IDC_RADIO_Y))->SetCheck(false);
        }

        // 处理倍率信号
        bool x1Selected = (mpgStatus & 0X0020) != 0;
        bool x10Selected = (mpgStatus & 0X0040) != 0;
        bool x100Selected = !(x1Selected || x10Selected);  // 默认x100

        ((CButton*)GetDlgItem(IDC_RADIO_X1))->SetCheck(x1Selected);
        ((CButton*)GetDlgItem(IDC_RADIO_X10))->SetCheck(x10Selected);
        ((CButton*)GetDlgItem(IDC_RADIO_X100))->SetCheck(x100Selected);

        // 设置手轮倍率
        if(x1Selected) SetHandwheelRatio(1, 1);
        else if(x10Selected) SetHandwheelRatio(1, 10);
        else SetHandwheelRatio(1, 100);
    }
}
```

### 示例6：完整状态监控
```cpp
// 综合的系统状态监控函数
void ComprehensiveStatusMonitor()
{
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if(iRes != 0)
    {
        TRACE("无法读取系统状态数据\n");
        return;
    }

    // 1. 更新位置显示
    for(int i = 0; i < 16; i++)
    {
        TRACE("轴%d - 规划位置: %ld, 编码器位置: %ld\n",
              i+1, statusData.lAxisPrfPos[i], statusData.lAxisEncPos[i]);
    }

    // 2. 检查报警状态
    int alarmCount = 0;
    for(int i = 0; i < 16; i++)
    {
        if(statusData.lAxisStatus[i] & AXIS_STATUS_SV_ALARM)
        {
            TRACE("警告：轴%d伺服报警\n", i+1);
            alarmCount++;
        }
    }

    if(alarmCount > 0)
    {
        HandleSystemAlarm(alarmCount);
    }

    // 3. 检查运行轴数量
    int runningAxisCount = 0;
    for(int i = 0; i < 16; i++)
    {
        if(statusData.lAxisStatus[i] & AXIS_STATUS_RUNNING)
        {
            runningAxisCount++;
        }
    }
    TRACE("当前运行轴数量: %d\n", runningAxisCount);

    // 4. 检查限位状态
    int limitTriggerCount = 0;
    for(int i = 0; i < 16; i++)
    {
        if(statusData.lAxisStatus[i] & (AXIS_STATUS_POS_HARD_LIMIT | AXIS_STATUS_NEG_HARD_LIMIT))
        {
            limitTriggerCount++;
        }
    }
    if(limitTriggerCount > 0)
    {
        TRACE("警告：有%d个轴触发限位\n", limitTriggerCount);
    }

    // 5. 更新系统状态指示器
    UpdateSystemStatusIndicators(&statusData);
}
```

## 参数映射表

| 应用场景 | pAllSysStatusData | 说明 |
|----------|------------------|------|
| 定时器更新 | m_AllSysStatusDataTemp | 定期刷新界面状态 |
| 状态监控 | 局部变量 | 监控轴和输入状态 |
| 故障诊断 | 临时变量 | 分析系统状态问题 |
| 数据记录 | 全局变量 | 记录状态变化历史 |

## 关键说明

1. **结构体内容**：
   - **位置信息**：规划位置、编码器位置
   - **状态信息**：轴状态、报警状态
   - **输入信息**：通用输入、限位输入
   - **手轮信息**：手轮轴选择和倍率

2. **数据完整性**：
   - 一次调用获取所有系统状态
   - 保证数据的时间一致性
   - 包含16个轴的完整信息

3. **使用频率**：
   - 建议在定时器中定期调用（如100ms）
   - 避免过于频繁的调用
   - 在状态变化时及时调用

4. **状态位含义**：
   - **AXIS_STATUS_SV_ALARM**：伺服报警
   - **AXIS_STATUS_RUNNING**：轴正在运行
   - **AXIS_STATUS_POS_HARD_LIMIT**：正向硬限位
   - **AXIS_STATUS_NEG_HARD_LIMIT**：负向硬限位
   - **AXIS_STATUS_HOME_RUNNING**：正在回零

5. **性能考虑**：
   - 数据量较大，避免不必要的频繁调用
   - 可以选择性更新需要的状态信息
   - 考虑多线程访问的安全性

6. **错误处理**：
   - 检查返回值确认读取成功
   - 处理读取失败的情况
   - 提供默认值或保持上次状态

## 函数区别

- **MC_GetAllSysStatusSX() vs 单个状态读取**: 一次性获取所有状态 vs 分别获取单项状态
- **MC_GetAllSysStatusSX() vs MC_GetCrdStatus()**: 系统状态 vs 坐标系状态
- **SX版本 vs 普通版本**: SX版本通常包含更完整的状态信息

---

**使用建议**: 在定时器中调用此函数更新界面状态，但要注意调用频率，避免过度占用CPU资源。对于只需要部分状态信息的应用，可以考虑使用更专门的状态读取函数以提高效率。