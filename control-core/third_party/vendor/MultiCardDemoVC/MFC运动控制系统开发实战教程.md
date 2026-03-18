# MFC运动控制系统开发实战教程

## 基于DemoVCDlg.cpp的实例教学

---

## 目录

### 第1章：MFC基础和项目结构
1.1 MFC应用程序架构简介
1.2 DemoVC项目文件结构分析
1.3 主要类和头文件介绍
1.4 开发环境搭建

### 第2章：类设计和核心架构
2.1 CDemoVCDlg类的设计思想
2.2 全局对象和多卡管理
2.3 数据成员和成员函数分类
2.4 模块化设计原则

### 第3章：消息映射和事件处理机制
3.1 MFC消息映射机制详解
3.2 按钮点击事件处理
3.3 下拉框选择事件
3.4 按键消息的预处理机制

### 第4章：实时状态监控和定时器
4.1 定时器的创建和管理
4.2 OnTimer函数的核心作用
4.3 状态数据的读取和显示
4.4 性能优化注意事项

### 第5章：运动控制功能实现
5.1 单轴点位运动控制
5.2 JOG连续运动实现
5.3 坐标系插补运动
5.4 回零控制功能

### 第6章：IO控制和通信功能
6.1 数字输入输出控制
6.2 模拟量采集
6.3 串口通信实现
6.4 比较输出功能

### 第7章：调试技巧和最佳实践
7.1 错误处理模式
7.2 调试技巧和方法
7.3 代码规范和优化
7.4 常见问题解决方案

---

## 前言

本教程基于一个真实的工业级多轴运动控制程序 DemoVCDlg.cpp，通过分析实际代码来学习MFC编程、运动控制系统开发等核心技术。本教程适合有一定C++基础，想要学习MFC和工业控制软件开发的程序员。

### 学习目标
- 掌握MFC应用程序的基本架构
- 理解运动控制系统的核心概念
- 学会实时状态监控的实现方法
- 掌握工业软件的调试和优化技巧

### 代码环境
- 开发工具：Visual Studio 2010+
- 编程语言：C++ with MFC
- 项目类型：基于对话框的Windows应用程序
- 硬件：多轴运动控制卡

---

## 第1章：MFC基础和项目结构

### 1.1 MFC应用程序架构简介

MFC（Microsoft Foundation Classes）是微软提供的C++类库，用于简化Windows应用程序开发。DemoVC项目采用了基于对话框的MFC应用程序架构。

#### MFC应用程序的基本组成部分

1. **应用程序类**（CWinApp派生类）
   - 负责应用程序的初始化和运行
   - 管理应用程序的消息循环

2. **框架窗口类**（CFrameWnd派生类）或对话框类（CDialogEx派生类）
   - 提供应用程序的主界面
   - 处理用户交互

3. **文档/视图结构**（可选）
   - 分离数据和显示逻辑
   - DemoVC采用简单的对话框模式，未使用文档/视图结构

#### DemoVC的架构特点

```cpp
// 应用程序类
class CDemoVCApp : public CWinAppEx
{
public:
    virtual BOOL InitInstance();
};

// 主对话框类
class CDemoVCDlg : public CDialogEx
{
    // 构造函数和析构函数
    CDemoVCDlg(CWnd* pParent = NULL);
    virtual ~CDemoVCDlg();

    // 对话框数据
    enum { IDD = IDD_DEMOVC_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);
    DECLARE_MESSAGE_MAP()
};
```

### 1.2 DemoVC项目文件结构分析

#### 主要源文件

```
DemoVC/
├── DemoVC.h/cpp           # 应用程序主类 CDemoVCApp
├── DemoVCDlg.h/cpp        # 主对话框类 CDemoVCDlg (核心文件)
├── MultiCardCPP.h         # 运动控制卡API接口定义
├── stdafx.h/cpp           # MFC预编译头文件
├── resource.h             # 资源ID定义
├── DemoVC.rc              # 资源文件(对话框、菜单、图标)
├── DemoVC.vcxproj         # Visual Studio项目文件
└── res/                   # 资源文件夹
    └── DemoVC.ico         # 应用程序图标
```

#### 文件功能说明

1. **DemoVC.h/cpp** - 应用程序入口
   - 包含应用程序类 CDemoVCApp
   - 负责程序初始化和主对话框的创建

2. **DemoVCDlg.h/cpp** - 核心功能实现
   - 包含主对话框类 CDemoVCDlg
   - 实现所有用户交互和控制逻辑
   - 包含约2200行代码，是项目的核心

3. **MultiCardCPP.h** - 硬件接口定义
   - 定义了运动控制卡的API接口
   - 包含数据结构、常量定义和函数声明
   - 约1024行，是硬件操作的基础

4. **resource.h** - 资源标识符
   - 定义所有控件的ID
   - 用于在代码中引用界面元素

5. **DemoVC.rc** - 资源文件
   - 定义对话框布局和控件属性
   - 包含菜单、图标等资源

### 1.3 主要类和头文件介绍

#### 核心类介绍

1. **CDemoVCApp类**（DemoVC.h）
```cpp
class CDemoVCApp : public CWinAppEx
{
public:
    CDemoVCApp();

public:
    virtual BOOL InitInstance();

    DECLARE_MESSAGE_MAP()
};

extern CDemoVCApp theApp;  // 全局应用程序对象
```

2. **CDemoVCDlg类**（DemoVCDlg.h）
```cpp
class CDemoVCDlg : public CDialogEx
{
public:
    CDemoVCDlg(CWnd* pParent = NULL);

    // 对话框数据
    enum { IDD = IDD_DEMOVC_DIALOG };

protected:
    virtual void DoDataExchange(CDataExchange* pDX);

    // 生成的消息映射函数
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    DECLARE_MESSAGE_MAP()
};
```

#### 重要的头文件包含

```cpp
#include "stdafx.h"           // MFC预编译头文件
#include "DemoVC.h"           // 应用程序类声明
#include "DemoVCDlg.h"        // 主对话框类声明
#include "afxdialogex.h"      // 扩展对话框支持
#include "MultiCardCPP.h"     // 运动控制卡API
```

#### 全局对象定义

```cpp
// 全局控制卡对象 - 每张卡一个实例
MultiCard g_MultiCard;        // 第一张控制卡
MultiCard g_MultiCard2;       // 第二张控制卡
MultiCard g_MultiCard3;       // 第三张控制卡

// 前瞻缓冲区 - 用于插补运动
TCrdData LookAheadBuf[FROCAST_LEN];
```

### 1.4 开发环境搭建

#### 系统要求

1. **操作系统**
   - Windows XP/7/8/10/11 (32位和64位)

2. **开发工具**
   - Visual Studio 2010 或更高版本
   - 必须安装MFC支持

3. **硬件要求**
   - 以太网接口
   - 多轴运动控制卡

#### 开发环境配置步骤

1. **安装Visual Studio**
   - 选择"使用C++的桌面开发"工作负载
   - 确保安装MFC组件

2. **配置项目**
   ```powershell
   # 打开解决方案
   Invoke-Item DemoVC.sln

   # 或直接构建
   msbuild DemoVC.sln /p:Configuration=Debug /p:Platform=Win32
   ```

3. **网络配置**
   - 设置PC IP地址：192.168.0.200
   - 设置控制卡IP地址：192.168.0.1
   - 配置防火墙例外端口：60000-60002

#### 编译和运行

1. **编译项目**
   - 在Visual Studio中按F7或Ctrl+Shift+B
   - 或使用命令行编译

2. **运行程序**
   - Debug版本：`bin\DemoVC.exe`
   - 确保MultiCard.dll在同目录

3. **调试准备**
   - 连接控制卡硬件
   - 配置正确的网络参数
   - 准备测试用电机和驱动器

### 本章小结

本章介绍了MFC应用程序的基本架构和DemoVC项目的文件结构。通过学习本章，读者应该：

- 理解MFC应用程序的基本组成部分
- 熟悉DemoVC项目的文件结构和功能
- 掌握开发环境的搭建方法
- 了解项目的整体架构和全局对象设计

在下一章中，我们将深入学习CDemoVCDlg类的设计思想和核心架构。

---

## 第2章：类设计和核心架构

### 2.1 CDemoVCDlg类的设计思想

CDemoVCDlg是整个DemoVC项目的核心类，承担了用户界面管理、硬件控制、状态监控等多重职责。该类采用了面向对象的设计思想，将复杂的运动控制功能封装在统一的类中。

#### 设计原则

1. **单一职责原则**：每个成员函数专注于特定功能
2. **模块化设计**：将相关功能组织在一起
3. **事件驱动**：基于MFC消息机制响应用户操作
4. **实时监控**：通过定时器实现状态的周期性更新

#### 类的基本结构

```cpp
class CDemoVCDlg : public CDialogEx
{
public:
    // 构造函数和析构函数
    CDemoVCDlg(CWnd* pParent = NULL);
    virtual ~CDemoVCDlg();

    // 对话框数据枚举
    enum { IDD = IDD_DEMOVC_DIALOG };

protected:
    // 数据交换函数
    virtual void DoDataExchange(CDataExchange* pDX);

    // 消息映射函数声明
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnTimer(UINT_PTR nIDEvent);

    // 大量的按钮事件处理函数
    afx_msg void OnBnClickedButton7();        // 打开控制卡
    afx_msg void OnBnClickedButton6();        // 点位运动
    afx_msg void OnBnClickedButtonAxisOn();   // 轴使能
    // ... 更多事件处理函数

    DECLARE_MESSAGE_MAP()
};
```

### 2.2 全局对象和多卡管理

#### 全局控制卡对象设计

```cpp
// 全局对象定义 - 每张控制卡对应一个对象
MultiCard g_MultiCard;        // 第一张控制卡（主卡）
MultiCard g_MultiCard2;       // 第二张控制卡（扩展卡1）
MultiCard g_MultiCard3;       // 第三张控制卡（扩展卡2）
```

#### 多卡管理的优势

1. **独立性**：每个控制卡对象独立运行，互不干扰
2. **可扩展性**：支持最多3张控制卡同时工作
3. **灵活配置**：不同卡可以使用不同的IP地址和端口

#### 多卡连接示例

```cpp
void CDemoVCDlg::OnBnClickedButton7()
{
    int iRes = 0;

    // 启动调试日志
    g_MultiCard.MC_StartDebugLog(0);

    // 连接第一张控制卡
    iRes = g_MultiCard.MC_Open(1, "192.168.0.200", 60000, "192.168.0.1", 60000);

    // 连接第二张控制卡（可选）
    // iRes = g_MultiCard2.MC_Open(2, "192.168.0.200", 60001, "192.168.0.2", 60001);

    if (iRes == 0) {
        g_MultiCard.MC_Reset();  // 复位控制卡
        MessageBox("控制卡连接成功！");
    } else {
        MessageBox("连接失败，请检查网络配置！");
    }
}
```

### 2.3 数据成员和成员函数分类

#### 数据成员分类

1. **控件成员变量**
```cpp
// 在头文件中定义的控件变量
CComboBox m_ComboBoxAxis;          // 轴选择下拉框
CComboBox m_ComboBoxCardSel;       // 卡选择下拉框
CSliderCtrl m_SliderCrdVel;        // 坐标系速度滑块
CComboBox m_ComboBoxCMPUnit;       // 比较输出单位选择
CComboBox m_ComboBoxUartNum;       // 串口号选择
```

2. **数据绑定变量**
```cpp
long m_lTargetPos;                 // 目标位置
long m_lOrginX;                    // 坐标系原点X
long m_lOrginY;                    // 坐标系原点Y
long m_lOrginZ;                    // 坐标系原点Z
long m_lSlaveEven;                 // 从轴倍率分子
long m_lMasterEven;                // 主轴倍率分母
int m_iBaudRate;                   // 串口波特率
int m_nDataLen;                    // 数据位长度
int m_iVerifyType;                 // 校验方式
int m_iStopBit;                    // 停止位
```

#### 成员函数分类

1. **系统初始化函数**
```cpp
// 构造函数
CDemoVCDlg::CDemoVCDlg(CWnd* pParent)
    : CDialogEx(CDemoVCDlg::IDD, pParent)
    , m_lTargetPos(0)
    , m_lOrginX(0)
    , m_lOrginY(0)
    // ... 其他成员初始化
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

// 对话框初始化
BOOL CDemoVCDlg::OnInitDialog()
{
    // 初始化界面控件
    // 设置定时器
    SetTimer(1, 100, NULL);
    return TRUE;
}
```

2. **消息处理函数**
```cpp
// 定时器消息处理
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent);

// 系统命令处理
void CDemoVCDlg::OnSysCommand(UINT nID, LPARAM lParam);

// 绘制消息处理
void CDemoVCDlg::OnPaint();
```

3. **功能实现函数**
```cpp
// 连接管理
void OnBnClickedButton7();         // 打开连接
void OnBnClickedButton9();         // 关闭连接
void OnBnClickedButton8();         // 复位控制卡

// 轴控制
void OnBnClickedButton6();         // 点位运动
void OnBnClickedButtonAxisOn();    // 轴使能控制
void OnBnClickedButtonClrSts();    // 清除状态

// 坐标系控制
void OnBnClickedButtonSetCrdPrm(); // 设置坐标系参数
void OnBnClickedButton10();        // 插补数据写入
void OnBnClickedButton11();        // 启动坐标系运动
```

### 2.4 模块化设计原则

#### 功能模块划分

1. **连接管理模块**
   - 负责控制卡的连接、断开、复位
   - 网络参数配置和状态监控

2. **单轴运动模块**
   - 点位运动控制
   - JOG连续运动
   - 轴使能和状态管理

3. **坐标系插补模块**
   - 坐标系参数配置
   - 插补数据管理
   - 运动启动和停止

4. **IO控制模块**
   - 数字输入输出
   - 模拟量采集
   - 限位和报警处理

5. **状态监控模块**
   - 定时器管理
   - 状态数据读取
   - 界面更新

#### 模块间的协作关系

