# MC_GetLmtsOnOff 函数教程

## 函数原型

```c
int MC_GetLmtsOnOff(short nAxis, short* pnPosLimOnOff, short* pnNegLimOnOff)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nAxis | short | 轴号，取值范围：1-16 |
| pnPosLimOnOff | short* | 正向限位状态指针。0：关闭；非0：开启 |
| pnNegLimOnOff | short* | 负向限位状态指针。0：关闭；非0：开启 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：读取限位状态并更新界面
```cpp
// 在轴选择改变时读取限位状态并更新界面显示
void CDemoVCDlg::OnCbnSelchangeCombo1()
{
    int iRes = 0;
    short nEncOnOff = 0;
    short nPosLimOnOff = 0;
    short nNegLimOfOff = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号

    // 读取编码器开关状态
    iRes = g_MultiCard.MC_GetEncOnOff(iAxisNum,&nEncOnOff);

    if(nEncOnOff)
    {
        ((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(false);
    }
    else
    {
        ((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(true);
    }

    // 读取限位状态
    iRes = g_MultiCard.MC_GetLmtsOnOff(iAxisNum,&nPosLimOnOff,&nNegLimOfOff);

    if(nPosLimOnOff)
    {
        ((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(true);  // 正向限位开启
    }
    else
    {
        ((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(false); // 正向限位关闭
    }

    if(nNegLimOfOff)
    {
        ((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(true);  // 负向限位开启
    }
    else
    {
        ((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(false); // 负向限位关闭
    }
}
```

### 示例2：系统限位状态检查
```cpp
// 检查所有轴的限位状态并生成报告
void CheckAllLimitsStatus()
{
    TRACE("=== 限位状态检查报告 ===\n");

    for(int i = 1; i <= 16; i++)
    {
        short nPosLimOnOff = 0;
        short nNegLimOnOff = 0;
        int iRes = g_MultiCard.MC_GetLmtsOnOff(i, &nPosLimOnOff, &nNegLimOnOff);

        if(iRes == 0)
        {
            TRACE("轴%d: ", i);
            if(nPosLimOnOff && nNegLimOnOff)
            {
                TRACE("双向限位开启\n");
            }
            else if(nPosLimOnOff)
            {
                TRACE("仅正向限位开启\n");
            }
            else if(nNegLimOnOff)
            {
                TRACE("仅负向限位开启\n");
            }
            else
            {
                TRACE("双向限位都关闭 - 警告！\n");
            }
        }
        else
        {
            TRACE("轴%d: 读取限位状态失败，错误代码: %d\n", i, iRes);
        }
    }
    TRACE("=== 检查完成 ===\n");
}
```

### 示例3：运动前安全检查
```cpp
// 在执行运动前检查限位保护状态
bool ValidateLimitsBeforeMotion(int axis, int targetPos, int currentPos)
{
    short nPosLimOnOff = 0;
    short nNegLimOnOff = 0;
    int iRes = g_MultiCard.MC_GetLmtsOnOff(axis, &nPosLimOnOff, &nNegLimOnOff);

    if(iRes != 0)
    {
        TRACE("无法读取轴%d限位状态\n", axis);
        return false;
    }

    // 检查运动方向和对应限位状态
    if(targetPos > currentPos)  // 正向运动
    {
        if(!nPosLimOnOff)
        {
            TRACE("警告：轴%d正向限位已关闭，正向运动存在风险\n", axis);

            int result = MessageBox("正向限位已关闭，继续运动吗？", "安全警告",
                                   MB_YESNO | MB_ICONWARNING);
            if(result != IDYES)
            {
                return false;
            }
        }
    }
    else if(targetPos < currentPos)  // 负向运动
    {
        if(!nNegLimOnOff)
        {
            TRACE("警告：轴%d负向限位已关闭，负向运动存在风险\n", axis);

            int result = MessageBox("负向限位已关闭，继续运动吗？", "安全警告",
                                   MB_YESNO | MB_ICONWARNING);
            if(result != IDYES)
            {
                return false;
            }
        }
    }

    TRACE("轴%d限位状态验证通过，可以执行运动\n", axis);
    return true;
}
```

### 示例4：限位状态监控和告警
```cpp
// 在定时器中监控限位状态变化
void MonitorLimitsStatusChanges()
{
    static short lastPosLimStatus[16] = {0};
    static short lastNegLimStatus[16] = {0};

    for(int i = 1; i <= 16; i++)
    {
        short nPosLimOnOff = 0;
        short nNegLimOnOff = 0;
        int iRes = g_MultiCard.MC_GetLmtsOnOff(i, &nPosLimOnOff, &nNegLimOnOff);

        if(iRes == 0)
        {
            // 检查正向限位状态变化
            if(nPosLimOnOff != lastPosLimStatus[i-1])
            {
                if(nPosLimOnOff)
                {
                    TRACE("轴%d正向限位已启用\n", i);
                }
                else
                {
                    TRACE("警告：轴%d正向限位已禁用！\n", i);
                    // 可以在这里添加告警逻辑
                }
                lastPosLimStatus[i-1] = nPosLimOnOff;
            }

            // 检查负向限位状态变化
            if(nNegLimOnOff != lastNegLimStatus[i-1])
            {
                if(nNegLimOnOff)
                {
                    TRACE("轴%d负向限位已启用\n", i);
                }
                else
                {
                    TRACE("警告：轴%d负向限位已禁用！\n", i);
                    // 可以在这里添加告警逻辑
                }
                lastNegLimStatus[i-1] = nNegLimOnOff;
            }
        }
    }
}
```

## 参数映射表

| 应用场景 | nAxis | pnPosLimOnOff | pnNegLimOnOff | 说明 |
|----------|-------|---------------|---------------|------|
| 界面更新 | m_ComboBoxAxis.GetCurSel()+1 | 局部变量 | 局部变量 | 同步界面显示 |
| 批量检查 | 循环变量 | 局部变量 | 局部变量 | 系统状态检查 |
| 运动验证 | 运动轴号 | 局部变量 | 局部变量 | 安全检查 |
| 状态监控 | 固定轴号 | 静态数组 | 静态数组 | 状态变化监控 |

## 关键说明

1. **状态值含义**：
   - **0**：对应方向的限位保护关闭
   - **非0**：对应方向的限位保护开启
   - 可以独立读取正向和负向限位状态

2. **使用时机**：
   - **界面更新**：轴选择改变时同步显示状态
   - **安全检查**：运动前验证限位保护状态
   - **状态监控**：定期检查限位配置变化
   - **故障诊断**：限位相关问题时检查状态

3. **注意事项**：
   - 读取状态不会改变限位实际设置
   - 需要确保两个指针参数都有效
   - 读取失败时需要处理错误情况
   - 状态变化可能影响系统安全性

4. **安全考虑**：
   - 关闭限位会降低系统安全性
   - 建议监控限位状态变化
   - 在运动前检查限位状态
   - 异常状态变化时及时告警

## 函数区别

- **MC_GetLmtsOnOff() vs MC_LmtsOn()/MC_LmtsOff()**: 读取状态 vs 设置状态
- **MC_GetLmtsOnOff() vs MC_GetEncOnOff()**: 限位状态 vs 编码器状态
- **双向读取 vs 单向读取**: 一次读取两个方向的状态

---

**使用建议**: 在系统关键操作前调用此函数验证限位保护状态，确保操作安全性。同时建议在界面中实时显示限位状态，让用户清楚当前的保护配置。