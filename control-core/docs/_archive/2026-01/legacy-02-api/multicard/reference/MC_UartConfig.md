# MC_UartConfig 函数教程

## 函数原型

```c
int MC_UartConfig(short nUartNum, long lBaudRate, short nDataLen, short nVerifyType, short nStopBit)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nUartNum | short | 串口号，取值范围：1-N（根据控制卡支持的串口数量） |
| lBaudRate | long | 波特率，如9600、19200、38400、57600、115200等 |
| nDataLen | short | 数据位，通常为7或8 |
| nVerifyType | short | 校验类型，0：无校验，1：奇校验，2：偶校验 |
| nStopBit | short | 停止位，通常为1或2 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本串口配置
```cpp
// 从DemoVCDlg.cpp中提取的串口配置代码
void CDemoVCDlg::OnBnClickedButtonUartSet()
{
	int iRes;

	UpdateData(true);

	iRes = g_MultiCard.MC_UartConfig(m_ComboBoxUartNum.GetCurSel()+1,m_iBaudRate,m_nDataLen,m_iVerifyType,m_iStopBit);

	if(0 == iRes)
	{
		AfxMessageBox("配置串口成功！");
	}
	else
	{
		AfxMessageBox("配置串口失败！");
		return;
	}
}
```

### 示例2：常用串口参数配置
```cpp
// 配置常用串口参数的示例函数
bool ConfigureStandardUart(short uartNum)
{
	int iRes = 0;

	// 标准9600,8,N,1配置
	iRes = g_MultiCard.MC_UartConfig(uartNum, 9600, 8, 0, 1);
	if(iRes != 0)
	{
		TRACE("串口%d配置9600,8,N,1失败，错误代码: %d\n", uartNum, iRes);
		return false;
	}

	TRACE("串口%d配置成功：9600,8,N,1\n", uartNum);
	return true;
}

// 高速串口配置示例
bool ConfigureHighSpeedUart(short uartNum)
{
	int iRes = 0;

	// 高速115200,8,N,1配置
	iRes = g_MultiCard.MC_UartConfig(uartNum, 115200, 8, 0, 1);
	if(iRes != 0)
	{
		TRACE("串口%d配置115200,8,N,1失败，错误代码: %d\n", uartNum, iRes);
		return false;
	}

	TRACE("串口%d配置成功：115200,8,N,1\n", uartNum);
	return true;
}
```

### 示例3：带校验的串口配置
```cpp
// 配置带校验的串口通信
bool ConfigureUartWithParity(short uartNum, long baudRate, short parity)
{
	int iRes = 0;

	// 配置带校验的串口参数
	iRes = g_MultiCard.MC_UartConfig(uartNum, baudRate, 8, parity, 1);
	if(iRes != 0)
	{
		TRACE("串口%d配置失败，波特率:%ld, 校验:%d, 错误代码: %d\n",
			  uartNum, baudRate, parity, iRes);
		return false;
	}

	const char* parityStr[] = {"无校验", "奇校验", "偶校验"};
	TRACE("串口%d配置成功：%ld,8,%s,1\n", uartNum, baudRate, parityStr[parity]);
	return true;
}
```

## 参数映射表

| 应用场景 | lBaudRate | nDataLen | nVerifyType | nStopBit | 说明 |
|----------|-----------|----------|-------------|----------|------|
| 标准配置 | 9600 | 8 | 0 | 1 | 9600,8,N,1最常用 |
| 高速通信 | 115200 | 8 | 0 | 1 | 高速数据传输 |
| 工业设备 | 19200 | 8 | 1 | 1 | 19200,8,O,1奇校验 |
| 仪器通信 | 38400 | 7 | 2 | 2 | 38400,7,E,2偶校验 |
| 调试模式 | 57600 | 8 | 0 | 1 | 57600,8,N,1调试用 |

## 关键说明

1. **串口参数选择**：
   - **波特率**：必须与通信设备保持一致，常用值：9600, 19200, 38400, 57600, 115200
   - **数据位**：通常为8位，某些设备可能使用7位
   - **校验位**：无校验最常用，奇偶校验用于提高通信可靠性
   - **停止位**：通常为1位，某些设备可能需要2位

2. **配置顺序**：
   - 在通信前必须先配置串口参数
   - 配置成功后才能进行发送和接收操作
   - 更改参数需要重新配置

3. **常见波特率应用**：
   - **9600**：低速设备，如传感器、仪表
   - **19200/38400**：工业设备，PLC、变频器
   - **115200**：高速通信，数据采集、图像传输

4. **校验类型选择**：
   - **无校验(0)**：通信距离短，环境干扰小
   - **奇校验(1)**：简单错误检测
   - **偶校验(2)**：工业标准，可靠性较高

5. **注意事项**：
   - 确保控制卡支持指定的串口号
   - 通信双方参数必须完全一致
   - 配置失败时检查参数是否正确
   - 高波特率可能需要较短的通信距离

6. **典型应用**：
   - **连接PLC**：通常使用19200或38400，带校验
   - **连接传感器**：通常使用9600，无校验
   - **连接显示器**：通常使用115200，无校验
   - **调试通信**：可使用任意标准参数

## 函数区别

- **MC_UartConfig() vs MC_SendEthToUartString()**: MC_UartConfig()配置参数，MC_SendEthToUartString()发送数据
- **MC_UartConfig() vs MC_ReadUartToEthString()**: MC_UartConfig()配置参数，MC_ReadUartToEthString()接收数据
- **配置 vs 通信**: 先配置串口参数，再进行数据收发

---

**使用建议**: 在配置串口前，先确认通信设备的参数要求。配置成功后建议先进行简单的收发测试，确保通信正常。对于长距离或高干扰环境，建议使用带校验的配置方式。