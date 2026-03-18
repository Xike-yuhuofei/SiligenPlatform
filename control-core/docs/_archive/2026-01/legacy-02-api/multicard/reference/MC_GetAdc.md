# MC_GetAdc 函数教程

## 函数原型

```c
int MC_GetAdc(short nChannel, short* pnValue, short nCount)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nChannel | short | ADC通道号，取值范围：1-16 |
| pnValue | short* | ADC值缓冲区指针，用于存储读取的ADC数值 |
| nCount | short | 读取数量，通常为1 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本ADC读取
```cpp
// 在定时器中读取并显示ADC模拟量值
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    int iRes = 0;

    short nValue = 0;

    // 读取ADC通道1的模拟量值
    g_MultiCard.MC_GetAdc(1, &nValue, 1);

    // 显示模拟量电压值
    CString strText;
    strText.Format("模拟量电压：%d", nValue);
    GetDlgItem(IDC_STATIC_ADC)->SetWindowText(strText);

    CDialogEx::OnTimer(nIDEvent);
}
```

### 示例2：多通道ADC监控
```cpp
// 监控多个ADC通道的模拟量值
void MonitorMultipleADCChannels()
{
    short adcValues[8] = {0};
    int channelCount = 8;

    // 读取多个ADC通道
    for(int i = 0; i < channelCount; i++)
    {
        int iRes = g_MultiCard.MC_GetAdc(i+1, &adcValues[i], 1);
        if(iRes == 0)
        {
            TRACE("ADC通道%d值: %d\n", i+1, adcValues[i]);
        }
        else
        {
            TRACE("读取ADC通道%d失败\n", i+1);
        }
    }

    // 处理ADC数据
    ProcessADCData(adcValues, channelCount);
}
```

### 示例3：ADC值转换为实际物理量
```cpp
// 将ADC值转换为实际电压值
float ConvertADCToVoltage(short adcValue)
{
    // 假设ADC是12位，参考电压为3.3V
    const float ADC_MAX_VALUE = 4095.0f;  // 2^12 - 1
    const float REF_VOLTAGE = 3.3f;

    float voltage = (float)adcValue / ADC_MAX_VALUE * REF_VOLTAGE;
    return voltage;
}

// 读取并转换ADC值为电压
void ReadADCAsVoltage(int channel)
{
    short adcValue = 0;
    int iRes = g_MultiCard.MC_GetAdc(channel, &adcValue, 1);

    if(iRes == 0)
    {
        float voltage = ConvertADCToVoltage(adcValue);
        TRACE("ADC通道%d: ADC值=%d, 电压=%.3fV\n", channel, adcValue, voltage);

        // 更新界面显示
        CString strText;
        strText.Format("通道%d电压: %.2fV", channel, voltage);
        GetDlgItem(IDC_STATIC_ADC_VOLTAGE)->SetWindowText(strText);
    }
    else
    {
        TRACE("读取ADC通道%d失败\n", channel);
    }
}
```

### 示例4：ADC数据滤波处理
```cpp
// 对ADC数据进行滤波处理，减少噪声影响
short GetFilteredADCValue(int channel)
{
    static short adcHistory[5] = {0};  // 保存最近5次的ADC值
    static int historyIndex = 0;
    int sampleCount = 5;

    // 读取当前ADC值
    short currentValue = 0;
    int iRes = g_MultiCard.MC_GetAdc(channel, &currentValue, 1);
    if(iRes != 0)
    {
        return 0;  // 读取失败返回默认值
    }

    // 更新历史记录
    adcHistory[historyIndex] = currentValue;
    historyIndex = (historyIndex + 1) % sampleCount;

    // 计算平均值（简单滤波）
    long sum = 0;
    for(int i = 0; i < sampleCount; i++)
    {
        sum += adcHistory[i];
    }
    short filteredValue = (short)(sum / sampleCount);

    return filteredValue;
}
```

### 示例5：ADC阈值检测
```cpp
// 检测ADC值是否超过设定阈值
void CheckADCThresholds()
{
    short adcValue = 0;
    int iRes = g_MultiCard.MC_GetAdc(1, &adcValue, 1);

    if(iRes != 0)
    {
        TRACE("ADC读取失败\n");
        return;
    }

    // 定义阈值
    const short WARNING_THRESHOLD = 3000;
    const short CRITICAL_THRESHOLD = 3800;

    // 检查阈值
    if(adcValue >= CRITICAL_THRESHOLD)
    {
        TRACE("警告：ADC值超过临界阈值！当前值: %d\n", adcValue);
        HandleCriticalADC(adcValue);
    }
    else if(adcValue >= WARNING_THRESHOLD)
    {
        TRACE("注意：ADC值超过警告阈值。当前值: %d\n", adcValue);
        HandleWarningADC(adcValue);
    }
    else
    {
        TRACE("ADC值正常: %d\n", adcValue);
    }
}
```

### 示例6：ADC数据记录和分析
```cpp
// 记录ADC数据用于后续分析
void LogADCData()
{
    static int logCount = 0;
    const int MAX_LOG_ENTRIES = 1000;

    if(logCount >= MAX_LOG_ENTRIES)
    {
        return;  // 达到最大记录数量
    }

    short adcValue = 0;
    int iRes = g_MultiCard.MC_GetAdc(1, &adcValue, 1);

    if(iRes == 0)
    {
        // 获取当前时间戳
        CTime currentTime = CTime::GetCurrentTime();
        CString timeStr = currentTime.Format("%Y-%m-%d %H:%M:%S");

        // 记录数据
        TRACE("ADC日志[%d]: %s - 值: %d\n", logCount, timeStr, adcValue);

        // 可以写入文件或数据库
        // WriteToLogFile(timeStr, adcValue);

        logCount++;
    }
}

