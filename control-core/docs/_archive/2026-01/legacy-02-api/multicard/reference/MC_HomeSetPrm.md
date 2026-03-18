# MC_HomeSetPrm 函数教程

## 函数原型

```c
int MC_HomeSetPrm(short nAxis, TAxisHomePrm* pHomePrm)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nAxis | short | 轴号，取值范围：1-16 |
| pHomePrm | TAxisHomePrm* | 回零参数结构体指针 |

### TAxisHomePrm 结构体主要成员
```c
typedef struct _AxisHomeParm{
	short		nHomeMode;					//回零方式：0--无 1--HOME回原点	2--HOME加Index回原点3----Z脉冲
	short		nHomeDir;					//回零方向，1-正向回零，0-负向回零
	long        lOffset;                    //回零偏移，回到零位后再走一个Offset作为零位
	double		dHomeRapidVel;			    //回零快移速度，单位：Pluse/ms
	double		dHomeLocatVel;			    //回零定位速度，单位：Pluse/ms
	double		dHomeIndexVel;			    //回零寻找INDEX速度，单位：Pluse/ms
	double      dHomeAcc;                   //回零使用的加速度
	long ulHomeIndexDis;           //找Index最大距离
	long ulHomeBackDis;            //回零时，第一次碰到零位后的回退距离
	unsigned short nDelayTimeBeforeZero;    //位置清零前，延时时间，单位ms
	unsigned long ulHomeMaxDis;//回零最大寻找范围，单位脉冲
}TAxisHomePrm;
```

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本回零参数设置
```cpp
// 从DemoVCDlg.cpp中提取的回零参数设置代码
TAxisHomePrm AxisHomePrm;

AxisHomePrm.nHomeMode = 1;//回零方式：0--无 1--HOME回原点	2--HOME加Index回原点3----Z脉冲
AxisHomePrm.nHomeDir = 0;//回零方向，1-正向回零，0-负向回零
AxisHomePrm.lOffset = 1;//回零偏移，回到零位后再走一个Offset作为零位
AxisHomePrm.dHomeRapidVel = 1;//回零快移速度，单位：Pluse/ms
AxisHomePrm.dHomeLocatVel = 1;//回零定位速度，单位：Pluse/ms
AxisHomePrm.dHomeIndexVel = 1;//回零寻找INDEX速度，单位：Pluse/ms
AxisHomePrm.dHomeAcc = 1;//回零使用的加速度

AxisHomePrm.ulHomeIndexDis=0;           //找Index最大距离
AxisHomePrm.ulHomeBackDis=0;            //回零时，第一次碰到零位后的回退距离
AxisHomePrm.nDelayTimeBeforeZero=1000;    //位置清零前，延时时间，单位ms
AxisHomePrm.ulHomeMaxDis=0;//回零最大寻找范围，单位脉冲

int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

g_MultiCard.MC_HomeSetPrm(iAxisNum,&AxisHomePrm);
```

## 参数映射表

| 应用场景 | homeMode | homeDir | velHigh | velLow | 说明 |
|----------|----------|---------|---------|--------|------|
| 标准回零 | HOME_MODE_LIMIT | DIR_NEG | 5000 | 1000 | 限位开关回零 |
| 原点回零 | HOME_MODE_ORG | DIR_POS | 3000 | 500 | 原点开关回零 |
| 编码器回零 | HOME_MODE_INDEX | DIR_NEG | 2000 | 200 | 编码器Z相信号回零 |

## 关键说明

1. **回零模式选择**：
   - 限位开关回零：使用机械限位开关
   - 原点开关回零：使用专用原点开关
   - 编码器回零：使用编码器Z相信号

2. **参数配置要点**：
   - 回零速度：高速用于搜索，低速用于精确定位
   - 回零方向：根据机械结构确定
   - 偏移量：用于精确定位到机械原点

3. **注意事项**：
   - 回零参数必须根据机械结构正确配置
   - 回零模式、方向、速度等参数需要合理设置
   - 确保回零开关信号稳定可靠

## 函数区别

- **MC_HomeSetPrm() vs MC_HomeStart()**: MC_HomeSetPrm()设置参数，MC_HomeStart()启动回零
- **MC_HomeSetPrm() vs MC_HomeStop()**: MC_HomeSetPrm()配置参数，MC_HomeStop()停止回零
- **参数设置 vs 执行**: 先调用MC_HomeSetPrm()设置参数，再调用MC_HomeStart()执行

---

**使用建议**: 在执行回零操作前，务必根据实际机械结构正确配置回零参数，确保回零过程安全可靠。
- 设置回零参数前确保轴已使能
- 回零过程中避免人工干预