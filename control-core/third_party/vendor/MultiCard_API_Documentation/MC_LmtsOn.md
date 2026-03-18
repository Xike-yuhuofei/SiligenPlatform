# MC_LmtsOn 函数教程

## 函数原型

```c
int MC_LmtsOn(short nAxis, short nLimitType)
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

### 示例1：正向限位开关控制
```cpp
// 通过复选框控制正向限位开关
void CDemoVCDlg::OnBnClickedCheckPosLimEnable()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号

    // 正向限位
    if(((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->GetCheck())
    {
        iRes = g_MultiCard.MC_LmtsOn(iAxisNum, MC_LIMIT_POSITIVE);
        ((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(true);
    }
    else
    {
        iRes = g_MultiCard.MC_LmtsOff(iAxisNum, MC_LIMIT_POSITIVE);
        ((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(false);
    }
}
```

### 示例2：负向限位开关控制
```cpp
// 通过复选框控制负向限位开关
void CDemoVCDlg::OnBnClickedCheckNegLimEnable()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号

    // 负向限位
    if(((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->GetCheck())
    {
        iRes = g_MultiCard.MC_LmtsOn(iAxisNum, MC_LIMIT_NEGATIVE);
        ((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(true);
    }
    else
    {
        iRes = g_MultiCard.MC_LmtsOff(iAxisNum, MC_LIMIT_NEGATIVE);
        ((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(false);
    }
}
```

### 示例3：系统初始化时启用所有限位
```cpp
// 在系统初始化时为指定轴启用双向限位
void InitializeLimitsForAxis(int axis)
{
    int iRes = 0;

    // 启用正向限位
    iRes = g_MultiCard.MC_LmtsOn(axis, MC_LIMIT_POSITIVE);
    if(iRes == 0)
    {
        TRACE("轴%d正向限位启用成功\n", axis);
    }
    else
    {
        TRACE("轴%d正向限位启用失败，错误代码: %d\n", axis, iRes);
    }

    // 启用负向限位
    iRes = g_MultiCard.MC_LmtsOn(axis, MC_LIMIT_NEGATIVE);
    if(iRes == 0)
    {
        TRACE("轴%d负向限位启用成功\n", axis);
    }
    else
    {
        TRACE("轴%d负向限位启用失败，错误代码: %d\n", axis, iRes);
    }
}
```

### 示例4：批量配置多个轴的限位
```cpp
// 为多个运动轴配置限位保护
void SetupLimitsForMotionAxes()
{
    int motionAxes[] = {1, 2, 3};  // 需要限位保护的轴
    int count = sizeof(motionAxes) / sizeof(motionAxes[0]);

    for(int i = 0; i < count; i++)
    {
        int axis = motionAxes[i];

        // 启用双向限位保护
        int iRes1 = g_MultiCard.MC_LmtsOn(axis, MC_LIMIT_POSITIVE);
        int iRes2 = g_MultiCard.MC_LmtsOn(axis, MC_LIMIT_NEGATIVE);

        if(iRes1 == 0 && iRes2 == 0)
        {
            TRACE("轴%d双向限位配置完成\n", axis);
        }
        else
        {
            TRACE("轴%d限位配置失败，请检查硬件连接\n", axis);
        }

        // 短暂延时避免操作过快
        Sleep(10);
    }
}
```

## 参数映射表

| 应用场景 | nAxis | nLimitType | 说明 |
|----------|-------|------------|------|
| 正向限位 | m_ComboBoxAxis.GetCurSel()+1 | MC_LIMIT_POSITIVE | 界面控制的正向限位 |
| 负向限位 | m_ComboBoxAxis.GetCurSel()+1 | MC_LIMIT_NEGATIVE | 界面控制的负向限位 |
| 双向配置 | 循环变量 | 两种类型都调用 | 系统初始化时的完整配置 |
| 批量设置 | 数组元素 | 两种类型都调用 | 多轴批量限位配置 |

## 关键说明

1. **限位功能**：
   - **正向限位**：防止轴向正方向超程
   - **负向限位**：防止轴向负方向超程
   - **硬件保护**：通过硬件限位开关触发
   - **紧急停止**：触发限位时立即停止运动

2. **使用时机**：
   - **系统初始化**：在轴使能前配置限位
   - **安全配置**：需要硬件保护的场合
   - **调试模式**：根据需要开启或关闭限位
   - **维护操作**：维护期间可能需要临时关闭限位

3. **注意事项**：
   - 确保限位开关硬件连接正确
   - 限位开关信号正常工作
   - 开启限位后轴会在限位位置停止
   - 关闭限位会失去硬件保护

4. **安全考虑**：
   - 生产环境建议始终开启限位
   - 关闭限位前确保有其他安全措施
   - 定期检查限位开关的功能
   - 限位触发后需要手动复位

## 函数区别

- **MC_LmtsOn() vs MC_LmtsOff()**: MC_LmtsOn()启用限位，MC_LmtsOff()禁用限位
- **MC_LmtsOn() vs MC_GetLmtsOnOff()**: MC_LmtsOn()设置状态，MC_GetLmtsOnOff()读取状态
- **正向 vs 负向限位**: MC_LIMIT_POSITIVE vs MC_LIMIT_NEGATIVE

---

**安全建议**: 在正常生产环境中，建议始终启用双向限位保护，只有在特殊调试或维护情况下才临时关闭限位功能。