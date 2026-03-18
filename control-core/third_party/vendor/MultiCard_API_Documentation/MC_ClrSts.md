# MC_ClrSts 函数教程

## 函数原型

```c
int MC_ClrSts(short nAxis)
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

### 示例1：清除单个轴状态
```cpp
// 通过按钮清除当前轴的状态
int iRes = 0;
int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号

iRes = g_MultiCard.MC_ClrSts(iAxisNum);
```

### 示例2：错误恢复后清除状态
```cpp
// 在处理完报警错误后清除轴状态
void ClearAxisStatusAfterError(int axis)
{
    // 首先处理具体的报警原因
    // ... 错误处理逻辑 ...

    // 然后清除轴状态
    int iRes = g_MultiCard.MC_ClrSts(axis);
    if(iRes == 0)
    {
        TRACE("轴%d状态清除成功\n", axis);

        // 重新使能轴
        g_MultiCard.MC_AxisOn(axis);
    }
    else
    {
        TRACE("轴%d状态清除失败，错误代码: %d\n", axis, iRes);
    }
}
```

### 示例3：批量清除多个轴状态
```cpp
// 在系统初始化或恢复时清除多个轴的状态
void ClearAllAxesStatus()
{
    for(int i = 1; i <= 16; i++)
    {
        int iRes = g_MultiCard.MC_ClrSts(i);
        if(iRes != 0)
        {
            TRACE("清除轴%d状态失败，错误代码: %d\n", i, iRes);
        }
        else
        {
            TRACE("轴%d状态清除成功\n", i);
        }

        // 短暂延时避免操作过快
        Sleep(10);
    }
}
```

### 示例4：状态监控和自动清除
```cpp
// 在定时器中监控轴状态，发现异常时自动清除
void MonitorAndClearAxisStatus()
{
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if(iRes == 0)
    {
        for(int i = 0; i < 16; i++)
        {
            // 检查是否有报警状态
            if(statusData.lAxisStatus[i] & AXIS_STATUS_SV_ALARM)
            {
                TRACE("检测到轴%d报警，尝试清除状态\n", i+1);

                // 清除状态
                g_MultiCard.MC_ClrSts(i+1);

                // 重新检查状态
                Sleep(100);
                g_MultiCard.MC_GetAllSysStatusSX(&statusData);

                if(!(statusData.lAxisStatus[i] & AXIS_STATUS_SV_ALARM))
                {
                    TRACE("轴%d报警已清除\n", i+1);
                }
            }
        }
    }
}
```

## 参数映射表

| 应用场景 | nAxis | 说明 |
|----------|-------|------|
| 单轴清除 | 1-16 | 指定具体轴号 |
| 界面选择 | m_ComboBoxAxis.GetCurSel()+1 | 从用户界面选择轴 |
| 批量清除 | 循环变量 | 批量处理多个轴 |
| 错误恢复 | 报警轴号 | 清除特定报警轴的状态 |

## 关键说明

1. **清除效果**：
   - 清除轴的报警和错误状态
   - 重置状态标志位
   - 不影响轴的位置和运动参数
   - 使轴可以重新接收运动命令

2. **状态类型**：
   - **报警状态**：伺服报警、过流、过压等
   - **限位状态**：硬限位触发状态
   - **运动状态**：运动完成、错误中断等
   - **错误标志**：各种运行错误标志

3. **使用时机**：
   - **报警处理**：在解决报警原因后清除
   - **错误恢复**：处理完错误条件后恢复
   - **系统重启**：重新开始新的操作
   - **状态重置**：需要干净状态时

4. **注意事项**：
   - 清除状态前先解决根本问题
   - 某些硬件报警可能无法通过软件清除
   - 清除后需要验证轴状态是否正常
   - 与MC_AxisOn()配合使用重新使能轴

## 函数区别

- **MC_ClrSts() vs MC_Reset()**: MC_ClrSts()清除单个轴状态，MC_Reset()重置整个控制卡
- **MC_ClrSts() vs MC_Stop()**: MC_ClrSts()清除状态标志，MC_Stop()停止运动
- **MC_ClrSts() vs MC_ZeroPos()**: MC_ClrSts()清除状态，MC_ZeroPos()清零位置

---

**使用建议**: 在清除状态前，先诊断并解决导致报警的根本原因，避免问题重复出现。