# MC_CrdClear 函数教程

## 函数原型

```c
int MC_CrdClear(short nCrdNum, short nFifoIndex)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| nFifoIndex | short | FIFO索引，通常为0 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本缓冲区清空
```cpp
// 在发送新数据前清空插补缓冲区原有数据
void CDemoVCDlg::OnBnClickedButtonClearBuffer()
{
    // 清空插补缓冲区原有数据
    int iRes = g_MultiCard.MC_CrdClear(1, 0);

    if(iRes == 0)
    {
        AfxMessageBox("坐标系1缓冲区清空成功！");
        TRACE("坐标系1插补缓冲区清空成功\n");

        // 更新界面显示
        UpdateBufferStatusDisplay(1);
    }
    else
    {
        AfxMessageBox("坐标系1缓冲区清空失败！");
        TRACE("坐标系1插补缓冲区清空失败，错误代码: %d\n", iRes);
    }
}
```

### 示例2：坐标系重置流程
```cpp
// 完整的坐标系重置流程
bool ResetCoordinateSystem(int crdNum)
{
    int iRes = 0;

    TRACE("开始重置坐标系%d\n", crdNum);

    // 1. 停止当前运动
    iRes = g_MultiCard.MC_CrdStop(0X0001 << (crdNum - 1), 0);
    if(iRes != 0)
    {
        TRACE("停止坐标系%d运动失败\n", crdNum);
        return false;
    }
    TRACE("坐标系%d运动已停止\n", crdNum);

    // 2. 等待运动完全停止
    if(!WaitForMotionStop(crdNum, 5000))  // 等待最多5秒
    {
        TRACE("等待坐标系%d停止超时\n", crdNum);
        return false;
    }

    // 3. 清空插补缓冲区
    iRes = g_MultiCard.MC_CrdClear(crdNum, 0);
    if(iRes != 0)
    {
        TRACE("清空坐标系%d缓冲区失败\n", crdNum);
        return false;
    }
    TRACE("坐标系%d缓冲区已清空\n", crdNum);

    // 4. 重置速度覆盖
    iRes = g_MultiCard.MC_SetOverride(crdNum, 1.0);
    if(iRes != 0)
    {
        TRACE("重置坐标系%d速度覆盖失败\n", crdNum);
    }

    // 5. 清除报警状态
    iRes = g_MultiCard.MC_ClrSts(0X0001 << (crdNum - 1), 0XFFFF);
    TRACE("坐标系%d状态清除完成\n", crdNum);

    // 6. 验证重置结果
    if(ValidateCoordinateSystemReset(crdNum))
    {
        TRACE("坐标系%d重置成功\n", crdNum);
        return true;
    }
    else
    {
        TRACE("坐标系%d重置验证失败\n", crdNum);
        return false;
    }
}

// 等待运动停止
bool WaitForMotionStop(int crdNum, int timeoutMs)
{
    CTime startTime = CTime::GetCurrentTime();
    CTimeSpan timeout(timeoutMs / 1000);

    while(CTime::GetCurrentTime() - startTime < timeout)
    {
        short crdStatus = 0;
        int iRes = g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);
        if(iRes == 0 && !(crdStatus & CRDSYS_STATUS_PROG_RUN))
        {
            TRACE("坐标系%d运动已停止\n", crdNum);
            return true;
        }
        Sleep(100);  // 每100ms检查一次
    }

    return false;  // 超时
}

