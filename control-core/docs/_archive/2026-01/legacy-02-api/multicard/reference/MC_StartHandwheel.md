# MC_StartHandwheel 函数教程

## 函数原型

```c
int MC_StartHandwheel(short nCrdNum, short nAxisNum, long lMasterEven, long lSlaveEven,
                      long lCounterDir, short nSynMode, short nFilterTime,
                      short nInputFilter, long lSegNum)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，通常固定为1 |
| nAxisNum | short | 轴号，指定进入手轮模式的轴（1-16） |
| lMasterEven | long | 比例分母，手轮摇动的格数基数 |
| lSlaveEven | long | 比例分子，电机移动的脉冲数 |
| lCounterDir | long | 计数方向，通常固定为0 |
| nSynMode | short | 同步模式，通常固定为1 |
| nFilterTime | short | 滤波时间，通常固定为1 |
| nInputFilter | short | 输入滤波，通常固定为50 |
| lSegNum | long | 段号，通常固定为0 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：X轴进入手轮模式
```cpp
// 从DemoVCDlg.cpp中提取的X轴手轮模式启动代码
void CDemoVCDlg::OnBnClickedButtonEnterHandwhell()
{
	int iRes;

	//之前为手轮模式的轴先退出手轮模式
	g_MultiCard.MC_EndHandwheel(1);

	//说明：MC_StartHandwheel函数共9个参数，实际使用中，只需要关心第1个参数（轴号），第3个参数（比例分母），第4个参数（比例分子）
	//第一个参数轴号决定了哪个轴进入手轮模式，第3和第4个参数比例分子和比例分母则决定了手轮摇动过程中，该轴的移动速度。如果比例分子和比例分母都是1，则手轮摇动1格，电机走1个脉冲
	//其他6个参数固定，不用修改。
	iRes = g_MultiCard.MC_StartHandwheel(1,4,m_lMasterEven,m_lSlaveEven,0,1,1,50,0);

	//轴选和倍率部分参见定时器函数OnTimer
	//X/Y/Z/A/B/X1/X10/X100这些逻辑，是自己组合的。

	//比如，读取到X轴的轴选IO信号时，将X轴切换到手轮模式，读取到Y轴的轴选IO信号时，首先将X轴退出手轮模式后，将Y轴设置为手轮模式。

	//比如：读取到X1倍率信号时，将比例分子分母设置为1：1，当读取到X10信号时，将比例分子分母设置为1：10(根据自己机床的螺距、电机每圈脉冲个数自行设置，不一定是1：10)
	//注意要先退出手轮模式，再进入手轮模式，比例分子和比例分母才能生效
}
```

### 示例2：多轴手轮切换逻辑
```cpp
// 根据轴选信号切换手轮控制轴（伪代码示例，基于实际项目逻辑）
void SwitchHandwheelAxis(int targetAxis)
{
	// 退出当前手轮模式
	g_MultiCard.MC_EndHandwheel(1);

	// 根据目标轴设置不同的比例参数
	switch(targetAxis)
	{
	case 1: // X轴
		g_MultiCard.MC_StartHandwheel(1,1,m_lMasterEven,m_lSlaveEven,0,1,1,50,0);
		break;
	case 2: // Y轴
		g_MultiCard.MC_StartHandwheel(1,2,m_lMasterEven,m_lSlaveEven,0,1,1,50,0);
		break;
	case 3: // Z轴
		g_MultiCard.MC_StartHandwheel(1,3,m_lMasterEven,m_lSlaveEven,0,1,1,50,0);
		break;
	}
}
```

## 参数映射表

| 应用场景 | nCrdNum | nAxisNum | lMasterEven | lSlaveEven | 说明 |
|----------|---------|----------|-------------|------------|------|
| X轴手轮 | 1 | 1 | m_lMasterEven | m_lSlaveEven | X轴手轮控制 |
| Y轴手轮 | 1 | 2 | m_lMasterEven | m_lSlaveEven | Y轴手轮控制 |
| Z轴手轮 | 1 | 3 | m_lMasterEven | m_lSlaveEven | Z轴手轮控制 |
| 高精度模式 | 1 | 目标轴 | 10 | 1 | 1:10高精度比例 |
| 快速模式 | 1 | 目标轴 | 1 | 10 | 10:1快速移动 |

## 关键说明

1. **核心参数**：
   - **nAxisNum**：决定哪个轴进入手轮模式
   - **lMasterEven** 和 **lSlaveEven**：决定手轮摇动与电机移动的比例关系
   - 比例 = lSlaveEven : lMasterEven（分子:分母）

2. **比例计算**：
   - 如果比例分子和比例分母都是1，手轮摇动1格，电机走1个脉冲
   - 需要根据机床螺距和电机每圈脉冲数调整比例
   - 例如：螺距5mm，电机每圈1000脉冲，手轮每圈100格，则比例为1:50

3. **切换逻辑**：
   - 必须先调用 MC_EndHandwheel() 退出当前手轮模式
   - 再调用 MC_StartHandwheel() 进入新的手轮模式
   - 比例参数只有在重新进入手轮模式时才生效

4. **参数简化**：
   - 实际使用中主要关心前4个参数
   - 后5个参数通常使用固定值：0, 1, 1, 50, 0

5. **应用场景**：
   - **手动对刀**：精确调整刀具位置
   - **工件找正**：手动调整工件位置
   - **设备调试**：手动测试各轴运动
   - **精密定位**：需要手动微调的场合

6. **注意事项**：
   - 手轮模式期间其他运动指令会被忽略
   - 需要配合轴选和倍率选择逻辑
   - 确保手轮硬件连接正常
   - 注意手轮运动的精度和安全性

## 函数区别

- **MC_StartHandwheel() vs MC_EndHandwheel()**: MC_StartHandwheel()进入手轮模式，MC_EndHandwheel()退出手轮模式
- **MC_StartHandwheel() vs 手动JOG**: 手轮模式更精确，适合微调；JOG模式适合快速移动
- **比例控制**: 通过调整 lMasterEven 和 lSlaveEven 实现不同的运动精度

---

**使用建议**: 在精密加工和调试场景中，合理设置手轮比例参数，既能保证精度又能提高操作效率。建议配合轴选和倍率选择功能，实现灵活的手轮控制。