# MC_HomeStart

## 函数签名
int MC_HomeStart(short nAxis);

## 参数
nAxis: 轴号，取值范围：1-16

## 返回值
成功返回 0，失败返回非 0 错误代码

## 实际代码示例

### 示例1：启动轴回零操作
```cpp
// 从DemoVCDlg.cpp中提取的回零启动代码
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
g_MultiCard.MC_HomeStop(iAxisNum);
g_MultiCard.MC_HomeStart(iAxisNum);
```

## 注意事项
- 启动回零前必须先设置回零参数
- 确保轴已使能且无报警状态
- 回零过程中避免人工干预
- 回零完成后轴位置会自动设为回零位置