// 验证坐标系重置结果
bool ValidateCoordinateSystemReset(int crdNum)
{
    short crdStatus = 0;
    long crdSpace = 0;

    // 检查状态
    int iRes1 = g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);
    int iRes2 = g_MultiCard.MC_CrdSpace(crdNum, &crdSpace, 0);

    if(iRes1 != 0 || iRes2 != 0)
    {
        TRACE("无法读取坐标系%d状态\n", crdNum);
        return false;
    }

    // 检查是否停止
    if(crdStatus & CRDSYS_STATUS_PROG_RUN)
    {
        TRACE("坐标系%d仍在运行\n", crdNum);
        return false;
    }

    // 检查缓冲区空间
    if(crdSpace < 400)  // 假设总容量为500
    {
        TRACE("坐标系%d缓冲区空间不足: %ld\n", crdNum, crdSpace);
        return false;
    }

    TRACE("坐标系%d重置验证通过，状态正常\n", crdNum);
    return true;
}
```

### 示例3：多坐标系批量清空
```cpp
// 批量清空多个坐标系的缓冲区
bool ClearMultipleCoordinateSystems(int* crdNums, int count)
{
    int successCount = 0;
    int totalResult = 0;

    TRACE("开始批量清空%d个坐标系的缓冲区\n", count);

    // 先停止所有坐标系
    long crdMask = 0;
    for(int i = 0; i < count; i++)
    {
        crdMask |= (0X0001 << (crdNums[i] - 1));
    }
    totalResult += g_MultiCard.MC_CrdStop(crdMask, 0);

    if(totalResult != 0)
    {
        TRACE("批量停止坐标系失败\n");
        return false;
    }

    // 等待所有坐标系停止
    Sleep(1000);

    // 逐个清空缓冲区
    for(int i = 0; i < count; i++)
    {
        int crdNum = crdNums[i];
        int iRes = g_MultiCard.MC_CrdClear(crdNum, 0);

        if(iRes == 0)
        {
            successCount++;
            TRACE("坐标系%d缓冲区清空成功\n", crdNum);
        }
        else
        {
            TRACE("坐标系%d缓冲区清空失败\n", crdNum);
        }
    }

    TRACE("批量清空完成，成功: %d/%d\n", successCount, count);
    return successCount == count;
}

// 使用示例
void BatchClearExample()
{
    int crdNums[] = {1, 2, 3};  // 要清空的坐标系号
    int count = sizeof(crdNums) / sizeof(crdNums[0]);

    if(ClearMultipleCoordinateSystems(crdNums, count))
    {
        AfxMessageBox("所有坐标系缓冲区清空成功！");
    }
    else
    {
        AfxMessageBox("部分坐标系缓冲区清空失败！");
    }
}
```

### 示例4：安全清空检查
```cpp
// 带安全检查的缓冲区清空
bool SafeBufferClear(int crdNum)
{
    TRACE("开始安全清空坐标系%d缓冲区\n", crdNum);

    // 1. 检查坐标系状态
    short crdStatus = 0;
    int iRes1 = g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);
    if(iRes1 != 0)
    {
        TRACE("无法读取坐标系%d状态\n", crdNum);
        return false;
    }

    // 2. 检查是否有报警
    if(crdStatus & CRDSYS_STATUS_ALARM)
    {
        TRACE("坐标系%d处于报警状态，需要先处理报警\n", crdNum);

        // 尝试清除报警
        int iRes2 = g_MultiCard.MC_ClrSts(0X0001 << (crdNum - 1), 0XFFFF);
        if(iRes2 != 0)
        {
            TRACE("清除坐标系%d报警失败\n", crdNum);
            return false;
        }

        // 再次检查状态
        iRes1 = g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);
        if(iRes1 != 0 || (crdStatus & CRDSYS_STATUS_ALARM))
        {
            TRACE("坐标系%d报警无法清除\n", crdNum);
            return false;
        }
    }

    // 3. 检查是否正在执行关键运动
    if(crdStatus & CRDSYS_STATUS_PROG_RUN)
    {
        TRACE("坐标系%d正在运行，需要先停止\n", crdNum);

        // 确认是否停止
        if(!ConfirmStopMotion(crdNum))
        {
            TRACE("用户取消停止操作\n", crdNum);
            return false;
        }

        // 停止运动
        int iRes3 = g_MultiCard.MC_CrdStop(0X0001 << (crdNum - 1), 0);
        if(iRes3 != 0)
        {
            TRACE("停止坐标系%d失败\n", crdNum);
            return false;
        }

        // 等待停止
        if(!WaitForMotionStop(crdNum, 3000))
        {
            TRACE("等待坐标系%d停止超时\n", crdNum);
            return false;
        }
    }

    // 4. 执行缓冲区清空
    int iRes4 = g_MultiCard.MC_CrdClear(crdNum, 0);
    if(iRes4 != 0)
    {
        TRACE("清空坐标系%d缓冲区失败\n", crdNum);
        return false;
    }

    // 5. 验证清空结果
    long crdSpace = 0;
    int iRes5 = g_MultiCard.MC_CrdSpace(crdNum, &crdSpace, 0);
    if(iRes5 != 0)
    {
        TRACE("验证坐标系%d缓冲区状态失败\n", crdNum);
        return false;
    }

    if(crdSpace < 450)  // 假设总容量为500
    {
        TRACE("坐标系%d缓冲区清空不彻底，剩余空间: %ld\n", crdNum, crdSpace);
        return false;
    }

    TRACE("坐标系%d缓冲区安全清空完成\n", crdNum);
    return true;
}

