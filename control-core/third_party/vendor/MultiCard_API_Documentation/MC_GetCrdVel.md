# MC_GetCrdVel 函数教程

## 函数原型

```c
int MC_GetCrdVel(short nCrdNum, double* pSynVel)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| pSynVel | double* | 速度指针，用于接收坐标系的当前合成速度 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本速度读取
```cpp
// 在定时器中读取坐标系当前速度
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    double dCrdVel;
    int iRes = g_MultiCard.MC_GetCrdVel(1, &dCrdVel);

    if(iRes == 0)
    {
        CString strText;
        strText.Format("%.3f", dCrdVel);
        GetDlgItem(IDC_STATIC_CRD_VEL)->SetWindowText(strText);
    }
}
```

### 示例2：速度状态监控
```cpp
// 监控坐标系运动状态和速度变化
void MonitorCoordinateVelocity()
{
    double currentVel = 0;
    static double lastVel = 0;
    static CTime lastVelChangeTime = CTime::GetCurrentTime();

    int iRes = g_MultiCard.MC_GetCrdVel(1, &currentVel);
    if(iRes != 0) return;

    // 检测速度变化
    double velChange = fabs(currentVel - lastVel);
    if(velChange > 0.01)  // 速度变化阈值
    {
        CTime currentTime = CTime::GetCurrentTime();
        CTimeSpan timeDiff = currentTime - lastVelChangeTime;

        TRACE("速度发生变化: %.3f -> %.3f (时间间隔: %dms)\n",
              lastVel, currentVel, timeDiff.GetTotalMilliseconds());

        lastVel = currentVel;
        lastVelChangeTime = currentTime;

        // 触发速度变化事件
        OnVelocityChanged(lastVel, currentVel);
    }

    // 判断运动状态
    if(currentVel > 0.1)
    {
        TRACE("坐标系正在运动，速度: %.3f\n", currentVel);
    }
    else
    {
        TRACE("坐标系静止或速度为0\n");
    }
}
```

### 示例3：速度曲线分析
```cpp
// 分析坐标系运动的速度特性
void AnalyzeVelocityProfile(int crdNum)
{
    struct VelocityData {
        double velocity;
        CTime timestamp;
    };

    static std::vector<VelocityData> velHistory;
    double currentVel = 0;
    int iRes = g_MultiCard.MC_GetCrdVel(crdNum, &currentVel);
    if(iRes != 0) return;

    // 记录当前速度
    VelocityData data;
    data.velocity = currentVel;
    data.timestamp = CTime::GetCurrentTime();
    velHistory.push_back(data);

    // 保持历史数据不超过1000个点
    if(velHistory.size() > 1000)
    {
        velHistory.erase(velHistory.begin());
    }

    // 分析速度特性
    if(velHistory.size() >= 10)
    {
        double maxVel = 0, minVel = 0, avgVel = 0;
        int movingCount = 0;

        for(size_t i = 0; i < velHistory.size(); i++)
        {
            double vel = velHistory[i].velocity;
            if(vel > maxVel) maxVel = vel;
            if(i == 0) minVel = vel;
            else if(vel < minVel) minVel = vel;

            if(vel > 0.01)  // 运动状态
            {
                avgVel += vel;
                movingCount++;
            }
        }

        if(movingCount > 0)
        {
            avgVel /= movingCount;
        }

        TRACE("速度分析 - 最大: %.3f, 最小: %.3f, 平均: %.3f, 运动率: %.1f%%\n",
              maxVel, minVel, avgVel, (double)movingCount / velHistory.size() * 100);
    }
}
```

### 示例4：速度控制优化
```cpp
// 根据当前速度优化运动参数
void OptimizeVelocityControl(int crdNum)
{
    double currentVel = 0;
    int iRes = g_MultiCard.MC_GetCrdVel(crdNum, &currentVel);
    if(iRes != 0) return;

    // 获取坐标系状态
    short crdStatus = 0;
    g_MultiCard.MC_GetCrdStatus(crdNum, &crdStatus, NULL);

    if(crdStatus & CRDSYS_STATUS_PROG_RUN)
    {
        // 运动中的速度调整策略
        if(currentVel > 1000)  // 速度过高
        {
            TRACE("检测到高速运动 (%.3f)，建议降低速度\n", currentVel);
            // 可以通过MC_SetOverride调整速度
            double newOverride = 0.8;  // 降低到80%
            g_MultiCard.MC_SetOverride(crdNum, newOverride);
        }
        else if(currentVel < 10)  // 速度过低
        {
            TRACE("检测到低速运动 (%.3f)，可能需要调整参数\n", currentVel);
        }

        // 检查速度稳定性
        static double lastVel = 0;
        double velFluctuation = fabs(currentVel - lastVel);
        if(velFluctuation > currentVel * 0.1)  // 速度波动超过10%
        {
            TRACE("速度波动较大: %.3f -> %.3f (波动: %.1f%%)\n",
                  lastVel, currentVel, velFluctuation / currentVel * 100);
        }
        lastVel = currentVel;
    }
}
```

### 示例5：多坐标系速度同步监控
```cpp
// 监控多个坐标系的速度同步性
void MonitorVelocitySynchronization()
{
    int crdCount = 3;
    double velocities[3] = {0};
    double avgVel = 0;
    int movingCount = 0;

    // 读取各坐标系速度
    for(int i = 0; i < crdCount; i++)
    {
        int iRes = g_MultiCard.MC_GetCrdVel(i + 1, &velocities[i]);
        if(iRes == 0 && velocities[i] > 0.01)
        {
            avgVel += velocities[i];
            movingCount++;
        }
    }

    if(movingCount > 1)
    {
        avgVel /= movingCount;

        // 检查速度一致性
        double maxDeviation = 0;
        for(int i = 0; i < crdCount; i++)
        {
            if(velocities[i] > 0.01)
            {
                double deviation = fabs(velocities[i] - avgVel);
                if(deviation > maxDeviation)
                {
                    maxDeviation = deviation;
                }

                if(deviation > avgVel * 0.05)  // 偏差超过5%
                {
                    TRACE("坐标系%d速度偏差较大: %.3f (平均: %.3f, 偏差: %.1f%%)\n",
                          i + 1, velocities[i], avgVel, deviation / avgVel * 100);
                }
            }
        }

        TRACE("速度同步状态 - 平均速度: %.3f, 最大偏差: %.3f (%.1f%%)\n",
              avgVel, maxDeviation, avgVel > 0 ? maxDeviation / avgVel * 100 : 0);
    }
}
```

### 示例6：速度预警系统
```cpp
// 坐标系速度预警和安全检查
void VelocitySafetyCheck(int crdNum)
{
    double currentVel = 0;
    int iRes = g_MultiCard.MC_GetCrdVel(crdNum, &currentVel);
    if(iRes != 0) return;

    // 定义速度阈值
    const double WARNING_SPEED = 800.0;    // 警告速度
    const double CRITICAL_SPEED = 1200.0;  // 危险速度
    const double STOP_SPEED = 0.1;         // 停止速度阈值

    // 检查超速情况
    if(currentVel > CRITICAL_SPEED)
    {
        TRACE("危险！坐标系%d速度超限: %.3f > %.3f\n", crdNum, currentVel, CRITICAL_SPEED);
        // 执行紧急停止
        HandleEmergencyStop(crdNum);
    }
    else if(currentVel > WARNING_SPEED)
    {
        TRACE("警告！坐标系%d速度过高: %.3f > %.3f\n", crdNum, currentVel, WARNING_SPEED);
        // 发出警告
        TriggerSpeedWarning(crdNum, currentVel);
    }

    // 检查异常停止
    static bool wasMoving = false;
    bool isMoving = currentVel > STOP_SPEED;

    if(wasMoving && !isMoving)
    {
        TRACE("坐标系%d意外停止，当前速度: %.3f\n", crdNum, currentVel);
        CheckUnexpectedStopReason(crdNum);
    }
    wasMoving = isMoving;

    // 记录速度统计
    UpdateVelocityStatistics(crdNum, currentVel);
}
```

## 参数映射表

| 应用场景 | nCrdNum | pSynVel | 说明 |
|----------|---------|---------|------|
| 单速度监控 | 1-16 | 局部变量 | 监控单个坐标系速度 |
| 多速度监控 | 循环变量 | 数组元素 | 批量监控多个坐标系 |
| 速度分析 | 固定坐标系 | 历史数组 | 用于速度曲线分析 |
| 安全检查 | 指定坐标系 | 比较变量 | 速度预警和安全控制 |

## 关键说明

1. **合成速度**：
   - 返回坐标系各轴的合成速度
   - 速度单位与坐标系配置一致（通常为脉冲/毫秒或毫米/秒）
   - 反映整体运动速度而非单轴速度

2. **读取时机**：
   - 可以在定时器中定期读取，实现实时速度监控
   - 运动过程中读取获取当前速度
   - 配合位置信息计算运动特性

3. **速度状态**：
   - 速度为0表示静止或未运动
   - 正值表示正向运动（具体方向需结合其他信息判断）
   - 速度变化反映加减速过程

4. **应用场景**：
   - **实时监控**：显示当前运动速度
   - **性能分析**：分析速度曲线和运动特性
   - **安全保护**：速度超限检测和预警
   - **同步控制**：多坐标系速度同步监控

5. **注意事项**：
   - 频繁读取可能影响系统性能
   - 速度数据有延迟，不是完全实时的
   - 需要结合位置和状态信息综合判断

6. **性能优化**：
   - 根据实际需要调整读取频率
   - 使用变化阈值减少不必要的处理
   - 批量处理多个坐标系的速度数据

## 函数区别

- **MC_GetCrdVel() vs MC_GetAxisVel()**: 坐标系合成速度 vs 单轴速度
- **MC_GetCrdVel() vs MC_GetCrdPos()**: 速度信息 vs 位置信息
- **MC_GetCrdVel() vs MC_GetCrdStatus()**: 速度数据 vs 状态数据

---

**使用建议**: 在需要速度控制或监控的应用中，建议建立速度监控系统，实时跟踪运动速度变化。对于高速应用，应该设置速度预警机制，防止超速运行。定期分析速度数据有助于优化运动参数和提高系统性能。