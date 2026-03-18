# MC_CrdStart 函数教程

## 函数原型

```c
int MC_CrdStart(long lCrdMask, short nFifoIndex)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| lCrdMask | long | 坐标系掩码。bit0对应坐标系1，bit1对应坐标系2，以此类推 |
| nFifoIndex | short | FIFO索引，通常为0 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：启动单个坐标系
```cpp
// 启动坐标系1开始插补运动
void CDemoVCDlg::OnBnClickedButton11()
{
    int iRes = 0;

    // 启动坐标系1，FIFO索引为0
    iRes = g_MultiCard.MC_CrdStart(1, 0);

    if(0 == iRes)
    {
        AfxMessageBox("启动坐标系成功！");
    }
    else
    {
        AfxMessageBox("启动坐标系失败！");
        return;
    }
}
```

### 礼例2：多坐标系同步启动
```cpp
// 同步启动多个坐标系进行协调运动
bool StartMultipleCoordinateSystems()
{
    int crdCount = 3;  // 假设要启动3个坐标系
    int crdMask = 0;

    // 计算坐标系掩码
    for(int i = 0; i < crdCount; i++)
    {
        crdMask |= (0X0001 << i);  // bit0-2分别对应坐标系1-3
    }

    // 同时启动多个坐标系
    int iRes = g_MultiCard.MC_CrdStart(crdMask, 0);

    if(iRes == 0)
    {
        TRACE("成功启动%d个坐标系，掩码: 0x%08lX\n", crdCount, crdMask);

        // 记录启动时间
        for(int i = 0; i < crdCount; i++)
        {
            g_CrdStartTime[i] = CTime::GetCurrentTime();
            TRACE("坐标系%d启动时间: %s\n", i+1, g_CrdStartTime[i].Format("%H:%M:%S"));
        }
        return true;
    }
    else
    {
        TRACE("启动坐标系失败，错误代码: %d\n", iRes);
        return false;
    }
}
```

### 示例3：条件启动坐标系
```cpp
// 根据条件选择性地启动坐标系
bool ConditionalCrdStart(int crdNum)
{
    // 检查坐标系是否已准备就绪
    if(!IsCoordinateSystemReady(crdNum))
    {
        TRACE("坐标系%d未准备就绪，无法启动\n", crdNum);
        return false;
    }

    // 检查是否有足够的缓冲区空间
    long crdSpace = 0;
    int iRes = g_MultiCard.MC_CrdSpace(crdNum, &crdSpace, 0);
    if(iRes != 0 || crdSpace < 10)
    {
        TRACE("坐标系%d缓冲区空间不足，无法启动\n", crdNum);
        return false;
    }

    // 启动坐标系
    iRes = g_MultiCard.MC_CrdStart(0X0001 << (crdNum - 1), 0);

    if(iRes == 0)
    {
        TRACE("坐标系%d条件启动成功\n", crdNum);

        // 更新状态显示
        UpdateCrdStatusDisplay(crdNum, true);
        return true;
    }
    else
    {
        TRACE("坐标系%d启动失败\n", crdNum);
        return false;
    }
}
```

### 示例4：安全启动检查
```cpp
// 安全的坐标系启动，包含完整的状态检查
bool SafeCrdStart(int crdNum)
{
    TRACE("开始安全启动坐标系%d...\n", crdNum);

    // 1. 检查坐标系参数是否已设置
    if(!IsCrdParameterConfigured(crdNum))
    {
        TRACE("错误：坐标系%d参数未配置\n", crdNum);
        return false;
    }

    // 2. 检查前瞻缓冲区是否已初始化
    if(!IsLookAheadInitialized(crdNum))
    {
        TRACE("错误：坐标系%d前瞻缓冲区未初始化\n", crdNum);
        return false;
    }

    // 3. 检查相关轴的状态
    if(!AreRequiredAxesReady(crdNum))
    {
        TRACE("错误：坐标系%d相关轴未就绪\n", crdNum);
        return false;
    }

    // 4. 检查系统状态
    if(!IsSystemSafeForCrdStart())
    {
        TRACE("错误：系统状态不安全，无法启动坐标系\n");
        return false;
    }

    // 5. 检查缓冲区状态
    long crdSpace = 0;
    int iRes = g_MultiCard.MC_CrdSpace(crdNum, &crdSpace, 0);
    if(iRes != 0)
    {
        TRACE("错误：无法检查坐标系%d缓冲区状态\n", crdNum);
        return false;
    }

    if(crdSpace < 5)
    {
        TRACE("警告：坐标系%d缓冲区空间不足: %ld\n", crdNum, crdSpace);
        // 可以选择继续或中止
    }

    // 6. 清除可能的报警状态
    ClearCrdAlarm(crdNum);

    // 7. 启动坐标系
    TRACE("正在启动坐标系%d...\n", crdNum);
    iRes = g_MultiCard.MC_CrdStart(0X0001 << (crdNum - 1), 0);

    if(iRes == 0)
    {
        TRACE("坐标系%d安全启动成功\n", crdNum);

        // 记录启动信息
        g_CrdStartTime[crdNum - 1] = CTime::GetCurrentTime();
        UpdateCrdStatusDisplay(crdNum, true);

        return true;
    }
    else
    {
        TRACE("错误：坐标系%d启动失败，错误代码: %d\n", crdNum, iRes);
        return false;
    }
}
```

### 礓例5：坐标系状态监控
```cpp
// 监控坐标系运行状态并在停止时重新启动
void MonitorAndRestartCoordinateSystems()
{
    static bool crdRunning[16] = {false};
    static CTime lastCheckTime[16] = {CTime::GetCurrentTime()};

    for(int i = 0; i < 16; i++)
    {
        // 跳过未使用的坐标系
        if(!IsCrdConfigured(i + 1)) continue;

        // 检查坐标系状态
        short crdStatus = 0;
        int iRes = g_MultiCard.MrdCrdStatus(i + 1, &crdStatus, NULL);
        if(iRes != 0) continue;

        bool isRunning = (crdStatus & CRDSYS_STATUS_PROG_RUN) != 0;
        CTime currentTime = CTime::GetCurrentTime();
        CTimeSpan timeSinceLastCheck = currentTime - lastCheck[i];

        // 检查状态变化
        if(isRunning != crdRunning[i])
        {
            if(isRunning)
            {
                TRACE("坐标系%d开始运行\n", i + 1);
                OnCrdStart(i + 1);
            }
            else
            {
                TRACE("坐标系%d停止运行\n", i + 1);
                OnCrdStop(i + 1);

                // 如果停止是由于错误，尝试重新启动
                if(crdStatus & CRDSYS_STATUS_ALARM)
                {
                    TRACE("检测到坐标系%d报警，5秒后尝试重新启动\n", i + 1);
                    // 设置定时器重新启动
                    SetRestartTimer(i + 1, 5000);
                }
            }
            crdRunning[i] = isRunning;
            lastCheckTime[i] = currentTime;
        }

        // 检查运行时间
        if(isRunning && timeSinceLastCheck.GetTotalSeconds() > 300)  // 5分钟
        {
            TRACE("坐标系%d运行时间: %ld秒\n", i + 1,
                  (long)timeSinceLastCheck.GetTotalSeconds());
        }
    }
}
```

### 礓练例6：坐标系启动日志记录
```cpp
// 记录坐标系启动的详细信息
void LogCrdStartEvent(int crdNum)
{
    // 获取当前系统状态
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    CString logEntry;
    logEntry.Format("=== 坐标系%d启动事件 ===\n", crdNum);
    logEntry += "时间: %s\n";
    logEntry += "系统状态:\n";

    if(iRes == 0)
    {
        // 记录相关轴的状态
        for(int i = 0; i < 8; i++)
        {
            if(statusData.lAxisStatus[i] & AXIS_STATUS_SV_ALARM)
            {
                logEntry += CString::Format("  轴%d: 伺服报警\n", i+1);
            }
            else if(statusData.lAxisStatus[i] & AXIS_STATUS_RUNNING)
            {
                logEntry += CString::Format("  轴%d: 正在运行\n", i+1);
            }
        }

        // 记录限位状态
        if(statusData.lLimitPosRaw != 0 || statusData.lLimitNegRaw != 0)
        {
            logEntry += "限位状态: ";
            if(statusData.lLimitPosRaw) logEntry += "正限位触发 ";
            if(statusData.lLimitNegRaw) logEntry += "负限位触发 ";
            logEntry += "\n";
        }
    }

    // 记录启动参数（如果需要）
    logEntry += "参数配置:\n";
    logEntry += "  FIFO索引: 0\n";
    logEntry += "  坐标系掩码: 0x%08lX\n", (0X0001 << (crdNum - 1));

    // 写入日志文件
    WriteToLogFile(logEntry);

    TRACE(logEntry);
}
```

## 参数映射表

| 应用场景 | lCrdMask | nFifoIndex | 说明 |
|----------|----------|------------|------|
| 单坐标系 | 0X0001 << (n-1) | 0 | 启动单个坐标系 |
| 多坐标系 | 位或计算 | 0 | 同时启动多个坐标系 |
| 批量启动 | 0XFFFF | 0 | 启动所有16个坐标系 |
| 特定组合 | 自定义组合 | 0 | 启动指定的坐标系组合 |

## 关键说明

1. **坐标系掩码计算**：
   - **坐标系1**: 0X0001
   - **坐标系2**: 0X0002
   - **坐标系3**: 0X0004
   - **坐标系n**: 0X0001 << (n-1)

2. **启动条件检查**：
   - 坐标系参数必须已设置（MC_SetCrdPrm）
   - 前瞻缓冲区必须已初始化（MC_InitLookAhead）
   - 相关轴必须处于使能状态
   - 缓冲区需要足够的空间

3. **启动效果**：
   - 开始执行坐标系缓冲区中的插补数据
   - 各轴开始协调运动
   - 系统进入运行状态
   - 状态标志更新

4. **FIFO索引**：
   - 通常使用0作为默认FIFO
   - 支持多FIFO的复杂应用
   - 需要与坐标系初始化时一致

5. **使用时机**：
   - **程序开始**：启动预定义的运动序列
   - **任务切换**：启动新的运动任务
   - **错误恢复**：清除报警后重新启动
   - **手动控制**：用户触发的运动启动

6. **注意事项**：
   - 启动前确保所有必要条件满足
   - 启动失败时需要检查错误原因
   - 避免重复启动已运行的坐标系
   - 监控启动后的运行状态

## 函数区别

- **MC_CrdStart() vs MC_CrdStop()**: 启动运动 vs 停止运动
- **MC_CrdStart() vs MC_SetCrdPrm()**: 启动运行 vs 设置参数
- **单坐标系 vs 多坐标系**: 通过掩码参数区分

---

**使用建议**: 在启动坐标系前，建议进行完整的状态检查，确保系统处于安全状态。对于关键应用，可以建立启动日志记录，便于问题追踪和分析。启动后应密切监控系统状态，及时发现和处理异常情况。