// 确认是否停止运动
bool ConfirmStopMotion(int crdNum)
{
    CString message;
    message.Format("坐标系%d正在运行，确认停止并清空缓冲区吗？", crdNum);

    int result = AfxMessageBox(message, MB_YESNO | MB_ICONQUESTION);
    return result == IDYES;
}
```

### 示例5：缓冲区状态监控和自动清理
```cpp
// 监控缓冲区状态，必要时自动清理
void MonitorAndAutoCleanBuffers()
{
    const int MAX_CRD_NUM = 3;
    const long SPACE_THRESHOLD = 50;  // 空间不足阈值
    const int ERROR_COUNT_THRESHOLD = 5;  // 错误次数阈值

    static int errorCounts[MAX_CRD_NUM + 1] = {0};  // 错误计数器

    for(int crdNum = 1; crdNum <= MAX_CRD_NUM; crdNum++)
    {
        long crdSpace = 0;
        short crdStatus = 0;

        // 读取缓冲区空间和状态
        int iRes1 = g_MultiCard.MC_CrdSpace(crdNum, &crdSpace, 0);
        int iRes2 = g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);

        if(iRes1 != 0 || iRes2 != 0)
        {
            errorCounts[crdNum]++;
            TRACE("读取坐标系%d状态失败，错误次数: %d\n", crdNum, errorCounts[crdNum]);

            // 错误次数过多时尝试清理
            if(errorCounts[crdNum] >= ERROR_COUNT_THRESHOLD)
            {
                TRACE("坐标系%d错误次数过多，尝试清理\n", crdNum);
                if(SafeBufferClear(crdNum))
                {
                    errorCounts[crdNum] = 0;  // 重置错误计数
                    TRACE("坐标系%d清理成功，错误计数重置\n", crdNum);
                }
            }
            continue;
        }

        // 重置错误计数
        errorCounts[crdNum] = 0;

        // 检查缓冲区空间
        if(crdSpace < SPACE_THRESHOLD)
        {
            TRACE("坐标系%d缓冲区空间不足: %ld，检查状态\n", crdNum, crdSpace);

            // 如果坐标系未运行，可以安全清理
            if(!(crdStatus & CRDSYS_STATUS_PROG_RUN))
            {
                TRACE("坐标系%d空闲，执行自动清理\n", crdNum);
                int iRes3 = g_MultiCard.MC_CrdClear(crdNum, 0);
                if(iRes3 == 0)
                {
                    TRACE("坐标系%d自动清理成功\n", crdNum);
                }
                else
                {
                    TRACE("坐标系%d自动清理失败\n", crdNum);
                }
            }
            else
            {
                TRACE("坐标系%d正在运行，暂不清理\n", crdNum);
            }
        }
    }
}
```

### 示例6：缓冲区清空日志记录
```cpp
// 记录缓冲区清空操作的详细信息
void LogBufferClearOperation(int crdNum, const CString& reason)
{
    // 获取清空前的状态
    short crdStatus = 0;
    long crdSpace = 0;
    double currentPos[8] = {0};

    g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);
    g_MultiCard.MC_CrdSpace(crdNum, &crdSpace, 0);
    g_MultiCard.MC_GetCrdPos(crdNum, currentPos);

    // 构建日志条目
    CString logEntry;
    CTime currentTime = CTime::GetCurrentTime();

    logEntry.Format("=== 坐标系%d缓冲区清空日志 ===\n", crdNum);
    logEntry += "时间: " + currentTime.Format("%Y-%m-%d %H:%M:%S") + "\n";
    logEntry += "原因: " + reason + "\n";
    logEntry += "清空前状态:\n";
    logEntry += CString::Format("  运行状态: %s\n", (crdStatus & CRDSYS_STATUS_PROG_RUN) ? "运行中" : "空闲");
    logEntry += CString::Format("  报警状态: %s\n", (crdStatus & CRDSYS_STATUS_ALARM) ? "报警" : "正常");
    logEntry += CString::Format("  缓冲区空间: %ld\n", crdSpace);
    logEntry += CString::Format("  当前位置: (%.3f, %.3f, %.3f)\n", currentPos[0], currentPos[1], currentPos[2]);

    // 执行清空操作
    int iRes = g_MultiCard.MC_CrdClear(crdNum, 0);

    // 记录清空结果
    if(iRes == 0)
    {
        logEntry += "清空结果: 成功\n";

        // 获取清空后的状态
        long newSpace = 0;
        g_MultiCard.MC_CrdSpace(crdNum, &newSpace, 0);
        logEntry += CString::Format("清空后空间: %ld\n", newSpace);
    }
    else
    {
        logEntry += CString::Format("清空结果: 失败 (错误代码: %d)\n", iRes);
    }

    logEntry += "=============================\n";

    // 写入日志文件
    WriteToLogFile(logEntry);

    // 输出到调试窗口
    TRACE(logEntry);
}