```cpp
// 状态监控模块调用其他模块
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // 读取系统状态
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_AllSysStatusDataTemp);

    // 更新轴状态显示（调用显示模块）
    UpdateAxisStatus();

    // 更新IO状态显示（调用IO模块）
    UpdateIOStatus();

    // 更新坐标系状态显示（调用坐标系模块）
    UpdateCrdStatus();
}
```

#### 错误处理模块

```cpp
// 统一的错误处理函数
bool CDemoVCDlg::HandleError(int iErrorCode, const CString& strOperation)
{
    if (iErrorCode != 0) {
        CString strMsg;
        strMsg.Format("%s失败，错误码：%d", strOperation, iErrorCode);
        AfxMessageBox(strMsg);
        return false;
    }
    return true;
}

// 使用示例
void CDemoVCDlg::OnBnClickedButtonAxisOn()
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;
    int iRes = g_MultiCard.MC_AxisOn(iAxisNum);

    if (!HandleError(iRes, "轴使能")) {
        return;
    }

    // 更新界面状态
    UpdateAxisEnableStatus(iAxisNum, true);
}
```

### 本章小结

本章详细介绍了CDemoVCDlg类的设计思想和核心架构。通过学习本章，读者应该：

- 理解CDemoVCDlg类的设计原则和架构特点
- 掌握全局对象管理和多卡控制的方法
- 熟悉数据成员和成员函数的分类和组织
- 了解模块化设计的实现方法

在下一章中，我们将深入学习MFC的消息映射机制和事件处理方法。

---

## 第3章：消息映射和事件处理机制

### 3.1 MFC消息映射机制详解

MFC的消息映射机制是连接Windows消息和类成员函数的桥梁，它使得我们能够用面向对象的方式处理事件。

#### 消息映射的基本原理

1. **Windows消息**：系统发送给窗口的事件通知
2. **消息映射表**：将消息映射到成员函数的表结构
3. **消息处理函数**：响应特定消息的成员函数

#### 消息映射的三个组成部分

1. **消息映射宏**（在.cpp文件中）
```cpp
BEGIN_MESSAGE_MAP(CDemoVCDlg, CDialogEx)
    ON_WM_SYSCOMMAND()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_WM_TIMER()
    // 按钮点击事件
    ON_BN_CLICKED(IDC_BUTTON4, &CDemoVCDlg::OnBnClickedButton4)
    ON_BN_CLICKED(IDC_BUTTON7, &CDemoVCDlg::OnBnClickedButton7)
    ON_BN_CLICKED(IDC_BUTTON6, &CDemoVCDlg::OnBnClickedButton6)
    // ... 更多消息映射
END_MESSAGE_MAP()
```

2. **消息处理函数声明**（在.h文件中）
```cpp
class CDemoVCDlg : public CDialogEx
{
    // ...
protected:
    afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
    afx_msg void OnPaint();
    afx_msg HCURSOR OnQueryDragIcon();
    afx_msg void OnTimer(UINT_PTR nIDEvent);
    afx_msg void OnBnClickedButton4();
    afx_msg void OnBnClickedButton7();
    // ...
    DECLARE_MESSAGE_MAP()
};
```

3. **消息处理函数实现**（在.cpp文件中）
```cpp
void CDemoVCDlg::OnBnClickedButton7()
{
    // 处理按钮点击事件的代码
    int iRes = 0;
    iRes = g_MultiCard.MC_Open(1, "192.168.0.200", 60000, "192.168.0.1", 60000);
    // ...
}
```

### 3.2 按钮点击事件处理

#### 按钮事件的注册和实现

1. **资源ID定义**（resource.h）
```cpp
#define IDC_BUTTON4         1001
#define IDC_BUTTON6         1002
#define IDC_BUTTON7         1003
#define IDC_BUTTON_AXIS_ON  1004
```

2. **消息映射**（DemoVCDlg.cpp）
```cpp
BEGIN_MESSAGE_MAP(CDemoVCDlg, CDialogEx)
    // ...
    ON_BN_CLICKED(IDC_BUTTON4, &CDemoVCDlg::OnBnClickedButton4)
    ON_BN_CLICKED(IDC_BUTTON6, &CDemoVCDlg::OnBnClickedButton6)
    ON_BN_CLICKED(IDC_BUTTON7, &CDemoVCDlg::OnBnClickedButton7)
    ON_BN_CLICKED(IDC_BUTTON_AXIS_ON, &CDemoVCDlg::OnBnClickedButtonAxisOn)
    // ...
END_MESSAGE_MAP()
```

3. **事件处理函数实现**
```cpp
// 位置清零按钮
void CDemoVCDlg::OnBnClickedButton4()
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;
    g_MultiCard.MC_ZeroPos(iAxisNum);
}

// 点位运动按钮
void CDemoVCDlg::OnBnClickedButton6()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

    UpdateData(true);  // 获取界面数据

    TTrapPrm TrapPrm;
    TrapPrm.acc = 0.1;        // 加速度
    TrapPrm.dec = 0.1;        // 减速度
    TrapPrm.smoothTime = 0;   // 平滑时间
    TrapPrm.velStart = 0;     // 起始速度

    // 设置梯形运动模式
    iRes = g_MultiCard.MC_PrfTrap(iAxisNum);
    iRes += g_MultiCard.MC_SetTrapPrm(iAxisNum, &TrapPrm);

    // 设置运动参数
    iRes += g_MultiCard.MC_SetPos(iAxisNum, m_lTargetPos);  // 目标位置
    iRes = g_MultiCard.MC_SetVel(iAxisNum, 5);              // 速度

    // 启动运动
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
}

// 轴使能切换按钮
void CDemoVCDlg::OnBnClickedButtonAxisOn()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;
    CString strText;

    GetDlgItemText(IDC_BUTTON_AXIS_ON, strText);

    if (strText == "伺服使能") {
        iRes = g_MultiCard.MC_AxisOn(iAxisNum);
        GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText("断开使能");
    } else {
        iRes = g_MultiCard.MC_AxisOff(iAxisNum);
        GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText("伺服使能");
    }
}
```

#### 按钮状态管理

```cpp
// 更新按钮状态
void CDemoVCDlg::UpdateButtonStates()
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

    // 获取轴状态
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if (iRes == 0) {
        // 根据轴状态更新按钮文本和状态
        if (statusData.lAxisStatus[iAxisNum-1] & AXIS_STATUS_ENABLE) {
            GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText("断开使能");
        } else {
            GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText("伺服使能");
        }

        // 更新其他按钮的启用/禁用状态
        bool bEnabled = (statusData.lAxisStatus[iAxisNum-1] & AXIS_STATUS_ENABLE);
        GetDlgItem(IDC_BUTTON6)->EnableWindow(bEnabled);  // 运动按钮
        GetDlgItem(IDC_BUTTON4)->EnableWindow(bEnabled);  // 清零按钮
    }
}
```

### 3.3 下拉框选择事件

#### 下拉框初始化

```cpp
BOOL CDemoVCDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // 初始化轴选择下拉框
    m_ComboBoxAxis.AddString("轴1（Axis1）");
    m_ComboBoxAxis.AddString("轴2（Axis2）");
    m_ComboBoxAxis.AddString("轴3（Axis3）");
    // ... 添加更多轴
    m_ComboBoxAxis.AddString("轴16（Axis16）");
    m_ComboBoxAxis.SetCurSel(0);  // 默认选择第一项

    // 初始化卡选择下拉框
    m_ComboBoxCardSel.AddString("主卡");
    m_ComboBoxCardSel.AddString("扩展卡1");
    m_ComboBoxCardSel.AddString("扩展卡2");
    m_ComboBoxCardSel.SetCurSel(0);

    // 初始化其他下拉框...

    return TRUE;
}
```

#### 下拉框选择事件处理

```cpp
// 消息映射
ON_CBN_SELCHANGE(IDC_COMBO1, &CDemoVCDlg::OnCbnSelchangeCombo1)

// 事件处理函数
void CDemoVCDlg::OnCbnSelchangeCombo1()
{
    int iRes = 0;
    short nEncOnOff = 0;      // 编码器状态
    short nPosLimOnOff = 0;   // 正限位状态
    short nNegLimOnOff = 0;   // 负限位状态

    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

    // 获取编码器状态
    iRes = g_MultiCard.MC_GetEncOnOff(iAxisNum, &nEncOnOff);
    if (nEncOnOff) {
        ((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(false);
    } else {
        ((CButton*)GetDlgItem(IDC_CHECK_NO_ENCODE))->SetCheck(true);
    }

    // 获取限位状态
    iRes = g_MultiCard.MC_GetLmtsOnOff(iAxisNum, &nPosLimOnOff, &nNegLimOnOff);

    if (nPosLimOnOff) {
        ((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_CHECK_POS_LIM_ENABLE))->SetCheck(false);
    }

    if (nNegLimOnOff) {
        ((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_CHECK_NEG_LIM_ENABLE))->SetCheck(false);
    }

    // 更新按钮状态
    UpdateButtonStates();
}
```

### 3.4 按键消息的预处理机制

#### PreTranslateMessage函数

对于需要精确控制按键行为的场景（如JOG运动），可以使用PreTranslateMessage函数来预处理按键消息。

```cpp
BOOL CDemoVCDlg::PreTranslateMessage(MSG* pMsg)
{
    // 处理鼠标左键按下消息
    switch(pMsg->message)
    {
    case WM_LBUTTONDOWN:
        UpdateData(TRUE);

        // 处理负向JOG按钮按下
        if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd)
        {
            int iRes = 0;
            TJogPrm m_JogPrm;
            int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

            // 配置JOG参数
            m_JogPrm.dAcc = 0.1;   // 加速度
            m_JogPrm.dDec = 0.1;   // 减速度
            m_JogPrm.dSmooth = 0;  // 平滑系数

            // 设置JOG模式
            iRes = g_MultiCard.MC_PrfJog(iAxisNum);
            iRes += g_MultiCard.MC_SetJogPrm(iAxisNum, &m_JogPrm);
            iRes += g_MultiCard.MC_SetVel(iAxisNum, -20);  // 负向速度
            iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

            if(0 == iRes) {
                TRACE("负向连续移动......\r\n");
            }
        }

        // 处理正向JOG按钮按下
        if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_POS)->m_hWnd)
        {
            int iRes = 0;
            TJogPrm m_JogPrm;
            int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

            // 配置JOG参数
            m_JogPrm.dAcc = 0.1;
            m_JogPrm.dDec = 0.1;
            m_JogPrm.dSmooth = 0;

            // 设置JOG模式
            iRes = g_MultiCard.MC_PrfJog(iAxisNum);
            iRes += g_MultiCard.MC_SetJogPrm(iAxisNum, &m_JogPrm);
            iRes += g_MultiCard.MC_SetVel(iAxisNum, 20);   // 正向速度
            iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

            if(0 == iRes) {
                TRACE("正向连续移动......\r\n");
            }
        }
        break;

    case WM_LBUTTONUP:
        // 处理JOG按钮释放 - 停止运动
        if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd ||
           pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_POS)->m_hWnd)
        {
            g_MultiCard.MC_Stop(0XFFFF, 0XFFFF);
        }
        break;
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}
```

#### 按钮事件和PreTranslateMessage的配合

```cpp
// 空的按钮事件处理函数（实际处理在PreTranslateMessage中）
void CDemoVCDlg::OnBnClickedButtonJogPos()
{
    // 具体实现在 PreTranslateMessage 函数中
    // 按钮按下，电机正转，按钮抬起，电机停止
}

void CDemoVCDlg::OnBnClickedButtonJogNeg()
{
    // 具体实现在 PreTranslateMessage 函数中
    // 按钮按下，电机反转，按钮抬起，电机停止
}
```

### 事件处理的最佳实践

#### 1. 参数验证

```cpp
void CDemoVCDlg::OnBnClickedButton6()
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

    // 参数验证
    if (iAxisNum < 1 || iAxisNum > 16) {
        AfxMessageBox("无效的轴号！");
        return;
    }

    // 检查轴是否已使能
    if (!IsAxisEnabled(iAxisNum)) {
        AfxMessageBox("请先使能轴！");
        return;
    }

    // 执行运动控制...
}
```

#### 2. 错误处理

```cpp
void CDemoVCDlg::OnBnClickedButton7()
{
    int iRes = 0;

    // 启动调试日志
    g_MultiCard.MC_StartDebugLog(0);

    // 连接控制卡
    iRes = g_MultiCard.MC_Open(1, "192.168.0.200", 60000, "192.168.0.1", 60000);

    if (iRes != 0) {
        CString strMsg;
        strMsg.Format("连接失败，错误码：%d\n请检查网络配置！", iRes);
        MessageBox(strMsg);
        return;
    }

    // 连接成功后的处理
    g_MultiCard.MC_Reset();
    MessageBox("控制卡连接成功！");

    // 更新界面状态
    OnCbnSelchangeCombo1();
}
```

#### 3. 状态同步

```cpp
void CDemoVCDlg::OnBnClickedButtonAxisOn()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;
    CString strText;

    GetDlgItemText(IDC_BUTTON_AXIS_ON, strText);

    if (strText == "伺服使能") {
        iRes = g_MultiCard.MC_AxisOn(iAxisNum);
        if (iRes == 0) {
            GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText("断开使能");
        } else {
            MessageBox("轴使能失败！");
        }
    } else {
        iRes = g_MultiCard.MC_AxisOff(iAxisNum);
        if (iRes == 0) {
            GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText("伺服使能");
        } else {
            MessageBox("轴断开使能失败！");
        }
    }

    // 立即更新相关按钮状态
    UpdateButtonStates();
}
```

### 本章小结

本章详细介绍了MFC的消息映射机制和事件处理方法。通过学习本章，读者应该：

- 理解MFC消息映射的基本原理和三个组成部分
- 掌握按钮点击事件的处理方法
- 熟悉下拉框选择事件的实现
- 了解PreTranslateMessage函数的高级用法
- 掌握事件处理的最佳实践，包括参数验证、错误处理和状态同步

在下一章中，我们将学习实时状态监控和定时器的重要作用。

---

## 第4章：实时状态监控和定时器

### 4.1 定时器的创建和管理

定时器是实时监控系统的心脏，它以固定的时间间隔执行状态更新任务。

