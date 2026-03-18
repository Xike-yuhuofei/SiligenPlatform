# MC_HomeStop

## 函数签名
int MC_HomeStop(short nAxis);

## 参数
nAxis: 轴号，取值范围：1-16

## 返回值
成功返回 0，失败返回非 0 错误代码

## 实际代码示例

### 示例1：停止轴回零操作
```cpp
// 从DemoVCDlg.cpp中提取的回零停止代码
void CDemoVCDlg::OnBnClickedButtonStopHome()
{
	int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

	g_MultiCard.MC_HomeStop(iAxisNum);
}
```

## 注意事项
- 用于紧急停止回零过程
- 停止后轴位置可能不在零位
- 如需重新回零，需重新设置参数并启动
- 停止回零后检查轴的状态和位置