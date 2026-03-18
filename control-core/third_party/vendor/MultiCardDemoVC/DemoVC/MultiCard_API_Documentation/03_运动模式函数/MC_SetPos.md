# MC_SetPos 函数教程

## 函数原型

```c
int MC_SetPos(short nAxis, long lPos)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nAxis | short | 轴号，取值范围：1-16 |
| lPos | long | 目标位置（脉冲数） |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本目标位置设置
```cpp
// 设置轴的目标位置并执行梯形运动
void CDemoVCDlg::OnBnClickedButton6()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;  // 获取当前选择的轴号
    UpdateData(true);  // 更新界面数据

    // 配置梯形速度曲线参数
    TTrapPrm TrapPrm;
    TrapPrm.acc = 0.1;        // 加速度
    TrapPrm.dec = 0.1;        // 减速度
    TrapPrm.smoothTime = 0;   // 平滑时间
    TrapPrm.velStart = 0;     // 起始速度

    // 设置梯形速度曲线模式
    iRes = g_MultiCard.MC_PrfTrap(iAxisNum);

    // 设置梯形运动参数
    iRes += g_MultiCard.MC_SetTrapPrm(iAxisNum, &TrapPrm);

    // 设置目标位置（从界面获取）
    iRes += g_MultiCard.MC_SetPos(iAxisNum, m_lTargetPos);

    // 设置运动速度
    iRes = g_MultiCard.MC_SetVel(iAxisNum, 5);

    // 更新轴参数，启动运动
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
}
```

### 示例2：绝对位置运动
```cpp
// 执行绝对位置运动到指定坐标
bool ExecuteAbsolutePositionMove(int axis, long targetPosition)
{
    int iRes = 0;

    // 设置梯形运动模式
    iRes = g_MultiCard.MC_PrfTrap(axis);
    if(iRes != 0)
    {
        TRACE("设置轴%d梯形模式失败\n", axis);
        return false;
    }

    // 配置运动参数
    TTrapPrm trapPrm;
    trapPrm.acc = 0.2;
    trapPrm.dec = 0.2;
    trapPrm.smoothTime = 0;
    trapPrm.velStart = 0;

    iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);
    if(iRes != 0)
    {
        TRACE("设置轴%d运动参数失败\n", axis);
        return false;
    }

    // 设置目标位置（绝对位置）
    g_MultiCard.MC_SetPos(axis, targetPosition);
    g_MultiCard.MC_SetVel(axis, 10.0);

    // 启动运动
    iRes = g_MultiCard.MC_Update(0X0001 << (axis-1));
    if(iRes == 0)
    {
        TRACE("轴%d开始运动到绝对位置: %ld\n", axis, targetPosition);
        return true;
    }

    return false;
}
```

### 示例3：相对位置运动
```cpp
// 执行相对位置运动（当前位置基础上移动指定距离）
bool ExecuteRelativePositionMove(int axis, long relativeDistance)
{
    int iRes = 0;

    // 获取当前位置
    TAllSysStatusDataSX statusData;
    iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);
    if(iRes != 0)
    {
        TRACE("获取轴%d当前位置失败\n", axis);
        return false;
    }

    long currentPos = statusData.lAxisPrfPos[axis-1];
    long targetPos = currentPos + relativeDistance;

    // 设置梯形运动模式
    iRes = g_MultiCard.MC_PrfTrap(axis);
    if(iRes != 0)
    {
        TRACE("设置轴%d梯形模式失败\n", axis);
        return false;
    }

    // 配置运动参数
    TTrapPrm trapPrm;
    trapPrm.acc = 0.15;
    trapPrm.dec = 0.15;
    trapPrm.smoothTime = 0;
    trapPrm.velStart = 0;

    iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);
    if(iRes != 0)
    {
        TRACE("设置轴%d运动参数失败\n", axis);
        return false;
    }

    // 设置目标位置（相对位置）
    g_MultiCard.MC_SetPos(axis, targetPos);
    g_MultiCard.MC_SetVel(axis, 8.0);

    // 启动运动
    iRes = g_MultiCard.MC_Update(0X0001 << (axis-1));
    if(iRes == 0)
    {
        TRACE("轴%d开始相对运动: 当前位置=%ld, 移动距离=%ld, 目标位置=%ld\n",
              axis, currentPos, relativeDistance, targetPos);
        return true;
    }

    return false;
}
```

### 示例4：多点位置运动
```cpp
// 执行多点位置运动，依次到达多个目标点
bool ExecuteMultiPointMove(int axis, long* positions, int pointCount)
{
    int iRes = 0;

    // 设置梯形运动模式
    iRes = g_MultiCard.MC_PrfTrap(axis);
    if(iRes != 0)
    {
        TRACE("设置轴%d梯形模式失败\n", axis);
        return false;
    }

    // 配置运动参数
    TTrapPrm trapPrm;
    trapPrm.acc = 0.1;
    trapPrm.dec = 0.1;
    trapPrm.smoothTime = 0;
    trapPrm.velStart = 0;

    iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);
    if(iRes != 0)
    {
        TRACE("设置轴%d运动参数失败\n", axis);
        return false;
    }

    // 依次运动到各个目标点
    for(int i = 0; i < pointCount; i++)
    {
        // 设置目标位置
        g_MultiCard.MC_SetPos(axis, positions[i]);
        g_MultiCard.MC_SetVel(axis, 5.0);

        // 启动运动
        iRes = g_MultiCard.MC_Update(0X0001 << (axis-1));
        if(iRes != 0)
        {
            TRACE("轴%d运动到第%d个点失败\n", axis, i+1);
            return false;
        }

        TRACE("轴%d开始运动到第%d个点: %ld\n", axis, i+1, positions[i]);

        // 等待运动完成
        WaitAxisMoveComplete(axis);

        // 短暂延时
        Sleep(100);
    }

    TRACE("轴%d多点运动完成\n", axis);
    return true;
}
```

### 示例5：安全位置检查后运动
```cpp
// 检查目标位置是否安全后执行运动
bool SafePositionMove(int axis, long targetPos)
{
    // 检查目标位置是否在安全范围内
    const long MIN_SAFE_POSITION = -100000;
    const long MAX_SAFE_POSITION = 100000;

    if(targetPos < MIN_SAFE_POSITION || targetPos > MAX_SAFE_POSITION)
    {
        TRACE("目标位置%ld超出安全范围[%ld, %ld]\n",
              targetPos, MIN_SAFE_POSITION, MAX_SAFE_POSITION);
        return false;
    }

    // 检查轴是否已使能
    short encStatus = 0;
    int iRes = g_MultiCard.MC_GetEncOnOff(axis, &encStatus);
    if(iRes != 0 || !encStatus)
    {
        TRACE("轴%d编码器未开启，无法执行精确位置运动\n", axis);
        return false;
    }

    // 执行位置运动
    iRes = g_MultiCard.MC_PrfTrap(axis);
    if(iRes != 0) return false;

    TTrapPrm trapPrm = {0.1, 0.1, 0, 0};
    iRes = g_MultiCard.MC_SetTrapPrm(axis, &trapPrm);
    if(iRes != 0) return false;

    g_MultiCard.MC_SetPos(axis, targetPos);
    g_MultiCard.MC_SetVel(axis, 5.0);
    iRes = g_MultiCard.MC_Update(0X0001 << (axis-1));

    if(iRes == 0)
    {
        TRACE("轴%d安全位置运动启动，目标位置: %ld\n", axis, targetPos);
        return true;
    }

    return false;
}
```

## 参数映射表

| 应用场景 | nAxis | lPos | 说明 |
|----------|-------|------|------|
| 界面控制 | m_ComboBoxAxis.GetCurSel()+1 | m_lTargetPos | 用户输入的目标位置 |
| 绝对定位 | 指定轴号 | 绝对坐标值 | 移动到指定坐标系位置 |
| 相对定位 | 指定轴号 | 当前位置+偏移 | 在当前位置基础上移动 |
| 多点运动 | 指定轴号 | 数组元素 | 依次到达多个目标点 |

## 关键说明

1. **位置单位**：
   - 单位：脉冲数
   - 根据机械传动比计算实际位置
   - 正值：正向运动
   - 负值：负向运动

2. **使用时机**：
   - 必须先设置运动模式（梯形或JOG）
   - 在调用MC_Update()前设置
   - 与MC_SetVel()配合使用
   - 可与MC_SetTrapPrm()配合优化运动曲线

3. **位置类型**：
   - **绝对位置**：相对于坐标系原点的位置
   - **相对位置**：相对于当前位置的偏移
   - **零点位置**：通过MC_ZeroPos()设置的位置

4. **注意事项**：
   - 确保目标位置在机械行程范围内
   - 考虑限位开关的影响
   - 大位置移动需要考虑运动时间
   - 精确定位需要较小的速度和加速度

5. **安全考虑**：
   - 检查目标位置是否安全
   - 避免撞击机械限位
   - 考虑负载和机械特性
   - 长距离移动时监控运动状态

## 函数区别

- **MC_SetPos() vs MC_SetVel()**: MC_SetPos()设置目标位置，MC_SetVel()设置运动速度
- **MC_SetPos() vs MC_ZeroPos()**: MC_SetPos()设置目标位置，MC_ZeroPos()设置当前位置为零
- **绝对 vs 相对**: MC_SetPos()默认是绝对位置，相对位置需要先获取当前位置

---

**使用建议**: 在设置目标位置前，建议先检查位置是否在安全范围内，并确保轴已正确配置（编码器开启、限位设置等）。对于长距离移动，建议分段执行或使用较小的速度以提高安全性。