#### 定时器的创建

```cpp
BOOL CDemoVCDlg::OnInitDialog()
{
    CDialogEx::OnInitDialog();

    // ... 其他初始化代码

    // 创建定时器，每100ms执行一次
    SetTimer(1, 100, NULL);

    return TRUE;
}
```

#### 定时器参数说明

```cpp
// SetTimer函数原型
UINT_PTR SetTimer(
    UINT_PTR nIDEvent,     // 定时器ID，用于区分多个定时器
    UINT nElapse,          // 时间间隔，单位毫秒
    TIMERPROC lpTimerFunc  // 回调函数，通常为NULL使用默认处理
);
```

#### 定时器的销毁

```cpp
// 在析构函数中销毁定时器
CDemoVCDlg::~CDemoVCDlg()
{
    KillTimer(1);  // 销毁ID为1的定时器
}

// 或在对话框销毁时处理
void CDemoVCDlg::OnDestroy()
{
    CDialogEx::OnDestroy();
    KillTimer(1);
}
```

### 4.2 OnTimer函数的核心作用

OnTimer函数是实时监控系统的核心，它负责读取硬件状态并更新界面显示。

#### OnTimer函数的基本结构

```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    int iRes = 0;
    unsigned long lValue = 0;
    CString strText;

    // 获取当前选择的轴号和卡号
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;
    int iCardIndex = m_ComboBoxCardSel.GetCurSel();

    // 读取系统状态数据
    TAllSysStatusDataSX m_AllSysStatusDataTemp;

    // 执行状态监控任务
    UpdateAxisStatus(iAxisNum, m_AllSysStatusDataTemp);
    UpdateIOStatus(m_AllSysStatusDataTemp);
    UpdateLimitStatus(m_AllSysStatusDataTemp);
    UpdateHandwheelStatus(m_AllSysStatusDataTemp);
    UpdateCompareStatus();
    UpdateCoordinateStatus();

    CDialogEx::OnTimer(nIDEvent);
}
```

#### 模拟量监控

```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // ... 其他代码

    // 读取模拟量输入
    short nValue = 0;
    g_MultiCard.MC_GetAdc(1, &nValue, 1);

    // 显示模拟量电压
    strText.Format("模拟量电压：%d", nValue);
    GetDlgItem(IDC_STATIC_ADC)->SetWindowText(strText);

    // ... 其他监控代码
}
```

#### 轴位置和状态监控

```cpp
void CDemoVCDlg::UpdateAxisStatus(int iAxisNum, const TAllSysStatusDataSX& statusData)
{
    CString strText;

    // 显示规划位置
    strText.Format("%d", statusData.lAxisPrfPos[iAxisNum-1]);
    GetDlgItem(IDC_STATIC_PRF_POS)->SetWindowText(strText);

    // 显示编码器位置
    strText.Format("%d", statusData.lAxisEncPos[iAxisNum-1]);
    GetDlgItem(IDC_STATIC_ENC_POS)->SetWindowText(strText);

    // 更新轴状态显示
    UpdateAxisStatusDisplay(iAxisNum, statusData.lAxisStatus[iAxisNum-1]);
}

void CDemoVCDlg::UpdateAxisStatusDisplay(int iAxisNum, long lAxisStatus)
{
    // 伺服报警状态
    if(lAxisStatus & AXIS_STATUS_SV_ALARM) {
        ((CButton*)GetDlgItem(IDC_RADIO_ALARM))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_ALARM))->SetCheck(false);
    }

    // 运行状态
    if(lAxisStatus & AXIS_STATUS_RUNNING) {
        ((CButton*)GetDlgItem(IDC_RADIO_RUNNING))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_RUNNING))->SetCheck(false);
    }

    // 正限位状态
    if(lAxisStatus & AXIS_STATUS_POS_HARD_LIMIT) {
        ((CButton*)GetDlgItem(IDC_RADIO_LIMP_STATUS))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_LIMP_STATUS))->SetCheck(false);
    }

    // 负限位状态
    if(lAxisStatus & AXIS_STATUS_NEG_HARD_LIMIT) {
        ((CButton*)GetDlgItem(IDC_RADIO_LIMN_STATUS))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_LIMN_STATUS))->SetCheck(false);
    }

    // 回零状态
    if(lAxisStatus & AXIS_STATUS_HOME_RUNNING) {
        ((CButton*)GetDlgItem(IDC_RADIO_HOMING_STATUS))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_HOMING_STATUS))->SetCheck(false);
    }
}
```

### 4.3 状态数据的读取和显示

#### 通用输入状态监控

```cpp
void CDemoVCDlg::UpdateIOStatus(const TAllSysStatusDataSX& statusData)
{
    // 检查16路通用输入状态
    for (int i = 0; i < 16; i++) {
        unsigned short mask = 0x0001 << i;
        int nCtrlID = IDC_RADIO_INPUT_1 + i;

        if(statusData.lGpiRaw[0] & mask) {
            ((CButton*)GetDlgItem(nCtrlID))->SetCheck(true);
        } else {
            ((CButton*)GetDlgItem(nCtrlID))->SetCheck(false);
        }
    }
}
```

#### 硬限位状态监控

```cpp
void CDemoVCDlg::UpdateLimitStatus(const TAllSysStatusDataSX& statusData)
{
    // 负限位状态（8路）
    for (int i = 0; i < 8; i++) {
        unsigned short mask = 0x0001 << i;
        int nCtrlID = IDC_RADIO_LIMN_1 + i;

        if(statusData.lLimitNegRaw & mask) {
            ((CButton*)GetDlgItem(nCtrlID))->SetCheck(true);
        } else {
            ((CButton*)GetDlgItem(nCtrlID))->SetCheck(false);
        }
    }

    // 正限位状态（8路）
    for (int i = 0; i < 8; i++) {
        unsigned short mask = 0x0001 << i;
        int nCtrlID = IDC_RADIO_LIMP_1 + i;

        if(statusData.lLimitPosRaw & mask) {
            ((CButton*)GetDlgItem(nCtrlID))->SetCheck(true);
        } else {
            ((CButton*)GetDlgItem(nCtrlID))->SetCheck(false);
        }
    }
}
```

#### 手轮状态监控

```cpp
void CDemoVCDlg::UpdateHandwheelStatus(const TAllSysStatusDataSX& statusData)
{
    // 轴选信号
    if(statusData.lMPG & 0x0001) {        // X轴选
        ((CButton*)GetDlgItem(IDC_RADIO_X))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_X))->SetCheck(false);
    }

    if(statusData.lMPG & 0x0002) {        // Y轴选
        ((CButton*)GetDlgItem(IDC_RADIO_Y))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_Y))->SetCheck(false);
    }

    if(statusData.lMPG & 0x0004) {        // Z轴选
        ((CButton*)GetDlgItem(IDC_RADIO_Z))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_Z))->SetCheck(false);
    }

    // 倍率信号
    if(statusData.lMPG & 0x0020) {        // X1倍率
        ((CButton*)GetDlgItem(IDC_RADIO_X1))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_X1))->SetCheck(false);
    }

    if(statusData.lMPG & 0x0040) {        // X10倍率
        ((CButton*)GetDlgItem(IDC_RADIO_X10))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_X10))->SetCheck(false);
    }

    // X100倍率（当X1和X10都没有触发时）
    if((0 == (statusData.lMPG & 0x0020)) &&
       (0 == (statusData.lMPG & 0x0040))) {
        ((CButton*)GetDlgItem(IDC_RADIO_X100))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_X100))->SetCheck(false);
    }
}
```

#### 坐标系状态监控

```cpp
void CDemoVCDlg::UpdateCoordinateStatus()
{
    int iRes = 0;
    CString strText;

    // 获取坐标系速度
    double dCrdVel;
    g_MultiCard.MC_GetCrdVel(1, &dCrdVel);

    // 获取坐标系位置
    double dPos[8];
    iRes = g_MultiCard.MC_GetCrdPos(1, dPos);

    strText.Format("X:%.3f", dPos[0]);
    GetDlgItem(IDC_STATIC_POS_x)->SetWindowText(strText);

    strText.Format("Y:%.3f", dPos[1]);
    GetDlgItem(IDC_STATIC_POS_Y)->SetWindowText(strText);

    strText.Format("Z:%.3f", dPos[2]);
    GetDlgItem(IDC_STATIC_POS_Z)->SetWindowText(strText);

    // 获取坐标系状态
    short nCrdStatus = 0;
    iRes = g_MultiCard.MC_CrdStatus(1, &nCrdStatus, NULL);

    // 坐标系报警状态
    if(nCrdStatus & CRDSYS_STATUS_ALARM) {
        ((CButton*)GetDlgItem(IDC_RADIO_CRD_ALARM))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_CRD_ALARM))->SetCheck(false);
    }

    // 坐标系运行状态
    if(nCrdStatus & CRDSYS_STATUS_PROG_RUN) {
        ((CButton*)GetDlgItem(IDC_RADIO_CRD_RUNNING))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_CRD_RUNNING))->SetCheck(false);
    }

    // 缓冲区执行完成状态
    if(nCrdStatus & CRDSYS_STATUS_FIFO_FINISH_0) {
        ((CButton*)GetDlgItem(IDC_RADIO_CRD_BUFFER_FINISH))->SetCheck(true);
    } else {
        ((CButton*)GetDlgItem(IDC_RADIO_CRD_BUFFER_FINISH))->SetCheck(false);
    }

    // 获取运行行号
    long lSegNum = 0;
    g_MultiCard.MC_GetUserSegNum(1, &lSegNum);

    strText.Format("运行行号：%d", lSegNum);
    GetDlgItem(IDC_STATIC_CUR_SEG_NUM)->SetWindowText(strText);
}
```

### 4.4 性能优化注意事项

#### 1. 避免耗时操作

```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // 快速读取状态数据
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_AllSysStatusDataTemp);
    if (iRes != 0) {
        return;  // 快速退出，避免界面卡顿
    }

    // 只更新当前需要显示的数据
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;
    UpdateCurrentAxisDisplay(iAxisNum);

    // 批量更新UI，减少重绘次数
    BeginUpdateUI();
    UpdateAxisStatus(iAxisNum, m_AllSysStatusDataTemp);
    UpdateIOStatus(m_AllSysStatusDataTemp);
    UpdateLimitStatus(m_AllSysStatusDataTemp);
    EndUpdateUI();

    CDialogEx::OnTimer(nIDEvent);
}
```

#### 2. 条件更新策略

```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    static long lastPrfPos[16] = {0};
    static long lastEncPos[16] = {0};

    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_AllSysStatusDataTemp);
    if (iRes != 0) return;

    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

    // 只在位置发生变化时更新显示
    long currentPrfPos = m_AllSysStatusDataTemp.lAxisPrfPos[iAxisNum-1];
    if (currentPrfPos != lastPrfPos[iAxisNum-1]) {
        CString strText;
        strText.Format("%d", currentPrfPos);
        GetDlgItem(IDC_STATIC_PRF_POS)->SetWindowText(strText);
        lastPrfPos[iAxisNum-1] = currentPrfPos;
    }

    // 类似地处理编码器位置
    long currentEncPos = m_AllSysStatusDataTemp.lAxisEncPos[iAxisNum-1];
    if (currentEncPos != lastEncPos[iAxisNum-1]) {
        CString strText;
        strText.Format("%d", currentEncPos);
        GetDlgItem(IDC_STATIC_ENC_POS)->SetWindowText(strText);
        lastEncPos[iAxisNum-1] = currentEncPos;
    }
}
```

#### 3. 异步更新策略

```cpp
// 使用PostMessage异步更新UI
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_AllSysStatusDataTemp);
    if (iRes != 0) return;

    // 异步更新UI，避免定时器阻塞
    PostMessage(WM_UPDATE_STATUS, 0, 0);

    CDialogEx::OnTimer(nIDEvent);
}

// 消息映射
BEGIN_MESSAGE_MAP(CDemoVCDlg, CDialogEx)
    // ...
    ON_MESSAGE(WM_UPDATE_STATUS, &CDemoVCDlg::OnUpdateStatus)
END_MESSAGE_MAP()

// 自定义消息处理函数
LRESULT CDemoVCDlg::OnUpdateStatus(WPARAM wParam, LPARAM lParam)
{
    // 在这里执行耗时的UI更新操作
    UpdateAllStatusDisplays();
    return 0;
}
```

#### 4. 错误处理和恢复

```cpp
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    static int nErrorCount = 0;

    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_AllSysStatusDataTemp);

    if (iRes != 0) {
        nErrorCount++;

        // 连续失败多次时停止定时器或尝试重新连接
        if (nErrorCount > 10) {
            KillTimer(1);
            MessageBox("控制卡通信失败，请检查连接！");
            nErrorCount = 0;
            return;
        }

        // 尝试重新连接
        if (nErrorCount == 5) {
            TryReconnect();
        }
    } else {
        nErrorCount = 0;  // 成功读取，重置错误计数
    }

    // 正常状态更新
    UpdateStatusDisplays();

    CDialogEx::OnTimer(nIDEvent);
}
```

### 本章小结

本章详细介绍了实时状态监控和定时器的重要作用。通过学习本章，读者应该：

- 掌握定时器的创建、管理和销毁方法
- 理解OnTimer函数在实时监控中的核心作用
- 学会各种状态数据的读取和显示技术
- 了解性能优化的注意事项和实现方法
- 掌握错误处理和恢复机制

在下一章中，我们将深入学习运动控制功能的实现方法。

---

## 第5章：运动控制功能实现

### 5.1 单轴点位运动控制

点位运动（Trap模式）是运动控制中最基本的功能，它实现从当前位置到目标位置的点对点运动。

#### 梯形运动参数配置

