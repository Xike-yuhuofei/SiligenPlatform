# MC_ArcXYC 函数教程

## 函数原型

```c
int MC_ArcXYC(short nCrdNum, long x, long y, double xCenter, double yCenter, short circleDir, double synVel, double synAcc, double velEnd, short nFifoIndex, long segNum)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，取值范围：1-16 |
| x | long | 圆弧终点X坐标（脉冲数） |
| y | long | 圆弧终点Y坐标（脉冲数） |
| xCenter | double | 圆心X坐标（脉冲数） |
| yCenter | double | 圆心Y坐标（脉冲数） |
| circleDir | short | 圆弧方向（0=逆时针，1=顺时针） |
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

### 示例1：基本圆弧插补运动
```cpp
// 从DemoVCDlg.cpp中提取的基本圆弧插补运动代码
void CDemoVCDlg::OnBnClickedButton13()
{
    int iRes = 0;

    // 插入圆弧数据：终点(40000,0)，圆心(20000,0)
    iRes = g_MultiCard.MC_ArcXYC(1,40000,0,20000,0,0,50,0.2,0,0,3);

    if(iRes == 0)
    {
        TRACE("圆弧插补数据发送成功\n");
        TRACE("圆弧参数 - 终点:(40000,0), 圆心:(20000,0), 速度:50, 加速度:0.2, 方向:顺时针\n");
    }
    else
    {
        TRACE("圆弧插补数据发送失败，错误代码: %d\n", iRes);
    }
}
```

### 示例2：复杂路径中的圆弧和直线组合
```cpp
// 从DemoVCDlg.cpp中提取的复杂路径组合代码
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

### 示例3：三维运动与圆弧组合
```cpp
// 从DemoVCDlg.cpp中提取的三维运动和圆弧组合代码
/*
// 插入三维直线数据，X=0,Y=0,Z=0,速度=50脉冲/ms，加速度=0.2脉冲/毫秒^2,终点速度为0，Fifo为0，用户自定义行号为7
iRes += g_MultiCard.MC_LnXYZ(1,0,0,0,50,0.2,0,0,7);
*/
//iRes += g_MultiCard.MC_ArcXYC(1,50000,50000,25000,0,0,50,0.2,0,0,8);

//iRes = g_MultiCard.MC_BufDelay(1,2000,0,8);

//iRes += g_MultiCard_MC_ArcXYC(1,50000,0,0,-25000,1,50,0.2,0,0,3);

//iRes = g_MultiCard_MC_BufDelay(1,2000,0,8);
```

### 示例4：注释中的圆弧运动示例
```cpp
// 从DemoVCDlg.cpp中提取的注释代码示例
/*
iRes = g_MultiCard.MC_ArcXYC(1,40000,0,20000,0,0,50,0.2,0,0,3);
iRes = g_MultiCard.MC_LnXY(1,0,0,50,0.2,0,0,1);

iRes = g_MultiCard.MC_LnXY(1,5000,2000,50,0.2,0,0,1);
iRes = g_MultiCard.MC_LnXY(1,15000,2000,50,0.2,0,0,2);
iRes = g_MultiCard.MC_ArcXYC(1,15000,12000,0,5000,0,50,0.2,0,0,3);
iRes = g_MultiCard.MC_LnXY(1,5000,12000,50,0.2,0,0,4);
*/
```

## 参数映射表

| 应用场景 | x,y | xCenter,yCenter | circleDir | synVel | synAcc | 说明 |
|----------|-----|----------------|-----------|--------|--------|------|
| 基本圆弧 | 目标终点 | 圆心坐标 | 0/1 | 50 | 0.2 | 标准圆弧运动 |
| 大圆弧 | 目标终点 | 圆心坐标 | 0/1 | 50 | 0.2 | 较大半径圆弧 |
| 反向圆弧 | 目标终点 | 圆心坐标 | 0/1 | 50 | 0.2 | 顺时针方向 |
| 组合路径 | 目标终点 | 圆心坐标 | 0/1 | 50 | 0.2 | 与直线组合 |

## 关键说明

1. **圆弧定义**：
   - 通过起点、终点和圆心定义圆弧
   - 圆心坐标为绝对坐标值
   - 方向参数确定圆弧走向

2. **方向控制**：
   - **0**: 逆时针方向
   - **1**: 顺时针方向
   - 影响圆弧的实际路径

3. **几何约束**：
   - 起点、终点、圆心三点不能共线
   - 终点必须位于以圆心为中心、半径为距离的圆上
   - 支持任意角度的圆弧（包括大于180度的圆弧）

4. **运动特性**：
   - 保持恒定的合成速度
   - 各轴速度按圆弧切线方向变化
   - 加速度影响圆弧的启动和停止过程

5. **段号管理**：
   - segNum用于标识不同的运动段
   - 便于调试和运动跟踪
   - 在复杂运动序列中起到重要标识作用

6. **注意事项**：
   - 确保圆弧参数的几何正确性
   - 小半径圆弧使用较低速度避免机械冲击
   - 注意终点速度设置对后续运动的影响
   - 验证圆弧路径不与机械限位冲突

## 函数区别

- **MC_ArcXYC() vs MC_LnXY()**: 圆弧插补 vs 直线插补
- **MC_ArcXYC() vs MC_Helix****: 平面圆弧 vs 空间螺旋线
- **圆弧插补 vs 多段直线**: 平滑曲线 vs 折线逼近

---

**使用建议**: 在使用圆弧插补前，建议验证圆弧参数的几何正确性，确保起点、终点和圆心的关系符合要求。对于小半径圆弧，使用较低的速度以避免机械冲击和保证精度。圆弧的终点速度设置会影响与后续运动的平滑过渡，需要合理规划。定期检查圆弧运动的实际轨迹是否符合预期，确保加工精度。