// 分析ADC数据趋势
void AnalyzeADCTrend()
{
    static short adcHistory[100] = {0};
    static int historyIndex = 0;
    const int ANALYSIS_WINDOW = 50;

    // 读取当前ADC值
    short currentValue = 0;
    int iRes = g_MultiCard.MC_GetAdc(1, &currentValue, 1);
    if(iRes != 0) return;

    // 更新历史记录
    adcHistory[historyIndex] = currentValue;
    historyIndex = (historyIndex + 1) % 100;

    // 分析趋势
    if(historyIndex >= ANALYSIS_WINDOW)
    {
        float sum = 0;
        for(int i = 0; i < ANALYSIS_WINDOW; i++)
        {
            sum += adcHistory[i];
        }
        float average = sum / ANALYSIS_WINDOW;

        // 检查是否偏离平均值较大
        float deviation = abs(currentValue - average);
        if(deviation > average * 0.2f)  // 偏离超过20%
        {
            TRACE("ADC值异常变化: 平均值=%.1f, 当前值=%d, 偏差=%.1f%%\n",
                  average, currentValue, deviation/average*100);
        }
    }
}
```

## 参数映射表

| 应用场景 | nChannel | pnValue | nCount | 说明 |
|----------|----------|---------|--------|------|
| 单通道读取 | 1-16 | 局部变量 | 1 | 读取单个ADC通道 |
| 多通道读取 | 循环变量 | 数组 | 1 | 批量读取多个通道 |
| 数据滤波 | 固定通道 | 静态数组 | 1 | 多次采样滤波 |
| 阈值检测 | 监控通道 | 局部变量 | 1 | 检查数值范围 |

## 关键说明

1. **ADC通道**：
   - **1-16**：支持的ADC通道范围
   - 每个通道独立工作
   - 具体功能需要参考硬件手册

2. **数值范围**：
   - 取决于ADC分辨率（通常为12位）
   - 范围：0到2^分辨率-1
   - 需要转换为实际物理量

3. **转换关系**：
   - ADC值与实际电压成线性关系
   - 需要知道参考电压
   - 可能需要校准系数

4. **应用场景**：
   - **温度监测**：连接温度传感器
   - **电压监测**：监控电源电压
   - **压力检测**：连接压力传感器
   - **位置反馈**：连接电位器等

5. **采样特性**：
   - 瞬时采样，反映当前状态
   - 可能存在噪声干扰
   - 建议进行软件滤波

6. **注意事项**：
   - 确保传感器连接正确
   - 注意输入电压范围
   - 考虑采样频率和响应时间
   - 定期校准确保准确性

## 函数区别

- **MC_GetAdc() vs 模拟量输出**: 读取模拟量 vs 输出模拟量
- **MC_GetAdc() vs 数字输入**: 模拟量输入 vs 数字量输入
- **单通道 vs 多通道**: 分别读取 vs 批量读取

---

**使用建议**: 对于重要的模拟量监测，建议实现数据滤波和异常检测功能。定期校准ADC系统以确保测量精度。根据具体应用选择合适的采样频率，避免过度采样或采样不足。