```cpp
void CDemoVCDlg::OnBnClickedButton6()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

    // 获取界面数据
    UpdateData(true);

    // 配置梯形运动参数
    TTrapPrm TrapPrm;
    TrapPrm.acc = 0.1;           // 加速度（脉冲/毫秒²）
    TrapPrm.dec = 0.1;           // 减速度（脉冲/毫秒²）
    TrapPrm.smoothTime = 0;      // 平滑时间（毫秒）
    TrapPrm.velStart = 0;        // 起始速度（脉冲/毫秒）

    // 设置梯形运动模式
    iRes = g_MultiCard.MC_PrfTrap(iAxisNum);
    if (iRes != 0) {
        AfxMessageBox("设置梯形模式失败！");
        return;
    }

    // 设置运动参数
    iRes += g_MultiCard.MC_SetTrapPrm(iAxisNum, &TrapPrm);
    if (iRes != 0) {
        AfxMessageBox("设置梯形参数失败！");
        return;
    }

    // 设置目标位置
    iRes += g_MultiCard.MC_SetPos(iAxisNum, m_lTargetPos);
    if (iRes != 0) {
        AfxMessageBox("设置目标位置失败！");
        return;
    }

    // 设置运动速度
    iRes = g_MultiCard.MC_SetVel(iAxisNum, 5);  // 5脉冲/毫秒
    if (iRes != 0) {
        AfxMessageBox("设置速度失败！");
        return;
    }

    // 启动运动
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));
    if (iRes != 0) {
        AfxMessageBox("启动运动失败！");
        return;
    }

    TRACE("轴%d开始点位运动，目标位置：%d\n", iAxisNum, m_lTargetPos);
}
```

#### 梯形运动参数详解

```cpp
// 梯形运动参数结构
typedef struct {
    double acc;           // 加速度，单位：脉冲/毫秒²
    double dec;           // 减速度，单位：脉冲/毫秒²
    double smoothTime;    // 平滑时间，单位：毫秒
    double velStart;      // 起始速度，单位：脉冲/毫秒
} TTrapPrm;
```

#### 运动状态检查

```cpp
// 检查轴是否在运动中
bool CDemoVCDlg::IsAxisMoving(int iAxisNum)
{
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if (iRes == 0) {
        return (statusData.lAxisStatus[iAxisNum-1] & AXIS_STATUS_RUNNING) != 0;
    }
    return false;
}

// 等待轴停止运动
bool CDemoVCDlg::WaitForAxisStop(int iAxisNum, int nTimeoutMs)
{
    int nCount = 0;
    int nInterval = 100;  // 检查间隔100ms

    while (IsAxisMoving(iAxisNum) && nCount < nTimeoutMs / nInterval) {
        Sleep(nInterval);
        nCount++;
    }

    return !IsAxisMoving(iAxisNum);  // 返回true表示已停止
}
```

### 5.2 JOG连续运动实现

JOG运动是连续运动模式，通常通过按钮按下启动运动，按钮松开停止运动。

#### JOG参数配置

```cpp
void CDemoVCDlg::StartJogMotion(int iAxisNum, double dVel)
{
    int iRes = 0;

    // 配置JOG参数
    TJogPrm m_JogPrm;
    m_JogPrm.dAcc = 0.1;      // 加速度
    m_JogPrm.dDec = 0.1;      // 减速度
    m_JogPrm.dSmooth = 0;     // 平滑系数

    // 设置JOG模式
    iRes = g_MultiCard.MC_PrfJog(iAxisNum);
    if (iRes != 0) {
        TRACE("设置JOG模式失败，轴号：%d\n", iAxisNum);
        return;
    }

    // 设置JOG参数
    iRes += g_MultiCard.MC_SetJogPrm(iAxisNum, &m_JogPrm);
    if (iRes != 0) {
        TRACE("设置JOG参数失败，轴号：%d\n", iAxisNum);
        return;
    }

    // 设置运动速度（正值为正向，负值为负向）
    iRes += g_MultiCard.MC_SetVel(iAxisNum, dVel);
    if (iRes != 0) {
        TRACE("设置JOG速度失败，轴号：%d\n", iAxisNum);
        return;
    }

    // 启动运动
    iRes += g_MultiCard.MC_Update(0X0001 << (iAxisNum-1));

    if (iRes == 0) {
        TRACE("轴%d开始JOG运动，速度：%.2f\n", iAxisNum, dVel);
    } else {
        TRACE("启动JOG运动失败，轴号：%d\n", iAxisNum);
    }
}

// 停止JOG运动
void CDemoVCDlg::StopJogMotion(int iAxisNum)
{
    int iRes = g_MultiCard.MC_Stop(0XFFFF, 0XFFFF);
    if (iRes == 0) {
        TRACE("轴%d停止JOG运动\n", iAxisNum);
    }
}
```

#### 按键控制实现（回顾PreTranslateMessage）

```cpp
BOOL CDemoVCDlg::PreTranslateMessage(MSG* pMsg)
{
    switch(pMsg->message)
    {
    case WM_LBUTTONDOWN:
        UpdateData(TRUE);

        // 负向JOG按钮按下
        if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd)
        {
            int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;
            StartJogMotion(iAxisNum, -20.0);  // 负向运动
        }

        // 正向JOG按钮按下
        if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_POS)->m_hWnd)
        {
            int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;
            StartJogMotion(iAxisNum, 20.0);   // 正向运动
        }
        break;

    case WM_LBUTTONUP:
        // JOG按钮释放 - 停止运动
        if(pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_NEG)->m_hWnd ||
           pMsg->hwnd == GetDlgItem(IDC_BUTTON_JOG_POS)->m_hWnd)
        {
            StopJogMotion(0);  // 停止所有轴
        }
        break;
    }

    return CDialogEx::PreTranslateMessage(pMsg);
}
```

#### JOG运动的安全控制

```cpp
// 安全的JOG运动控制
void CDemoVCDlg::SafeStartJogMotion(int iAxisNum, double dVel)
{
    // 检查轴是否已使能
    if (!IsAxisEnabled(iAxisNum)) {
        AfxMessageBox("轴未使能，无法启动JOG运动！");
        return;
    }

    // 检查限位状态
    if (IsLimitTriggered(iAxisNum, dVel > 0)) {
        AfxMessageBox("限位已触发，无法向该方向运动！");
        return;
    }

    // 检查轴是否已在运动
    if (IsAxisMoving(iAxisNum)) {
        AfxMessageBox("轴已在运动中！");
        return;
    }

    // 启动JOG运动
    StartJogMotion(iAxisNum, dVel);
}

// 检查限位状态
bool CDemoVCDlg::IsLimitTriggered(int iAxisNum, bool bPositive)
{
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if (iRes == 0) {
        if (bPositive) {
            return (statusData.lAxisStatus[iAxisNum-1] & AXIS_STATUS_POS_HARD_LIMIT) != 0;
        } else {
            return (statusData.lAxisStatus[iAxisNum-1] & AXIS_STATUS_NEG_HARD_LIMIT) != 0;
        }
    }
    return false;
}
```

### 5.3 坐标系插补运动

坐标系插补运动实现多轴协调运动，支持直线插补、圆弧插补等复杂轨迹。

#### 坐标系参数配置

```cpp
void CDemoVCDlg::OnBnClickedButtonSetCrdPrm()
{
    int iRes = 0;

    UpdateData(true);

    // 坐标系参数初始化
    TCrdPrm crdPrm;
    memset(&crdPrm, 0, sizeof(crdPrm));

    // 设置坐标系最小匀速时间为0
    crdPrm.evenTime = 0;

    // 设置原点坐标值标志
    // 0: 默认当前规划位置为原点位置
    // 1: 用户指定原点位置
    crdPrm.setOriginFlag = 1;

    // 设置坐标系原点
    crdPrm.originPos[0] = m_lOrginX;  // X原点
    crdPrm.originPos[1] = m_lOrginY;  // Y原点
    crdPrm.originPos[2] = m_lOrginZ;  // Z原点
    crdPrm.originPos[3] = 0;
    crdPrm.originPos[4] = 0;
    crdPrm.originPos[5] = 0;

    // 设置坐标系维数为2（XY平面）
    crdPrm.dimension = 2;

    // 设置坐标系规划对应轴通道
    // 通道1对应轴1，通道2对应轴2
    crdPrm.profile[0] = 1;   // X轴映射到轴1
    crdPrm.profile[1] = 2;   // Y轴映射到轴2
    crdPrm.profile[2] = 0;   // Z轴未使用
    crdPrm.profile[3] = 0;
    crdPrm.profile[4] = 0;
    crdPrm.profile[5] = 0;

    // 设置坐标系最大加速度和速度
    crdPrm.synAccMax = 0.2;    // 最大加速度
    crdPrm.synVelMax = 1000;   // 最大速度

    // 设置坐标系参数，建立坐标系
    iRes = g_MultiCard.MC_SetCrdPrm(1, &crdPrm);

    if (iRes == 0) {
        AfxMessageBox("建立坐标系成功！");
    } else {
        AfxMessageBox("建立坐标系失败！");
        return;
    }

    // 初始化前瞻缓冲区
    InitLookAheadBuffer();
}

void CDemoVCDlg::InitLookAheadBuffer()
{
    int iRes = 0;

    // 前瞻缓冲区参数
    TLookAheadPrm LookAheadPrm;
    memset(&LookAheadPrm, 0, sizeof(LookAheadPrm));

    // 各轴的最大加速度
    LookAheadPrm.dAccMax[0] = 2;   // X轴最大加速度
    LookAheadPrm.dAccMax[1] = 2;   // Y轴最大加速度
    LookAheadPrm.dAccMax[2] = 2;   // Z轴最大加速度

    // 各轴的最大速度变化量（启动速度）
    LookAheadPrm.dMaxStepSpeed[0] = 10;
    LookAheadPrm.dMaxStepSpeed[1] = 10;
    LookAheadPrm.dMaxStepSpeed[2] = 10;

    // 各轴的脉冲当量（通常为1）
    LookAheadPrm.dScale[0] = 1;
    LookAheadPrm.dScale[1] = 1;
    LookAheadPrm.dScale[2] = 1;
    LookAheadPrm.dScale[3] = 1;
    LookAheadPrm.dScale[4] = 1;
    LookAheadPrm.dScale[5] = 1;

    // 各轴的最大速度
    LookAheadPrm.dSpeedMax[0] = 1000;
    LookAheadPrm.dSpeedMax[1] = 1000;
    LookAheadPrm.dSpeedMax[2] = 1000;
    LookAheadPrm.dSpeedMax[3] = 1000;
    LookAheadPrm.dSpeedMax[4] = 1000;

    // 定义前瞻缓冲区长度和指针
    LookAheadPrm.lookAheadNum = FROCAST_LEN;
    LookAheadPrm.pLookAheadBuf = LookAheadBuf;

    // 初始化前瞻缓冲区
    iRes = g_MultiCard.MC_InitLookAhead(1, 0, &LookAheadPrm);

    if (iRes == 0) {
        AfxMessageBox("初始化前瞻缓冲区成功！");
    } else {
        AfxMessageBox("初始化前瞻缓冲区失败！");
    }
}
```

#### 插补数据写入

```cpp
void CDemoVCDlg::OnBnClickedButton10()
{
    int iRes = 0;

    // 添加直线插补数据：从原点移动到(100000, 100000)
    iRes = g_MultiCard.MC_LnXY(1,           // 坐标系号
                              100000,       // X终点位置
                              100000,       // Y终点位置
                              50,           // 终点速度
                              2,            // 加速度
                              0,            // 终点速度
                              0,            // FIFO号
                              1);           // 用户自定义行号

    if (iRes != 0) {
        AfxMessageBox("添加直线插补数据失败！");
        return;
    }

    // 添加IO输出指令（在运动过程中）
    iRes = g_MultiCard.MC_BufIO(1,        // 坐标系号
                              MC_GPO,     // IO类型：通用输出
                              0,          // 卡索引
                              0X0001,     // IO掩码：第0位
                              0X0001,     // IO数据：第0位置1
                              0,          // 延时时间
                              1);         // 用户自定义行号

    // 添加延时指令
    iRes = g_MultiCard.MC_BufDelay(1,      // 坐标系号
                                   1000,   // 延时时间（毫秒）
                                   0,      // FIFO号
                                   2);     // 用户自定义行号

    // 添加圆弧插补数据：从(100000, 100000)到(0, 0)的半圆
    iRes = g_MultiCard.MC_ArcXYC(1,       // 坐标系号
                                0,        // X终点位置
                                0,        // Y终点位置
                                50000,    // 圆心X坐标
                                50000,    // 圆心Y坐标
                                0,        // 旋转方向：0=逆时针，1=顺时针
                                50,       // 终点速度
                                2,        // 加速度
                                0,        // 终点速度
                                0,        // FIFO号
                                3);       // 用户自定义行号

    // 添加返回原点的直线插补
    iRes = g_MultiCard.MC_LnXY(1, 0, 0, 50, 2, 0, 0, 4);

    // 插入最后一行标识符（系统会把前瞻缓冲区数据全部压入板卡）
    iRes += g_MultiCard.MC_CrdData(1, NULL, 0);

    if (iRes == 0) {
        AfxMessageBox("插入插补数据成功！");
    } else {
        AfxMessageBox("插入插补数据失败！");
    }
}
```

#### 坐标系运动控制

```cpp
// 启动坐标系运动
void CDemoVCDlg::OnBnClickedButton11()
{
    int iRes = 0;

    iRes = g_MultiCard.MC_CrdStart(1, 0);  // 启动坐标系1，FIFO号0

    if (iRes == 0) {
        AfxMessageBox("启动坐标系成功！");
    } else {
        AfxMessageBox("启动坐标系失败！");
    }
}

// 停止坐标系运动
void CDemoVCDlg::OnBnClickedButton12()
{
    int iRes = 0;

    // 停止所有坐标系运动
    iRes = g_MultiCard.MC_Stop(0XFFFFFF00, 0XFFFFFF00);

    if (iRes == 0) {
        AfxMessageBox("停止坐标系成功！");
    } else {
        AfxMessageBox("停止坐标系失败！");
    }
}

// 速度覆盖控制
void CDemoVCDlg::OnNMReleasedcaptureSliderOverRide(NMHDR *pNMHDR, LRESULT *pResult)
{
    int iRes = 0;
    double dOverRide = 0;

    // 获取滑块位置（0-120）
    int nSliderPos = m_SliderCrdVel.GetPos();

    // 显示当前速度百分比
    CString strText;
    strText.Format("%d%%", nSliderPos);
    GetDlgItem(IDC_STATIC_CRD_VEL_OVER_RIDE)->SetWindowText(strText);

    // 计算速度覆盖比例
    dOverRide = nSliderPos / 100.00;

    // 设置速度覆盖
    iRes = g_MultiCard.MC_SetOverride(1, dOverRide);

    *pResult = 0;
}
```

