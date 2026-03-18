# MC_CmpBufSts 函数教程

## 函数原型

```c
int MC_CmpBufSts(unsigned short* pStatus, unsigned short* pRemainData1,
 unsigned short* pRemainData2, unsigned short* pRemainSpace1,
 unsigned short* pRemainSpace2)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| pStatus | unsigned short* | 比较器状态指针 |
| pRemainData1 | unsigned short* | 比较缓冲区1剩余数据指针 |
| pRemainData2 | unsigned short* | 比较缓冲区2剩余数据指针 |
| pRemainSpace1 | unsigned short* | 比较缓冲区1剩余空间指针 |
| pRemainSpace2 | unsigned short* | 比较缓冲区2剩余空间指针 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：读取比较缓冲区状态
```cpp
// 从DemoVCDlg.cpp中提取的比较缓冲区状态读取代码
unsigned short CmpStatus;
unsigned short CmpRemainData1;
unsigned short CmpRemainData2;

unsigned short CmpRemainSpace1;
unsigned short CmpRemainSpace2;

g_MultiCard.MC_CmpBufSts(&CmpStatus,&CmpRemainData1,&CmpRemainData2,&CmpRemainSpace1,&CmpRemainSpace2);

strText.Format("剩余待比较数据个数：%d",CmpRemainData1);
GetDlgItem(IDC_STATIC_CMP_COUNT)->SetWindowText(strText);

if(CmpStatus & 0X0001)
{
 ((CButton*)GetDlgItem(IDC_RADIO_EMPTY))->SetCheck(false);
}
else
{
 ((CButton*)GetDlgItem(IDC_RADIO_EMPTY))->SetCheck(true);
}
```

### 示例2：定时监控比较器状态
```cpp
// 在定时器中定期检查比较器状态
void MonitorComparatorStatus()
{
 unsigned short status;
 unsigned short remainData1, remainData2;
 unsigned short remainSpace1, remainSpace2;

 // 读取比较器状态
 int iRes = g_MultiCard.MC_CmpBufSts(&status, &remainData1, &remainData2,
 &remainSpace1, &remainSpace2);

 if(iRes == 0)
 {
 // 显示剩余数据数量
 TRACE("缓冲区1剩余数据：%d，剩余空间：%d\n", remainData1, remainSpace1);
 TRACE("缓冲区2剩余数据：%d，剩余空间：%d\n", remainData2, remainSpace2);

 // 检查状态位
 if(status & 0x0001)
 {
 TRACE("比较器空闲，可以写入新数据\n");
 }
 else
 {
 TRACE("比较器忙碌，正在执行比较任务\n");
 }

 // 检查缓冲区状态
 if(remainData1 == 0)
 {
 TRACE("缓冲区1数据已处理完毕\n");
 }

 // 检查空间是否充足
 if(remainSpace1 < 10)
 {
 TRACE("警告：缓冲区1空间不足，建议清理或等待\n");
 }
 }
 else
 {
 TRACE("读取比较器状态失败，错误代码：%d\n", iRes);
 }
}
```

### 示例3：状态位详细解析
```cpp
// 详细解析比较器状态位
void AnalyzeComparatorStatus(unsigned short status)
{
 TRACE("比较器状态详细分析：\n");
 TRACE("状态值：0x%04X\n", status);

 // 检查各个状态位
 if(status & 0x0001)
 {
 TRACE(" 比较器空闲\n");
 }
 else
 {
 TRACE(" 比较器忙碌\n");
 }

 // 检查其他状态位（根据实际硬件定义）
 if(status & 0x0002)
 {
 TRACE(" 缓冲区1有数据\n");
 }

 if(status & 0x0004)
 {
 TRACE(" 缓冲区2有数据\n");
 }

 if(status & 0x0008)
 {
 TRACE(" 比较器使能\n");
 }

 if(status & 0x0010)
 {
 TRACE(" 比较器错误\n");
 }

 TRACE("\n");
}
```

### 示例4：缓冲区管理
```cpp
// 管理比较缓冲区的数据写入
bool ManageCompareBuffer(long* newData, int dataCount)
{
 unsigned short status;
 unsigned short remainData1, remainSpace1;
 unsigned short dummyData2, dummySpace2;

 // 读取当前状态
 int iRes = g_MultiCard.MC_CmpBufSts(&status, &remainData1, &dummyData2,
 &remainSpace1, &dummySpace2);

 if(iRes != 0)
 {
 TRACE("读取缓冲区状态失败\n");
 return false;
 }

 // 检查是否有足够空间
 if(remainSpace1 >= dataCount)
 {
 // 有足够空间，可以写入数据
 TRACE("缓冲区空间充足，可以写入%d个数据\n", dataCount);
 return true;
 }
 else
 {
 // 空间不足，需要等待
 TRACE("缓冲区空间不足，当前空间：%d，需要：%d\n", remainSpace1, dataCount);
 TRACE("剩余数据：%d，建议等待处理完成后再写入\n", remainData1);
 return false;
 }
}
```

## 参数映射表

| 应用场景 | pStatus | pRemainData1 | pRemainSpace1 | 说明 |
|----------|---------|---------------|----------------|------|
| 基本状态检查 | 状态变量 | 剩余数据变量 | 剩余空间变量 | 获取基本状态信息 |
| 定时监控 | 状态数组 | 数据计数数组 | 空间计数数组 | 定时器中监控状态 |
| 缓冲区管理 | 状态指针 | 数据量指针 | 空间量指针 | 管理数据写入时机 |
| 调试诊断 | 状态变量 | 详细信息 | 详细信息 | 调试时获取详细信息 |

## 关键说明

1. **状态位含义**：
 - **bit0 (0x0001)**：比较器空闲状态，1表示空闲，0表示忙碌
 - **其他位**：根据具体硬件定义，可能包含错误、使能等状态

2. **缓冲区信息**：
 - **剩余数据(pRemainData1/2)**：未处理的数据数量
 - **剩余空间(pRemainSpace1/2)**：可写入的数据空间
 - **通道2**：通常不使用，但状态信息仍然提供

3. **状态监控**：
 - 建议在定时器中定期检查状态
 - 根据状态决定是否可以写入新数据
 - 监控缓冲区使用情况，避免溢出

4. **缓冲区管理**：
 - 在写入数据前检查剩余空间
 - 等待数据处理完成后再写入新数据
 - 根据缓冲区大小优化数据写入策略

5. **错误处理**：
 - 检查函数返回值确认读取成功
 - 监控异常状态位，及时处理错误
 - 记录状态变化，便于故障诊断

6. **应用场景**：
 - **实时监控**：监控比较器工作状态
 - **数据管理**：管理比较数据的写入时机
 - **调试诊断**：获取详细的运行状态信息
 - **性能优化**：根据缓冲区状态优化数据处理

7. **注意事项**：
 - 状态信息实时变化，需要定期读取
 - 注意数据类型为unsigned short
 - 通道2的信息通常可以忽略
 - 状态位的具体含义需要参考硬件文档

## 函数区别

- **MC_CmpBufSts() vs MC_CmpBufData()**: MC_CmpBufSts()读取状态，MC_CmpBufData()写入数据
- **MC_CmpBufSts() vs MC_CmpPluse()**: MC_CmpBufSts()读取缓冲状态，MC_CmpPluse()控制输出
- **读取 vs 写入**: 先读取状态确认可以写入，再调用写入函数

---

**使用建议**: 在写入比较数据前，先调用MC_CmpBufSts()检查缓冲区状态。建议在定时器中定期监控比较器状态，及时发现和处理异常情况。对于高频应用，可以优化读取频率，平衡实时性和系统负载。