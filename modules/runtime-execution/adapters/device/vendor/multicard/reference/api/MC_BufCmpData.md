# MC_BufCmpData 函数教程

## 函数原型

```c
int MC_BufCmpData(short nCrdNum, unsigned short nCardIndex, unsigned short nAxis, unsigned short nCmpOutNum, unsigned short nCmpMode, long* pData, unsigned short nDataNum, unsigned short nDataStart, unsigned short nFilterTime, short nFifoIndex, long segNum)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| nCardIndex | unsigned short | 卡索引 |
| nAxis | unsigned short | 比较轴号 |
| nCmpOutNum | unsigned short | 比较输出编号 |
| nCmpMode | unsigned short | 比较模式 |
| pData | long* | 比较数据数组指针 |
| nDataNum | unsigned short | 数据数量 |
| nDataStart | unsigned short | 数据起始位置 |
| nFilterTime | unsigned short | 滤波时间（毫秒） |
| nFifoIndex | short | FIFO索引，通常为0 |
| segNum | long | 用户自定义段号 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本比较数据设置
```cpp
// 从DemoVCDlg.cpp中提取的比较数据设置代码
void CDemoVCDlg::OnBnClickedButton22()
{
    long lData[8] = {0};

    // 设置比较位置数据
    lData[0] = 10000;  // 第一个比较位置
    lData[1] = 20000;  // 第二个比较位置
    lData[2] = 30000;  // 第三个比较位置
    lData[3] = 40000;  // 第四个比较位置

    // 设置比较数据：卡1，轴2，输出3，模式4，8个数据，从位置1开始
    int iRes = g_MultiCard.MC_BufCmpData(1,1,2,3,4,&lData[0],8,1,0,0,0);

    if(iRes == 0)
    {
        TRACE("比较数据设置成功\n");
        TRACE("轴2，输出3，设置8个比较位置\n");
    }
    else
    {
        TRACE("比较数据设置失败，错误代码: %d\n", iRes);
    }
}
```

## 参数映射表

| 应用场景 | nAxis | nCmpMode | nDataNum | 说明 |
|----------|-------|----------|----------|------|
| 位置比较 | 1-8 | 1-4 | 1-8 | 单轴位置比较输出 |
| 多点比较 | 指定轴 | 指定模式 | 多个 | 连续位置比较 |
| 精密定位 | 控制轴 | 精密模式 | 少量 | 高精度比较输出 |

## 关键说明

1. **比较机制**：
   - 当轴位置达到指定数据时触发比较输出
   - 支持多个位置点的比较
   - 比较输出可用于激光控制、标记等

2. **轴选择**：
   - nAxis指定进行比较的轴号
   - 支持所有轴的位置比较
   - 每个轴可独立设置比较参数

3. **输出控制**：
   - nCmpOutNum指定比较输出编号
   - 不同编号对应不同的物理输出
   - 支持多个比较输出同时工作

4. **比较模式**：
   - nCmpMode定义比较方式
   - 包括位置触发、方向触发等模式
   - 根据应用需求选择合适模式

5. **数据管理**：
   - pData存储比较位置数据
   - nDataNum指定数据数量
   - nDataStart指定数据起始位置

6. **滤波功能**：
   - nFilterTime为信号滤波时间
   - 消除位置抖动影响
   - 确保比较输出的稳定性

7. **缓冲区特性**：
   - 比较数据进入缓冲区队列
   - 与运动指令同步执行
   - 支持动态修改比较参数

8. **应用场景**：
   - 激光打标位置控制
   - 飞行切割位置触发
   - 精密定位标记
   - 多点同步控制

## 函数区别

- **MC_BufCmpData() vs MC_SetCmp()**: 缓冲区比较 vs 立即比较
- **MC_BufCmpData() vs MC_BufIO()**: 位置比较 vs IO控制
- **单点比较 vs 多点比较**: 单一位置 vs 多个位置

---

**使用建议**: 比较输出功能适用于需要精确位置触发的应用，如激光打标、飞行切割等。比较位置应根据实际工艺需求精确计算，确保触发时机准确。滤波时间应根据运动速度和精度要求合理设置。建议在使用前进行测试验证，确认比较输出的准确性和稳定性。定期检查比较参数设置，确保满足生产工艺要求。