### 5.4 回零控制功能

回零是运动控制系统的重要功能，用于建立机床的坐标系原点。

#### 回零参数配置

```cpp
void CDemoVCDlg::OnBnClickedButtonStartHome()
{
    TAxisHomePrm AxisHomePrm;

    // 配置回零参数
    AxisHomePrm.nHomeMode = 1;          // 回零方式：1=HOME回原点
    AxisHomePrm.nHomeDir = 0;           // 回零方向：0=负向，1=正向
    AxisHomePrm.lOffset = 1;            // 回零偏移：回到零位后再走1个脉冲作为零位
    AxisHomePrm.dHomeRapidVel = 1;      // 回零快移速度（脉冲/毫秒）
    AxisHomePrm.dHomeLocatVel = 1;      // 回零定位速度（脉冲/毫秒）
    AxisHomePrm.dHomeIndexVel = 1;      // 回零寻找INDEX速度（脉冲/毫秒）
    AxisHomePrm.dHomeAcc = 1;           // 回零使用的加速度

    // 高级回零参数
    AxisHomePrm.ulHomeIndexDis = 0;                 // 找INDEX最大距离
    AxisHomePrm.ulHomeBackDis = 0;                  // 回零时，第一次碰到零位后的回退距离
    AxisHomePrm.nDelayTimeBeforeZero = 1000;        // 位置清零前延时时间（毫秒）
    AxisHomePrm.ulHomeMaxDis = 0;                   // 回零最大寻找范围（脉冲）

    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

    // 设置回零参数
    int iRes = g_MultiCard.MC_HomeSetPrm(iAxisNum, &AxisHomePrm);
    if (iRes != 0) {
        AfxMessageBox("设置回零参数失败！");
        return;
    }

    // 停止当前回零（如果正在执行）
    g_MultiCard.MC_HomeStop(iAxisNum);

    // 启动回零
    iRes = g_MultiCard.MC_HomeStart(iAxisNum);
    if (iRes == 0) {
        AfxMessageBox("回零启动成功！");
        TRACE("轴%d开始回零\n", iAxisNum);
    } else {
        AfxMessageBox("回零启动失败！");
    }
}

// 停止回零
void CDemoVCDlg::OnBnClickedButtonStopHome()
{
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

    int iRes = g_MultiCard.MC_HomeStop(iAxisNum);
    if (iRes == 0) {
        AfxMessageBox("回零停止成功！");
        TRACE("轴%d回零停止\n", iAxisNum);
    } else {
        AfxMessageBox("回零停止失败！");
    }
}
```

#### 回零状态监控

```cpp
// 在定时器中监控回零状态
void CDemoVCDlg::UpdateHomeStatus(int iAxisNum, long lAxisStatus)
{
    static bool bLastHomeRunning = false;
    bool bCurrentHomeRunning = (lAxisStatus & AXIS_STATUS_HOME_RUNNING) != 0;

    // 回零状态变化时的处理
    if (bLastHomeRunning != bCurrentHomeRunning) {
        if (bCurrentHomeRunning) {
            // 开始回零
            TRACE("轴%d开始回零\n", iAxisNum);
            SetDlgItemText(IDC_STATIC_HOME_STATUS, "回零中...");
        } else {
            // 回零结束
            TRACE("轴%d回零完成\n", iAxisNum);
            SetDlgItemText(IDC_STATIC_HOME_STATUS, "回零完成");

            // 检查回零是否成功
            if (lAxisStatus & AXIS_STATUS_SV_ALARM) {
                AfxMessageBox("回零过程中发生报警！");
            } else {
                AfxMessageBox("回零成功！");
            }
        }
        bLastHomeRunning = bCurrentHomeRunning;
    }

    // 更新回零状态指示灯
    ((CButton*)GetDlgItem(IDC_RADIO_HOMING_STATUS))->SetCheck(bCurrentHomeRunning);
}
```

### 本章小结

本章详细介绍了运动控制功能的实现方法。通过学习本章，读者应该：

- 掌握单轴点位运动的参数配置和实现方法
- 理解JOG连续运动的控制机制和安全措施
- 学会坐标系插补运动的参数设置和数据写入
- 掌握回零控制的参数配置和状态监控
- 了解各种运动模式的适用场景和注意事项

在下一章中，我们将学习IO控制和通信功能的实现。

---

## 第6章：IO控制和通信功能

### 6.1 数字输入输出控制

数字IO是控制系统与外部设备交互的重要接口，用于读取传感器状态和控制执行器。

#### 数字输出控制

```cpp
// 数字输出按钮事件处理（以Output1为例）
void CDemoVCDlg::OnBnClickedButtonOutPut1()
{
    int iCardIndex = m_ComboBoxCardSel.GetCurSel();  // 获取选择的卡号
    CString strText;

    // 获取当前按钮文本
    GetDlgItem(IDC_BUTTON_OUT_PUT_1)->GetWindowText(strText);

    if(strstr(strText, "ON")) {
        // 当前是ON状态，切换为OFF
        g_MultiCard.MC_SetExtDoBit(iCardIndex, 0, 0);  // 设置第0位为0
        GetDlgItem(IDC_BUTTON_OUT_PUT_1)->SetWindowText("OutPut1 OFF");
        TRACE("卡%d 输出1设置为OFF\n", iCardIndex);
    } else {
        // 当前是OFF状态，切换为ON
        g_MultiCard.MC_SetExtDoBit(iCardIndex, 0, 1);  // 设置第0位为1
        GetDlgItem(IDC_BUTTON_OUT_PUT_1)->SetWindowText("OutPut1 ON");
        TRACE("卡%d 输出1设置为ON\n", iCardIndex);
    }
}

// 通用的数字输出控制函数
void CDemoVCDlg::SetDigitalOutput(int iCardIndex, int iBitIndex, bool bState)
{
    int iRes = g_MultiCard.MC_SetExtDoBit(iCardIndex, iBitIndex, bState ? 1 : 0);

    if (iRes == 0) {
        // 更新按钮文本
        int nButtonID = IDC_BUTTON_OUT_PUT_1 + iBitIndex;
        CString strText;
        strText.Format("OutPut%d %s", iBitIndex + 1, bState ? "ON" : "OFF");
        GetDlgItem(nButtonID)->SetWindowText(strText);

        TRACE("卡%d 输出%d设置为%s\n", iCardIndex, iBitIndex + 1, bState ? "ON" : "OFF");
    } else {
        TRACE("设置卡%d 输出%d失败，错误码：%d\n", iCardIndex, iBitIndex + 1, iRes);
    }
}

// 批量设置数字输出
void CDemoVCDlg::SetMultipleOutputs(int iCardIndex, unsigned short wData, unsigned short wMask)
{
    int iRes = g_MultiCard.MC_SetExtDo(iCardIndex, wData, wMask);

    if (iRes == 0) {
        // 更新相关按钮状态
        for (int i = 0; i < 16; i++) {
            if (wMask & (0x0001 << i)) {
                bool bState = (wData & (0x0001 << i)) != 0;
                UpdateOutputButton(i, bState);
            }
        }
    }
}
```

#### 数字输入状态读取

```cpp
// 在定时器中更新数字输入状态
void CDemoVCDlg::UpdateDigitalInputs(const TAllSysStatusDataSX& statusData)
{
    // 检查16路通用输入状态
    for (int i = 0; i < 16; i++) {
        unsigned short mask = 0x0001 << i;
        int nRadioID = IDC_RADIO_INPUT_1 + i;

        // 更新单选按钮状态
        if(statusData.lGpiRaw[0] & mask) {
            ((CButton*)GetDlgItem(nRadioID))->SetCheck(true);
        } else {
            ((CButton*)GetDlgItem(nRadioID))->SetCheck(false);
        }

        // 记录状态变化
        static bool lastInputState[16] = {false};
        bool currentState = (statusData.lGpiRaw[0] & mask) != 0;

        if (lastInputState[i] != currentState) {
            OnInputChange(i, currentState);  // 输入状态变化处理
            lastInputState[i] = currentState;
        }
    }
}

// 输入状态变化处理函数
void CDemoVCDlg::OnInputChange(int iInputIndex, bool bState)
{
    TRACE("输入%d状态变化：%s\n", iInputIndex + 1, bState ? "ON" : "OFF");

    // 可以根据需要添加特定的处理逻辑
    // 例如：触发报警、启动程序等
    if (iInputIndex == 0 && bState) {
        // 输入1触发，执行特定操作
        // ProcessSpecialOperation();
    }
}
```

#### 限位开关处理

```cpp
// 更新限位开关状态
void CDemoVCDlg::UpdateLimitSwitches(const TAllSysStatusDataSX& statusData)
{
    // 负限位状态更新（8路）
    for (int i = 0; i < 8; i++) {
        unsigned short mask = 0x0001 << i;
        int nRadioID = IDC_RADIO_LIMN_1 + i;

        bool bTriggered = (statusData.lLimitNegRaw & mask) != 0;
        ((CButton*)GetDlgItem(nRadioID))->SetCheck(bTriggered);

        // 限位触发时的处理
        static bool lastNegLimit[8] = {false};
        if (lastNegLimit[i] != bTriggered) {
            OnLimitChange(i + 1, false, bTriggered);  // 负限位变化
            lastNegLimit[i] = bTriggered;
        }
    }

    // 正限位状态更新（8路）
    for (int i = 0; i < 8; i++) {
        unsigned short mask = 0x0001 << i;
        int nRadioID = IDC_RADIO_LIMP_1 + i;

        bool bTriggered = (statusData.lLimitPosRaw & mask) != 0;
        ((CButton*)GetDlgItem(nRadioID))->SetCheck(bTriggered);

        // 限位触发时的处理
        static bool lastPosLimit[8] = {false};
        if (lastPosLimit[i] != bTriggered) {
            OnLimitChange(i + 1, true, bTriggered);  // 正限位变化
            lastPosLimit[i] = bTriggered;
        }
    }
}

// 限位变化处理函数
void CDemoVCDlg::OnLimitChange(int iAxisNum, bool bPositive, bool bTriggered)
{
    CString strMsg;
    strMsg.Format("轴%d %s限位%s", iAxisNum, bPositive ? "正" : "负", bTriggered ? "触发" : "释放");
    TRACE("%s\n", strMsg);

    if (bTriggered) {
        // 限位触发时的安全处理
        // 可以停止轴的运动或禁止向该方向运动
        int iRes = g_MultiCard.MC_Stop(0X0001 << (iAxisNum-1), 0);
        if (iRes == 0) {
            TRACE("轴%d因限位触发而停止\n", iAxisNum);
        }

        // 显示警告信息
        strMsg += "，轴已停止！";
        SetDlgItemText(IDC_STATIC_LIMIT_WARNING, strMsg);
    } else {
        // 限位释放
        SetDlgItemText(IDC_STATIC_LIMIT_WARNING, "");
    }
}
```

### 6.2 模拟量采集

模拟量采集用于读取连续的物理量，如电压、电流、温度等。

#### ADC数据读取

```cpp
// 在定时器中读取模拟量数据
void CDemoVCDlg::UpdateAnalogInput()
{
    short nValue = 0;
    CString strText;

    // 读取ADC通道1的数据
    int iRes = g_MultiCard.MC_GetAdc(1, &nValue, 1);

    if (iRes == 0) {
        // 显示原始ADC值
        strText.Format("ADC原始值：%d", nValue);
        GetDlgItem(IDC_STATIC_ADC_RAW)->SetWindowText(strText);

        // 转换为电压值（假设12位ADC，参考电压10V）
        double dVoltage = (double)nValue * 10.0 / 4096.0;
        strText.Format("电压：%.3f V", dVoltage);
        GetDlgItem(IDC_STATIC_ADC_VOLTAGE)->SetWindowText(strText);

        // 根据电压值进行相应的处理
        ProcessAnalogValue(dVoltage);

        // 记录数据（用于绘图或分析）
        static int nSampleCount = 0;
        if (nSampleCount < 1000) {  // 最多保存1000个采样点
            m_AdcData[nSampleCount] = dVoltage;
            nSampleCount++;
        }
    } else {
        strText.Format("ADC读取失败，错误码：%d", iRes);
        GetDlgItem(IDC_STATIC_ADC_VOLTAGE)->SetWindowText(strText);
    }
}

// 模拟量数据处理
void CDemoVCDlg::ProcessAnalogValue(double dVoltage)
{
    // 设置报警阈值
    const double HIGH_VOLTAGE_THRESHOLD = 8.0;
    const double LOW_VOLTAGE_THRESHOLD = 2.0;

    static bool bHighAlarm = false;
    static bool bLowAlarm = false;

    // 高电压报警
    if (dVoltage > HIGH_VOLTAGE_THRESHOLD && !bHighAlarm) {
        bHighAlarm = true;
        OnAnalogAlarm(true, dVoltage);
    } else if (dVoltage <= HIGH_VOLTAGE_THRESHOLD - 0.5) {  // 滞回
        bHighAlarm = false;
    }

    // 低电压报警
    if (dVoltage < LOW_VOLTAGE_THRESHOLD && !bLowAlarm) {
        bLowAlarm = true;
        OnAnalogAlarm(false, dVoltage);
    } else if (dVoltage >= LOW_VOLTAGE_THRESHOLD + 0.5) {  // 滞回
        bLowAlarm = false;
    }
}

// 模拟量报警处理
void CDemoVCDlg::OnAnalogAlarm(bool bHighVoltage, double dVoltage)
{
    CString strMsg;
    if (bHighVoltage) {
        strMsg.Format("高电压报警：%.3f V", dVoltage);
        SetDlgItemText(IDC_STATIC_ADC_ALARM, strMsg);

        // 触发输出报警信号
        SetDigitalOutput(0, 15, true);  // 设置输出16为报警信号
    } else {
        strMsg.Format("低电压报警：%.3f V", dVoltage);
        SetDlgItemText(IDC_STATIC_ADC_ALARM, strMsg);

        // 触发输出报警信号
        SetDigitalOutput(0, 14, true);  // 设置输出15为报警信号
    }

    TRACE("%s\n", strMsg);

    // 可以添加声音报警、邮件通知等功能
    // PlayAlarmSound();
}
```