// 使用示例
void LogBufferClearExample()
{
    // 手动清空并记录日志
    LogBufferClearOperation(1, "用户手动清空");

    // 系统自动清理并记录日志
    LogBufferClearOperation(2, "系统自动清理 - 缓冲区空间不足");
}
```

## 参数映射表

| 应用场景 | nCrdNum | nFifoIndex | 说明 |
|----------|---------|------------|------|
| 单缓冲区清空 | 1-16 | 0 | 清空单个坐标系缓冲区 |
| 批量清空 | 循环变量 | 0 | 批量清空多个坐标系 |
| 安全清空 | 指定坐标系 | 0 | 带安全检查的清空 |
| 自动清理 | 固定坐标系 | 0 | 监控触发的自动清理 |

## 关键说明

1. **清空功能**：
   - 清空坐标系的插补缓冲区
   - 删除所有未执行的运动指令
   - 释放缓冲区空间

2. **使用时机**：
   - 发送新数据前清空旧数据
   - 坐标系重置时使用
   - 错误恢复时清理缓冲区
   - 系统初始化时使用

3. **安全考虑**：
   - 清空前建议停止相关运动
   - 检查坐标系状态和报警
   - 避免在关键运动中清空
   - 确认操作安全性

4. **效果验证**：
   - 检查缓冲区空间变化
   - 验证运动指令是否清除
   - 确认坐标系状态正常

5. **错误处理**：
   - 检查函数返回值
   - 处理清空失败情况
   - 建立重试机制

6. **注意事项**：
   - 清空操作不可撤销
   - 可能影响正在执行的运动
   - 需要重新初始化前瞻缓冲区
   - 建议记录清空操作日志

## 函数区别

- **MC_CrdClear() vs MC_CrdStop()**: 清空缓冲区 vs 停止运动
- **MC_CrdClear() vs MC_CrdData()**: 清空数据 vs 结束标识
- **清空 vs 重置**: 缓冲区清理 vs 完整系统重置

---

**使用建议**: 在清空坐标系缓冲区前，建议先停止相关运动并检查系统状态。建立完整的安全检查流程，避免在关键操作中进行清空。对于自动化系统，可以建立缓冲区监控和自动清理机制。重要操作建议记录日志以便追踪和调试。