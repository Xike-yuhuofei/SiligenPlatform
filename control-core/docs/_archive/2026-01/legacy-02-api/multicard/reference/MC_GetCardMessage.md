# MC_GetCardMessage 函数教程

## 函数原型

```c
int MC_GetCardMessage(unsigned char* pMessage)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| pMessage | unsigned char* | 消息缓冲区指针，用于存储返回的卡消息信息 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：获取并显示控制卡消息
```cpp
// 获取控制卡的消息并显示给用户
unsigned char cMessage[32];
CString strText;

g_MultiCard.MC_GetCardMessage(cMessage);

strText.Format("%s",cMessage);
AfxMessageBox(strText);
```

### 示例2：错误诊断中的应用
```cpp
// 在API调用失败后获取详细错误信息
int iRes = g_MultiCard.MC_Open(1,"192.168.0.200",0,"192.168.0.1",0);

if(iRes != 0)
{
    // 获取详细的错误消息
    unsigned char cMessage[32];
    g_MultiCard.MC_GetCardMessage(cMessage);

    CString strText;
    strText.Format("连接失败: %s", cMessage);
    MessageBox(strText);
}
```

### 示例3：定时检查控制卡状态
```cpp
// 在定时器中定期检查控制卡状态消息
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    if(nIDEvent == 1)  // 状态检查定时器
    {
        unsigned char cMessage[32];
        g_MultiCard.MC_GetCardMessage(cMessage);

        // 检查是否有新的状态消息
        if(strlen((char*)cMessage) > 0)
        {
            // 更新状态显示或记录日志
            TRACE("控制卡消息: %s\n", cMessage);
        }
    }

    CDialogEx::OnTimer(nIDEvent);
}
```

## 参数映射表

| 缓冲区大小 | 用途 | 说明 |
|------------|------|------|
| 32字节 | 标准消息 | 足够存储一般状态和错误消息 |
| 64字节 | 扩展消息 | 如果需要更详细的信息 |
| 128字节 | 详细诊断 | 用于复杂的错误诊断 |

## 关键说明

1. **消息类型**：
   - **状态消息**：控制卡当前状态信息
   - **错误消息**：操作失败的原因说明
   - **警告消息**：潜在问题的提醒
   - **调试信息**：详细的执行过程（需要开启调试日志）

2. **缓冲区管理**：
   - 缓冲区应有足够空间存储消息
   - 通常32字节足够存储一般消息
   - 消息以null字符结尾
   - 每次调用会获取最新的消息

3. **使用时机**：
   - **错误诊断**：API调用失败后获取具体原因
   - **状态监控**：定期检查控制卡状态
   - **调试分析**：配合MC_StartDebugLog()使用
   - **用户反馈**：将控制卡消息显示给用户

4. **注意事项**：
   - 每次调用会覆盖之前的消息内容
   - 消息内容是控制卡生成的，不是本地错误
   - 需要确保缓冲区足够大
   - 消息内容可能包含中文字符

## 函数区别

- **MC_GetCardMessage() vs MC_StartDebugLog()**: MC_GetCardMessage()获取消息内容；MC_StartDebugLog()控制调试输出
- **消息 vs 状态**: MC_GetCardMessage()获取文本消息；MC_GetAllSysStatusSX()获取数值状态
- **本地 vs 远程**: MC_GetCardMessage()获取控制卡端的消息；不是PC端的错误信息

---

**调试技巧**: 结合MC_StartDebugLog(1)使用，可以获得更详细和实时的控制卡消息信息。