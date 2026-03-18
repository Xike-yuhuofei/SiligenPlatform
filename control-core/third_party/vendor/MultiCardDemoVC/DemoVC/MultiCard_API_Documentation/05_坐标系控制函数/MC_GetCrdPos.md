# MC_GetCrdPos 函数教程

## 函数原型

```c
int MC_GetCrdPos(short nCrdNum, double* pPos)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| pPos | double* | 位置数组指针，用于接收各轴的当前位置 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本位置读取和显示
```cpp
// 在定时器中实时显示坐标系XYZ轴位置
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    double dPos[8];
    int iRes = g_MultiCard.MC_GetCrdPos(1, dPos);

    if(iRes == 0)
    {
        CString strText;

        // 显示X轴位置
        strText.Format("X:%.3f", dPos[0]);
        GetDlgItem(IDC_STATIC_POS_x)->SetWindowText(strText);

        // 显示Y轴位置
        strText.Format("Y:%.3f", dPos[1]);
        GetDlgItem(IDC_STATIC_POS_Y)->SetWindowText(strText);

        // 显示Z轴位置
        strText.Format("Z:%.3f", dPos[2]);
        GetDlgItem(IDC_STATIC_POS_Z)->SetWindowText(strText);
    }
}
```

### 示例2：多坐标系位置监控
```cpp
// 监控多个坐标系的当前位置
void MonitorMultipleCoordinatePositions()
{
    int crdCount = 3;
    double positions[3][8] = {0};

    for(int i = 0; i < crdCount; i++)
    {
        int iRes = g_MultiCard.MC_GetCrdPos(i + 1, positions[i]);
        if(iRes == 0)
        {
            TRACE("坐标系%d位置: X=%.3f, Y=%.3f, Z=%.3f\n",
                  i + 1, positions[i][0], positions[i][1], positions[i][2]);
        }
        else
        {
            TRACE("读取坐标系%d位置失败\n", i + 1);
        }
    }
}
```

### 示例3：位置变化检测
```cpp
// 检测坐标系位置变化
void DetectPositionChanges()
{
    static double lastPos[8] = {0};
    double currentPos[8] = {0};
    double threshold = 0.001;  // 位置变化阈值

    int iRes = g_MultiCard.MC_GetCrdPos(1, currentPos);
    if(iRes != 0) return;

    // 检查各轴位置变化
    for(int i = 0; i < 3; i++)  // 检查XYZ三轴
    {
        double delta = fabs(currentPos[i] - lastPos[i]);
        if(delta > threshold)
        {
            TRACE("轴%d位置发生变化: %.3f -> %.3f (变化量: %.3f)\n",
                  i + 1, lastPos[i], currentPos[i], delta);

            // 触发位置变化事件
            OnPositionChanged(i + 1, lastPos[i], currentPos[i]);
        }
    }

    // 更新上次位置记录
    memcpy(lastPos, currentPos, sizeof(lastPos));
}
```

### 示例4：原点位置校准
```cpp
// 检查坐标系是否回到原点
bool CheckCoordinateSystemHome(int crdNum)
{
    double currentPos[8] = {0};
    int iRes = g_MultiCard.MC_GetCrdPos(crdNum, currentPos);

    if(iRes != 0)
    {
        TRACE("无法读取坐标系%d位置\n", crdNum);
        return false;
    }

    // 检查各轴是否接近原点（误差±0.1）
    double tolerance = 0.1;
    for(int i = 0; i < 3; i++)  // XYZ三轴
    {
        if(fabs(currentPos[i]) > tolerance)
        {
            TRACE("轴%d未回到原点，当前位置: %.3f\n", i + 1, currentPos[i]);
            return false;
        }
    }

    TRACE("坐标系%d已回到原点位置\n", crdNum);
    return true;
}
```

### 示例5：运动进度跟踪
```cpp
// 跟踪运动进度，计算当前位置到目标位置的距离
void TrackMotionProgress(int crdNum, double targetX, double targetY, double targetZ)
{
    double currentPos[8] = {0};
    int iRes = g_MultiCard.MC_GetCrdPos(crdNum, currentPos);

    if(iRes != 0) return;

    // 计算到目标位置的距离
    double deltaX = targetX - currentPos[0];
    double deltaY = targetY - currentPos[1];
    double deltaZ = targetZ - currentPos[2];

    double totalDistance = sqrt(deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ);
    double progress = 0.0;

    // 如果有初始距离，可以计算进度百分比
    static double initialDistance = -1.0;
    if(initialDistance < 0)
    {
        initialDistance = totalDistance;
    }

    if(initialDistance > 0)
    {
        progress = (1.0 - totalDistance / initialDistance) * 100.0;
    }

    TRACE("当前位置: (%.3f, %.3f, %.3f), 剩余距离: %.3f, 进度: %.1f%%\n",
          currentPos[0], currentPos[1], currentPos[2], totalDistance, progress);

    // 检查是否到达目标位置
    if(totalDistance < 0.1)  // 到达误差范围
    {
        TRACE("运动完成，已到达目标位置\n");
        initialDistance = -1.0;  // 重置初始距离
        OnMotionComplete(crdNum);
    }
}
```

### 示例6：位置数据记录
```cpp
// 记录坐标系运动轨迹
void RecordPositionTrajectory(int crdNum, int durationMs)
{
    static CTime startTime;
    static bool isRecording = false;

    if(!isRecording)
    {
        startTime = CTime::GetCurrentTime();
        isRecording = true;
        TRACE("开始记录坐标系%d运动轨迹\n", crdNum);
        return;
    }

    double currentPos[8] = {0};
    int iRes = g_MultiCard.MC_GetCrdPos(crdNum, currentPos);
    if(iRes != 0) return;

    CTime currentTime = CTime::GetCurrentTime();
    CTimeSpan elapsedTime = currentTime - startTime;

    // 记录位置数据（可以写入文件或数据库）
    double elapsedSeconds = (double)elapsedTime.GetTotalSeconds();
    TRACE("时间: %.3fs, 位置: (%.3f, %.3f, %.3f)\n",
          elapsedSeconds, currentPos[0], currentPos[1], currentPos[2]);

    // 检查记录是否完成
    if(elapsedTime.GetTotalMilliseconds() >= durationMs)
    {
        TRACE("位置轨迹记录完成\n");
        isRecording = false;
    }
}
```

## 参数映射表

| 应用场景 | nCrdNum | pPos | 说明 |
|----------|---------|------|------|
| 单坐标系监控 | 1-16 | 局部数组 | 监控单个坐标系位置 |
| 多坐标系监控 | 循环变量 | 二维数组 | 批量监控多个坐标系 |
| 实时显示 | 固定坐标系 | 全局数组 | 用于界面实时更新 |
| 轨迹记录 | 指定坐标系 | 历史数组 | 记录运动轨迹数据 |

## 关键说明

1. **位置数据**：
   - 返回各轴在坐标系中的当前位置
   - 位置单位与坐标系配置一致（通常为脉冲或毫米）
   - 数组索引对应坐标系的各维度

2. **读取时机**：
   - 可以在定时器中定期读取，实现实时位置监控
   - 运动过程中读取获取当前位置
   - 运动完成后读取确认到达位置

3. **数据精度**：
   - 位置数据为双精度浮点数，精度高
   - 实际精度取决于编码器分辨率和机械系统
   - 注意处理浮点数比较的精度问题

4. **错误处理**：
   - 读取失败时返回错误代码
   - 需要检查返回值确保数据有效性
   - 坐标系未初始化时可能读取失败

5. **性能考虑**：
   - 频繁读取可能影响系统性能
   - 建议根据实际需要调整读取频率
   - 在高速运动中注意读取延迟

6. **应用场景**：
   - **实时监控**：显示当前位置
   - **精度控制**：确认到达目标位置
   - **轨迹记录**：记录运动路径
   - **进度跟踪**：计算运动进度

## 函数区别

- **MC_GetCrdPos() vs MC_GetAxisPos()**: 坐标系位置 vs 单轴位置
- **MC_GetCrdPos() vs MC_GetCrdStatus()**: 位置信息 vs 状态信息
- **MC_GetCrdPos() vs MC_GetEncPos()**: 坐标系逻辑位置 vs 编码器原始位置

---

**使用建议**: 在需要精确位置控制的应用中，建议建立位置监控机制，实时跟踪运动位置。对于关键定位任务，可以设置位置容差检查，确保到达目标位置。定期记录位置数据有助于系统调试和性能分析。