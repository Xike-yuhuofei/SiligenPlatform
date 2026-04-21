# MC_CmpBufData 函数教程

## 函数原型

```c
int MC_CmpBufData(short nCmpEncodeNum, short nPluseType, short nStartLevel, short nTime,
                 long *pBuf1, short nBufLen1, long *pBuf2, short nBufLen2,
                 short nAbsPosFlag=0, short nTimerFlag=0)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCmpEncodeNum | short | 比较编码器号，1代表轴1的编码器 |
| nPluseType | short | 脉冲类型，2表示到比较位置后输出一个脉冲 |
| nStartLevel | short | 初始电平，0代表初始化为低电平 |
| nTime | short | 脉冲持续时间，单位取决于nTimerFlag参数 |
| pBuf1 | long* | 待比较数据1指针，存储位置比较点 |
| nBufLen1 | short | 待比较数据1长度 |
| pBuf2 | long* | 通道2待比较数据指针，一般用不到 |
| nBufLen2 | short | 通道2待比较数据长度，一般用不到 |
| nAbsPosFlag | short | 位置类型，0：相对当前位置，1：绝对位置 |
| nTimerFlag | short | 时间单位，0：μs，1：ms |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：写入位置比较数据
```cpp
// 从DemoVCDlg.cpp中提取的比较缓冲区数据设置代码
void CDemoVCDlg::OnBnClickedButtonCmpData()
{
    long CMPData1[10];
    long CMPData2[10];

    //初始化待比较数据，这里一次下发10个待比较数据
    for(int i=0; i<10; i++)
    {
        CMPData1[i] = 10000*(i+1);  // 10000, 20000, 30000...100000
    }

    //第一个参数代表轴1，说明是用轴1的编码器进行比较
    //第二个参数2的含义是脉冲，意思是到比较位置后，输出一个脉冲
    //第三个参数含义是初始电平，0代表初始化为低电平
    //第四个参数1000，代表脉冲持续时间
    //第五个参数是待比较数据指针
    //第六个参数是待比较数据长度
    //第七个参数是通道2带比较数据存放指针，一般用不到
    //第八个参数是待比较数据长度，一般用不到
    //第九个参数代表位置类型，0：相对当前位置1：绝对位置
    //最后一个参数是脉冲持续时间的单位，0us，1ms

    g_MultiCard.MC_CmpBufData(1,2,0,1000,CMPData1,10,CMPData2,0,0,0);

    //数据写完以后，当轴1运动到10000、20000、30000...位置的时候，比较器会输出一个脉冲
}
```

### 示例2：多位置点精确比较
```cpp
// 设置多个精确位置点的比较触发
bool SetMultiplePositionCompare()
{
    // 定义精确位置点（单位：脉冲）
    long positionPoints[] = {5000, 15000, 25000, 35000, 45000};
    int pointCount = sizeof(positionPoints) / sizeof(positionPoints[0]);

    long dummyBuffer[10];  // 第二通道缓冲区，一般不使用

    // 配置比较参数
    short encoderNum = 1;           // 使用轴1编码器
    short pulseType = 2;            // 脉冲输出
    short startLevel = 0;           // 初始低电平
    short pulseTime = 500;          // 脉冲时间（μs）
    short posType = 1;              // 绝对位置
    short timeUnit = 0;             // 微秒单位

    // 写入比较数据
    int iRes = g_MultiCard.MC_CmpBufData(encoderNum, pulseType, startLevel,
                                          pulseTime, positionPoints, pointCount,
                                          dummyBuffer, 0, posType, timeUnit);

    if(iRes == 0)
    {
        TRACE("多位置比较设置成功，位置点数：%d\n", pointCount);

        // 显示设置的位置点
        for(int i = 0; i < pointCount; i++)
        {
            TRACE("位置点%d: %ld 脉冲\n", i+1, positionPoints[i]);
        }
        return true;
    }
    else
    {
        TRACE("多位置比较设置失败，错误代码：%d\n", iRes);
        return false;
    }
}
```

### 示例3：相对位置比较
```cpp
// 设置相对于当前位置的比较点
bool SetRelativePositionCompare(long relativeOffset)
{
    long comparePosition[1] = {relativeOffset};  // 相对偏移量
    long dummyBuffer[1];

    // 使用相对位置模式（nAbsPosFlag = 0）
    int iRes = g_MultiCard.MC_CmpBufData(1, 2, 0, 1000,
                                          comparePosition, 1,
                                          dummyBuffer, 0, 0, 0);

    if(iRes == 0)
    {
        TRACE("相对位置比较设置成功，偏移量：%ld 脉冲\n", relativeOffset);
        return true;
    }
    else
    {
        TRACE("相对位置比较设置失败，错误代码：%d\n", iRes);
        return false;
    }
}
```

## 参数映射表

| 应用场景 | nCmpEncodeNum | nPluseType | nStartLevel | nTime | nAbsPosFlag | nTimerFlag | 说明 |
|----------|---------------|------------|--------------|-------|-------------|------------|------|
| 基本比较 | 1 | 2 | 0 | 1000 | 0 | 0 | 轴1绝对位置，1ms脉冲 |
| 精密控制 | 轴号 | 2 | 0 | 500 | 1 | 0 | 绝对位置，0.5ms脉冲 |
| 相对偏移 | 轴号 | 2 | 1 | 2000 | 0 | 0 | 相对位置，高电平开始 |
| 长脉冲 | 轴号 | 2 | 0 | 10000 | 1 | 0 | 10ms长脉冲 |
| 微秒精度 | 轴号 | 2 | 0 | 100 | 1 | 0 | 100μs精密脉冲 |

## 关键说明

1. **比较输出原理**：
   - 当编码器位置达到设定值时，输出指定脉冲
   - 支持多个位置点的连续比较
   - 适用于精确定位和位置触发应用

2. **参数配置要点**：
   - **nCmpEncodeNum**：指定使用哪个轴的编码器信号
   - **nPluseType**：通常设为2，表示输出脉冲
   - **nTime**：脉冲持续时间，受nTimerFlag影响单位
   - **pBuf1**：位置比较点数组，必须递增排列

3. **位置类型选择**：
   - **绝对位置(nAbsPosFlag=1)**：相对于机械零点的位置
   - **相对位置(nAbsPosFlag=0)**：相对于当前位置的偏移

4. **时间单位设置**：
   - **微秒精度(nTimerFlag=0)**：适合高精度应用
   - **毫秒单位(nTimerFlag=1)**：适合一般应用

5. **数据准备要求**：
   - 位置数据必须按递增顺序排列
   - 数据范围应在编码器有效范围内
   - 建议设置合理的间距避免冲突

6. **应用场景**：
   - **激光打标**：精确位置触发激光输出
   - **飞行切割**：运动过程中的精确位置控制
   - **位置检测**：到达特定位置时的信号输出
   - **同步控制**：多轴协调运动的位置同步

7. **注意事项**：
   - 通道2参数通常不使用，但需要提供有效的缓冲区指针
   - 比较数据写入后需要启动运动才能触发
   - 建议在设置前清除之前的比较数据
   - 注意脉冲时间不要过短，确保设备能正确识别

## 函数区别

- **MC_CmpBufData() vs MC_CmpPluse()**: MC_CmpBufData()设置多点位置比较，MC_CmpPluse()直接输出单个脉冲
- **MC_CmpBufData() vs MC_CmpBufSts()**: MC_CmpBufData()设置比较数据，MC_CmpBufSts()读取比较状态
- **缓冲区 vs 直接**: MC_CmpBufData()支持多点缓冲，MC_CmpPluse()是即时输出

---

**使用建议**: 在设置位置比较前，建议先清空之前的比较数据。对于精密应用，使用微秒时间单位和绝对位置模式。确保位置数据按递增顺序设置，并留出足够的间距避免触发冲突。