### 6.3 串口通信实现

串口通信用于与外部设备进行数据交换，如传感器、PLC、其他控制器等。

#### 串口参数配置

```cpp
// 串口配置按钮事件
void CDemoVCDlg::OnBnClickedButtonUartSet()
{
    int iRes;

    UpdateData(true);  // 获取界面数据

    // 配置串口参数
    iRes = g_MultiCard.MC_UartConfig(
        m_ComboBoxUartNum.GetCurSel() + 1,  // 串口号（1-3）
        m_iBaudRate,                        // 波特率
        m_nDataLen,                         // 数据位
        m_iVerifyType,                      // 校验方式
        m_iStopBit                          // 停止位
    );

    if (iRes == 0) {
        AfxMessageBox("串口配置成功！");
        TRACE("串口%d配置成功：波特率=%d，数据位=%d，校验=%d，停止位=%d\n",
               m_ComboBoxUartNum.GetCurSel() + 1, m_iBaudRate,
               m_nDataLen, m_iVerifyType, m_iStopBit);
    } else {
        AfxMessageBox("串口配置失败！");
        TRACE("串口%d配置失败，错误码：%d\n",
               m_ComboBoxUartNum.GetCurSel() + 1, iRes);
    }
}

// 串口参数验证
bool CDemoVCDlg::ValidateUartParameters()
{
    // 验证波特率
    switch (m_iBaudRate) {
    case 1200:
    case 2400:
    case 4800:
    case 9600:
    case 19200:
    case 38400:
    case 57600:
    case 115200:
        break;  // 有效波特率
    default:
        AfxMessageBox("无效的波特率，请选择标准波特率！");
        return false;
    }

    // 验证数据位
    if (m_nDataLen < 5 || m_nDataLen > 9) {
        AfxMessageBox("数据位必须在5-9之间！");
        return false;
    }

    // 验证校验方式
    if (m_iVerifyType < 0 || m_iVerifyType > 2) {
        AfxMessageBox("校验方式无效（0=无校验，1=奇校验，2=偶校验）！");
        return false;
    }

    // 验证停止位
    if (m_iStopBit < 1 || m_iStopBit > 2) {
        AfxMessageBox("停止位必须是1或2！");
        return false;
    }

    return true;
}
```

#### 串口数据发送

```cpp
// 串口发送按钮事件
void CDemoVCDlg::OnBnClickedButtonUartSend()
{
    // 发送测试字符串
    const char* pTestData = "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";

    int iRes = g_MultiCard.MC_SendEthToUartString(
        m_ComboBoxUartNum.GetCurSel() + 1,  // 串口号
        (unsigned char*)pTestData,           // 数据指针
        100                                  // 数据长度
    );

    if (iRes == 0) {
        AfxMessageBox("串口发送成功！");
        TRACE("串口%d发送数据：%s\n",
               m_ComboBoxUartNum.GetCurSel() + 1, pTestData);
    } else {
        AfxMessageBox("串口发送失败！");
        TRACE("串口%d发送失败，错误码：%d\n",
               m_ComboBoxUartNum.GetCurSel() + 1, iRes);
    }
}

// 发送格式化数据
void CDemoVCDlg::SendFormattedData(int nUartNum, const char* fmt, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    int iRes = g_MultiCard.MC_SendEthToUartString(
        nUartNum,
        (unsigned char*)buffer,
        strlen(buffer)
    );

    if (iRes == 0) {
        TRACE("串口%d发送：%s\n", nUartNum, buffer);
    } else {
        TRACE("串口%d发送失败\n", nUartNum);
    }
}

// 发送命令并等待响应
CString CDemoVCDlg::SendUartCommand(int nUartNum, const char* pCommand, int nTimeoutMs)
{
    // 发送命令
    SendFormattedData(nUartNum, "%s\r\n", pCommand);

    // 等待响应
    int nStartTime = GetTickCount();
    CString strResponse = "";

    while (GetTickCount() - nStartTime < nTimeoutMs) {
        unsigned char cTemp[1024];
        short nLen;

        // 尝试读取数据
        g_MultiCard.MC_ReadUartToEthString(nUartNum, cTemp, &nLen);

        if (nLen > 0) {
            cTemp[nLen] = '\0';
            strResponse += (char*)cTemp;

            // 检查是否收到完整响应
            if (strResponse.Find("\r\n") >= 0) {
                break;
            }
        }

        Sleep(10);  // 避免过度占用CPU
    }

    return strResponse;
}
```

#### 串口数据接收

```cpp
// 串口接收按钮事件
void CDemoVCDlg::OnBnClickedButtonUartRec()
{
    unsigned char cTemp[1024];
    short nLen;
    CString strText;

    // 读取串口数据
    int iRes = g_MultiCard.MC_ReadUartToEthString(
        m_ComboBoxUartNum.GetCurSel() + 1,  // 串口号
        cTemp,                              // 接收缓冲区
        &nLen                               // 接收数据长度
    );

    if (nLen > 0 && nLen < 1024) {
        cTemp[nLen] = '\0';  // 添加字符串结束符
        strText.Format("%s", cTemp);
        GetDlgItem(IDC_STATIC_REV_UART)->SetWindowText(strText);

        TRACE("串口%d接收数据：%s\n",
               m_ComboBoxUartNum.GetCurSel() + 1, strText);

        // 处理接收到的数据
        ProcessUartData(m_ComboBoxUartNum.GetCurSel() + 1, (char*)cTemp, nLen);
    } else {
        // 没有收到数据或接收失败
        GetDlgItem(IDC_STATIC_REV_UART)->SetWindowText("无数据");
    }
}

// 处理接收到的串口数据
void CDemoVCDlg::ProcessUartData(int nUartNum, const char* pData, int nLen)
{
    CString strData(pData, nLen);

    // 解析命令格式
    if (strData.Left(3) == "CMD") {
        ProcessCommand(nUartNum, strData);
    } else if (strData.Left(5) == "DATA:") {
        ProcessDataPacket(nUartNum, strData);
    } else if (strData.Left(6) == "STATUS") {
        ProcessStatusReport(nUartNum, strData);
    } else {
        // 未知数据格式
        TRACE("串口%d收到未知格式数据：%s\n", nUartNum, strData);
    }
}

// 处理命令响应
void CDemoVCDlg::ProcessCommand(int nUartNum, const CString& strData)
{
    // 解析命令：CMD:参数1,参数2,...,参数N
    CString strCmd = strData.Mid(4);  // 去掉"CMD:"前缀

    // 分割参数
    CStringArray params;
    int nPos = 0;
    CString strToken = strCmd.Tokenize(",", nPos);

    while (!strToken.IsEmpty()) {
        params.Add(strToken);
        strToken = strCmd.Tokenize(",", nPos);
    }

    // 处理具体命令
    if (params.GetSize() > 0) {
        CString strCommand = params[0];

        if (strCommand == "MOVE") {
            // 处理移动命令：CMD:MOVE,axis,position,velocity
            if (params.GetSize() >= 4) {
                int nAxis = atoi(params[1]);
                long lPos = atol(params[2]);
                double dVel = atof(params[3]);
                ExecuteMoveCommand(nAxis, lPos, dVel);
            }
        } else if (strCommand == "STOP") {
            // 处理停止命令：CMD:STOP
            ExecuteStopCommand();
        } else if (strCommand == "HOME") {
            // 处理回零命令：CMD:HOME,axis
            if (params.GetSize() >= 2) {
                int nAxis = atoi(params[1]);
                ExecuteHomeCommand(nAxis);
            }
        }
    }
}
```

### 6.4 比较输出功能

比较输出功能用于在轴到达指定位置时触发输出信号，常用于激光打标、位置检测等应用。

#### 比较输出调试

```cpp
// 强制比较器输出高电平
void CDemoVCDlg::OnBnClickedComboCmpH()
{
    /*
    函数说明：设置比较器输出IO立即输出指定电平或者脉冲
    参数说明：
    - nChannelMask: bit0表示通道1，bit1表示通道2
    - nPluseType1: 通道1输出类型，0=低电平，1=高电平，2=脉冲
    - nPluseType2: 通道2输出类型，0=低电平，1=高电平，2=脉冲
    - nTime1: 通道1脉冲持续时间，输出类型为脉冲时该参数有效
    - nTime2: 通道2脉冲持续时间，输出类型为脉冲时该参数有效
    - nTimeFlag1: 比较器1的脉冲时间单位，0=us，1=ms
    - nTimeFlag2: 比较器2的脉冲时间单位，0=us，1=ms
    */

    int iRes = g_MultiCard.MC_CmpPluse(1,    // 通道1
                                      1,    // 输出高电平
                                      1,    // 通道2也输出高电平
                                      0, 0, 0, 0);  // 时间参数，电平模式时不使用

    if (iRes == 0) {
        AfxMessageBox("比较器输出高电平成功！");
        TRACE("比较器通道1和2强制输出高电平\n");
    } else {
        AfxMessageBox("比较器输出失败！");
    }
}

// 强制比较器输出低电平
void CDemoVCDlg::OnBnClickedComboCmpL()
{
    int iRes = g_MultiCard.MC_CmpPluse(1, 0, 0, 0, 0, 0, 0);

    if (iRes == 0) {
        AfxMessageBox("比较器输出低电平成功！");
        TRACE("比较器通道1和2强制输出低电平\n");
    } else {
        AfxMessageBox("比较器输出失败！");
    }
}

// 强制比较器输出脉冲
void CDemoVCDlg::OnBnClickedComboCmpP()
{
    int iRes = g_MultiCard.MC_CmpPluse(1,        // 通道1
                                      2,        // 输出脉冲
                                      0,        // 通道2不输出
                                      1000,     // 脉冲持续时间
                                      0,        // 通道2时间
                                      m_ComboBoxCMPUnit.GetCurSel(),  // 时间单位
                                      0);       // 通道2时间单位

    if (iRes == 0) {
        CString strUnit = (m_ComboBoxCMPUnit.GetCurSel() == 0) ? "us" : "ms";
        AfxMessageBox("比较器输出脉冲成功！");
        TRACE("比较器通道1输出1000%s脉冲\n", strUnit);
    } else {
        AfxMessageBox("比较器输出失败！");
    }
}
```

#### 位置比较数据配置

```cpp
// 写入比较数据
void CDemoVCDlg::OnBnClickedButtonCmpData()
{
    long CMPData1[10];
    long CMPData2[10];

    // 初始化待比较数据，一次下发10个位置
    for (int i = 0; i < 10; i++) {
        CMPData1[i] = 10000 * (i + 1);  // 10000, 20000, 30000...100000
    }

    /*
    函数参数说明：
    - 第一个参数：轴号，说明是用轴1的编码器进行比较
    - 第二个参数：输出类型，2=脉冲，意思是到比较位置后输出一个脉冲
    - 第三个参数：初始电平，0=初始化为低电平
    - 第四个参数：脉冲持续时间
    - 第五个参数：待比较数据指针
    - 第六个参数：待比较数据长度
    - 第七个参数：通道2带比较数据存放指针
    - 第八个参数：通道2待比较数据长度
    - 第九个参数：位置类型，0=相对当前位置，1=绝对位置
    - 最后一个参数：脉冲持续时间单位，0=us，1=ms
    */

    int iRes = g_MultiCard.MC_CmpBufData(
        1,              // 轴1的编码器进行比较
        2,              // 输出脉冲
        0,              // 初始电平为低
        1000,           // 脉冲持续时间1000单位
        CMPData1,       // 比较数据数组
        10,             // 数据长度10
        CMPData2,       // 通道2数据（未使用）
        0,              // 通道2数据长度
        0,              // 相对位置
        m_ComboBoxCMPUnit.GetCurSel()  // 时间单位
    );

    if (iRes == 0) {
        AfxMessageBox("写入比较数据成功！");
        TRACE("写入10个比较位置数据，轴1运动到指定位置时将输出脉冲\n");

        // 显示比较状态
        UpdateCompareStatus();
    } else {
        AfxMessageBox("写入比较数据失败！");
        TRACE("写入比较数据失败，错误码：%d\n", iRes);
    }
}
```

#### 比较输出状态监控

```cpp
// 更新比较输出状态
void CDemoVCDlg::UpdateCompareStatus()
{
    unsigned short CmpStatus;          // 比较器状态
    unsigned short CmpRemainData1;     // 通道1剩余数据个数
    unsigned short CmpRemainData2;     // 通道2剩余数据个数
    unsigned short CmpRemainSpace1;    // 通道1剩余空间
    unsigned short CmpRemainSpace2;    // 通道2剩余空间

    // 获取比较缓冲区状态
    int iRes = g_MultiCard.MC_CmpBufSts(
        &CmpStatus,
        &CmpRemainData1, &CmpRemainData2,
        &CmpRemainSpace1, &CmpRemainSpace2
    );

    if (iRes == 0) {
        // 显示剩余待比较数据个数
        CString strText;
        strText.Format("剩余待比较数据个数：%d", CmpRemainData1);
        GetDlgItem(IDC_STATIC_CMP_COUNT)->SetWindowText(strText);

        // 更新缓冲区空满状态
        if (CmpStatus & 0X0001) {
            ((CButton*)GetDlgItem(IDC_RADIO_EMPTY))->SetCheck(false);  // 非空
        } else {
            ((CButton*)GetDlgItem(IDC_RADIO_EMPTY))->SetCheck(true);   // 空
        }

        // 显示详细状态信息
        strText.Format("状态：0x%04X, 通道1剩余：%d, 空间：%d",
                      CmpStatus, CmpRemainData1, CmpRemainSpace1);
        TRACE("比较器状态：%s\n", strText);
    }
}

// 在定时器中调用比较状态更新
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // ... 其他状态更新代码

    // 更新比较输出状态
    UpdateCompareStatus();

    // ... 其他代码
}
```

