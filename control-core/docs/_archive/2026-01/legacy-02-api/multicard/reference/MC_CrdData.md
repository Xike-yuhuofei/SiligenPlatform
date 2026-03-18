# MC_CrdData 函数教程

## 函数原型

```c
int MC_CrdData(short nCrdNum, void* pCrdData, short nFifoIndex)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| pCrdData | void* | 坐标系数据指针，通常为NULL作为结束标识 |
| nFifoIndex | short | FIFO索引，通常为0 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本数据流结束标识
```cpp
// 发送插补数据后的结束标识
void CDemoVCDlg::OnBnClickedButtonSendData()
{
    int iRes = 0;

    // ... 在此之前已发送多条插补数据（MC_LnXY, MC_ArcXYC等）

    // 插入最后一行标识符（系统会把前瞻缓冲区数据全部压入板卡）
    iRes += g_MultiCard.MC_CrdData(1, NULL, 0);

    if(0 == iRes)
    {
        AfxMessageBox("插补数据发送成功！");
        TRACE("坐标系1数据流结束标识发送成功\n");
    }
    else
    {
        AfxMessageBox("插补数据发送失败！");
        TRACE("坐标系1数据流结束标识发送失败，错误代码: %d\n", iRes);
    }
}
```

### 示例2：完整的数据发送流程
```cpp
// 完整的坐标系插补数据发送流程
bool CompleteInterpolationDataSending(int crdNum)
{
    int totalResult = 0;

    // 1. 清空原有数据
    g_MultiCard.MC_CrdClear(crdNum, 0);
    TRACE("清空坐标系%d缓冲区\n", crdNum);

    // 2. 发送直线插补数据
    totalResult += g_MultiCard.MC_LnXY(crdNum, 10000, 20000, 50, 0.2, 0, 0, 1);
    TRACE("发送直线1: (10000, 20000)\n");

    totalResult += g_MultiCard.MC_LnXY(crdNum, 30000, 30000, 50, 0.2, 0, 0, 2);
    TRACE("发送直线2: (30000, 30000)\n");

    // 3. 发送圆弧插补数据
    totalResult += g_MultiCard.MC_ArcXYC(crdNum, 40000, 30000, 35000, 25000,
                                         50, 0.2, 0, 0, 3, 0);
    TRACE("发送圆弧: 终点(40000, 30000), 圆心(35000, 25000)\n");

    // 4. 发送更多直线数据
    totalResult += g_MultiCard.MC_LnXY(crdNum, 20000, 40000, 50, 0.2, 0, 0, 4);
    TRACE("发送直线3: (20000, 40000)\n");

    // 5. 发送数据流结束标识
    totalResult += g_MultiCard.MC_CrdData(crdNum, NULL, 0);
    TRACE("发送数据流结束标识\n");

    // 6. 检查发送结果
    if(totalResult == 0)
    {
        TRACE("坐标系%d所有插补数据发送成功\n", crdNum);
        return true;
    }
    else
    {
        TRACE("坐标系%d插补数据发送失败，累积错误: %d\n", crdNum, totalResult);
        return false;
    }
}
```

### 示例3：批量数据发送管理
```cpp
// 批量管理和发送插补数据
class BatchDataManager {
private:
    struct InterpolationCommand {
        int type;        // 命令类型：1=直线，2=圆弧
        long x, y, z;    // 目标坐标
        long cx, cy;     // 圆心坐标（圆弧用）
        double vel, acc; // 速度和加速度
        int segmentNum;  // 段号
    };

    std::vector<InterpolationCommand> m_commands;
    int m_crdNum;

public:
    BatchDataManager(int crdNum) : m_crdNum(crdNum) {}

    void AddLine(long x, long y, double vel, double acc, int segNum) {
        InterpolationCommand cmd;
        cmd.type = 1;
        cmd.x = x; cmd.y = y; cmd.z = 0;
        cmd.vel = vel; cmd.acc = acc;
        cmd.segmentNum = segNum;
        m_commands.push_back(cmd);
    }

    void AddArc(long x, long y, long cx, long cy, double vel, double acc, int segNum) {
        InterpolationCommand cmd;
        cmd.type = 2;
        cmd.x = x; cmd.y = y; cmd.z = 0;
        cmd.cx = cx; cmd.cy = cy;
        cmd.vel = vel; cmd.acc = acc;
        cmd.segmentNum = segNum;
        m_commands.push_back(cmd);
    }

