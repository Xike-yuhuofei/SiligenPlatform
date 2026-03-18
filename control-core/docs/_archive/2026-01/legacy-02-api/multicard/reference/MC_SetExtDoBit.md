# MC_SetExtDoBit 函数教程

## 函数原型

```c
int MC_SetExtDoBit(short nCardIndex, short nBitIndex, short nValue)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCardIndex | short | 卡索引。0：主卡；1-7：扩展卡1-7 |
| nBitIndex | short | 输出位索引。0-15：对应输出1-16 |
| nValue | short | 输出值。0：低电平（OFF）；1：高电平（ON） |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：单个输出位控制
```cpp
// 控制输出1的状态切换
void CDemoVCDlg::OnBnClickedButtonOutPut1()
{
    int iCardIndex = m_ComboBoxCardSel.GetCurSel();  // 获取选择的卡索引
    CString strText;

    // 获取当前按钮文本
    GetDlgItem(IDC_BUTTON_OUT_PUT_1)->GetWindowText(strText);

    if(strstr(strText,"ON"))
    {
        // 当前是ON状态，切换到OFF
        g_MultiCard.MC_SetExtDoBit(iCardIndex, 0, 1);  // 设置输出1为高电平
        GetDlgItem(IDC_BUTTON_OUT_PUT_1)->SetWindowText("OutPut1 OFF");
    }
    else
    {
        // 当前是OFF状态，切换到ON
        g_MultiCard.MC_SetExtDoBit(iCardIndex, 0, 0);  // 设置输出1为低电平
        GetDlgItem(IDC_BUTTON_OUT_PUT_1)->SetWindowText("OutPut1 ON");
    }
}
```

### 示例2：输出2控制
```cpp
// 控制输出2的状态切换
void CDemoVCDlg::OnBnClickedButtonOutPut2()
{
    int iCardIndex = m_ComboBoxCardSel.GetCurSel();  // 获取选择的卡索引
    CString strText;

    GetDlgItem(IDC_BUTTON_OUT_PUT_2)->GetWindowText(strText);

    if(strstr(strText,"ON"))
    {
        g_MultiCard.MC_SetExtDoBit(iCardIndex, 1, 1);  // 输出2设为高电平
        GetDlgItem(IDC_BUTTON_OUT_PUT_2)->SetWindowText("OutPut2 OFF");
    }
    else
    {
        g_MultiCard.MC_SetExtDoBit(iCardIndex, 1, 0);  // 输出2设为低电平
        GetDlgItem(IDC_BUTTON_OUT_PUT_2)->SetWindowText("OutPut2 ON");
    }
}
```

### 示例3：批量输出控制
```cpp
// 批量设置多个输出位的状态
void SetMultipleOutputs(int cardIndex, bool* outputStates, int count)
{
    for(int i = 0; i < count && i < 16; i++)
    {
        int value = outputStates[i] ? 1 : 0;
        int iRes = g_MultiCard.MC_SetExtDoBit(cardIndex, i, value);

        if(iRes == 0)
        {
            TRACE("输出%d设置成功，状态: %s\n", i+1, value ? "ON" : "OFF");
        }
        else
        {
            TRACE("输出%d设置失败\n", i+1);
        }
    }
}
```

### 示例4：输出模式控制
```cpp
// 实现不同的输出模式
void ExecuteOutputPattern(int cardIndex, int pattern)
{
    // 清除所有输出
    for(int i = 0; i < 16; i++)
    {
        g_MultiCard.MC_SetExtDoBit(cardIndex, i, 0);
    }

    switch(pattern)
    {
    case 1: // 交替模式
        for(int i = 0; i < 8; i++)
        {
            g_MultiCard.MC_SetExtDoBit(cardIndex, i*2, 1);     // 偶数位设为ON
            g_MultiCard.MC_SetExtDoBit(cardIndex, i*2+1, 0);   // 奇数位设为OFF
        }
        break;

    case 2: // 前半部分ON
        for(int i = 0; i < 8; i++)
        {
            g_MultiCard.MC_SetExtDoBit(cardIndex, i, 1);
        }
        break;

    case 3: // 全部ON
        for(int i = 0; i < 16; i++)
        {
            g_MultiCard.MC_SetExtDoBit(cardIndex, i, 1);
        }
        break;
    }
}
```

### 示例5：安全输出控制
```cpp
// 安全的输出控制，包含状态检查
bool SafeOutputControl(int cardIndex, int bitIndex, bool desiredState)
{
    // 参数检查
    if(cardIndex < 0 || cardIndex > 7)
    {
        TRACE("卡索引%d超出范围\n", cardIndex);
        return false;
    }

    if(bitIndex < 0 || bitIndex > 15)
    {
        TRACE("输出位索引%d超出范围\n", bitIndex);
        return false;
    }

    // 设置输出
    int value = desiredState ? 1 : 0;
    int iRes = g_MultiCard.MC_SetExtDoBit(cardIndex, bitIndex, value);

    if(iRes == 0)
    {
        TRACE("安全设置输出%d: %s\n", bitIndex+1, desiredState ? "ON" : "OFF");
        return true;
    }
    else
    {
        TRACE("设置输出%d失败，错误代码: %d\n", bitIndex+1, iRes);
        return false;
    }
}
```

### 示例6：输出状态同步
```cpp
// 将界面按钮状态与实际输出状态同步
void SyncOutputButtonStates(int cardIndex)
{
    // 这里假设有读取输出状态的函数
    // 实际项目中可能需要实现状态读取功能

    for(int i = 0; i < 16; i++)
    {
        // 模拟读取输出状态（实际需要相应的读取函数）
        bool isOn = false;  // 这里应该调用读取函数获取实际状态

        CString buttonText;
        buttonText.Format("OutPut%d %s", i+1, isOn ? "ON" : "OFF");

        // 更新对应的按钮文本
        switch(i)
        {
        case 0: GetDlgItem(IDC_BUTTON_OUT_PUT_1)->SetWindowText(buttonText); break;
        case 1: GetDlgItem(IDC_BUTTON_OUT_PUT_2)->SetWindowText(buttonText); break;
        // ... 其他输出位
        }
    }
}
```

## 参数映射表

| 应用场景 | nCardIndex | nBitIndex | nValue | 说明 |
|----------|------------|-----------|---------|------|
| 单输出控制 | m_ComboBoxCardSel.GetCurSel() | 0-15 | 0/1 | 界面按钮控制 |
| 批量控制 | 固定卡号 | 循环变量 | 数组值 | 批量设置多个输出 |
| 模式控制 | 固定卡号 | 0-15 | 模式值 | 预定义输出模式 |
| 安全控制 | 验证后的值 | 验证后的值 | 0/1 | 包含安全检查 |

## 关键说明

1. **输出位索引**：
   - **0-15**：对应输出1-16
   - 扩展卡的输出位索引与主卡相同
   - 不同卡通过nCardIndex区分

2. **输出值含义**：
   - **0**：低电平，通常表示OFF状态
   - **1**：高电平，通常表示ON状态
   - 具体电平标准需要参考硬件手册

3. **卡索引说明**：
   - **0**：主控制卡
   - **1-7**：扩展卡1-7
   - 每张卡都有16个独立的输出位

4. **使用时机**：
   - **设备控制**：控制外部设备的启停
   - **状态指示**：显示系统运行状态
   - **信号输出**：输出控制信号给其他设备
   - **调试测试**：测试外部设备的响应

5. **注意事项**：
   - 确保输出负载在允许范围内
   - 避免频繁切换造成器件损坏
   - 考虑输出的电气特性
   - 必要时添加电气隔离

6. **硬件考虑**：
   - **输出类型**：继电器、晶体管、固态继电器等
   - **负载能力**：电压、电流限制
   - **响应时间**：开关切换时间
   - **寿命考虑**：机械寿命和电气寿命

## 函数区别

- **MC_SetExtDoBit() vs 输入读取**: 设置输出 vs 读取输入
- **位控制 vs 字节控制**: 单个位控制 vs 批量位控制
- **主卡 vs 扩展卡**: 通过nCardIndex参数区分不同卡

---

**安全建议**: 在控制重要设备时，建议添加状态检查和确认机制。对于感性负载，考虑添加续流二极管等保护措施。定期检查输出连接和负载状态。