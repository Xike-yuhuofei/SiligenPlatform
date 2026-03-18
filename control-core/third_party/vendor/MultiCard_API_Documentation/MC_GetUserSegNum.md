# MC_GetUserSegNum 函数教程

## 函数原型

```c
int MC_GetUserSegNum(short nCrdNum, long* plSegNum)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| plSegNum | long* | 段号指针，用于存储当前运行的段号 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本段号读取
```cpp
// 在定时器中读取并显示当前运行段号
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // ... 其他定时器处理代码 ...

    long lSegNum = 0;
    g_MultiCard.MC_GetUserSegNum(1, &lSegNum);

    CString strText;
    strText.Format("运行行号：%d", lSegNum);
    GetDlgItem(IDC_STATIC_CUR_SEG_NUM)->SetWindowText(strText);

    // ... 其他定时器处理代码 ...
}
```

### 示例2：段号监控和记录
```cpp
// 监控坐标系段号变化并记录
void MonitorSegmentNumbers()
{
    static long lastSegNum = -1;
    long currentSegNum = 0;

    // 读取当前段号
    int iRes = g_MultiCard.MC_GetUserSegNum(1, &currentSegNum);
    if(iRes != 0)
    {
        TRACE("读取段号失败\n");
        return;
    }

    // 检查段号是否发生变化
    if(currentSegNum != lastSegNum && lastSegNum != -1)
    {
        TRACE("坐标系1段号变化: %ld -> %ld\n", lastSegNum, currentSegNum);

        // 记录段号变化
        LogSegmentChange(1, lastSegNum, currentSegNum);

        // 检查是否到达关键段
        CheckKeySegment(currentSegNum);
    }

    lastSegNum = currentSegNum;
}
```

### 示例3：多坐标系段号监控
```cpp
// 监控多个坐标系的段号
void MonitorMultipleCoordinateSystems()
{
    int crdCount = 3;  // 假设有3个坐标系
    long segNumbers[3] = {0};

    // 读取各坐标系的段号
    for(int i = 0; i < crdCount; i++)
    {
        int iRes = g_MultiCard.MC_GetUserSegNum(i+1, &segNumbers[i]);
        if(iRes == 0)
        {
            TRACE("坐标系%d当前段号: %ld\n", i+1, segNumbers[i]);
        }
        else
        {
            TRACE("读取坐标系%d段号失败\n", i+1);
        }
    }

    // 更新界面显示
    UpdateSegmentDisplay(segNumbers, crdCount);
}
```

### 示例4：段号进度显示
```cpp
// 显示程序执行的进度
void ShowProgramProgress(int crdNum, int totalSegments)
{
    long currentSeg = 0;
    int iRes = g_MultiCard.MC_GetUserSegNum(crdNum, &currentSeg);

    if(iRes == 0 && totalSegments > 0)
    {
        // 计算完成百分比
        double progress = (double)currentSeg / totalSegments * 100.0;

        TRACE("坐标系%d执行进度: %.1f%% (段号: %ld/%d)\n",
              crdNum, progress, currentSeg, totalSegments);

        // 更新进度条
        UpdateProgressBar(progress);

        // 检查是否完成
        if(currentSeg >= totalSegments)
        {
            TRACE("坐标系%d程序执行完成\n", crdNum);
            HandleProgramComplete(crdNum);
        }
    }
}
```

### 示例5：段号同步验证
```cpp
// 验证段号与预期是否一致
bool ValidateSegmentNumber(int crdNum, long expectedSegNum)
{
    long actualSegNum = 0;
    int iRes = g_MultiCard.MC_GetUserSegNum(crdNum, &actualSegNum);

    if(iRes != 0)
    {
        TRACE("无法读取坐标系%d段号\n", crdNum);
        return false;
    }

    if(actualSegNum == expectedSegNum)
    {
        TRACE("坐标系%d段号验证通过: %ld\n", crdNum, actualSegNum);
        return true;
    }
    else
    {
        TRACE("坐标系%d段号验证失败: 预期%ld, 实际%ld\n",
              crdNum, expectedSegNum, actualSegNum);
        HandleSegmentMismatch(crdNum, expectedSegNum, actualSegNum);
        return false;
    }
}
```

### 示例6：段号断点调试
```cpp
// 在指定段号设置断点用于调试
void DebugSegmentBreakpoint(int crdNum, long breakpointSeg)
{
    long currentSeg = 0;
    static bool breakpointHit = false;

    int iRes = g_MultiCard.MC_GetUserSegNum(crdNum, &currentSeg);
    if(iRes != 0) return;

    // 检查是否到达断点
    if(currentSeg == breakpointSeg && !breakpointHit)
    {
        TRACE("=== 调试断点：坐标系%d到达段号%ld ===\n", crdNum, currentSeg);

        // 执行调试操作
        ExecuteDebugOperations(crdNum, currentSeg);

        breakpointHit = true;

        // 可以选择暂停程序
        // g_MultiCard.MC_Stop(0XFFFFFF00, 0XFFFFFF00);
    }
    else if(currentSeg != breakpointSeg)
    {
        breakpointHit = false;  // 重置断点标志
    }
}
```

## 参数映射表

| 应用场景 | nCrdNum | plSegNum | 说明 |
|----------|---------|----------|------|
| 界面显示 | 1 | 局部变量 | 显示当前运行段号 |
| 进度监控 | 坐标系号 | 局部变量 | 跟踪程序执行进度 |
| 多坐标系监控 | 循环变量 | 数组 | 同时监控多个坐标系 |
| 调试验证 | 坐标系号 | 局部变量 | 验证程序执行位置 |

## 关键说明

1. **段号含义**：
   - 表示坐标系当前执行的程序段
   - 通常对应G代码或运动指令的行号
   - 用于跟踪程序执行进度

2. **坐标系编号**：
   - **1-16**：支持的坐标系范围
   - 每个坐标系独立维护段号
   - 需要确保坐标系已正确初始化

3. **使用时机**：
   - **进度显示**：显示程序执行进度
   - **状态监控**：监控程序运行状态
   - **调试诊断**：定位程序执行位置
   - **同步验证**：验证程序执行的正确性

4. **数值范围**：
   - 通常从1开始计数
   - 0可能表示未开始或空闲状态
   - 最大值取决于程序长度

5. **注意事项**：
   - 段号在坐标系停止或重置时可能清零
   - 不同坐标系的段号相互独立
   - 读取失败时需要处理错误情况

6. **应用场景**：
   - **生产线监控**：跟踪生产进度
   - **质量控制**：记录加工位置
   - **故障诊断**：定位问题发生位置
   - **性能分析**：分析执行效率

## 函数区别

- **MC_GetUserSegNum() vs MC_GetCrdStatus()**: 段号 vs 坐标系状态
- **MC_GetUserSegNum() vs MC_GetAllSysStatusSX()**: 单项信息 vs 完整系统状态
- **用户段号 vs 系统段号**: 用户定义的程序段 vs 内部系统段

---

**使用建议**: 在生产线或自动化应用中，段号是重要的进度指标。建议定期读取段号并记录执行日志，便于生产统计和质量追溯。对于复杂程序，可以设置关键段号作为检查点。