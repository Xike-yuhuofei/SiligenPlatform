# MC_GetDiRaw 函数教程

## 函数原型

```c
int MC_GetDiRaw(short nDiType, long* plValue)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nDiType | short | 输入类型。MC_LIMIT_POSITIVE：正限位；MC_LIMIT_NEGATIVE：负限位 |
| plValue | long* | 输入值缓冲区指针，用于存储读取的原始输入状态 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：读取正限位状态
```cpp
// 读取所有轴的正限位状态
void ReadPositiveLimitStatus()
{
    long lValue = 0;

    // 读取正限位原始输入状态
    int iRes = g_MultiCard.MC_GetDiRaw(MC_LIMIT_POSITIVE, (long*)&lValue);

    if(iRes == 0)
    {
        TRACE("正限位状态值: 0x%08lX\n", lValue);

        // 检查各轴的正限位状态
        for(int i = 0; i < 16; i++)
        {
            bool limitTriggered = (lValue & (0X0001 << i)) != 0;
            if(limitTriggered)
            {
                TRACE("轴%d正限位触发！\n", i+1);
                HandlePositiveLimitTrigger(i+1);
            }
        }
    }
    else
    {
        TRACE("读取正限位状态失败，错误代码: %d\n", iRes);
    }
}
```

### 示例2：读取负限位状态
```cpp
// 读取所有轴的负限位状态
void ReadNegativeLimitStatus()
{
    long lValue = 0;

    // 读取负限位原始输入状态
    int iRes = g_MultiCard.MC_GetDiRaw(MC_LIMIT_NEGATIVE, (long*)&lValue);

    if(iRes == 0)
    {
        TRACE("负限位状态值: 0x%08lX\n", lValue);

        // 检查各轴的负限位状态
        for(int i = 0; i < 16; i++)
        {
            bool limitTriggered = (lValue & (0X0001 << i)) != 0;
            if(limitTriggered)
            {
                TRACE("轴%d负限位触发！\n", i+1);
                HandleNegativeLimitTrigger(i+1);
            }
        }
    }
    else
    {
        TRACE("读取负限位状态失败，错误代码: %d\n", iRes);
    }
}
```

### 示例3：限位状态监控
```cpp
// 监控限位状态变化
void MonitorLimitStatus()
{
    static long lastPosLimitValue = 0;
    static long lastNegLimitValue = 0;
    long currentPosLimitValue = 0;
    long currentNegLimitValue = 0;

    // 读取正限位状态
    int iRes1 = g_MultiCard.MC_GetDiRaw(MC_LIMIT_POSITIVE, &currentPosLimitValue);
    int iRes2 = g_MultiCard.MC_GetDiRaw(MC_LIMIT_NEGATIVE, &currentNegLimitValue);

    if(iRes1 == 0 && iRes2 == 0)
    {
        // 检查正限位变化
        long posLimitChanges = lastPosLimitValue ^ currentPosLimitValue;
        for(int i = 0; i < 16; i++)
        {
            if(posLimitChanges & (0X0001 << i))
            {
                bool currentState = (currentPosLimitValue & (0X0001 << i)) != 0;
                if(currentState)
                {
                    TRACE("轴%d正限位触发\n", i+1);
                    HandlePositiveLimitTrigger(i+1);
                }
                else
                {
                    TRACE("轴%d正限位释放\n", i+1);
                    HandlePositiveLimitRelease(i+1);
                }
            }
        }

        // 检查负限位变化
        long negLimitChanges = lastNegLimitValue ^ currentNegLimitValue;
        for(int i = 0; i < 16; i++)
        {
            if(negLimitChanges & (0X0001 << i))
            {
                bool currentState = (currentNegLimitValue & (0X0001 << i)) != 0;
                if(currentState)
                {
                    TRACE("轴%d负限位触发\n", i+1);
                    HandleNegativeLimitTrigger(i+1);
                }
                else
                {
                    TRACE("轴%d负限位释放\n", i+1);
                    HandleNegativeLimitRelease(i+1);
                }
            }
        }

        lastPosLimitValue = currentPosLimitValue;
        lastNegLimitValue = currentNegLimitValue;
    }
}
```

### 示例4：安全检查函数
```cpp
// 检查轴是否处于安全状态（无限位触发）
bool IsAxisSafe(int axis)
{
    long posLimitValue = 0;
    long negLimitValue = 0;

    // 读取限位状态
    int iRes1 = g_MultiCard.MC_GetDiRaw(MC_LIMIT_POSITIVE, &posLimitValue);
    int iRes2 = g_MultiCard.MC_GetDiRaw(MC_LIMIT_NEGATIVE, &negLimitValue);

    if(iRes1 != 0 || iRes2 != 0)
    {
        TRACE("读取限位状态失败\n");
        return false;
    }

    // 检查指定轴的限位状态
    bool posLimitTriggered = (posLimitValue & (0X0001 << (axis-1))) != 0;
    bool negLimitTriggered = (negLimitValue & (0X0001 << (axis-1))) != 0;

    if(posLimitTriggered || negLimitTriggered)
    {
        TRACE("轴%d处于限位状态，不安全\n", axis);
        return false;
    }

    TRACE("轴%d处于安全状态\n", axis);
    return true;
}
```

### 示例5：限位状态与界面同步
```cpp
// 将限位状态同步到界面显示
void SyncLimitStatusToDisplay()
{
    long posLimitValue = 0;
    long negLimitValue = 0;

    // 读取正限位状态
    int iRes1 = g_MultiCard.MC_GetDiRaw(MC_LIMIT_POSITIVE, &posLimitValue);
    // 读取负限位状态
    int iRes2 = g_MultiCard.MC_GetDiRaw(MC_LIMIT_NEGATIVE, &negLimitValue);

    if(iRes1 == 0)
    {
        // 更新正限位显示
        for(int i = 0; i < 8; i++)  // 假设只显示前8轴
        {
            bool limitTriggered = (posLimitValue & (0X0001 << i)) != 0;
            ((CButton*)GetDlgItem(IDC_RADIO_LIMP_1 + i))->SetCheck(limitTriggered);
        }
    }

    if(iRes2 == 0)
    {
        // 更新负限位显示
        for(int i = 0; i < 8; i++)  // 假设只显示前8轴
        {
            bool limitTriggered = (negLimitValue & (0X0001 << i)) != 0;
            ((CButton*)GetDlgItem(IDC_RADIO_LIMN_1 + i))->SetCheck(limitTriggered);
        }
    }
}
```

### 示例6：限位故障诊断
```cpp
// 诊断限位系统故障
void DiagnoseLimitSystem()
{
    long posLimitValue = 0;
    long negLimitValue = 0;

    // 读取限位状态
    int iRes1 = g_MultiCard.MC_GetDiRaw(MC_LIMIT_POSITIVE, &posLimitValue);
    int iRes2 = g_MultiCard.MC_GetDiRaw(MC_LIMIT_NEGATIVE, &negLimitValue);

    if(iRes1 == 0 && iRes2 == 0)
    {
        TRACE("=== 限位系统诊断报告 ===\n");

        for(int i = 0; i < 16; i++)
        {
            bool posLimit = (posLimitValue & (0X0001 << i)) != 0;
            bool negLimit = (negLimitValue & (0X0001 << i)) != 0;

            TRACE("轴%d: ", i+1);
            if(posLimit && negLimit)
            {
                TRACE("正负限位同时触发 - 故障！\n");
                LogLimitFault(i+1, "正负限位同时触发");
            }
            else if(posLimit)
            {
                TRACE("正限位触发\n");
            }
            else if(negLimit)
            {
                TRACE("负限位触发\n");
            }
            else
            {
                TRACE("无限位触发\n");
            }
        }

        TRACE("=== 诊断完成 ===\n");
    }
    else
    {
        TRACE("限位系统诊断失败，无法读取限位状态\n");
    }
}
```

## 参数映射表

| 应用场景 | nDiType | plValue | 说明 |
|----------|---------|---------|------|
| 正限位读取 | MC_LIMIT_POSITIVE | 局部变量 | 读取所有轴正限位 |
| 负限位读取 | MC_LIMIT_NEGATIVE | 局部变量 | 读取所有轴负限位 |
| 状态监控 | 两种类型都调用 | 静态变量 | 连续监控限位变化 |
| 安全检查 | 两种类型都调用 | 临时变量 | 检查轴的安全状态 |

## 关键说明

1. **限位类型**：
   - **MC_LIMIT_POSITIVE**：正限位（正向硬限位）
   - **MC_LIMIT_NEGATIVE**：负限位（负向硬限位）
   - 通常为机械限位开关信号

2. **返回值格式**：
   - 32位数值，每位对应一个轴
   - **bit0**：轴1限位状态
   - **bit1**：轴2限位状态
   - ...
   - **bit15**：轴16限位状态

3. **限位状态**：
   - **1**：限位触发（限位开关闭合）
   - **0**：限位未触发（限位开关断开）
   - 硬件逻辑可能需要根据实际电路调整

4. **使用时机**：
   - **安全检查**：运动前检查限位状态
   - **状态监控**：实时监控限位触发
   - **故障诊断**：诊断限位系统问题
   - **界面同步**：更新限位状态显示

5. **安全考虑**：
   - 限位触发时应立即停止相关轴运动
   - 正负限位同时触发表示系统故障
   - 定期检查限位开关的功能
   - 考虑限位失效的保护措施

6. **注意事项**：
   - 限位状态是硬件保护的最后防线
   - 软件限位和硬件限位应配合使用
   - 处理限位读取失败的情况
   - 注意限位信号的电气噪声

## 函数区别

- **MC_GetDiRaw() vs MC_GetExtDiValue()**: 特殊功能输入 vs 通用输入
- **MC_GetDiRaw() vs MC_LmtsOn/Off()**: 读取限位状态 vs 设置限位功能
- **正限位 vs 负限位**: MC_LIMIT_POSITIVE vs MC_LIMIT_NEGATIVE

---

**安全警告**: 限位系统是重要的安全保护机制，必须定期检查和维护。不建议在正常运行中禁用限位保护功能。当检测到限位系统故障时，应立即停止相关运动并检查硬件连接。