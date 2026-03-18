# MC_LnXY 函数教程

## 函数原型

```c
int MC_LnXY(short nCrdNum, long x, long y, double synVel, double synAcc, double velEnd, short nFifoIndex, long segNum)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| x | long | X轴目标位置（脉冲数） |
| y | long | Y轴目标位置（脉冲数） |
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

### 示例1：基本直线插补运动
```cpp
// 从DemoVCDlg.cpp中提取的直线插补运动代码
void CDemoVCDlg::OnBnClickedButton10()
{
    int iRes = 0;

    // 发送第一条直线：从当前位置移动到(100000, 100000)
    iRes = g_MultiCard.MC_LnXY(1,100000,100000,50,2,0,0,1);

    // 发送第二条直线：返回原点
    iRes = g_MultiCard.MC_LnXY(1,0,0,50,2,0,0,1);

    if(iRes == 0)
    {
        TRACE("直线插补数据发送成功\n");
    }
    else
    {
        TRACE("直线插补数据发送失败，错误代码: %d\n", iRes);
    }
}
```

### 示例2：复杂运动序列中的直线插补
```cpp
// 从DemoVCDlg.cpp中提取的复杂运动序列代码
void CDemoVCDlg::OnBnClickedButton15()
{
    int iRes = 0;

    // 清空插补缓冲区原有数据
    //g_MultiCard.MC_CrdClear(1,0);

    // 插入二维直线数据，X=10000,Y=20000,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为1
    //iRes = g_MultiCard.MC_LnXY(1,30000,30000,50,0.2,0,0,1);
    //iRes = g_MultiCard.MC_LnXY(1,0,0,50,0.2,0,0,1);

    // 插入圆弧数据
    //iRes = g_MultiCard.MC_ArcXYC(1,40000,0,20000,0,0,50,0.2,0,0,3);
    //iRes = g_MultiCard.MC_LnXY(1,0,0,50,0.2,0,0,1);

    // 插入多个直线段构成复杂路径
    //iRes = g_MultiCard.MC_LnXY(1,5000,2000,50,0.2,0,0,1);
    //iRes = g_MultiCard.MC_LnXY(1,15000,2000,50,0.2,0,0,2);
    //iRes = g_MultiCard.MC_ArcXYC(1,15000,12000,0,5000,0,50,0.2,0,0,3);
    //iRes = g_MultiCard.MC_LnXY(1,5000,12000,50,0.2,0,0,4);
}
```

### 示例3：带IO控制的直线插补运动
```cpp
// 从DemoVCDlg.cpp中提取的带IO控制的插补运动代码
void CDemoVCDlg::OnBnClickedButton16()
{
    int iRes = 0;

    // 插入三维直线数据
    iRes += g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,4);

    // 插入二维直线数据，X=10000,Y=20000
    iRes = g_MultiCard.MC_LnXY(1,10000,20000,50,0.2,0,0,5);

    // 插入IO输出指令（不会影响运动）
    iRes = g_MultiCard.MC_BufIO(1,MC_GPO,0,0X0001,0X0001,0,5);

    // 插入二维直线数据，X=50000,Y=60000
    iRes = g_MultiCard.MC_LnXY(1,50000,60000,50,0.2,0,0,6);

    // 插入IO输出指令（关闭输出）
    iRes = g_MultiCard.MC_BufIO(1,MC_GPO,0,0X0001,0X0000,0,6);

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

## 参数映射表

| 应用场景 | x,y | synVel | synAcc | velEnd | 说明 |
|----------|-----|--------|--------|--------|------|
| 基本运动 | 目标坐标 | 50 | 2.0 | 0 | 标准直线运动 |
| 精密运动 | 目标坐标 | 50 | 0.2 | 0 | 低加速度精密运动 |
| IO同步 | 目标坐标 | 50 | 0.2 | 0 | 与IO控制同步 |
| 连续路径 | 目标坐标 | 50 | 0.2 | 0 | 多段连续运动 |

## 关键说明

1. **坐标系统**：
   - 基于已建立的坐标系进行插补
   - 坐标值为脉冲数，需要根据机械参数转换
   - 原点位置由坐标系配置决定

2. **运动参数**：
   - **synVel**: 合成速度，整体运动速度
   - **synAcc**: 合成加速度，加减速性能
   - **velEnd**: 终点速度，用于连续运动平滑过渡

3. **运动轨迹**：
   - 直线插补保证两点间的直线运动
   - 各轴协调运动，保证路径精度
   - 支持任意角度的直线运动

4. **段号管理**：
   - segNum用于标识不同的运动段
   - 便于调试和运动跟踪
   - 在复杂运动序列中起到重要标识作用

5. **FIFO缓冲**：
   - 运动数据先发送到缓冲区
   - 通过MC_CrdData(NULL)触发执行
   - 支持多段运动的连续执行

6. **注意事项**：
   - 确保坐标系已正确配置
   - 运动前检查系统状态
   - 合理设置运动参数避免机械冲击
   - 注意终点速度设置对连续运动的影响

## 函数区别

- **MC_LnXY() vs MC_LnXYZ()**: 二维直线 vs 三维直线
- **MC_LnXY() vs MC_ArcXYC()**: 直线插补 vs 圆弧插补
- **直线插补 vs 点位运动**: 路径控制 vs 位置控制

---

**使用建议**: 在使用直线插补前，确保坐标系已正确配置且各轴处于正常状态。根据具体应用选择合适的运动参数，短距离精密运动使用较低速度，长距离运动可以适当提高速度。对于连续路径运动，合理设置终点速度以实现平滑过渡。