### 本章小结

本章详细介绍了IO控制和通信功能的实现方法。通过学习本章，读者应该：

- 掌握数字输入输出的控制方法和状态监控
- 理解模拟量采集的数据处理和报警机制
- 学会串口通信的参数配置和数据收发
- 掌握比较输出功能的配置和状态监控
- 了解各种IO功能的实际应用场景

在下一章中，我们将学习调试技巧和最佳实践。

---

## 第7章：调试技巧和最佳实践

### 7.1 错误处理模式

良好的错误处理是工业软件稳定运行的关键。

#### 标准错误处理函数

```cpp
// 通用错误处理函数
bool CDemoVCDlg::HandleApiError(int iErrorCode, const CString& strOperation, const CString& strDetails)
{
    if (iErrorCode != 0) {
        CString strMsg;
        strMsg.Format("%s失败，错误码：%d", strOperation, iErrorCode);

        if (!strDetails.IsEmpty()) {
            strMsg += "\n详细信息：" + strDetails;
        }

        // 记录错误日志
        LogError(strMsg);

        // 显示用户友好的错误信息
        AfxMessageBox(strMsg);

        return false;
    }
    return true;
}

// 特定功能的错误处理
bool CDemoVCDlg::HandleMotionError(int iErrorCode, int iAxisNum, const CString& strOperation)
{
    if (iErrorCode != 0) {
        CString strMsg;
        strMsg.Format("轴%d %s失败，错误码：%d", iAxisNum, strOperation, iErrorCode);

        // 根据错误码提供具体的错误描述
        switch (iErrorCode) {
        case 1:
            strMsg += "\n错误描述：参数无效";
            break;
        case 2:
            strMsg += "\n错误描述：轴未使能";
            break;
        case 3:
            strMsg += "\n错误描述：限位触发";
            break;
        case 4:
            strMsg += "\n错误描述：伺服报警";
            break;
        default:
            strMsg += "\n错误描述：未知错误";
            break;
        }

        // 记录到错误日志
        LogError(strMsg);

        // 显示错误信息
        AfxMessageBox(strMsg);

        // 触发错误处理流程
        OnMotionError(iAxisNum, iErrorCode);

        return false;
    }
    return true;
}
```

#### 错误日志记录

```cpp
// 错误日志记录函数
void CDemoVCDlg::LogError(const CString& strError)
{
    // 获取当前时间
    CTime currentTime = CTime::GetCurrentTime();
    CString strTime = currentTime.Format("%Y-%m-%d %H:%M:%S");

    // 构造日志条目
    CString strLogEntry;
    strLogEntry.Format("[%s] ERROR: %s\n", strTime, strError);

    // 写入日志文件
    CStdioFile logFile;
    if (logFile.Open("DemoVC_Error.log", CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite)) {
        logFile.SeekToEnd();
        logFile.WriteString(strLogEntry);
        logFile.Close();
    }

    // 同时输出到调试窗口
    TRACE("%s", strLogEntry);
}

// 操作日志记录
void CDemoVCDlg::LogOperation(const CString& strOperation)
{
    CTime currentTime = CTime::GetCurrentTime();
    CString strTime = currentTime.Format("%Y-%m-%d %H:%M:%S");

    CString strLogEntry;
    strLogEntry.Format("[%s] OPERATION: %s\n", strTime, strOperation);

    // 写入操作日志
    CStdioFile logFile;
    if (logFile.Open("DemoVC_Operation.log", CFile::modeCreate | CFile::modeNoTruncate | CFile::modeWrite)) {
        logFile.SeekToEnd();
        logFile.WriteString(strLogEntry);
        logFile.Close();
    }

    TRACE("%s", strLogEntry);
}
```

#### 错误恢复机制

```cpp
// 运动错误处理函数
void CDemoVCDlg::OnMotionError(int iAxisNum, int iErrorCode)
{
    // 停止所有运动
    g_MultiCard.MC_Stop(0XFFFF, 0XFFFF);

    // 记录错误状态
    m_nLastError[iAxisNum-1] = iErrorCode;
    m_bErrorState[iAxisNum-1] = true;

    // 更新界面显示
    UpdateErrorStatus(iAxisNum);

    // 尝试错误恢复
    if (AttemptErrorRecovery(iAxisNum, iErrorCode)) {
        LogOperation(CString().Format("轴%d错误恢复成功", iAxisNum));
    } else {
        LogOperation(CString().Format("轴%d错误恢复失败，需要人工干预", iAxisNum));
    }
}

// 错误恢复尝试
bool CDemoVCDlg::AttemptErrorRecovery(int iAxisNum, int iErrorCode)
{
    bool bRecoverySuccess = false;

    switch (iErrorCode) {
    case 2:  // 轴未使能
        // 尝试重新使能轴
        if (g_MultiCard.MC_AxisOn(iAxisNum) == 0) {
            bRecoverySuccess = true;
            LogOperation(CString().Format("轴%d重新使能成功", iAxisNum));
        }
        break;

    case 3:  // 限位触发
        // 清除错误状态
        if (g_MultiCard.MC_ClrSts(iAxisNum) == 0) {
            bRecoverySuccess = true;
            LogOperation(CString().Format("轴%d限位错误清除成功", iAxisNum));
        }
        break;

    case 4:  // 伺服报警
        // 伺服报警需要人工检查硬件
        bRecoverySuccess = false;
        break;

    default:
        // 通用错误恢复：清除状态并复位
        if (g_MultiCard.MC_ClrSts(iAxisNum) == 0) {
            Sleep(100);  // 等待状态清除完成
            bRecoverySuccess = true;
        }
        break;
    }

    // 更新错误状态标志
    if (bRecoverySuccess) {
        m_bErrorState[iAxisNum-1] = false;
        m_nLastError[iAxisNum-1] = 0;
    }

    return bRecoverySuccess;
}
```

### 7.2 调试技巧和方法

#### 调试宏定义

```cpp
// 调试宏定义
#ifdef _DEBUG
#define DEBUG_PRINT(fmt, ...) \
    { \
        CString strDebug; \
        strDebug.Format(fmt, __VA_ARGS__); \
        TRACE(strDebug); \
        OutputDebugString(strDebug); \
    }

#define DEBUG_FUNCTION() \
    TRACE("进入函数：%s, 行号：%d\n", __FUNCTION__, __LINE__);

#define DEBUG_ASSERT(condition) \
    if (!(condition)) { \
        CString strMsg; \
        strMsg.Format("断言失败：%s, 文件：%s, 行号：%d", #condition, __FILE__, __LINE__); \
        TRACE("%s\n", strMsg); \
        AfxMessageBox(strMsg); \
    }
#else
#define DEBUG_PRINT(fmt, ...)
#define DEBUG_FUNCTION()
#define DEBUG_ASSERT(condition)
#endif
```

#### 状态监控和调试

```cpp
// 系统状态调试信息
void CDemoVCDlg::DebugPrintSystemStatus()
{
    TAllSysStatusDataSX statusData;
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

    if (iRes == 0) {
        DEBUG_PRINT("=== 系统状态调试信息 ===\n");

        // 打印轴状态
        for (int i = 0; i < 16; i++) {
            if (statusData.lAxisStatus[i] != 0) {
                DEBUG_PRINT("轴%d状态：0x%08X", i+1, statusData.lAxisStatus[i]);

                if (statusData.lAxisStatus[i] & AXIS_STATUS_ENABLE) {
                    DEBUG_PRINT(" [使能]");
                }
                if (statusData.lAxisStatus[i] & AXIS_STATUS_RUNNING) {
                    DEBUG_PRINT(" [运行中]");
                }
                if (statusData.lAxisStatus[i] & AXIS_STATUS_SV_ALARM) {
                    DEBUG_PRINT(" [伺服报警]");
                }
                if (statusData.lAxisStatus[i] & AXIS_STATUS_POS_HARD_LIMIT) {
                    DEBUG_PRINT(" [正限位]");
                }
                if (statusData.lAxisStatus[i] & AXIS_STATUS_NEG_HARD_LIMIT) {
                    DEBUG_PRINT(" [负限位]");
                }
                DEBUG_PRINT("\n");
            }
        }

        // 打印IO状态
        DEBUG_PRINT("通用输入状态：0x%04X\n", statusData.lGpiRaw[0]);
        DEBUG_PRINT("通用输出状态：0x%04X\n", statusData.lGpoRaw[0]);
        DEBUG_PRINT("正限位状态：0x%04X\n", statusData.lLimitPosRaw);
        DEBUG_PRINT("负限位状态：0x%04X\n", statusData.lLimitNegRaw);

        DEBUG_PRINT("=== 调试信息结束 ===\n");
    } else {
        DEBUG_PRINT("读取系统状态失败，错误码：%d\n", iRes);
    }
}

// 运动参数调试
void CDemoVCDlg::DebugPrintMotionParams(int iAxisNum)
{
    DEBUG_FUNCTION();

    // 读取当前运动参数
    TTrapPrm trapPrm;
    int iRes = g_MultiCard.MC_GetTrapPrm(iAxisNum, &trapPrm);

    if (iRes == 0) {
        DEBUG_PRINT("轴%d梯形运动参数：\n", iAxisNum);
        DEBUG_PRINT("  加速度：%.3f 脉冲/ms²\n", trapPrm.acc);
        DEBUG_PRINT("  减速度：%.3f 脉冲/ms²\n", trapPrm.dec);
        DEBUG_PRINT("  平滑时间：%.3f ms\n", trapPrm.smoothTime);
        DEBUG_PRINT("  起始速度：%.3f 脉冲/ms\n", trapPrm.velStart);
    } else {
        DEBUG_PRINT("读取轴%d梯形参数失败，错误码：%d\n", iAxisNum, iRes);
    }

    // 读取当前速度和位置
    double dVel;
    long lPos;
    iRes = g_MultiCard.MC_GetVel(iAxisNum, &dVel);
    iRes += g_MultiCard.MC_GetPrfPos(iAxisNum, &lPos);

    if (iRes == 0) {
        DEBUG_PRINT("  当前速度：%.3f 脉冲/ms\n", dVel);
        DEBUG_PRINT("  当前位置：%d 脉冲\n", lPos);
    }
}
```

#### 网络通信调试

```cpp
// 网络连接状态检查
bool CDemoVCDlg::DebugCheckNetworkConnection()
{
    DEBUG_FUNCTION();

    bool bConnectionOK = true;

    // 检查第一张控制卡连接
    int iRes = g_MultiCard.MC_GetCardSts();
    if (iRes == 0) {
        DEBUG_PRINT("控制卡1连接正常\n");
    } else {
        DEBUG_PRINT("控制卡1连接异常，状态码：%d\n", iRes);
        bConnectionOK = false;
    }

    // 检查网络延迟（通过读取状态测试响应时间）
    DWORD dwStartTime = GetTickCount();
    TAllSysStatusDataSX statusData;
    iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);
    DWORD dwEndTime = GetTickCount();

    if (iRes == 0) {
        DWORD dwDelay = dwEndTime - dwStartTime;
        DEBUG_PRINT("网络延迟：%d ms\n", dwDelay);

        if (dwDelay > 100) {
            DEBUG_PRINT("警告：网络延迟较高，可能影响实时性能\n");
        }
    } else {
        DEBUG_PRINT("网络通信测试失败，错误码：%d\n", iRes);
        bConnectionOK = false;
    }

    return bConnectionOK;
}

// 定期网络诊断
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    static int nNetworkCheckCounter = 0;

    // 每10秒检查一次网络状态
    if (++nNetworkCheckCounter >= 100) {  // 100次 * 100ms = 10秒
        nNetworkCheckCounter = 0;

        if (!DebugCheckNetworkConnection()) {
            LogOperation("网络连接检查发现问题");
            SetDlgItemText(IDC_STATIC_NETWORK_STATUS, "网络异常");
        } else {
            SetDlgItemText(IDC_STATIC_NETWORK_STATUS, "网络正常");
        }
    }

    // ... 其他定时器代码
}
```

### 7.3 代码规范和优化

#### 命名规范

```cpp
// 好的命名规范示例
class CDemoVCDlg : public CDialogEx
{
private:
    // 成员变量使用m_前缀，类型缩写 + 描述性名称
    CComboBox m_ComboBoxAxis;          // 轴选择下拉框
    CEdit      m_EditTargetPosition;    // 目标位置编辑框
    long       m_lCurrentPosition;      // 当前位置（long类型，l前缀）
    double     m_dCurrentVelocity;      // 当前速度（double类型，d前缀）
    bool       m_bAxisEnabled[16];      // 轴使能状态数组（bool类型，b前缀）
    int        m_nSelectedAxis;         // 选择的轴号（int类型，n前缀）

    // 常量使用全大写，下划线分隔
    static const int MAX_AXIS_COUNT = 16;
    static const int TIMER_INTERVAL_MS = 100;
    static const double DEFAULT_VELOCITY = 50.0;

    // 函数命名使用动词开头或动宾结构
    bool InitializeAxis(int iAxisNum);              // 初始化轴
    void UpdateAxisStatus(int iAxisNum);            // 更新轴状态
    bool IsAxisMoving(int iAxisNum);                // 判断轴是否运动
    void StartJogMotion(int iAxisNum, double dVel); // 开始JOG运动
    void StopAllMotion();                           // 停止所有运动

public:
    // 获取函数使用Get前缀
    long GetAxisPosition(int iAxisNum) const;
    double GetAxisVelocity(int iAxisNum) const;
    bool IsAxisEnabled(int iAxisNum) const;

    // 设置函数使用Set前缀
    void SetTargetPosition(long lPosition);
    void SetAxisVelocity(int iAxisNum, double dVelocity);
    void SetAxisEnabled(int iAxisNum, bool bEnabled);
};
```

#### 内存管理优化

