# MC_EndHandwheel 函数教程

## 函数原型

```c
int MC_EndHandwheel(short nCrdNum)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCrdNum | short | 坐标系号，通常固定为1 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：X轴退出手轮模式
```cpp
// 从DemoVCDlg.cpp中提取的X轴手轮模式退出代码
void CDemoVCDlg::OnBnClickedButton14()
{
	g_MultiCard.MC_EndHandwheel(1);
}
```

### 示例2：手轮模式切换前退出当前模式
```cpp
// 从DemoVCDlg.cpp中提取的手轮切换逻辑代码
void CDemoVCDlg::OnBnClickedButtonEnterHandwhell()
{
	int iRes;

	//之前为手轮模式的轴先退出手轮模式
	g_MultiCard.MC_EndHandwheel(1);

	//然后重新启动手轮模式，使用新的参数
	iRes = g_MultiCard.MC_StartHandwheel(1,4,m_lMasterEven,m_lSlaveEven,0,1,1,50,0);
}
```

### 示例3：程序退出时清理手轮模式
```cpp
// 在应用程序退出或关闭时清理手轮模式
void CleanupHandwheelMode()
{
	// 退出所有可能的手轮模式
	g_MultiCard.MC_EndHandwheel(1);

	TRACE("手轮模式已清理\n");
}
```

## 参数映射表

| 应用场景 | nCrdNum | 说明 |
|----------|---------|------|
| 退出手轮 | 1 | 退出当前手轮模式 |
| 模式切换 | 1 | 切换前先退出当前模式 |
| 程序清理 | 1 | 程序退出时清理状态 |
| 异常处理 | 1 | 出现问题时强制退出 |

## 关键说明

1. **退出效果**：
   - 停止手轮对轴的控制
   - 恢复轴的正常运动控制模式
   - 释放手轮相关的资源

2. **使用时机**：
   - **切换轴控制时**：在启动新的手轮模式前
   - **按钮控制退出**：用户点击退出手轮按钮时
   - **程序结束时**：应用程序关闭前清理状态
   - **异常情况时**：需要强制退出手轮模式时

3. **与MC_StartHandwheel的配合**：
   - 先调用 MC_EndHandwheel() 退出当前模式
   - 再调用 MC_StartHandwheel() 进入新模式
   - 这样确保新的比例参数能够生效

4. **注意事项**：
   - 退出手轮模式后，轴位置保持不变
   - 可以继续使用其他运动控制方式
   - 确保在程序退出前调用此函数
   - 避免重复调用导致错误

5. **安全考虑**：
   - 退出手轮模式前确保轴在安全位置
   - 退出后用户仍需注意轴的位置状态
   - 配合其他安全措施使用

6. **应用场景**：
   - **轴切换**：从X轴手轮切换到Y轴手轮时
   - **模式切换**：从手轮模式切换到JOG或点位模式时
   - **程序结束**：应用程序正常退出时
   - **紧急停止**：需要立即取消手轮控制时

## 函数区别

- **MC_EndHandwheel() vs MC_StartHandwheel()**: MC_EndHandwheel()退出手轮模式，MC_StartHandwheel()进入手轮模式
- **MC_EndHandwheel() vs MC_Stop()**: MC_EndHandwheel()退出手轮控制模式，MC_Stop()停止当前运动
- **退出 vs 进入**: 手轮控制必须先退出才能重新进入或切换到其他模式

---

**使用建议**: 在任何需要更改手轮参数或切换控制轴的操作前，都应先调用MC_EndHandwheel()，然后再调用MC_StartHandwheel()，确保新参数能够正确生效。