# MC_GetCrdStatus 函数教程

## 函数原型

```c
int MC_GetCrdStatus(short nCrdNum, short* pnCrdStatus, short* pnFifoStatus)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| pnCrdStatus | short* | 坐标系状态指针 |
| pnFifoStatus | short* | FIFO状态指针（可选，可为NULL） |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本坐标系状态读取
```cpp
// 读取坐标系状态并更新界面显示
void UpdateCoordinateSystemStatus()
{
    short nCrdStatus = 0;
    int iRes = g_MultiCard.MC_GetCrdStatus(1, &nCrdStatus, NULL);

    if(iRes == 0)
    {
        // 检查坐标系报警状态
        if(nCrdStatus & CRDSYS_STATUS_ALARM)
        {
            ((CButton*)GetDlgItem(IDC_RADIO_CRD_ALARM))->SetCheck(true);
            TRACE("坐标系1报警\n");
        }
        else
        {
            ((CButton*)GetDlgItem(IDC_RADIO_CRD_ALARM))->SetCheck(false);
        }

        // 检查坐标系运行状态
        if(nCrdStatus & CRDSYS_STATUS_PROG_RUN)
        {
            ((CButton*)GetDlgItem(IDC_RADIO_CRD_RUNNING))->SetCheck(true);
            TRACE("坐标系1正在运行\n");
        }
        else
        {
            ((CButton*)GetDlgItem(IDC_RADIO_CRD_RUNNING))->SetCheck(false);
        }

        // 检查缓冲区执行完成状态
        if(nCrdStatus & CRDSYS_STATUS_FIFO_FINISH_0)
        {
            ((CButton*)GetDlgItem(IDC_RADIO_CRD_BUFFER_FINISH))->SetCheck(true);
        }
        else
        {
            ((CButton*)GetDlgItem(IDC_RADIO_CRD_BUFFER_FINISH))->SetCheck(false);
        }
    }
    else
    {
        TRACE("读取坐标系状态失败\n");
    }
}
```

### 示例2：多坐标系状态监控
```cpp
// 监控多个坐标系的状态
void MonitorMultipleCoordinateSystems()
{
    int crdCount = 3;  // 假设有3个坐标系
    short crdStatus[3] = {0};

    // 读取各坐标系状态
    for(int i = 0; i < crdCount; i++)
    {
        int iRes = g_MultiCard.MC_GetCrdStatus(i+1, &crdStatus[i], NULL);
        if(iRes == 0)
        {
            TRACE("坐标系%d状态: 0x%04X\n", i+1, crdStatus[i]);

            // 分析状态
            AnalyzeCrdStatus(i+1, crdStatus[i]);
        }
        else
        {
            TRACE("读取坐标系%d状态失败\n", i+1);
        }
    }

    // 统计运行中的坐标系数量
    int runningCount = 0;
    for(int i = 0; i < crdCount; i++)
    {
        if(crdStatus[i] & CRDSYS_STATUS_PROG_RUN)
        {
            runningCount++;
        }
    }
    TRACE("当前运行中的坐标系数量: %d\n", runningCount);
}
```

### 示例3：坐标系状态变化检测
```cpp
// 检测坐标系状态变化
void DetectCrdStatusChanges()
{
    static short lastCrdStatus[16] = {0};
    short currentCrdStatus = 0;

    // 读取坐标系1状态
    int iRes = g_MultiCard.MC_GetCrdStatus(1, &currentCrdStatus, NULL);
    if(iRes != 0) return;

    // 检查状态变化
    short statusChanges = lastCrdStatus[0] ^ currentCrdStatus;

    if(statusChanges != 0)
    {
        // 检查报警状态变化
        if(statusChanges & CRDSYS_STATUS_ALARM)
        {
            if(currentCrdStatus & CRDSYS_STATUS_ALARM)
            {
                TRACE("坐标系1进入报警状态\n");
                HandleCrdAlarm(1, true);
            }
            else
            {
                TRACE("坐标系1报警状态清除\n");
                HandleCrdAlarm(1, false);
            }
        }

        // 检查运行状态变化
        if(statusChanges & CRDSYS_STATUS_PROG_RUN)
        {
            if(currentCrdStatus & CRDSYS_STATUS_PROG_RUN)
            {
                TRACE("坐标系1开始运行\n");
                HandleCrdStart(1);
            }
            else
            {
                TRACE("坐标系1停止运行\n");
                HandleCrdStop(1);
            }
        }

        // 检查缓冲区完成状态变化
        if(statusChanges & CRDSYS_STATUS_FIFO_FINISH_0)
        {
            if(currentCrdStatus & CRDSYS_STATUS_FIFO_FINISH_0)
            {
                TRACE("坐标系1缓冲区执行完成\n");
                HandleCrdBufferComplete(1);
            }
        }

        lastCrdStatus[0] = currentCrdStatus;
    }
}
```

### 示例4：坐标系安全检查
```cpp
// 检查坐标系是否处于安全状态
bool IsCoordinateSystemSafe(int crdNum)
{
    short crdStatus = 0;
    int iRes = g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);

    if(iRes != 0)
    {
        TRACE("无法读取坐标系%d状态\n", crdNum);
        return false;
    }

    // 检查报警状态
    if(crdStatus & CRDSYS_STATUS_ALARM)
    {
        TRACE("坐标系%d处于报警状态，不安全\n", crdNum);
        return false;
    }

    // 可以根据需要添加其他安全检查
    // 例如：检查限位状态、轴状态等

    TRACE("坐标系%d状态安全\n", crdNum);
    return true;
}
```

### 示例5：坐标系运行状态管理
```cpp
// 管理坐标系的运行状态
void ManageCrdOperation(int crdNum)
{
    short crdStatus = 0;
    int iRes = g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);

    if(iRes != 0)
    {
        TRACE("无法读取坐标系%d状态\n", crdNum);
        return;
    }

    // 根据状态执行相应操作
    if(crdStatus & CRDSYS_STATUS_ALARM)
    {
        // 处理报警状态
        HandleCrdAlarm(crdNum, true);

        // 尝试清除报警
        AttemptToClearCrdAlarm(crdNum);
    }
    else if(crdStatus & CRDSYS_STATUS_PROG_RUN)
    {
        // 坐标系正在运行，监控进度
        MonitorCrdProgress(crdNum);
    }
    else if(crdStatus & CRDSYS_STATUS_FIFO_FINISH_0)
    {
        // 缓冲区执行完成
        HandleCrdCompletion(crdNum);
    }
    else
    {
        // 坐标系空闲
        TRACE("坐标系%d空闲，可以接收新指令\n", crdNum);
    }
}
```

### 示例6：完整坐标系状态报告
```cpp
// 生成完整的坐标系状态报告
void GenerateCrdStatusReport()
{
    short crdStatus = 0;
    short fifoStatus = 0;
    int iRes = g_MultiCard.MC_GetCrdStatus(1, &crdStatus, &fifoStatus);

    if(iRes != 0)
    {
        TRACE("无法生成坐标系状态报告\n");
        return;
    }

    TRACE("=== 坐标系1状态报告 ===\n");
    TRACE("状态值: 0x%04X\n", crdStatus);
    TRACE("FIFO状态: 0x%04X\n", fifoStatus);

    // 解析各个状态位
    TRACE("报警状态: %s\n", (crdStatus & CRDSYS_STATUS_ALARM) ? "报警" : "正常");
    TRACE("运行状态: %s\n", (crdStatus & CRDSYS_STATUS_PROG_RUN) ? "运行中" : "空闲");
    TRACE("缓冲区完成: %s\n", (crdStatus & CRDSYS_STATUS_FIFO_FINISH_0) ? "完成" : "执行中");

    // 可以添加更多状态信息解析
    // TRACE("=== 报告结束 ===\n");
}
```

## 参数映射表

| 应用场景 | nCrdNum | pnCrdStatus | pnFifoStatus | 说明 |
|----------|---------|-------------|--------------|------|
| 单坐标系监控 | 1-16 | 局部变量 | NULL | 监控单个坐标系 |
| 多坐标系监控 | 循环变量 | 数组 | NULL | 批量监控多个坐标系 |
| 状态变化检测 | 固定坐标系 | 静态变量 | NULL | 检测状态跳变 |
| 完整状态读取 | 坐标系号 | 局部变量 | 局部变量 | 获取完整状态信息 |

## 关键说明

1. **坐标系状态位**：
   - **CRDSYS_STATUS_ALARM**：坐标系报警
   - **CRDSYS_STATUS_PROG_RUN**：程序运行中
   - **CRDSYS_STATUS_FIFO_FINISH_0**：FIFO-0缓冲区完成
   - 其他状态位需要参考具体硬件手册

2. **FIFO状态**：
   - 反映缓冲区的执行状态
   - 可以检查数据缓冲情况
   - 用于优化数据下发策略

3. **使用时机**：
   - **状态监控**：定期检查坐标系状态
   - **错误处理**：检测和处理报警状态
   - **进度跟踪**：监控程序执行进度
   - **系统诊断**：分析系统运行状态

4. **状态含义**：
   - **报警**：系统出现错误或异常
   - **运行**：正在执行插补运动
   - **完成**：缓冲区数据执行完毕
   - **空闲**：无运动任务

5. **注意事项**：
   - 定期检查状态以及时发现问题
   - 报警状态需要立即处理
   - 状态读取失败时需要处理错误
   - 注意状态变化的时序关系

6. **应用场景**：
   - **生产线监控**：实时监控运动状态
   - **故障诊断**：快速定位问题原因
   - **性能优化**：分析系统运行效率
   - **安全保护**：确保系统安全运行

## 函数区别

- **MC_GetCrdStatus() vs MC_GetAllSysStatusSX()**: 坐标系状态 vs 完整系统状态
- **MC_GetCrdStatus() vs MC_GetCrdPos()**: 状态信息 vs 位置信息
- **坐标系 vs 轴**: 坐标系级状态 vs 单轴状态

---

**使用建议**: 在关键应用中，建议建立状态监控机制，及时发现和处理异常状态。对于报警状态，需要有明确的处理流程和恢复策略。定期记录状态变化有助于系统维护和故障分析。