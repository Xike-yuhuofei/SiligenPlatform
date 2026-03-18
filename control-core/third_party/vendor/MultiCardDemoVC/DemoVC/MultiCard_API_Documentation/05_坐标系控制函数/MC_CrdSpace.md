# MC_CrdSpace 函数教程

## 函数原型

```c
int MC_CrdSpace(short nCrdNum, long* pSpace, short nFifoIndex)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| pSpace | long* | 空间值指针，用于接收缓冲区剩余空间大小 |
| nFifoIndex | short | FIFO索引，通常为0 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本空间读取和显示
```cpp
// 在定时器中显示坐标系插补缓冲区剩余空间
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    long lCrdSpace = 0;
    CString strText;

    // 获取坐标系剩余空间
    int iRes = g_MultiCard.MC_CrdSpace(1, &lCrdSpace, 0);

    if(iRes == 0)
    {
        strText.Format("板卡插补缓冲区剩余空间：%d", lCrdSpace);
        GetDlgItem(IDC_STATIC_CRD_SPACE)->SetWindowText(strText);
    }
}
```

### 示例2：缓冲区空间监控和预警
```cpp
// 监控缓冲区空间使用情况，提供预警机制
void MonitorBufferSpace(int crdNum)
{
    long currentSpace = 0;
    int iRes = g_MultiCard.MC_CrdSpace(crdNum, &currentSpace, 0);
    if(iRes != 0) return;

    // 定义空间阈值
    const long CRITICAL_SPACE = 10;    // 危险空间
    const long WARNING_SPACE = 50;     // 警告空间
    const long OPTIMAL_SPACE = 200;    // 最佳空间

    // 空间状态判断
    if(currentSpace < CRITICAL_SPACE)
    {
        TRACE("危险！坐标系%d缓冲区空间严重不足: %ld\n", crdNum, currentSpace);
        // 触发紧急停止或暂停数据发送
        HandleCriticalBufferSpace(crdNum, currentSpace);
    }
    else if(currentSpace < WARNING_SPACE)
    {
        TRACE("警告！坐标系%d缓冲区空间不足: %ld\n", crdNum, currentSpace);
        // 降低数据发送速度或暂停发送
        HandleWarningBufferSpace(crdNum, currentSpace);
    }
    else if(currentSpace > OPTIMAL_SPACE)
    {
        TRACE("信息：坐标系%d缓冲区空间充足: %ld\n", crdNum, currentSpace);
        // 可以恢复或提高数据发送速度
        HandleOptimalBufferSpace(crdNum, currentSpace);
    }
}
```

### 示例3：流量控制和数据发送策略
```cpp
// 根据缓冲区空间实现智能流量控制
bool SmartDataTransmission(int crdNum)
{
    long bufferSpace = 0;
    int iRes = g_MultiCard.MC_CrdSpace(crdNum, &bufferSpace, 0);
    if(iRes != 0) return false;

    // 定义发送策略
    struct TransmissionStrategy {
        long minSpace;
        long maxSendCount;
        long sendDelay;
        CString description;
    };

    TransmissionStrategy strategies[] = {
        {500, 100, 0,   "高速发送模式"},
        {200, 50,  10,  "正常发送模式"},
        {100, 20,  50,  "缓冲发送模式"},
        {50,  10,  100, "低速发送模式"},
        {10,  5,   500, "谨慎发送模式"}
    };

    // 选择合适的发送策略
    TransmissionStrategy* selectedStrategy = &strategies[4]; // 默认最谨慎策略
    for(int i = 0; i < 5; i++)
    {
        if(bufferSpace >= strategies[i].minSpace)
        {
            selectedStrategy = &strategies[i];
            break;
        }
    }

    TRACE("选择发送策略: %s (缓冲空间: %ld, 最大发送: %ld, 延迟: %ldms)\n",
          selectedStrategy->description, bufferSpace,
          selectedStrategy->maxSendCount, selectedStrategy->sendDelay);

    // 执行数据发送
    return ExecuteDataTransmission(crdNum, selectedStrategy);
}
```

### 示例4：多坐标系缓冲区管理
```cpp
// 统一管理多个坐标系的缓冲区空间
void ManageMultipleBufferSpaces()
{
    int crdCount = 3;
    long spaces[3] = {0};
    long totalSpace = 0;
    int healthyCount = 0;

    // 读取各坐标系空间
    for(int i = 0; i < crdCount; i++)
    {
        int iRes = g_MultiCard.MC_CrdSpace(i + 1, &spaces[i], 0);
        if(iRes == 0)
        {
            totalSpace += spaces[i];
            if(spaces[i] > 50)  // 健康阈值
            {
                healthyCount++;
            }
            TRACE("坐标系%d缓冲区空间: %ld\n", i + 1, spaces[i]);
        }
        else
        {
            TRACE("读取坐标系%d缓冲区空间失败\n", i + 1);
        }
    }

    // 系统状态评估
    if(healthyCount == crdCount)
    {
        TRACE("所有坐标系缓冲区状态良好，总空间: %ld\n", totalSpace);
        // 可以正常发送数据
    }
    else if(healthyCount > 0)
    {
        TRACE("部分坐标系缓冲区状态异常，健康率: %.1f%%\n",
              (double)healthyCount / crdCount * 100);
        // 调整发送策略，优先处理健康坐标系
    }
    else
    {
        TRACE("所有坐标系缓冲区空间不足，暂停数据发送\n");
        // 暂停所有数据发送
        PauseAllDataTransmission();
    }
}
```

### 示例5：缓冲区性能分析
```cpp
// 分析缓冲区使用性能和效率
void AnalyzeBufferPerformance(int crdNum)
{
    struct BufferRecord {
        long space;
        CTime timestamp;
    };

    static std::vector<BufferRecord> bufferHistory;
    long currentSpace = 0;
    int iRes = g_MultiCard.MC_CrdSpace(crdNum, &currentSpace, 0);
    if(iRes != 0) return;

    // 记录当前空间
    BufferRecord record;
    record.space = currentSpace;
    record.timestamp = CTime::GetCurrentTime();
    bufferHistory.push_back(record);

    // 保持历史记录不超过1000个
    if(bufferHistory.size() > 1000)
    {
        bufferHistory.erase(bufferHistory.begin());
    }

    // 分析缓冲区使用模式
    if(bufferHistory.size() >= 100)
    {
        long minSpace = LONG_MAX, maxSpace = 0, avgSpace = 0;
        long totalVariation = 0;
        int lowSpaceCount = 0;  // 低空间次数

        for(size_t i = 0; i < bufferHistory.size(); i++)
        {
            long space = bufferHistory[i].space;
            if(space < minSpace) minSpace = space;
            if(space > maxSpace) maxSpace = space;
            avgSpace += space;

            if(space < 50) lowSpaceCount++;

            if(i > 0)
            {
                totalVariation += abs(space - bufferHistory[i-1].space);
            }
        }

        avgSpace /= bufferHistory.size();
        double avgVariation = (double)totalVariation / (bufferHistory.size() - 1);
        double lowSpaceRate = (double)lowSpaceCount / bufferHistory.size() * 100;

        TRACE("缓冲区性能分析 - 最小: %ld, 最大: %ld, 平均: %ld\n",
              minSpace, maxSpace, avgSpace);
        TRACE("平均变化幅度: %.1f, 低空间率: %.1f%%\n",
              avgVariation, lowSpaceRate);

        // 性能建议
        if(lowSpaceRate > 30)
        {
            TRACE("建议：缓冲区经常不足，考虑降低数据发送频率\n");
        }
        if(avgVariation > 100)
        {
            TRACE("建议：缓冲区波动较大，可能需要优化数据发送策略\n");
        }
    }
}
```

### 示例6：缓冲区空间与运动同步
```cpp
// 确保缓冲区空间与运动状态同步
void SyncBufferWithMotion(int crdNum)
{
    long bufferSpace = 0;
    int iRes = g_MultiCard.MC_CrdSpace(crdNum, &bufferSpace, 0);
    if(iRes != 0) return;

    // 获取坐标系状态
    short crdStatus = 0;
    g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);

    bool isRunning = (crdStatus & CRDSYS_STATUS_PROG_RUN) != 0;

    // 状态同步检查
    if(isRunning && bufferSpace > 800)
    {
        TRACE("注意：坐标系正在运行但缓冲区空间过大(%ld)，可能数据供应不足\n",
              bufferSpace);
        // 可以尝试发送更多数据
        RequestMoreData(crdNum);
    }
    else if(!isRunning && bufferSpace < 100)
    {
        TRACE("注意：坐标系已停止但缓冲区空间过小(%ld)，可能数据未处理完成\n",
              bufferSpace);
        // 等待缓冲区清空或强制清空
        WaitForBufferClear(crdNum);
    }

    // 空间使用效率分析
    static long lastSpace = 0;
    long spaceChange = bufferSpace - lastSpace;
    if(isRunning)
    {
        // 运动中，空间应该逐渐减少
        if(spaceChange > 10)  // 空间意外增加
        {
            TRACE("警告：运动中缓冲区空间异常增加 %ld\n", spaceChange);
        }
    }

    lastSpace = bufferSpace;
}
```

## 参数映射表

| 应用场景 | nCrdNum | pSpace | nFifoIndex | 说明 |
|----------|---------|--------|------------|------|
| 单缓冲区监控 | 1-16 | 局部变量 | 0 | 监控单个坐标系缓冲区 |
| 多缓冲区监控 | 循环变量 | 数组元素 | 0 | 批量监控多个坐标系 |
| 流量控制 | 指定坐标系 | 状态变量 | 0 | 控制数据发送速度 |
| 性能分析 | 固定坐标系 | 历史数组 | 0 | 分析缓冲区使用效率 |

## 关键说明

1. **缓冲区空间**：
   - 返回插补缓冲区中剩余的空间段数
   - 空间大小影响可以发送的运动数据量
   - 空间不足时需要暂停或减慢数据发送

2. **流量控制**：
   - 根据剩余空间调整数据发送策略
   - 避免缓冲区溢出导致数据丢失
   - 确保运动数据的连续性和完整性

3. **监控意义**：
   - 预防缓冲区溢出和运动中断
   - 优化数据发送效率
   - 评估系统性能和稳定性

4. **空间阈值**：
   - **危险空间**：<10段，需要立即停止发送
   - **警告空间**：<50段，需要降低发送速度
   - **安全空间**：>100段，可以正常发送
   - **充足空间**：>500段，系统运行良好

5. **应用场景**：
   - **连续插补**：长路径运动的缓冲区管理
   - **高速运动**：大数据量的流量控制
   - **多轴协调**：复杂轨迹的缓冲区同步
   - **实时监控**：系统运行状态监控

6. **最佳实践**：
   - 建立空间监控和预警机制
   - 根据应用特点调整发送策略
   - 定期分析缓冲区使用效率
   - 与运动状态同步检查

## 函数区别

- **MC_CrdSpace() vs MC_GetLookAheadSpace()**: 插补缓冲区 vs 前瞻缓冲区
- **MC_CrdSpace() vs MC_GetCrdStatus()**: 空间信息 vs 状态信息
- **主缓冲区 vs 前瞻缓冲区**: 数据执行 vs 路径规划

---

**使用建议**: 在连续插补或高速运动应用中，建议建立缓冲区监控机制，实时跟踪空间变化。根据应用需求设置合适的空间阈值，实现智能流量控制。定期分析缓冲区使用模式有助于优化系统性能和避免运动中断。