```cpp
// 避免频繁内存分配的缓冲区管理
class CDemoVCDlg : public CDialogEx
{
private:
    // 预分配缓冲区，避免频繁分配
    static const int BUFFER_SIZE = 1024;
    char m_chTextBuffer[BUFFER_SIZE];
    wchar_t m_wchTextBuffer[BUFFER_SIZE];

    // 对象池模式
    std::vector<CString> m_strPool;
    int m_nCurrentPoolIndex;

    CString& GetPooledString()
    {
        if (m_nCurrentPoolIndex >= m_strPool.size()) {
            m_strPool.push_back(CString());
        }
        CString& str = m_strPool[m_nCurrentPoolIndex];
        str.Empty();
        m_nCurrentPoolIndex = (m_nCurrentPoolIndex + 1) % m_strPool.size();
        return str;
    }

    // 高效的字符串格式化
    CString FormatString(const char* fmt, ...)
    {
        CString& str = GetPooledString();
        va_list args;
        va_start(args, fmt);
        str.FormatV(fmt, args);
        va_end(args);
        return str;
    }

public:
    // 优化的状态更新函数
    void UpdateAxisStatusOptimized(int iAxisNum)
    {
        TAllSysStatusDataSX statusData;
        int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);

        if (iRes != 0) return;

        // 使用预分配的缓冲区进行字符串格式化
        CString strPos = FormatString("%d", statusData.lAxisPrfPos[iAxisNum-1]);
        CString strEnc = FormatString("%d", statusData.lAxisEncPos[iAxisNum-1]);

        // 批量更新UI控件
        BeginUpdateUI();
        GetDlgItem(IDC_STATIC_PRF_POS)->SetWindowText(strPos);
        GetDlgItem(IDC_STATIC_ENC_POS)->SetWindowText(strEnc);
        EndUpdateUI();
    }

private:
    // UI更新优化
    void BeginUpdateUI()
    {
        // 禁用窗口重绘
        SendMessage(WM_SETREDRAW, FALSE, 0);
    }

    void EndUpdateUI()
    {
        // 重新启用窗口重绘
        SendMessage(WM_SETREDRAW, TRUE, 0);
        InvalidateRect(NULL, TRUE);
    }
};
```

#### 算法优化

```cpp
// 优化的状态查找算法
class CDemoVCDlg : public CDialogEx
{
private:
    // 使用位运算优化状态检查
    inline bool IsAxisEnabled(const TAllSysStatusDataSX& status, int iAxisNum)
    {
        return (status.lAxisStatus[iAxisNum-1] & AXIS_STATUS_ENABLE) != 0;
    }

    inline bool IsAxisMoving(const TAllSysStatusDataSX& status, int iAxisNum)
    {
        return (status.lAxisStatus[iAxisNum-1] & AXIS_STATUS_RUNNING) != 0;
    }

    inline bool IsLimitTriggered(const TAllSysStatusDataSX& status, int iAxisNum, bool bPositive)
    {
        long lMask = 0x0001 << (iAxisNum-1);
        if (bPositive) {
            return (status.lLimitPosRaw & lMask) != 0;
        } else {
            return (status.lLimitNegRaw & lMask) != 0;
        }
    }

    // 优化的批量状态更新
    void UpdateAllAxisStatusOptimized()
    {
        TAllSysStatusDataSX statusData;
        int iRes = g_MultiCard.MC_GetAllSysStatusSX(&statusData);
        if (iRes != 0) return;

        // 使用位运算批量处理多个轴
        DWORD dwEnabledMask = 0;
        DWORD dwMovingMask = 0;
        DWORD dwErrorMask = 0;

        for (int i = 0; i < 16; i++) {
            if (IsAxisEnabled(statusData, i+1)) {
                dwEnabledMask |= (0x0001 << i);
            }
            if (IsAxisMoving(statusData, i+1)) {
                dwMovingMask |= (0x0001 << i);
            }
            if (statusData.lAxisStatus[i] & AXIS_STATUS_SV_ALARM) {
                dwErrorMask |= (0x0001 << i);
            }
        }

        // 批量更新界面显示
        UpdateStatusDisplay(dwEnabledMask, dwMovingMask, dwErrorMask);
    }

    void UpdateStatusDisplay(DWORD dwEnabledMask, DWORD dwMovingMask, DWORD dwErrorMask)
    {
        // 使用位运算快速更新多个控件的显示状态
        for (int i = 0; i < 16; i++) {
            DWORD dwMask = 0x0001 << i;

            // 更新使能状态指示灯
            bool bEnabled = (dwEnabledMask & dwMask) != 0;
            UpdateStatusLight(IDC_LED_ENABLE_1 + i, bEnabled);

            // 更新运动状态指示灯
            bool bMoving = (dwMovingMask & dwMask) != 0;
            UpdateStatusLight(IDC_LED_MOVING_1 + i, bMoving);

            // 更新错误状态指示灯
            bool bError = (dwErrorMask & dwMask) != 0;
            UpdateStatusLight(IDC_LED_ERROR_1 + i, bError);
        }
    }

    inline void UpdateStatusLight(int nControlID, bool bState)
    {
        CButton* pButton = (CButton*)GetDlgItem(nControlID);
        if (pButton) {
            pButton->SetCheck(bState);
        }
    }
};
```

### 7.4 常见问题解决方案

#### 问题1：连接失败

```cpp
// 诊断和解决连接问题
void CDemoVCDlg::DiagnoseConnectionProblem()
{
    DEBUG_FUNCTION();

    CString strDiagnosis = "连接问题诊断：\n\n";

    // 1. 检查网络配置
    strDiagnosis += "1. 检查网络配置\n";

    // 检查IP地址格式
    CString strPCIP = "192.168.0.200";
    CString strCardIP = "192.168.0.1";

    if (!IsValidIPAddress(strPCIP)) {
        strDiagnosis += "   - PC IP地址格式无效\n";
    } else {
        strDiagnosis += "   - PC IP地址格式正确\n";
    }

    if (!IsValidIPAddress(strCardIP)) {
        strDiagnosis += "   - 控制卡IP地址格式无效\n";
    } else {
        strDiagnosis += "   - 控制卡IP地址格式正确\n";
    }

    // 2. 测试网络连通性
    strDiagnosis += "\n2. 测试网络连通性\n";

    if (TestNetworkConnectivity(strCardIP)) {
        strDiagnosis += "   - 网络连通正常\n";
    } else {
        strDiagnosis += "   - 网络连通失败，请检查：\n";
        strDiagnosis += "     * 网线是否连接\n";
        strDiagnosis += "     * IP地址是否正确\n";
        strDiagnosis += "     * 防火墙设置\n";
        strDiagnosis += "     * 控制卡是否上电\n";
    }

    // 3. 检查DLL文件
    strDiagnosis += "\n3. 检查DLL文件\n";

    if (CheckDLLFile()) {
        strDiagnosis += "   - MultiCard.dll存在且可访问\n";
    } else {
        strDiagnosis += "   - MultiCard.dll缺失或无法访问\n";
    }

    // 4. 检查端口占用
    strDiagnosis += "\n4. 检查端口占用\n";

    if (IsPortAvailable(60000)) {
        strDiagnosis += "   - 端口60000可用\n";
    } else {
        strDiagnosis += "   - 端口60000被占用，请关闭其他程序\n";
    }

    // 显示诊断结果
    AfxMessageBox(strDiagnosis);
}

// 辅助函数：检查IP地址格式
bool CDemoVCDlg::IsValidIPAddress(const CString& strIP)
{
    // 简单的IP地址格式验证
    int nDotCount = 0;
    for (int i = 0; i < strIP.GetLength(); i++) {
        if (strIP[i] == '.') nDotCount++;
    }
    return nDotCount == 3;
}

// 辅助函数：测试网络连通性
bool CDemoVCDlg::TestNetworkConnectivity(const CString& strIP)
{
    // 使用ping命令测试连通性
    CString strCommand;
    strCommand.Format("ping -n 1 -w 1000 %s", strIP);

    int nResult = system(strCommand);
    return nResult == 0;
}
```

#### 问题2：运动不平滑

```cpp
// 解决运动不平滑问题
void CDemoVCDlg::OptimizeMotionSmoothness()
{
    DEBUG_FUNCTION();

    CString strOptimization = "运动平滑性优化：\n\n";

    // 1. 检查前瞻参数
    strOptimization += "1. 前瞻参数优化\n";

    TLookAheadPrm lookPrm;
    int iRes = g_MultiCard.MC_GetLookAheadPrm(1, 0, &lookPrm);

    if (iRes == 0) {
        strOptimization += CString().Format("   - 当前前瞻段数：%d\n", lookPrm.lookAheadNum);

        if (lookPrm.lookAheadNum < 100) {
            strOptimization += "   - 建议：增加前瞻段数到100-200\n";
        }

        // 优化前瞻参数
        lookPrm.lookAheadNum = 150;
        lookPrm.dAccMax[0] = 2.0;    // 适当的加速度
        lookPrm.dAccMax[1] = 2.0;
        lookPrm.dMaxStepSpeed[0] = 10.0;  // 平滑的起步速度
        lookPrm.dMaxStepSpeed[1] = 10.0;

        iRes = g_MultiCard.MC_InitLookAhead(1, 0, &lookPrm);
        if (iRes == 0) {
            strOptimization += "   - 前瞻参数已优化\n";
        }
    }

    // 2. 检查加速度设置
    strOptimization += "\n2. 加速度参数检查\n";

    for (int i = 1; i <= 2; i++) {  // 检查前2个轴
        TTrapPrm trapPrm;
        iRes = g_MultiCard.MC_GetTrapPrm(i, &trapPrm);

        if (iRes == 0) {
            strOptimization += CString().Format("   - 轴%d 加速度：%.3f\n", i, trapPrm.acc);
            strOptimization += CString().Format("   - 轴%d 减速度：%.3f\n", i, trapPrm.dec);

            if (trapPrm.acc < 0.05 || trapPrm.dec < 0.05) {
                strOptimization += "   - 警告：加速度过小可能导致运动不平滑\n";
            }

            if (trapPrm.smoothTime < 20) {
                strOptimization += "   - 建议：增加平滑时间到20-50ms\n";
                trapPrm.smoothTime = 30;
                g_MultiCard.MC_SetTrapPrm(i, &trapPrm);
            }
        }
    }

    // 3. 网络优化建议
    strOptimization += "\n3. 网络优化建议\n";
    strOptimization += "   - 使用有线网络连接\n";
    strOptimization += "   - 禁用WiFi避免干扰\n";
    strOptimization += "   - 确保网络延迟小于10ms\n";
    strOptimization += "   - 关闭不必要的网络服务\n";

    // 显示优化结果
    AfxMessageBox(strOptimization);
}
```

#### 问题3：内存泄漏检测

```cpp
// 内存泄漏检测工具
#ifdef _DEBUG
class CMemoryLeakDetector
{
private:
    _CrtMemState m_memStateOld;
    _CrtMemState m_memStateNew;
    _CrtMemState m_memStateDiff;
    CString m_strContext;

public:
    CMemoryLeakDetector(const char* pszContext)
    {
        m_strContext = pszContext;
        _CrtMemCheckpoint(&m_memStateOld);
        DEBUG_PRINT("开始内存检测：%s\n", pszContext);
    }

    ~CMemoryLeakDetector()
    {
        _CrtMemCheckpoint(&m_memStateNew);

        if (_CrtMemDifference(&m_memStateDiff, &m_memStateOld, &m_memStateNew)) {
            DEBUG_PRINT("内存泄漏检测 - %s：\n", m_strContext);
            _CrtMemDumpStatistics(&m_memStateDiff);
            _CrtMemDumpAllObjectsSince(&m_memStateOld);
        } else {
            DEBUG_PRINT("内存检测完成 - %s：无内存泄漏\n", m_strContext);
        }
    }
};

#define MEMORY_LEAK_DETECTOR(context) CMemoryLeakDetector detector(context)
#else
#define MEMORY_LEAK_DETECTOR(context)
#endif

// 使用示例
void CDemoVCDlg::OnBnClickedButtonComplexOperation()
{
    MEMORY_LEAK_DETECTOR("复杂操作");

    // 执行可能产生内存泄漏的操作
    std::vector<CString*> stringArray;

    for (int i = 0; i < 100; i++) {
        stringArray.push_back(new CString());
        stringArray.back()->Format("字符串%d", i);
    }

    // 确保正确释放内存
    for (auto* pStr : stringArray) {
        delete pStr;
    }
    stringArray.clear();
}
```

### 本章小结

本章详细介绍了调试技巧和最佳实践。通过学习本章，读者应该：

- 掌握完善的错误处理模式和恢复机制
- 学会使用调试宏和状态监控技术
- 了解代码规范和性能优化的方法
- 掌握常见问题的诊断和解决方案
- 学会使用内存泄漏检测工具

通过本教程的学习，读者应该能够理解工业级运动控制软件的设计思想和实现方法，为今后的开发工作打下坚实的基础。

---

## 教材总结

本教程基于DemoVCDlg.cpp这个真实的项目，系统地介绍了MFC运动控制系统开发的各个方面：

### 学习成果

1. **MFC基础掌握**
   - 理解MFC应用程序架构
   - 掌握消息映射机制
   - 学会界面设计和事件处理

2. **运动控制专业技能**
   - 掌握单轴和坐标系运动控制
   - 理解各种运动模式的实现
   - 学会实时状态监控技术

3. **工业软件开发能力**
   - 掌握IO控制和通信技术
   - 学会错误处理和调试技巧
   - 理解代码规范和优化方法

4. **实际项目经验**
   - 通过真实代码学习最佳实践
   - 掌握工业级软件的设计思想
   - 学会解决实际工程问题

### 后续学习建议

1. **深入学习运动控制算法**
   - PID控制算法
   - 运动轨迹规划
   - 高级插补算法

2. **扩展硬件知识**
   - 伺服电机原理
   - 编码器技术
   - 传感器应用

3. **学习其他开发框架**
   - Qt框架
   - C# WinForms/WPF
   - Web界面开发

4. **参与实际项目**
   - 从简单的单轴控制开始
   - 逐步增加功能复杂度
   - 积累实际工程经验

希望本教程能够帮助读者在运动控制领域取得进步，成为一名优秀的工业软件开发工程师。