    bool SendAllCommands() {
        int totalResult = 0;

        // 清空缓冲区
        g_MultiCard.MC_CrdClear(m_crdNum, 0);

        // 发送所有命令
        for(size_t i = 0; i < m_commands.size(); i++) {
            const InterpolationCommand& cmd = m_commands[i];

            if(cmd.type == 1) {
                // 发送直线命令
                totalResult += g_MultiCard.MC_LnXY(m_crdNum, cmd.x, cmd.y,
                                                   cmd.vel, cmd.acc, 0, 0, cmd.segmentNum);
                TRACE("发送直线%d: (%ld, %ld), 段号: %d\n",
                      i+1, cmd.x, cmd.y, cmd.segmentNum);
            } else if(cmd.type == 2) {
                // 发送圆弧命令
                totalResult += g_MultiCard.MC_ArcXYC(m_crdNum, cmd.x, cmd.y,
                                                      cmd.cx, cmd.cy, cmd.vel, cmd.acc,
                                                      0, 0, cmd.segmentNum, 0);
                TRACE("发送圆弧%d: 终点(%ld, %ld), 圆心(%ld, %ld), 段号: %d\n",
                      i+1, cmd.x, cmd.y, cmd.cx, cmd.cy, cmd.segmentNum);
            }

            // 检查缓冲区空间
            long bufferSpace = 0;
            g_MultiCard.MC_CrdSpace(m_crdNum, &bufferSpace, 0);
            if(bufferSpace < 20) {
                TRACE("警告：缓冲区空间不足: %ld\n", bufferSpace);
                break;
            }
        }

        // 发送结束标识
        totalResult += g_MultiCard.MC_CrdData(m_crdNum, NULL, 0);
        TRACE("发送数据流结束标识，总共发送%zu条命令\n", m_commands.size());

        return totalResult == 0;
    }

    void ClearCommands() {
        m_commands.clear();
    }
};

// 使用示例
void UseBatchDataManager() {
    BatchDataManager batchManager(1);

    // 添加一系列插补命令
    batchManager.AddLine(10000, 0, 50, 0.2, 1);
    batchManager.AddLine(10000, 10000, 50, 0.2, 2);
    batchManager.AddArc(0, 10000, 5000, 5000, 50, 0.2, 3);
    batchManager.AddLine(0, 0, 50, 0.2, 4);

    // 批量发送
    if(batchManager.SendAllCommands()) {
        AfxMessageBox("批量数据发送成功！");
    }
}
```

### 示例4：条件性数据发送
```cpp
// 根据条件决定是否发送数据流结束标识
bool ConditionalDataSending(int crdNum)
{
    // 检查当前系统状态
    short crdStatus = 0;
    g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);

    // 检查缓冲区状态
    long crdSpace = 0;
    g_MultiCard.MC_CrdSpace(crdNum, &crdSpace, 0);

    // 条件判断
    if(crdStatus & CRDSYS_STATUS_PROG_RUN) {
        TRACE("坐标系正在运行，暂不发送结束标识\n");
        return false;
    }

    if(crdSpace < 10) {
        TRACE("缓冲区空间过小，可能数据未发送成功\n");
        return false;
    }

    // 发送数据流结束标识
    int iRes = g_MultiCard.MC_CrdData(crdNum, NULL, 0);
    if(iRes == 0) {
        TRACE("条件满足，发送数据流结束标识成功\n");
        return true;
    } else {
        TRACE("发送数据流结束标识失败\n");
        return false;
    }
}
```

### 示例5：分批数据发送策略
```cpp
// 大量数据的分批发送策略
bool SendDataInBatches(int crdNum, int totalCommands, int batchSize)
{
    int totalResult = 0;
    int sentCommands = 0;

    while(sentCommands < totalCommands) {
        int currentBatch = min(batchSize, totalCommands - sentCommands);

        TRACE("开始发送第%d批数据，数量: %d\n",
              (sentCommands / batchSize) + 1, currentBatch);

        // 发送当前批次的数据
        for(int i = 0; i < currentBatch; i++) {
            // 这里简化处理，实际应该根据具体命令类型发送
            totalResult += g_MultiCard.MC_LnXY(crdNum,
                                               sentCommands * 1000,
                                               sentCommands * 1000,
                                               50, 0.2, 0, 0,
                                               sentCommands + i + 1);
            sentCommands++;
        }

        // 检查发送结果
        if(totalResult != 0) {
            TRACE("批次发送失败，累积错误: %d\n", totalResult);
            return false;
        }

        // 检查缓冲区空间
        long bufferSpace = 0;
        g_MultiCard.MC_CrdSpace(crdNum, &bufferSpace, 0);
        TRACE("批次发送完成，剩余缓冲区空间: %ld\n", bufferSpace);

        // 如果不是最后一批，等待一段时间
        if(sentCommands < totalCommands) {
            Sleep(100);  // 等待100ms
        }
    }

    // 所有数据发送完毕，发送结束标识
    totalResult += g_MultiCard.MC_CrdData(crdNum, NULL, 0);
    TRACE("所有数据发送完成，发送结束标识\n");

    return totalResult == 0;
}
```

### 示例6：数据发送错误处理和恢复
```cpp
// 带错误处理和恢复机制的数据发送
bool RobustDataSending(int crdNum)
{
    int maxRetries = 3;
    int retryCount = 0;

    while(retryCount < maxRetries) {
        int totalResult = 0;

        try {
            // 清空缓冲区
            g_MultiCard.MC_CrdClear(crdNum, 0);

            // 发送测试数据
            totalResult += g_MultiCard.MC_LnXY(crdNum, 1000, 1000, 30, 0.1, 0, 0, 1);
            totalResult += g_MultiCard.MC_LnXY(crdNum, 2000, 2000, 30, 0.1, 0, 0, 2);
            totalResult += g_MultiCard.MC_LnXY(crdNum, 1000, 2000, 30, 0.1, 0, 0, 3);

            // 发送结束标识
            totalResult += g_MultiCard.MC_CrdData(crdNum, NULL, 0);

            if(totalResult == 0) {
                TRACE("数据发送成功，重试次数: %d\n", retryCount);
                return true;
            } else {
                TRACE("数据发送失败，错误代码: %d\n", totalResult);
                throw std::runtime_error("数据发送失败");
            }

        } catch(const std::exception& e) {
            retryCount++;
            TRACE("第%d次重试: %s\n", retryCount, e.what());

            // 重置系统状态
            ResetCoordinateSystem(crdNum);

            if(retryCount >= maxRetries) {
                TRACE("达到最大重试次数，发送失败\n");
                return false;
            }

            // 等待后重试
            Sleep(1000 * retryCount);
        }
    }

    return false;
}

