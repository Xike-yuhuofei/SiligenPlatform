# MC_GetExtDiValue 函数教程

## 函数原型

```c
int MC_GetExtDiValue(short nCardIndex, long* plValue, short nCount)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCardIndex | short | 卡索引。0：主卡；1-7：扩展卡1-7 |
| plValue | long* | 输入值缓冲区指针，用于存储读取的输入状态 |
| nCount | short | 读取数量，通常为1 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本输入值读取
```cpp
// 读取扩展数字输入的值
void ReadDigitalInputs()
{
    int iCardIndex = m_ComboBoxCardSel.GetCurSel();  // 获取选择的卡索引
    long lValue = 0;

    // 读取扩展数字输入值
    int iRes = g_MultiCard.MC_GetExtDiValue(iCardIndex, &lValue, 1);

    if(iRes == 0)
    {
        TRACE("卡%d扩展输入值: 0x%08lX\n", iCardIndex, lValue);

        // 解析各个输入位
        for(int i = 0; i < 16; i++)
        {
            bool inputState = (lValue & (0X0001 << i)) != 0;
            TRACE("输入%d: %s\n", i+1, inputState ? "ON" : "OFF");
        }
    }
    else
    {
        TRACE("读取扩展输入失败，错误代码: %d\n", iRes);
    }
}
```

### 示例2：输入状态监控
```cpp
// 监控特定输入位的状态变化
void MonitorInputChanges(int cardIndex, int inputBit)
{
    static long lastInputValue = 0;
    long currentValue = 0;

    // 读取当前输入值
    int iRes = g_MultiCard.MC_GetExtDiValue(cardIndex, &currentValue, 1);
    if(iRes != 0)
    {
        TRACE("读取输入值失败\n");
        return;
    }

    // 检查特定位是否发生变化
    bool lastState = (lastInputValue & (0X0001 << inputBit)) != 0;
    bool currentState = (currentValue & (0X0001 << inputBit)) != 0;

    if(lastState != currentState)
    {
        TRACE("输入%d状态变化: %s -> %s\n",
              inputBit+1,
              lastState ? "ON" : "OFF",
              currentState ? "ON" : "OFF");

        // 执行相应的处理逻辑
        HandleInputChange(inputBit, currentState);
    }

    lastInputValue = currentValue;
}
```

### 示例3：批量读取多卡输入
```cpp
// 读取多张控制卡的输入状态
void ReadAllCardsInputs()
{
    int cardCount = 3;  // 假设有3张卡
    long inputValues[3] = {0};

    // 读取每张卡的输入值
    for(int i = 0; i < cardCount; i++)
    {
        int iRes = g_MultiCard.MC_GetExtDiValue(i, &inputValues[i], 1);

        if(iRes == 0)
        {
            TRACE("卡%d输入值: 0x%08lX\n", i, inputValues[i]);
        }
        else
        {
            TRACE("卡%d输入读取失败\n", i);
            inputValues[i] = 0;  // 设置默认值
        }
    }

    // 处理所有输入
    ProcessAllInputs(inputValues, cardCount);
}
```

### 示例4：输入状态与界面同步
```cpp
// 将输入状态同步到界面显示
void SyncInputsToDisplay(int cardIndex)
{
    long inputValues[1] = {0};

    // 读取输入值
    int iRes = g_MultiCard.MC_GetExtDiValue(cardIndex, inputValues, 1);
    if(iRes != 0)
    {
        TRACE("读取输入失败，无法同步界面\n");
        return;
    }

    long inputValue = inputValues[0];

    // 更新界面显示
    for(int i = 0; i < 16; i++)
    {
        bool inputState = (inputValue & (0X0001 << i)) != 0;

        // 更新对应的显示控件
        switch(i)
        {
        case 0:
            ((CButton*)GetDlgItem(IDC_RADIO_INPUT_1))->SetCheck(inputState);
            break;
        case 1:
            ((CButton*)GetDlgItem(IDC_RADIO_INPUT_2))->SetCheck(inputState);
            break;
        case 2:
            ((CButton*)GetDlgItem(IDC_RADIO_INPUT_3))->SetCheck(inputState);
            break;
        // ... 其他输入位
        }
    }
}
```

### 示例5：输入信号滤波
```cpp
// 对输入信号进行软件滤波，防止干扰
bool GetFilteredInputState(int cardIndex, int inputBit)
{
    static long inputHistory[5] = {0};  // 保存最近5次的读取值
    int sampleCount = 5;
    int onCount = 0;

    // 多次采样
    for(int i = 0; i < sampleCount; i++)
    {
        long inputValue = 0;
        int iRes = g_MultiCard.MC_GetExtDiValue(cardIndex, &inputValue, 1);

        if(iRes == 0)
        {
            inputHistory[i] = inputValue;
        }
        else
        {
            inputHistory[i] = 0;  // 读取失败时使用默认值
        }

        // 短暂延时
        Sleep(10);
    }

    // 统计目标位为ON的次数
    for(int i = 0; i < sampleCount; i++)
    {
        if(inputHistory[i] & (0X0001 << inputBit))
        {
            onCount++;
        }
    }

    // 超过一半为ON则认为输入为ON
    return (onCount > sampleCount / 2);
}
```

### 示例6：输入边缘检测
```cpp
// 检测输入信号的上升沿和下降沿
void DetectInputEdges(int cardIndex)
{
    static long lastInputValue = 0;
    long currentValue = 0;

    // 读取当前输入值
    int iRes = g_MultiCard.MC_GetExtDiValue(cardIndex, &currentValue, 1);
    if(iRes != 0) return;

    // 计算变化位（上升沿）
    long risingEdges = (~lastInputValue) & currentValue;

    // 计算变化位（下降沿）
    long fallingEdges = lastInputValue & (~currentValue);

    // 处理上升沿
    for(int i = 0; i < 16; i++)
    {
        if(risingEdges & (0X0001 << i))
        {
            TRACE("检测到输入%d上升沿\n", i+1);
            HandleRisingEdge(i);
        }
    }

    // 处理下降沿
    for(int i = 0; i < 16; i++)
    {
        if(fallingEdges & (0X0001 << i))
        {
            TRACE("检测到输入%d下降沿\n", i+1);
            HandleFallingEdge(i);
        }
    }

    lastInputValue = currentValue;
}
```

## 参数映射表

| 应用场景 | nCardIndex | plValue | nCount | 说明 |
|----------|------------|---------|--------|------|
| 单次读取 | m_ComboBoxCardSel.GetCurSel() | 局部变量 | 1 | 读取一张卡的输入 |
| 多卡读取 | 循环变量 | 数组 | 1 | 批量读取多张卡 |
| 状态监控 | 固定卡号 | 静态变量 | 1 | 连续监控输入变化 |
| 信号滤波 | 固定卡号 | 临时数组 | 1 | 多次采样滤波 |

## 关键说明

1. **输入值格式**：
   - 返回32位数值，每位对应一个输入
   - **bit0**：输入1
   - **bit1**：输入2
   - ...
   - **bit15**：输入16
   - 高位通常为0或保留

2. **读取方式**：
   - **阻塞读取**：立即返回当前输入状态
   - **实时反映**：反映硬件输入的实时状态
   - **无缓存**：每次读取都获取最新状态

3. **使用时机**：
   - **状态监控**：定期检查输入状态
   - **事件响应**：响应输入信号变化
   - **安全检测**：检测安全相关的输入信号
   - **系统控制**：根据输入状态控制系统行为

4. **信号处理**：
   - **软件滤波**：多次采样避免干扰
   - **边缘检测**：检测信号跳变
   - **状态同步**：与界面或其他系统同步
   - **历史记录**：记录输入状态变化

5. **注意事项**：
   - 读取频率要适中，避免过度占用CPU
   - 考虑输入信号的电气特性
   - 处理读取失败的情况
   - 注意多线程访问的安全性

6. **硬件考虑**：
   - **输入类型**：干接点、PNP、NPN等
   - **电压等级**：5V、12V、24V等
   - **滤波要求**：硬件滤波和软件滤波
   - **隔离要求**：光电隔离等

## 函数区别

- **MC_GetExtDiValue() vs MC_GetDiRaw()**: 扩展输入 vs 特殊功能输入
- **MC_GetExtDiValue() vs MC_SetExtDoBit()**: 读取输入 vs 设置输出
- **实时 vs 缓存**: MC_GetExtDiValue()直接读取硬件状态

---

**使用建议**: 在工业环境中，建议对输入信号进行适当的软件滤波，避免因干扰导致的误触发。对于安全相关的输入信号，建议采用多重确认机制，确保系统的可靠性。