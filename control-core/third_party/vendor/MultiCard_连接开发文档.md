# MultiCard 运动控制卡连接开发文档

## 目录
1. [连接架构概述](#1-连接架构概述)
2. [网络配置详解](#2-网络配置详解)
3. [连接实现流程](#3-连接实现流程)
4. [多卡管理机制](#4-多卡管理机制)
5. [连接状态监控](#5-连接状态监控)
6. [完整代码示例](#6-完整代码示例)
7. [故障排除指南](#7-故障排除指南)

---

## 1. 连接架构概述

### 1.1 MultiCard类架构
本项目采用面向对象的设计模式，通过`MultiCard`类封装运动控制卡的所有操作功能：

```cpp
// 全局控制卡实例声明 (DemoVCDlg.cpp:18-21)
MultiCard g_MultiCard;    // 第一张控制卡
MultiCard g_MultiCard2;   // 第二张控制卡
MultiCard g_MultiCard3;   // 第三张控制卡
```

### 1.2 通信架构
- **通信方式**: 以太网 UDP/IP 通信
- **支持卡数**: 最多支持16张控制卡同时连接
- **轴数支持**: 每张卡最多16轴，总计最多32轴
- **坐标系数**: 最多16个坐标系

### 1.3 核心API类
`MultiCard`类提供了完整的运动控制卡接口，包括：
- 通信连接管理
- 运动控制（点位、插补、JOG等）
- IO控制
- 状态监控
- 参数配置

---

## 2. 网络配置详解

### 2.1 默认网络配置
项目采用静态IP地址配置方案：

| 设备 | IP地址 | 说明 |
|------|--------|------|
| PC主机 | 192.168.0.200 | 上位机IP地址 |
| 控制卡1 | 192.168.0.1 | 第一张运动控制卡 |
| 控制卡2 | 192.168.0.2 | 第二张运动控制卡 |
| 控制卡3 | 192.168.0.3 | 第三张运动控制卡 |

### 2.2 端口分配策略
每张控制卡使用独立的UDP端口进行通信：

```cpp
// 端口分配方案
控制卡1: 端口 60000  (主卡)
控制卡2: 端口 60001  (扩展卡1)
控制卡3: 端口 60002  (扩展卡2)
// 以此类推...
```

### 2.3 网络配置要求

#### PC端网络设置
1. **配置静态IP**: 将PC以太网适配器设置为静态IP（如 192.168.0.200）
2. **关闭WiFi**: 避免无线网络干扰，建议使用有线网络连接
3. **防火墙配置**: 关闭防火墙或添加端口例外（60000-60002）
4. **网络测试**: 使用`ping`命令测试与控制卡的连通性

#### 控制卡端设置
1. **IP地址配置**: 确保每张控制卡的IP地址唯一且正确设置
2. **固件版本匹配**: 确保控制卡固件与MultiCard.dll版本匹配

---

## 3. 连接实现流程

### 3.1 MC_Open函数详解
连接控制卡的核心函数是`MC_Open`，函数原型：

```cpp
int MC_Open(short nCardNum,          // 控制卡物理编号
            char* cPCEthernetIP,     // PC端IP地址
            unsigned short nPCEthernetPort,  // PC端端口号
            char* cCardEthernetIP,   // 控制卡端IP地址
            unsigned short nCardEthernetPort); // 控制卡端端口号
```

### 3.2 连接参数说明

| 参数 | 类型 | 说明 | 示例值 |
|------|------|------|--------|
| nCardNum | short | 控制卡物理编号 | 1-16 |
| cPCEthernetIP | char* | PC端IP地址 | "192.168.0.200" |
| nPCEthernetPort | unsigned short | PC端端口号 | 60000 |
| cCardEthernetIP | char* | 控制卡IP地址 | "192.168.0.1" |
| nCardEthernetPort | unsigned short | 控制卡端口号 | 60000 |

### 3.3 连接实现代码

```cpp
// 完整连接实现 (DemoVCDlg.cpp:851-878)
void CDemoVCDlg::OnBnClickedButton7()
{
    int iRes = 0;

    // 启动调试日志（可选）
    g_MultiCard.MC_StartDebugLog(0);

    // 连接第一张控制卡
    iRes = g_MultiCard.MC_Open(1,              // 卡号1
                               "192.168.0.200", // PC IP
                               60000,           // PC端口
                               "192.168.0.1",   // 控制卡IP
                               60000);          // 控制卡端口

    // 连接第二张控制卡（可选，注释状态）
    // iRes = g_MultiCard2.MC_Open(2, "192.168.0.200", 60001, "192.168.0.2", 60001);

    // 连接第三张控制卡（可选，注释状态）
    // iRes = g_MultiCard3.MC_Open(3, "192.168.0.200", 60002, "192.168.0.3", 60002);

    if (iRes != 0) {
        MessageBox("Open Card Fail, Please turn off wifi, check PC IP address or connection!");
        return;
    }

    // 连接成功后重置控制卡
    g_MultiCard.MC_Reset();

    MessageBox("Open Card Successful!");

    // 更新界面状态
    OnCbnSelchangeCombo1();
}
```

### 3.4 连接错误处理
```cpp
// 错误代码含义
#define MC_COM_SUCCESS             (0)   // 执行成功
#define MC_COM_ERR_EXEC_FAIL       (1)   // 执行失败
#define MC_COM_ERR_LICENSE_WRONG   (2)   // license不支持
#define MC_COM_ERR_DATA_WORRY      (7)   // 参数错误
#define MC_COM_ERR_SEND           (-1)   // 发送失败
#define MC_COM_ERR_CARD_OPEN_FAIL (-6)   // 打开失败
#define MC_COM_ERR_TIME_OUT       (-7)   // 无响应
#define MC_COM_ERR_COM_OPEN_FAIL  (-8)   // 打开串口失败
```

---

## 4. 多卡管理机制

### 4.1 全局对象管理
项目通过声明多个全局`MultiCard`实例来管理多张控制卡：

```cpp
// 全局控制卡对象声明
MultiCard g_MultiCard;   // 主控制卡，处理所有基本操作
MultiCard g_MultiCard2;  // 第二张控制卡，用于扩展轴数
MultiCard g_MultiCard3;  // 第三张控制卡，用于进一步扩展
```

### 4.2 卡号分配策略
- **主卡**: 卡号1，处理主要的运动控制和状态监控
- **扩展卡1**: 卡号2，提供额外的轴和IO资源
- **扩展卡2**: 卡号3，进一步扩展系统能力

### 4.3 端口管理原则
1. **唯一性**: 每张控制卡必须使用独立的端口号
2. **连续性**: 建议从60000开始连续分配端口
3. **一致性**: PC端端口和控制卡端端口必须保持一致

### 4.4 多卡连接示例
```cpp
// 连接多张控制卡的完整示例
int ConnectMultipleCards()
{
    int result = 0;

    // 连接第一张卡
    result = g_MultiCard.MC_Open(1, "192.168.0.200", 60000, "192.168.0.1", 60000);
    if (result != 0) return result;

    // 连接第二张卡
    result = g_MultiCard2.MC_Open(2, "192.168.0.200", 60001, "192.168.0.2", 60001);
    if (result != 0) return result;

    // 连接第三张卡
    result = g_MultiCard3.MC_Open(3, "192.168.0.200", 60002, "192.168.0.3", 60002);
    if (result != 0) return result;

    return 0; // 所有连接成功
}
```

---

## 5. 连接状态监控

### 5.1 状态监控机制
项目通过定时器机制实现连接状态的实时监控：

```cpp
// 定时器设置 (DemoVCDlg.cpp:243)
SetTimer(1, 100, NULL);  // 每100ms更新一次状态

// 定时器回调函数 (DemoVCDlg.cpp:306-848)
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // 读取完整的系统状态
    TAllSysStatusDataSX m_AllSysStatusDataTemp;
    iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_AllSysStatusDataTemp);

    // 更新各种状态显示...
}
```

### 5.2 关键状态监控指标

#### 轴状态监控
```cpp
// 检查轴的运行状态
if (m_AllSysStatusDataTemp.lAxisStatus[iAxisNum-1] & AXIS_STATUS_RUNNING) {
    // 轴正在运动
}

// 检查轴的使能状态
if (m_AllSysStatusDataTemp.lAxisStatus[iAxisNum-1] & AXIS_STATUS_ENABLE) {
    // 轴已使能
}

// 检查伺服报警状态
if (m_AllSysStatusDataTemp.lAxisStatus[iAxisNum-1] & AXIS_STATUS_SV_ALARM) {
    // 伺服报警
}
```

#### 坐标系状态监控
```cpp
// 获取坐标系状态
short nCrdStatus = 0;
iRes = g_MultiCard.MC_CrdStatus(1, &nCrdStatus, NULL);

// 检查坐标系运行状态
if (nCrdStatus & CRDSYS_STATUS_PROG_RUN) {
    // 坐标系正在运行
}

// 检查坐标系缓冲区状态
if (nCrdStatus & CRDSYS_STATUS_FIFO_FINISH_0) {
    // 缓冲区数据执行完毕
}
```

#### 通信状态监控
```cpp
// 获取通信相关状态
long lCrdSpace = 0;
iRes = g_MultiCard.MC_CrdSpace(1, &lCrdSpace, 0);  // 缓冲区剩余空间
iRes = g_MultiCard.MC_GetLookAheadSpace(1, &lCrdSpace, 0);  // 前瞻缓冲区空间
```

### 5.3 连接断开处理
```cpp
// 关闭连接函数 (DemoVCDlg.cpp:1417-1420)
void CDemoVCDlg::OnBnClickedButton9()
{
    g_MultiCard.MC_Close();
}

// 重置控制卡 (DemoVCDlg.cpp:1423-1426)
void CDemoVCDlg::OnBnClickedButton8()
{
    g_MultiCard.MC_Reset();
}
```

---

## 6. 完整代码示例

### 6.1 基础连接类封装
```cpp
class MotionControllerManager {
private:
    MultiCard m_cards[16];           // 支持最多16张卡
    bool m_connected[16];            // 连接状态标志
    int m_cardCount;                 // 已连接卡数量

public:
    MotionControllerManager() {
        memset(m_connected, false, sizeof(m_connected));
        m_cardCount = 0;
    }

    // 连接单张控制卡
    int ConnectCard(int cardNum, const char* pcIP, int pcPort,
                   const char* cardIP, int cardPort) {
        if (cardNum < 1 || cardNum > 16) return -1;

        int result = m_cards[cardNum-1].MC_Open(cardNum,
                                                (char*)pcIP, pcPort,
                                                (char*)cardIP, cardPort);

        if (result == 0) {
            m_connected[cardNum-1] = true;
            m_cardCount++;

            // 连接成功后重置控制卡
            m_cards[cardNum-1].MC_Reset();
        }

        return result;
    }

    // 断开控制卡连接
    void DisconnectCard(int cardNum) {
        if (cardNum < 1 || cardNum > 16) return;
        if (m_connected[cardNum-1]) {
            m_cards[cardNum-1].MC_Close();
            m_connected[cardNum-1] = false;
            m_cardCount--;
        }
    }

    // 获取控制卡实例
    MultiCard* GetCard(int cardNum) {
        if (cardNum < 1 || cardNum > 16 || !m_connected[cardNum-1]) {
            return nullptr;
        }
        return &m_cards[cardNum-1];
    }

    // 检查连接状态
    bool IsConnected(int cardNum) {
        if (cardNum < 1 || cardNum > 16) return false;
        return m_connected[cardNum-1];
    }
};
```

### 6.2 网络配置工具类
```cpp
class NetworkConfig {
public:
    struct CardNetworkInfo {
        int cardNum;
        std::string pcIP;
        int pcPort;
        std::string cardIP;
        int cardPort;
    };

    // 默认网络配置生成器
    static std::vector<CardNetworkInfo> GenerateDefaultConfig(int cardCount) {
        std::vector<CardNetworkInfo> config;

        for (int i = 0; i < cardCount && i < 16; i++) {
            CardNetworkInfo info;
            info.cardNum = i + 1;
            info.pcIP = "192.168.0.200";
            info.pcPort = 60000 + i;
            info.cardIP = "192.168.0." + std::to_string(i + 1);
            info.cardPort = 60000 + i;
            config.push_back(info);
        }

        return config;
    }

    // 网络连通性测试
    static bool TestNetworkConnectivity(const std::string& targetIP) {
        // 在实际应用中可以实现ping测试
        // 这里仅作为示例
        return true;
    }
};
```

### 6.3 完整的连接管理示例
```cpp
class CompleteMotionSystem {
private:
    MotionControllerManager m_controllerManager;
    std::vector<NetworkConfig::CardNetworkInfo> m_networkConfig;

public:
    // 初始化系统
    bool InitializeSystem(int cardCount = 1) {
        // 生成默认网络配置
        m_networkConfig = NetworkConfig::GenerateDefaultConfig(cardCount);

        // 连接所有控制卡
        for (const auto& config : m_networkConfig) {
            int result = m_controllerManager.ConnectCard(
                config.cardNum,
                config.pcIP.c_str(),
                config.pcPort,
                config.cardIP.c_str(),
                config.cardPort
            );

            if (result != 0) {
                printf("Failed to connect card %d, error code: %d\n",
                       config.cardNum, result);
                return false;
            }
        }

        return true;
    }

    // 启动状态监控
    void StartStatusMonitoring() {
        // 创建监控线程或定时器
        // 实现类似OnTimer的功能
    }

    // 执行运动控制示例
    bool ExecuteMotionExample() {
        MultiCard* mainCard = m_controllerManager.GetCard(1);
        if (!mainCard) return false;

        // 使能轴1
        mainCard->MC_AxisOn(1);

        // 设置点位运动参数
        TTrapPrm trapPrm;
        trapPrm.acc = 0.1;
        trapPrm.dec = 0.1;
        trapPrm.smoothTime = 0;
        trapPrm.velStart = 0;

        mainCard->MC_PrfTrap(1);
        mainCard->MC_SetTrapPrm(1, &trapPrm);
        mainCard->MC_SetPos(1, 10000);  // 移动到10000位置
        mainCard->MC_SetVel(1, 50);     // 速度50
        mainCard->MC_Update(0x0001);    // 启动轴1

        return true;
    }

    // 系统关闭
    void ShutdownSystem() {
        for (const auto& config : m_networkConfig) {
            m_controllerManager.DisconnectCard(config.cardNum);
        }
    }
};
```

---

## 7. 故障排除指南

### 7.1 常见连接问题及解决方案

#### 问题1: 连接失败 (MC_Open返回非0值)
**可能原因:**
- 网络连接问题
- IP地址配置错误
- 端口被占用
- 防火墙阻止

**解决方案:**
```cpp
// 连接失败诊断流程
void DiagnoseConnectionFailure(int errorCode) {
    switch (errorCode) {
        case MC_COM_ERR_CARD_OPEN_FAIL:
            printf("检查网络连接和IP地址配置\n");
            printf("确保控制卡已上电且网络连接正常\n");
            break;

        case MC_COM_ERR_TIME_OUT:
            printf("网络超时，检查防火墙设置\n");
            printf("尝试关闭WiFi，使用有线连接\n");
            break;

        case MC_COM_ERR_SEND:
            printf("数据发送失败，检查网络配置\n");
            break;

        default:
            printf("未知错误代码: %d\n", errorCode);
    }
}
```

#### 问题2: 运动控制无响应
**可能原因:**
- 轴未使能
- 伺服报警
- 软限位触发

**解决方案:**
```cpp
// 轴状态诊断
void DiagnoseAxisStatus(int axisNum) {
    TAllSysStatusDataSX status;
    if (g_MultiCard.MC_GetAllSysStatusSX(&status) == 0) {
        if (status.lAxisStatus[axisNum-1] & AXIS_STATUS_SV_ALARM) {
            printf("伺服报警，请检查驱动器\n");
        }
        if (!(status.lAxisStatus[axisNum-1] & AXIS_STATUS_ENABLE)) {
            printf("轴未使能，请先使能轴\n");
        }
        if (status.lAxisStatus[axisNum-1] & AXIS_STATUS_POS_HARD_LIMIT) {
            printf("正限位触发\n");
        }
        if (status.lAxisStatus[axisNum-1] & AXIS_STATUS_NEG_HARD_LIMIT) {
            printf("负限位触发\n");
        }
    }
}
```

### 7.2 网络配置最佳实践

#### PC网络配置脚本
```batch
@echo off
echo 配置运动控制卡网络连接
echo.

set PC_IP=192.168.0.200
set NETMASK=255.255.255.0
set GATEWAY=192.168.0.1

echo 设置PC IP地址为 %PC_IP%
netsh interface ip set address "本地连接" static %PC_IP% %NETMASK% %GATEWAY%

echo 关闭Windows防火墙
netsh advfirewall set allprofiles state off

echo 测试网络连通性
ping 192.168.0.1 -n 4

echo 网络配置完成
pause
```

#### 连接测试工具
```cpp
// 连接测试工具类
class ConnectionTester {
public:
    static bool TestSingleCard(const NetworkConfig::CardNetworkInfo& config) {
        MultiCard testCard;
        int result = testCard.MC_Open(config.cardNum,
                                     (char*)config.pcIP.c_str(),
                                     config.pcPort,
                                     (char*)config.cardIP.c_str(),
                                     config.cardPort);

        if (result == 0) {
            testCard.MC_Close();
            return true;
        }
        return false;
    }

    static void TestAllCards(const std::vector<NetworkConfig::CardNetworkInfo>& configs) {
        printf("开始测试控制卡连接...\n");

        for (const auto& config : configs) {
            printf("测试卡%d (%s) ... ", config.cardNum, config.cardIP.c_str());

            if (TestSingleCard(config)) {
                printf("成功\n");
            } else {
                printf("失败\n");
            }
        }
    }
};
```

### 7.3 性能优化建议

1. **网络优化**
   - 使用专用网络交换机
   - 配置QoS保证实时性
   - 避免网络拥塞

2. **定时器优化**
   - 根据实际需要调整状态更新频率
   - 避免过于频繁的状态查询

3. **错误处理优化**
   - 实现自动重连机制
   - 添加连接状态监控
   - 记录详细的错误日志

---

## 总结

本文档详细说明了MultiCard运动控制卡连接系统的完整实现方案，包括：

1. **架构设计**: 基于MultiCard类的面向对象设计
2. **网络配置**: 静态IP和端口分配策略
3. **连接实现**: MC_Open函数的使用方法和参数配置
4. **多卡管理**: 全局对象声明和管理机制
5. **状态监控**: 实时状态更新和错误检测
6. **代码示例**: 完整的封装类和工具类
7. **故障排除**: 常见问题的诊断和解决方案

该方案支持最多16张运动控制卡的并发连接，每张卡最多16轴，可满足大多数工业自动化应用的需求。通过合理的网络配置和错误处理机制，确保系统的稳定性和可靠性。