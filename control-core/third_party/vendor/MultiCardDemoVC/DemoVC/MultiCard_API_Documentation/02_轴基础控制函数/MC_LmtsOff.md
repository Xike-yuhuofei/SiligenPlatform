# MC_LmtsOff 函数教程

## 函数原型

```c
int MC_LmtsOff(short nAxis, short nLimitType)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nAxis | short | 轴号，取值范围：1-16 |
| nLimitType | short | 限位类型。MC_LIMIT_POSITIVE：正向限位；MC_LIMIT_NEGATIVE：负向限位 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：关闭正向限位开关
```cpp
// 通过复选框关闭正向限位开关
void CDemoVCDlg::OnBnClickedCheckPosLimEnable()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号

    // 正向限位控制
    if(((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->GetCheck())
    {
        iRes = g_MultiCard.MC_LmtsOn(iAxisNum, MC_LIMIT_POSITIVE);
        ((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(true);
    }
    else
    {
        iRes = g_MultiCard.MC_LmtsOff(iAxisNum, MC_LIMIT_POSITIVE);  // 关闭正向限位
        ((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(false);
    }
}
```

### 示例2：关闭负向限位开关
```cpp
// 通过复选框关闭负向限位开关
void CDemoVCDlg::OnBnClickedCheckNegLimEnable()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号

    // 负向限位控制
    if(((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->GetCheck())
    {
        iRes = g_MultiCard.MC_LmtsOn(iAxisNum, MC_LIMIT_NEGATIVE);
        ((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(true);
    }
    else
    {
        iRes = g_MultiCard.MC_LmtsOff(iAxisNum, MC_LIMIT_NEGATIVE);  // 关闭负向限位
        ((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(false);
    }
}
```

### 示例3：调试模式下关闭所有限位
```cpp
// 在调试模式下关闭轴的双向限位以进行全行程测试
void DisableLimitsForDebugging(int axis)
{
    TRACE("警告：关闭轴%d的双向限位，进入调试模式\n", axis);

    // 关闭正向限位
    int iRes1 = g_MultiCard.MC_LmtsOff(axis, MC_LIMIT_POSITIVE);
    if(iRes1 == 0)
    {
        TRACE("轴%d正向限位已关闭\n", axis);
    }
    else
    {
        TRACE("轴%d正向限位关闭失败\n", axis);
    }

    // 关闭负向限位
    int iRes2 = g_MultiCard.MC_LmtsOff(axis, MC_LIMIT_NEGATIVE);
    if(iRes2 == 0)
    {
        TRACE("轴%d负向限位已关闭\n", axis);
    }
    else
    {
        TRACE("轴%d负向限位关闭失败\n", axis);
    }

    if(iRes1 == 0 && iRes2 == 0)
    {
        // 设置调试模式标志
        isDebugMode = true;
        MessageBox("限位已关闭，请注意安全！", "调试模式", MB_ICONWARNING);
    }
}
```

### 示例4：维护时临时关闭限位
```cpp
// 在设备维护时临时关闭限位以便进行机械调整
void TemporaryDisableLimitsForMaintenance()
{
    int maintenanceAxes[] = {1, 2, 3};  // 需要维护的轴
    int count = sizeof(maintenanceAxes) / sizeof(maintenanceAxes[0]);

    // 确认用户了解风险
    int result = MessageBox("维护模式将关闭限位保护，确认继续吗？",
                           "维护模式确认", MB_YESNO | MB_ICONWARNING);
    if(result != IDYES)
    {
        return;
    }

    for(int i = 0; i < count; i++)
    {
        int axis = maintenanceAxes[i];

        // 关闭双向限位
        g_MultiCard.MC_LmtsOff(axis, MC_LIMIT_POSITIVE);
        g_MultiCard.MC_LmtsOff(axis, MC_LIMIT_NEGATIVE);

        TRACE("轴%d限位已关闭，可进行维护操作\n", axis);
    }

    // 记录维护开始时间
    maintenanceStartTime = CTime::GetCurrentTime();
    isMaintenanceMode = true;
}
```

### 示例5：维护完成后重新启用限位
```cpp
// 维护完成后重新启用所有限位保护
void ReenableLimitsAfterMaintenance()
{
    if(!isMaintenanceMode)
    {
        return;  // 不在维护模式，无需操作
    }

    int maintenanceAxes[] = {1, 2, 3};
    int count = sizeof(maintenanceAxes) / sizeof(maintenanceAxes[0]);

    for(int i = 0; i < count; i++)
    {
        int axis = maintenanceAxes[i];

        // 重新启用双向限位
        int iRes1 = g_MultiCard.MC_LmtsOn(axis, MC_LIMIT_POSITIVE);
        int iRes2 = g_MultiCard.MC_LmtsOn(axis, MC_LIMIT_NEGATIVE);

        if(iRes1 == 0 && iRes2 == 0)
        {
            TRACE("轴%d限位保护已重新启用\n", axis);
        }
        else
        {
            TRACE("警告：轴%d限位重新启用失败\n", axis);
        }
    }

    isMaintenanceMode = false;
    MessageBox("限位保护已重新启用，系统恢复正常", "维护完成", MB_ICONINFORMATION);
}
```

## 参数映射表

| 应用场景 | nAxis | nLimitType | 说明 |
|----------|-------|------------|------|
| 界面控制 | m_ComboBoxAxis.GetCurSel()+1 | MC_LIMIT_POSITIVE | 用户关闭正向限位 |
| 界面控制 | m_ComboBoxAxis.GetCurSel()+1 | MC_LIMIT_NEGATIVE | 用户关闭负向限位 |
| 调试模式 | 指定轴号 | 两种类型都调用 | 全行程测试 |
| 维护模式 | 数组元素 | 两种类型都调用 | 机械调整需要 |
| 恢复保护 | 维护轴号 | 两种类型都调用 | 维护后重新启用 |

## 关键说明

1. **关闭效果**：
   - 轴不再检测限位开关信号
   - 失去硬件限位保护
   - 可能导致机械超程
   - 增加操作风险

2. **使用时机**：
   - **调试测试**：需要全行程范围测试时
   - **设备维护**：机械调整和维修期间
   - **特殊应用**：某些不需要限位的应用
   - **限位故障**：限位开关故障时临时关闭

3. **安全考虑**：
   - 关闭限位会失去重要安全保护
   - 需要其他安全措施补偿
   - 操作人员需要特别小心
   - 建议在受控环境下使用

4. **操作规范**：
   - 关闭前确认操作必要性
   - 关闭期间加强监控
   - 完成后立即重新启用
   - 记录关闭和启用时间

## 函数区别

- **MC_LmtsOff() vs MC_LmtsOn()**: MC_LmtsOff()禁用限位，MC_LmtsOn()启用限位
- **MC_LmtsOff() vs MC_Stop()**: MC_LmtsOff()关闭保护，MC_Stop()停止运动
- **临时关闭 vs 永久关闭**: MC_LmtsOff()是临时关闭，可随时重新启用

---

**安全警告**: 关闭限位保护会显著增加操作风险，仅在必要时使用，并确保有其他适当的安全措施。操作人员需要充分了解风险并采取预防措施。