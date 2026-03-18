# 如何连接博派运动控制卡

本文档详细说明了如何通过以太网连接到博派（Bopai）运动控制卡。

## 核心函数 `MC_Open`

连接控制卡的核心是通过调用库提供的 `MC_Open` 函数来完成的。该函数负责初始化PC与运动控制卡之间的网络通信。

### 函数原型 (C++)

```cpp
int MC_Open(
    short nCardNum,
    char* cPCEthernetIP,
    unsigned short nPCEthernetPort,
    char* cCardEthernetIP,
    unsigned short nCardEthernetPort
);
```

### 参数详解

| 参数 | 类型 | 描述 | 示例 |
| :--- | :--- | :--- | :--- |
| `nCardNum` | `short` | 控制卡的物理编号或ID。 | `1` |
| `cPCEthernetIP` | `char*` | 运行上位机软件的PC的IP地址。 | `"192.168.0.200"` |
| `nPCEthernetPort` | `unsigned short` | PC上用于通信的UDP端口号。 | `60000` |
| `cCardEthernetIP` | `char*` | 运动控制卡的IP地址。 | `"192.168.0.1"` |
| `nCardEthernetPort` | `unsigned short` | 运动控制卡上用于通信的UDP端口号。 | `60000` |

**注意：** PC和运动控制卡必须在同一个子网内，否则无法建立通信。

### 返回值

函数执行后会返回一个整型值，表示连接的结果。

| 返回值 | 宏定义 | 描述 |
| :--- | :--- | :--- |
| `0` | `MC_COM_SUCCESS` | **执行成功**，与控制卡成功建立连接。 |
| `-1` | `MC_COM_ERR_SEND` | 发送失败，通常是网络问题。 |
| `-6` | `MC_COM_ERR_CARD_OPEN_FAIL` | **打开失败**，无法连接到指定的控制卡IP和端口。 |
| `-7` | `MC_COM_ERR_TIME_OUT` | **无响应**，控制卡没有在预期时间内回应。 |
| `-8` | `MC_COM_ERR_COM_OPEN_FAIL` | 打开串口失败（此函数主要用于以太网，但保留了此错误码）。 |
| `7` | `MC_COM_ERR_DATA_WORRY` | 参数错误，传入的参数不符合要求。 |

### 完整代码示例

以下是一个完整的C++示例，展示了如何调用 `MC_Open` 函数并处理其返回值。

```cpp
#include "MultiCardCPP.h"
#include <iostream>

// 假设 g_MultiCard 是一个 MultiCard 类的全局实例
MultiCard g_MultiCard;

void ConnectToCard()
{
    // --- 连接参数 ---
    short cardId = 1;
    char* pcIp = "192.168.0.200";
    unsigned short pcPort = 60000;
    char* cardIp = "192.168.0.1";
    unsigned short cardPort = 60000;

    std::cout << "正在尝试连接到运动控制卡 " << cardIp << "..." << std::endl;

    // --- 调用连接函数 ---
    int result = g_MultiCard.MC_Open(cardId, pcIp, pcPort, cardIp, cardPort);

    // --- 检查连接结果 ---
    if (result == MC_COM_SUCCESS)
    {
        std::cout << "成功连接到运动控制卡！" << std::endl;
        // 在这里可以继续执行其他操作...
    }
    else
    {
        std::cerr << "连接失败！错误代码: " << result << std::endl;
        // 根据错误代码进行相应的处理
        switch (result)
        {
            case MC_COM_ERR_CARD_OPEN_FAIL:
                std::cerr << "原因: 打开失败。请检查IP地址、端口号是否正确，以及网络连接是否正常。" << std::endl;
                break;
            case MC_COM_ERR_TIME_OUT:
                std::cerr << "原因: 通信超时。请检查控制卡是否已上电并正常运行。" << std::endl;
                break;
            default:
                std::cerr << "原因: 未知错误。" << std::endl;
                break;
        }
    }
}

int main()
{
    ConnectToCard();
    return 0;
}
```

### 重要提示

*   **防火墙设置**: 确保Windows防火墙或其他网络安全软件没有阻止本程序使用的UDP端口（示例中为 `60000`）。
*   **网络配置**: 确保PC的IP地址（示例中为 `192.168.0.200`）与控制卡的IP地址（`192.168.0.1`）在同一个网段，并且子网掩码设置正确（例如 `255.255.255.0`）。
*   **物理连接**: 检查网线是否已正确连接到PC和运动控制卡。
