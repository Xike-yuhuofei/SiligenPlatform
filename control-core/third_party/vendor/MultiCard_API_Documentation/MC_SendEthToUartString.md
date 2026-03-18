# MC_SendEthToUartString 函数教程

## 函数原型

```c
int MC_SendEthToUartString(short nUartNum, unsigned char* pSendBuf, short nSendLen)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nUartNum | short | 串口号，取值范围：1-N（与MC_UartConfig中的串口号一致） |
| pSendBuf | unsigned char* | 发送数据缓冲区指针 |
| nSendLen | short | 发送数据长度（字节数） |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：发送固定字符串
```cpp
// 从DemoVCDlg.cpp中提取的串口发送代码
void CDemoVCDlg::OnBnClickedButtonUartSend()
{
	g_MultiCard.MC_SendEthToUartString(m_ComboBoxUartNum.GetCurSel()+1,
		(unsigned char *)"1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890",100);
}
```

### 示例2：发送用户输入的字符串
```cpp
// 发送用户在界面中输入的字符串
void SendUserInputString(short uartNum, CString userInput)
{
	// 转换为UTF-8编码
	CT2CA utf8String(userInput, CP_UTF8);
	const char* sendData = utf8String;
	int dataLength = strlen(sendData);

	if(dataLength > 0)
	{
		int iRes = g_MultiCard.MC_SendEthToUartString(uartNum,
			(unsigned char*)sendData, dataLength);

		if(iRes == 0)
		{
			TRACE("串口%d发送成功，长度: %d字节\n", uartNum, dataLength);
		}
		else
		{
			TRACE("串口%d发送失败，错误代码: %d\n", uartNum, iRes);
		}
	}
}
```

### 示例3：发送十六进制数据
```cpp
// 发送十六进制格式的数据包
void SendHexDataPacket(short uartNum)
{
	unsigned char hexPacket[] = {
		0xAA, 0x55, // 包头
		0x01,       // 命令字
		0x04,       // 数据长度
		0x12, 0x34, 0x56, 0x78, // 数据
		0x00        // 校验和（简化示例）
	};

	int packetLength = sizeof(hexPacket);

	int iRes = g_MultiCard.MC_SendEthToUartString(uartNum, hexPacket, packetLength);
	if(iRes == 0)
	{
		TRACE("串口%d发送十六进制数据包成功\n", uartNum);

		// 显示发送的数据
		CString hexDisplay;
		for(int i = 0; i < packetLength; i++)
		{
			CString temp;
			temp.Format("%02X ", hexPacket[i]);
			hexDisplay += temp;
		}
		TRACE("发送数据: %s\n", hexDisplay);
	}
	else
	{
		TRACE("串口%d发送失败，错误代码: %d\n", uartNum, iRes);
	}
}
```

### 示例4：批量发送数据
```cpp
// 分批发送大量数据
void SendLargeData(short uartNum, const char* largeData, int totalSize)
{
	const int CHUNK_SIZE = 256; // 每次发送256字节
	int sentSize = 0;

	while(sentSize < totalSize)
	{
		int chunkSize = min(CHUNK_SIZE, totalSize - sentSize);

		int iRes = g_MultiCard.MC_SendEthToUartString(uartNum,
			(unsigned char*)(largeData + sentSize), chunkSize);

		if(iRes != 0)
		{
			TRACE("串口%d发送失败，已发送: %d字节，错误代码: %d\n",
				  uartNum, sentSize, iRes);
			return;
		}

		sentSize += chunkSize;
		TRACE("串口%d已发送: %d/%d字节\n", uartNum, sentSize, totalSize);

		// 短暂延时，避免发送过快
		Sleep(10);
	}

	TRACE("串口%d数据发送完成，总计: %d字节\n", uartNum, totalSize);
}
```

## 参数映射表

| 应用场景 | pSendBuf | nSendLen | 说明 |
|----------|----------|----------|------|
| 固定字符串 | "123456..." | 100 | 发送测试字符串 |
| 用户输入 | 界面获取的字符串 | 实际长度 | 发送用户输入 |
| 十六进制数据 | {0xAA, 0x55...} | 数组长度 | 发送协议数据包 |
| 分批发送 | 数据指针 | 256 | 大数据分块发送 |
| 命令指令 | 命令结构体 | 结构体大小 | 发送控制命令 |

## 关键说明

1. **发送前准备**：
   - 必须先调用 MC_UartConfig() 配置串口参数
   - 确保通信设备已准备好接收数据
   - 检查串口号是否正确

2. **数据格式**：
   - 发送的数据可以是字符串或二进制数据
   - 字符串需要正确处理编码（UTF-8、GBK等）
   - 二进制数据可以直接发送字节数组

3. **发送长度**：
   - nSendLen 必须小于或等于实际数据长度
   - 发送过长数据可能导致缓冲区溢出
   - 建议大数据分批发送

4. **错误处理**：
   - 检查返回值确认发送是否成功
   - 发送失败时检查串口配置和连接状态
   - 记录发送失败的详细信息

5. **性能考虑**：
   - 避免过于频繁的小数据发送
   - 大数据建议分批发送，添加适当延时
   - 考虑通信设备的接收缓冲区大小

6. **应用场景**：
   - **命令发送**：向PLC、传感器发送控制命令
   - **数据上传**：向监控系统上传采集数据
   - **参数配置**：向设备发送配置参数
   - **调试通信**：测试串口通信功能

## 函数区别

- **MC_SendEthToUartString() vs MC_ReadUartToEthString()**: MC_SendEthToUartString()发送数据，MC_ReadUartToEthString()接收数据
- **MC_SendEthToUartString() vs MC_UartConfig()**: MC_SendEthToUartString()发送数据，MC_UartConfig()配置参数
- **发送 vs 接收**: 串口通信是双向的，发送和接收成对使用

---

**使用建议**: 在发送重要数据前，建议先建立通信握手。对于关键命令，可以实现发送确认机制，确保设备正确接收并处理了发送的指令。发送完成后可以调用接收函数等待设备的响应。