# MC_LnXYZ 函数教程

## 函数原型

```c
int MC_LnXYZ(short nCrdNum, long x, long y, long z, double synVel, double synAcc, double velEnd, short nFifoIndex, long segNum)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| x | long | X轴目标位置（脉冲数） |
| y | long | Y轴目标位置（脉冲数） |
| z | long | Z轴目标位置（脉冲数） |
| synVel | double | 合成速度（脉冲/毫秒） |
| synAcc | double | 合成加速度（脉冲/毫秒²） |
| velEnd | double | 终点速度（脉冲/毫秒） |
| nFifoIndex | short | FIFO索引，通常为0 |
| segNum | long | 用户自定义段号 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：循环三维直线运动
```cpp
// 从DemoVCDlg.cpp中提取的三维直线循环运动代码
for(int i = 0; i < 3; i++)
{
    // 返回原点 (0,0,0)
    iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,2);

    // 移动到指定位置 (10000,20000,30000)
    iRes = g_MultiCard.MC_LnXYZ(1,10000,20000,30000,50,0.2,0,0,2);
}

// 最后返回原点
iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,2);
```

### 示例2：三维直线与二维直线混合运动
```cpp
// 从DemoVCDlg.cpp中提取的混合运动代码
void CDemoVCDlg::OnBnClickedButton16()
{
    int iRes = 0;

    // 插入三维直线数据，从原点开始
    iRes += g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

    // 插入二维直线数据，X=10000,Y=20000
    iRes = g_MultiCard.MC_LnXY(1,10000,20000,50,0.2,0,0,5);

    // 插入IO输出指令（不会影响运动）
    iRes = g_MultiCard.MC_BufIO(1,MC_GPO,0,0X0001,0X0001,0,5);

    // 插入二维直线数据，X=50000,Y=60000
    iRes = g_MultiCard.MC_LnXY(1,50000,60000,50,0.2,0,0,6);

    // 插入IO输出指令（关闭输出）
    iRes = g_MultiCard.MC_BufIO(1,MC_GPO,0,0X0001,0X0000,0,6);

    // 插入三维直线数据，返回原点
    iRes += g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,7);

    // 插入最后一行标识符
    iRes += g_MultiCard.MC_CrdData(1,NULL,0);

    if(0 == iRes)
    {
        TRACE("插补数据发送成功！\n");
    }
    else
    {
        TRACE("插补数据发送失败！\n");
    }
}
```

### 示例3：三维运动序列
```cpp
// 从DemoVCDlg.cpp中提取的连续三维运动代码
/*
// 插入三维直线数据，X=0,Y=0,Z=0速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为2
iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,2);

// 插入三维直线数据，X=10000,Y=20000,Z=2000,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为3
iRes += g_MultiCard.MC_LnXYZ(1,10000,20000,2000,50,0.2,0,0,3);

// 插入三维直线数据，X=0,Y=0,Z=0,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为4
iRes += g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

// 插入二维直线数据，X=10000,Y=20000,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为5
iRes = g_MultiCard.MC_LnXY(1,10000,20000,50,0.2,0,0,5);

// 插入三维直线数据，X=0,Y=0,Z=0,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为7
iRes += g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,7);
*/
```

### 示例4：与IO控制结合的三维运动
```cpp
// 从DemoVCDlg.cpp中提取的带IO控制的三维运动代码
/*
iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,1);
iRes = g_MultiCard.MC_LnXYZ(1,50000,0,0,50,0.2,0,0,2);

iRes = g_MultiCard.MC_BufIO(1,MC_GPO,1,0X0010,0X0010,0,0);
iRes = g_MultiCard.MC_BufDelay(1,1000,0,8);

iRes = g_MultiCard.MC_BufIO(1,MC_GPO,1,0X0020,0X0020,0,0);
iRes = g_MultiCard.MC_BufIO(1,MC_GPO,0,0X0100,0X0100,0,0);

iRes = g_MultiCard.MC_BufDelay(1,1000,0,8);

iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

// 插入最后一行标识符（系统会把前瞻缓冲区数据全部压入板卡）
iRes += g_MultiCard.MC_CrdData(1,NULL,0);
*/
```

### 示例5：带IO等待的三维运动
```cpp
// 从DemoVCDlg.cpp中提取的带IO等待的三维运动代码
/*
// 等待IO，nLevel=1等待有信号输入，lWaitTimeMS=0一直等待
g_MultiCard.MC_BufWaitIO(1,0,0,1,0,100,0,3);

iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

iRes = g_MultiCard.MC_BufDelay(1,2000,0,8);

iRes = g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

// 插入最后一行标识符（系统会把前瞻缓冲区数据全部压入板卡）
iRes += g_MultiCard.MC_CrdData(1,NULL,0);
*/
```

## 参数映射表

| 应用场景 | x,y,z | synVel | synAcc | velEnd | 说明 |
|----------|-------|--------|--------|--------|------|
| 基本三维运动 | 目标3D坐标 | 50 | 0.2 | 0 | 标准三维直线运动 |
| 原点返回 | (0,0,0) | 50 | 0.2 | 0 | 返回坐标系原点 |
| 混合运动 | 三维或二维 | 50 | 0.2 | 0 | 三维与二维混合 |
| IO同步运动 | 任意坐标 | 50 | 0.2 | 0 | 与IO控制同步 |

## 关键说明

1. **三维坐标系**：
   - 需要预先建立三维坐标系（dimension=3）
   - X、Y、Z轴映射到物理轴
   - 支持任意三维空间直线运动

2. **运动协调**：
   - 三轴同时启动和停止
   - 保持直线路径精度
   - 各轴速度按比例分配

3. **空间计算**：
   - 合成速度为三轴速度的矢量和
   - 运动距离为三维空间直线距离
   - 加速度影响整体运动性能

4. **段号管理**：
   - segNum用于标识不同的运动段
   - 便于调试和运动跟踪
   - 在复杂运动序列中起到重要标识作用

5. **混合运动**：
   - 可以与二维直线插补混合使用
   - 支持从三维运动切换到二维运动
   - 通过坐标系参数实现灵活切换

6. **注意事项**：
   - 确保三轴机械系统状态良好
   - 注意Z轴的负载能力和稳定性
   - 合理设置运动参数避免机械冲击
   - 监控各轴的运动状态

## 函数区别

- **MC_LnXYZ() vs MC_LnXY()**: 三维直线 vs 二维直线
- **MC_LnXYZ() vs MC_ArcXYC()**: 直线插补 vs 圆弧插补
- **三维运动 vs 多轴点位运动**: 路径控制 vs 位置控制

---

**使用建议**: 在使用三维直线插补前，确保已正确配置三维坐标系且各轴工作正常。对于垂直运动（主要是Z轴变化），建议使用较保守的速度参数。长距离三维运动时，注意机械系统的刚性和稳定性。定期检查各轴的同步性能，确保三维运动的精度和重复性。