void ResetCoordinateSystem(int crdNum) {
    // 停止坐标系
    g_MultiCard.MC_CrdStop(1 << (crdNum - 1), 0);

    // 清空缓冲区
    g_MultiCard.MC_CrdClear(crdNum, 0);

    // 重新初始化（如果需要）
    // g_MultiCard.MC_InitLookAhead(crdNum, 0, &lookAheadPrm);

    TRACE("重置坐标系%d完成\n", crdNum);
}
```

## 参数映射表

| 应用场景 | nCrdNum | pCrdData | nFifoIndex | 说明 |
|----------|---------|----------|------------|------|
| 结束标识 | 1-16 | NULL | 0 | 数据流结束标识 |
| 数据发送 | 坐标系号 | 数据指针 | 0 | 发送特定数据结构 |
| 批量管理 | 固定坐标系 | NULL | 0 | 批量发送后结束 |
| 错误恢复 | 指定坐标系 | NULL | 0 | 重置后结束标识 |

## 关键说明

1. **结束标识功能**：
   - pCrdData为NULL时作为数据流结束标识
   - 强制将前瞻缓冲区数据压入执行队列
   - 确保所有运动指令都被执行

2. **数据流程**：
   - 先发送各种插补指令（直线、圆弧等）
   - 最后必须调用MC_CrdData(NULL)作为结束
   - 没有结束标识可能导致运动不执行

3. **前瞻缓冲区**：
   - 结束标识触发前瞻处理
   - 进行速度规划和优化
   - 将数据转移到执行缓冲区

4. **使用时机**：
   - 所有插补数据发送完毕后
   - 需要立即执行运动时
   - 切换运动任务前

5. **错误处理**：
   - 检查每次发送的返回值
   - 失败时需要重新发送
   - 可能需要清空缓冲区重试

6. **注意事项**：
   - 每次数据发送都必须有结束标识
   - 结束标识前确保所有数据已发送
   - 避免在运动过程中多次发送结束标识

## 函数区别

- **MC_CrdData() vs MC_LnXY()**: 结束标识 vs 直线插补数据
- **MC_CrdData() vs MC_CrdClear()**: 数据结束 vs 缓冲区清空
- **结束标识 vs 普通数据**: NULL指针 vs 有效数据指针

---

**使用建议**: 在发送插补数据时，务必确保在所有数据发送完成后调用MC_CrdData(NULL)作为结束标识。建议建立完整的数据发送流程管理，包括错误检测、重试机制和状态验证。对于批量数据发送，可以采用分批策略提高可靠性。