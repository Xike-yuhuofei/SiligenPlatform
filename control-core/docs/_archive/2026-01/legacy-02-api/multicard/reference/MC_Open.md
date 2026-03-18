# MC_Open 函数教程

## 函数原型

```c
int MC_Open(short nCardNum, const char* pcPCAddr, short nPCPort, const char* pmcAddr, short nmcPort)
```

## 参数说明

| 参数名 | 类型 | 说明 |
|--------|------|------|
| nCardNum | short | 控制卡编号，取值范围：1-16 |
| pcPCAddr | const char* | PC端IP地址字符串 |
| nPCPort | short | PC端端口号 |
| pmcAddr | const char* | 控制卡端IP地址字符串 |
| nmcPort | short | 控制卡端端口号 |

## 返回值

| 返回值 | 说明 |
|--------|------|
| 0 | 成功 |
| 非0 | 失败，错误代码 |

## 实际代码示例

### 示例1：基本连接配置
```cpp
// 基本的网络连接配置
int iRes = 0;
iRes = g_MultiCard.MC_Open(1,"192.168.0.200",0,"192.168.0.1",0);

if(iRes)
{
    MessageBox("Open Card Fail,Please turn off wifi ,check PC IP address or connection!");
}
else
{
    g_MultiCard.MC_Reset();
    MessageBox("Open Card Successful!");
    OnCbnSelchangeCombo1();  // 初始化界面状态
}
```

### 示例2：多卡连接配置
```cpp
// 第一张控制卡连接
iRes = g_MultiCard.MC_Open(1,"192.168.0.200",0,"192.168.0.1",0);

// 第二张控制卡连接
//iRes = g_MultiCard2.MC_Open(2,"192.168.0.200",0,"192.168.0.2",0);

// 第三张控制卡连接
//iRes = g_MultiCard3.MC_Open(3,"192.168.0.200",0,"192.168.0.3",0);
```

### 示例3：调试用网络配置
```cpp
// 调试时可能使用的网络配置
//iRes = g_MultiCard.MC_Open(1,"10.129.41.200",0,"10.129.41.112",0);
```

## 参数映射表

| 场景 | nCardNum | pcPCAddr | nPCPort | pmcAddr | nmcPort |
|------|----------|----------|---------|----------|---------|
| 单卡默认配置 | 1 | "192.168.0.200" | 0 | "192.168.0.1" | 0 |
| 第二张卡 | 2 | "192.168.0.200" | 0 | "192.168.0.2" | 0 |
| 第三张卡 | 3 | "192.168.0.200" | 0 | "192.168.0.3" | 0 |
| 调试环境 | 1 | "10.129.41.200" | 0 | "10.129.41.112" | 0 |

## 关键说明

1. **网络配置要求**：
   - PC和控制卡需要在同一局域网内
   - IP地址不能冲突，建议使用连续地址
   - 端口号需要保持对应关系：PC端口 == 控制卡端口

2. **多卡支持**：
   - 每张控制卡使用端口0，系统自动分配唯一端口号
   - 不同控制卡的IP地址需要不同

3. **连接前准备**：
   - 确保网络连接正常（可使用ping测试）
   - 关闭WiFi避免网络干扰
   - 检查防火墙设置，确保端口未被阻止

4. **错误处理**：
   - 连接失败时检查网络配置和硬件连接
   - 确认控制卡已上电且网络配置正确
   - 检查MultiCard.dll版本与控制卡固件是否匹配

## 函数区别

- **MC_Open() vs MC_Close()**: MC_Open()建立网络连接，MC_Close()断开连接
- **MC_Open() vs MC_Reset()**: MC_Open()是通信连接，MC_Reset()是控制卡硬件复位
- **单卡 vs 多卡**: 多卡场景下需要为每张卡调用MC_Open()，使用不同的IP地址（端口都使用0）

---

**注意事项**: 成功连接后建议立即调用MC_Reset()确保控制卡处于初始状态。