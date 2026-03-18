# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 项目概述

这是一个基于 MFC 的多轴运动控制卡演示程序(DemoVC),用于通过以太网与运动控制卡通信,实现多轴运动控制、IO 控制、插补运动、回零等功能。

### 项目结构
```
MultiCard_DemoVC_opt/
├── DemoVC.sln              # Visual Studio 解决方案文件
├── bin/                    # 编译输出目录 (Debug版本)
│   ├── DemoVC.exe          # Debug版本可执行文件
│   ├── MultiCard.dll       # 运动控制库 (必须)
│   ├── MultiCard.lib       # Debug库文件
│   └── [MFC运行时库]       # 调试依赖
├── DemoVC/                 # 主要源代码目录
│   ├── DemoVC.h/cpp        # 应用程序主类
│   ├── DemoVCDlg.h/cpp     # 主对话框类 (核心UI和业务逻辑)
│   ├── MultiCardCPP.h      # 运动控制卡API接口定义 (1024行)
│   ├── stdafx.h/cpp        # MFC预编译头文件
│   ├── resource.h          # 资源ID定义
│   ├── DemoVC.rc           # 资源文件 (对话框、菜单、图标)
│   ├── DemoVC.vcxproj      # 项目文件
│   ├── Debug/              # Debug编译输出 (可删除)
│   ├── Release/            # Release编译输出 (可删除)
│   └── res/                # 资源文件夹
│       └── DemoVC.ico      # 应用程序图标
└── MultiCard/              # MultiCard库文件目录
    ├── MultiCard.h         # 库头文件
    ├── MultiCard.lib       # Release库文件
    └── ReadMe.txt          # 版本和使用说明
```

## 构建和运行

### 编译环境
- Visual Studio 2010 或更高版本
- 需要 MFC 支持
- 平台配置:Win32 和 x64
- 配置:Debug 和 Release

### 编译命令

**使用 PowerShell（推荐）:**
```powershell
# 打开解决方案
Invoke-Item DemoVC.sln

# 构建所有配置
msbuild DemoVC.sln /p:Configuration=Debug /p:Platform=Win32 /v:minimal /t:Rebuild
msbuild DemoVC.sln /p:Configuration=Release /p:Platform=Win32 /v:minimal /t:Rebuild
msbuild DemoVC.sln /p:Configuration=Debug /p:Platform=x64 /v:minimal /t:Rebuild
msbuild DemoVC.sln /p:Configuration=Release /p:Platform=x64 /v:minimal /t:Rebuild

# 快速构建（增量编译）
msbuild DemoVC.sln /p:Configuration=Debug /p:Platform=Win32 /v:minimal
msbuild DemoVC.sln /p:Configuration=Release /p:Platform=Win32 /v:minimal

# 并行构建（多核加速）
msbuild DemoVC.sln /p:Configuration=Debug /p:Platform=Win32 /v:minimal /m
msbuild DemoVC.sln /p:Configuration=Release /p:Platform=Win32 /v:minimal /m

# 清理所有配置
msbuild DemoVC.sln /t:Clean /p:Configuration=Debug /p:Platform=Win32
msbuild DemoVC.sln /t:Clean /p:Configuration=Release /p:Platform=Win32
msbuild DemoVC.sln /t:Clean /p:Configuration=Debug /p:Platform=x64
msbuild DemoVC.sln /t:Clean /p:Configuration=Release /p:Platform=x64

# 清理全部配置
msbuild DemoVC.sln /t:Clean

# 重新构建全部
msbuild DemoVC.sln /t:Rebuild

# 构建指定项目
msbuild DemoVC/DemoVC.vcxproj /p:Configuration=Debug /p:Platform=Win32

# 输出详细日志
msbuild DemoVC.sln /p:Configuration=Debug /p:Platform=Win32 /v:detailed

# 输出到指定目录
msbuild DemoVC.sln /p:Configuration=Debug /p:Platform=Win32 /p:OutDir=..\build\debug\

# 检查项目依赖关系
msbuild DemoVC.sln /t:ShowDependencies
```

**使用 Visual Studio IDE:**
1. 双击 `DemoVC.sln` 打开解决方案
2. 选择配置：Debug/Release 和 Win32/x64 平台
3. 右键项目 → 生成 或按 `Ctrl+Shift+B`
4. 批量构建：右键解决方案 → 批量生成...
5. 清理：右键项目/解决方案 → 清理

### 运行
编译后的可执行文件位于:
- Debug 版本: `bin\DemoVC.exe`
- Release 版本: `DemoVC\Release\DemoVC.exe`

**依赖文件要求:**
- `MultiCard.dll` - 运动控制库（必须与可执行文件同目录）
- MFC 运行时库（通常系统已安装）：
  - `mfc100d.dll`, `msvcp100d.dll`, `msvcr100d.dll` (Debug)
  - `mfc100.dll`, `msvcp100.dll`, `msvcr100.dll` (Release)

## 核心架构

### 项目文件结构
```
DemoVC/
├── DemoVC.h/cpp          # 应用程序主类 CDemoVCApp
├── DemoVCDlg.h/cpp       # 主对话框类 CDemoVCDlg - 核心UI和业务逻辑
├── MultiCardCPP.h        # 运动控制卡接口类定义（核心API）
├── stdafx.h/cpp          # MFC预编译头文件
├── resource.h            # 资源ID定义
├── DemoVC.rc             # 资源文件（对话框、控件等）
├── DemoVC.vcxproj        # Visual Studio项目文件
└── res/                  # 资源文件夹
    └── DemoVC.ico        # 应用程序图标
```

### 主要组件

1. **MultiCard 类** (`MultiCardCPP.h`)
   - 运动控制卡的核心接口类
   - 提供所有运动控制、IO 控制、通信功能
   - 通过以太网与控制卡通信
   - 支持多卡同时控制(通过不同实例)

2. **全局控制卡实例** (`DemoVCDlg.cpp`)
   ```cpp
   MultiCard g_MultiCard;   // 第一张控制卡
   MultiCard g_MultiCard2;  // 第二张控制卡
   MultiCard g_MultiCard3;  // 第三张控制卡
   ```

3. **主对话框** (`CDemoVCDlg`)
   - 应用程序的主界面
   - 实现所有用户交互和控制逻辑
   - 使用定时器 (`OnTimer`) 周期性更新状态显示
   - 事件处理机制：
     - `OnBnClickedButton*` - 按钮点击事件
     - `OnCbnSelchange*` - 下拉框选择事件
     - `OnTimer` - 定时器回调（每100ms更新状态）
     - `PreTranslateMessage` - 按键按下/松开事件处理
   - 主要功能模块：
     - **连接管理** - 打开/关闭控制卡连接，多卡控制
     - **单轴运动控制** - 点位运动、JOG模式、编码器控制
     - **坐标系插补运动控制** - 直线插补、圆弧插补、前瞻缓冲区
     - **IO控制** - 通用输入输出、限位、报警、数字输出控制
     - **回零控制** - 多种回零方式和参数配置
     - **比较输出控制** - 位置比较触发输出脉冲
     - **手轮模式控制** - 手轮轴选和倍率控制
     - **串口通信** - UART配置和数据收发
     - **实时状态监控** - 轴位置、状态、IO信号实时显示
     - **多卡控制** - 支持最多3张控制卡同时控制

### 关键功能模块

1. **通信连接**
   - `MC_Open()` - 打开控制卡连接,需要指定 PC 和控制卡的 IP 地址和端口
   - 默认配置: PC IP `192.168.0.200`, 控制卡 IP `192.168.0.1`, 端口 `60000`
   - 多卡时需要使用不同的端口号(60000, 60001, 60002...)

2. **运动控制模式**
   - **点位运动模式** (Trap) - 梯形速度曲线的点对点运动
   - **JOG 模式** - 连续运动,按键按下时运动,松开时停止
   - **插补运动模式** - 多轴协调运动(直线插补、圆弧插补、螺旋插补等)
   - **齿轮模式** (Gear) - 主从轴跟随运动
   - **凸轮模式** (Cam) - 按凸轮表运动
   - **手轮模式** - 通过手轮控制轴运动

3. **坐标系管理**
   - 支持最多 16 个坐标系
   - 每个坐标系可包含多个轴
   - 前瞻缓冲区用于平滑插补运动
   - 坐标系参数结构: `TCrdPrm` / `TCrdPrmEx`

4. **IO 控制**
   - 通用输入 (GPI) - 16 路数字输入
   - 通用输出 (GPO) - 16 路数字输出
   - 硬件限位输入 - 正负限位检测
   - 报警输入 - 伺服报警信号
   - 支持主卡和扩展卡 IO

5. **比较输出功能**
   - 位置比较触发输出脉冲
   - 用于激光打标、飞行打标等应用
   - 支持缓冲比较数据

### 数据结构

**运动参数结构**
- `TTrapPrm` - 梯形曲线参数(加减速度、起始速度、平滑时间)
- `TJogPrm` - JOG 参数(加减速度、平滑系数)
- `TAxisHomePrm` - 回零参数(回零方式、方向、速度等)

**坐标系结构**
- `TCrdPrm` - 坐标系参数(维度、轴映射、最大速度和加速度)
- `TLookAheadPrm` - 前瞻参数(前瞻段数、最大速度、加速度、缩放比例)

**状态结构**
- `TAllSysStatusDataSX` - 系统完整状态(16 轴位置、状态、IO 状态等)

### 关键常量

**轴状态位** (`AXIS_STATUS_*`)
- `AXIS_STATUS_ENABLE` - 轴使能状态
- `AXIS_STATUS_RUNNING` - 规划运动中
- `AXIS_STATUS_ARRIVE` - 到达目标位置
- `AXIS_STATUS_POS_HARD_LIMIT` - 正向硬限位触发
- `AXIS_STATUS_NEG_HARD_LIMIT` - 负向硬限位触发
- `AXIS_STATUS_SV_ALARM` - 伺服报警

**坐标系状态位** (`CRDSYS_STATUS_*`)
- `CRDSYS_STATUS_PROG_RUN` - 程序运行中
- `CRDSYS_STATUS_FIFO_FINISH_0` - FIFO-0 缓冲区执行完成
- `CRDSYS_STATUS_ALARM` - 坐标系报警

### 核心功能模块详细说明

#### 1. 系统连接和管理模块
- **连接函数**: `OnBnClickedButton7()` - 建立以太网连接
- **关闭函数**: `OnBnClickedButton9()` - 关闭连接
- **复位函数**: `OnBnClickedButton8()` - 复位控制卡
- **多卡支持**: 全局多个 `MultiCard` 实例，每个使用不同端口

#### 2. 实时状态监控模块 (OnTimer核心)
- **更新频率**: 每100ms执行一次
- **监控内容**:
  - 轴规划位置和编码器位置显示
  - 轴状态监控（运行中、报警、限位、回零）
  - 16路通用输入信号状态显示
  - 8路正负硬限位状态显示
  - 手轮轴选和倍率信号显示
  - 比较输出缓冲区状态
  - 坐标系位置和运行状态
  - 缓冲区剩余空间监控

#### 3. 单轴运动控制模块
- **点位运动**: `OnBnClickedButton6()` - 梯形速度曲线点对点运动
- **JOG连续运动**:
  - `OnBnClickedButton5()` - 正向连续运动
  - `PreTranslateMessage()` - 处理按键按下/松开事件
- **轴使能控制**: `OnBnClickedButtonAxisOn()` - 伺服使能/断开
- **位置清零**: `OnBnClickedButton4()` - 当前位置清零
- **状态清除**: `OnBnClickedButtonClrSts()` - 清除轴状态

#### 4. 坐标系插补运动模块
- **坐标系配置**:
  - `OnBnClickedButtonSetCrdPrm()` - 坐标系1参数设置
  - `OnBnClickedButtonSetCrdPrm2()` - 坐标系2参数设置
- **插补数据写入**: `OnBnClickedButton10()` - 各种插补指令（直线、圆弧等）
- **运动控制**:
  - `OnBnClickedButton11()` - 启动坐标系运动
  - `OnBnClickedButton12()` - 停止坐标系运动
- **速度覆盖**: 滑块控制实时速度调整

#### 5. IO控制模块
- **数字输出**: `OnBnClickedButtonOutPut1-16()` - 16路数字输出控制
- **限位使能**:
  - `OnBnClickedCheckPosLimEnable()` - 正限位开关控制
  - `OnBnClickedCheckNegLimEnable()` - 负限位开关控制
- **编码器控制**: `OnBnClickedCheckNoEncode()` - 编码器开关

#### 6. 手轮控制模块
- **模式切换**:
  - `OnBnClickedButtonEnterHandwhell()` - 进入手轮模式
  - `OnBnClickedButton14()` - 退出手轮模式
- **轴选逻辑**: 在OnTimer中实现X/Y/Z/A/B轴选切换
- **倍率控制**: X1/X10/X100倍率自动识别和切换

#### 7. 回零控制模块
- **回零启动**: `OnBnClickedButtonStartHome()` - 配置回零参数并启动
- **回零停止**: `OnBnClickedButtonStopHome()` - 停止回零过程
- **回零方式**: 支持HOME回零、HOME+Index回零、Z脉冲回零

#### 8. 比较输出功能模块
- **调试输出**: 强制输出高电平、低电平、脉冲
- **比较数据**: `OnBnClickedButtonCmpData()` - 写入位置比较数据
- **状态监控**: 比较缓冲区剩余数据量和状态

#### 9. 串口通信模块
- **串口配置**: `OnBnClickedButtonUartSet()` - 波特率、数据位、校验位配置
- **数据发送**: `OnBnClickedButtonUartSend()` - 发送字符串数据
- **数据接收**: `OnBnClickedButtonUartRec()` - 接收并显示数据

## 重要开发注意事项

### 编码问题
- **源代码编码**: GBK/GB2312（中文注释）
- **读取时注意**: 在 UTF-8 环境下可能出现乱码，建议使用支持GBK的编辑器打开
- **修改时注意**: 保持编码一致性，避免混合使用不同编码

### 控制卡通信配置
**网络设置步骤：**
1. 配置PC以太网适配器为静态IP（如 `192.168.0.200`）
2. 确保控制卡IP地址正确设置（如 `192.168.0.1`）
3. 关闭防火墙或添加端口例外（默认端口60000-60002）
4. 建议使用有线网络连接，禁用WiFi避免干扰
5. 通过 `ping` 命令测试网络连通性

**通信流程：**
- 必须先调用 `MC_Open()` 建立连接
- 每个控制卡需要唯一的端口号（第一张卡60000，第二张卡60001...）
- 通信失败时检查：网络连接、IP地址配置、防火墙设置、DLL版本匹配

### 运动控制流程

**单轴点位运动**:
```cpp
MC_PrfTrap(axis);           // 设置为梯形模式
MC_SetTrapPrm(axis, &prm);  // 设置加减速参数
MC_SetPos(axis, targetPos); // 设置目标位置
MC_SetVel(axis, vel);       // 设置速度
MC_Update(axisMask);        // 启动运动
```

**坐标系插补运动**:
```cpp
MC_SetCrdPrm(crd, &crdPrm);        // 配置坐标系
MC_InitLookAhead(crd, 0, &lookPrm); // 初始化前瞻
MC_LnXY(crd, x, y, vel, acc, ...);  // 添加直线插补数据
MC_ArcXYC(crd, x, y, cx, cy, ...);  // 添加圆弧插补数据
MC_CrdData(crd, NULL, 0);           // 结束标识
MC_CrdStart(crdMask, 0);            // 启动坐标系运动
```

### 库文件链接
- Debug 版本链接 `bin\MultiCard.lib`
- 通过预处理器自动选择正确的库

### 定时器更新
- 主对话框使用 100ms 定时器更新状态（`SetTimer(0, 100, NULL)`）
- 定时器回调函数 `OnTimer()` 中读取控制卡状态并更新界面显示
- 避免在定时器中执行耗时操作
- 状态读取函数：`MC_GetAllSysStatus()` 或 `MC_GetAllSysStatusSX()`

### 调试和故障排除

**常见编译错误：**
- 缺少MFC支持：安装Visual Studio时需要选择"使用C++的桌面开发"工作负载
- 头文件找不到：检查项目包含目录配置
- 链接错误：确保正确链接 `MultiCard.lib`（Debug或Release版本）

**运行时问题排查：**
1. **连接失败**：
   - 检查网络配置和IP地址设置
   - 使用 `ping` 测试控制卡连通性
   - 确认防火墙未阻止端口
   - 验证 `MultiCard.dll` 版本与控制卡固件匹配

2. **运动不平滑**：
   - 调整前瞻参数（`lookAheadNum`）
   - 优化加速度参数（`dAccMax`）
   - 检查网络延迟和丢包

3. **DLL找不到**：
   - 确保 `MultiCard.dll` 在可执行文件同目录
   - 或将DLL路径添加到系统PATH环境变量

4. **中文乱码**：
   - 源文件使用GBK编码打开
   - 编辑器设置为GBK/GB2312编码

## 常见问题

1. **连接失败** - 检查网络配置和 IP 地址设置，使用 ping 命令测试连通性
2. **运动不平滑** - 调整前瞻参数和加速度参数，检查网络稳定性
3. **DLL 找不到** - 确保 MultiCard.dll 在可执行文件同目录
4. **中文乱码** - 注意源文件编码，建议使用 GBK 编码打开
5. **编译失败** - 确认已安装 MFC 支持和正确的 Visual Studio 工作负载
6. **运动不响应** - 检查轴是否使能（`MC_AxisOn()`），查看错误状态位
7. **坐标系运动失败** - 确认坐标系参数配置正确，缓冲区空间足够

## 版本和环境信息

**当前软件版本:**
- MultiCard.dll: V2.1.1.8
- 控制卡固件: 必须与DLL版本匹配
- 开发环境: Visual Studio 2010 (兼容更高版本)

**系统要求:**
- 操作系统: Windows XP/7/8/10/11 (32位和64位)
- 网络: 以太网接口，支持TCP/IP
- 硬件: 多轴运动控制卡（型号特定）

## 开发工作流程

### 新功能开发步骤

#### 1. UI界面设计
1. **资源编辑**: 在Visual Studio资源编辑器中添加控件（按钮、编辑框、下拉框等）
2. **控件ID命名**: 使用有意义的ID，如 `IDC_BUTTON_AXIS_ON`、`IDC_EDIT_TARGET_POS`
3. **控件布局**: 合理布局控件，确保用户界面友好易用

#### 2. 代码实现
1. **头文件声明** (`DemoVCDlg.h`):
   - 添加控件成员变量：`CButton m_ButtonAxisOn;`
   - 添加事件处理函数声明：`afx_msg void OnBnClickedButtonAxisOn();`
   - 添加数据成员变量：`long m_lTargetPos;`

2. **消息映射** (`DemoVCDlg.cpp`):
   - 在 `BEGIN_MESSAGE_MAP` 中添加消息映射：`ON_BN_CLICKED(IDC_BUTTON_AXIS_ON, &CDemoVCDlg::OnBnClickedButtonAxisOn)`

3. **数据绑定** (`DoDataExchange`):
   - 添加控件数据绑定：`DDX_Control(pDX, IDC_BUTTON_AXIS_ON, m_ButtonAxisOn);`
   - 添加变量绑定：`DDX_Text(pDX, IDC_EDIT1, m_lTargetPos);`

4. **事件处理函数实现**:
   ```cpp
   void CDemoVCDlg::OnBnClickedButtonAxisOn()
   {
       int iRes = 0;
       int iAxisNum = m_ComboBoxAxis.GetCurSel()+1;

       // 调用MultiCard API
       iRes = g_MultiCard.MC_AxisOn(iAxisNum);

       // 错误处理
       if (iRes != 0) {
           AfxMessageBox("轴使能失败！");
           return;
       }

       // 更新UI状态
       GetDlgItem(IDC_BUTTON_AXIS_ON)->SetWindowText(_T("断开使能"));
   }
   ```

5. **状态监控** (在 `OnTimer()` 中):
   - 添加相关状态的读取和显示逻辑
   - 避免在定时器中执行耗时操作

#### 3. 调试和测试
1. **单元测试**: 分别测试各个功能模块
2. **集成测试**: 测试模块间的交互
3. **边界测试**: 测试异常情况和边界值
4. **硬件测试**: 连接实际硬件进行测试

### 调试流程

#### 1. 编译调试
- **Debug模式**: 包含调试信息，便于断点调试
- **Release模式**: 优化版本，用于最终测试
- **日志输出**: 使用 `TRACE()` 宏输出调试信息

#### 2. 常见调试技巧
- **断点调试**: 在关键函数设置断点，观察变量值
- **状态监控**: 检查轴状态位和错误码
- **网络调试**: 使用 `ping` 命令测试网络连通性
- **API调试**: 检查所有API调用的返回值

#### 3. 错误排查流程
1. **检查连接**: 确认控制卡连接正常
2. **检查状态**: 查看轴状态和错误标志
3. **检查参数**: 确认运动参数设置合理
4. **检查权限**: 确认轴已使能，限位未触发

### 代码规范

#### 1. 命名规范
- **变量命名**: 使用有意义的名称，遵循驼峰命名法
- **全局变量**: 使用 `g_` 前缀，如 `g_MultiCard`
- **成员变量**: 使用 `m_` 前缀，如 `m_lTargetPos`
- **函数命名**: 使用动词开头，如 `OnBnClickedButtonAxisOn`

#### 2. 错误处理
- **API调用**: 所有MultiCard API调用都应检查返回值
- **用户提示**: 使用 `AfxMessageBox()` 给用户友好的错误提示
- **状态检查**: 定时器中检查系统状态，及时发现问题

#### 3. 注释规范
- **中文注释**: 保持中文注释风格，使用GBK编码
- **函数注释**: 在复杂函数前添加功能说明
- **参数说明**: 对关键参数添加注释说明

#### 4. 界面更新规范
- **线程安全**: 避免在非UI线程中直接操作界面控件
- **状态同步**: 确保界面显示与实际状态同步
- **用户体验**: 提供及时的用户反馈

### 性能优化

#### 1. 定时器优化
- **避免耗时操作**: OnTimer中避免执行耗时操作
- **合理更新频率**: 根据需要调整定时器间隔
- **条件更新**: 只在状态变化时更新界面

#### 2. 内存管理
- **及时释放**: 动态分配的内存及时释放
- **避免内存泄漏**: 检查指针和对象的生命周期

#### 3. 网络通信优化
- **批量操作**: 尽量批量发送命令，减少通信次数
- **错误重试**: 实现网络通信的重试机制

## 技术细节补充

### 网络配置详细说明

#### IP地址配置
- **PC端IP**: `192.168.0.200` (推荐)
- **控制卡IP**: `192.168.0.1` (主卡), `192.168.0.2` (扩展卡1)...
- **端口号**: 60000 (主卡), 60001 (扩展卡1), 60002 (扩展卡2)...

#### 网络设置步骤
1. **配置PC网络适配器**:
   ```powershell
   # 查看网络适配器
   Get-NetAdapter

   # 设置静态IP地址
   New-NetIPAddress -InterfaceAlias "以太网" -IPAddress 192.168.0.200 -PrefixLength 24
   ```

2. **测试网络连通性**:
   ```cmd
   ping 192.168.0.1
   ping 192.168.0.2
   ```

3. **防火墙配置**:
   ```powershell
   # 添加防火墙规则
   New-NetFirewallRule -DisplayName "MultiCard" -Direction Inbound -Protocol UDP -LocalPort 60000-60002 -Action Allow
   ```

### 多线程编程注意事项

#### 定时器线程安全
- **UI更新**: 所有UI控件操作必须在主线程中进行
- **数据同步**: 使用临界区保护共享数据
- **避免阻塞**: 定时器中避免执行耗时操作

#### 示例代码
```cpp
// 安全的UI更新方式
void CDemoVCDlg::UpdateStatusDisplay()
{
    if (GetSafeHwnd() == NULL) return;

    CString strText;
    strText.Format("%d", m_lCurrentPos);
    GetDlgItem(IDC_STATIC_POS)->SetWindowText(strText);
}

// 定时器中的正确处理方式
void CDemoVCDlg::OnTimer(UINT_PTR nIDEvent)
{
    // 快速读取状态
    int iRes = g_MultiCard.MC_GetAllSysStatusSX(&m_StatusData);

    // 简单的状态判断和显示更新
    if (iRes == 0) {
        UpdateStatusDisplay();
    }

    CDialogEx::OnTimer(nIDEvent);
}
```

### 内存管理最佳实践

#### 对象生命周期管理
```cpp
// 在构造函数中初始化
CDemoVCDlg::CDemoVCDlg() : m_lTargetPos(0)
{
    // 初始化成员变量
}

// 在析构函数中清理资源
CDemoVCDlg::~CDemoVCDlg()
{
    // 停止定时器
    KillTimer(1);

    // 关闭控制卡连接
    if (g_MultiCard.IsOpen()) {
        g_MultiCard.MC_Close();
    }
}
```

#### 缓冲区管理
```cpp
// 前瞻缓冲区声明
TCrdData LookAheadBuf[FROCAST_LEN];

// 安全的缓冲区操作
void CDemoVCDlg::InitLookAheadBuffer()
{
    memset(LookAheadBuf, 0, sizeof(LookAheadBuf));

    TLookAheadPrm lookPrm;
    memset(&lookPrm, 0, sizeof(lookPrm));

    lookPrm.lookAheadNum = FROCAST_LEN;
    lookPrm.pLookAheadBuf = LookAheadBuf;

    int iRes = g_MultiCard.MC_InitLookAhead(1, 0, &lookPrm);
}
```

### 错误处理模式

#### 标准错误处理模板
```cpp
void CDemoVCDlg::SafeApiCall()
{
    int iRes = 0;
    int iAxisNum = m_ComboBoxAxis.GetCurSel() + 1;

    // 参数检查
    if (iAxisNum < 1 || iAxisNum > 16) {
        AfxMessageBox("轴号无效！");
        return;
    }

    // API调用
    iRes = g_MultiCard.MC_AxisOn(iAxisNum);

    // 错误处理
    if (iRes != 0) {
        CString strError;
        strError.Format("轴使能失败，错误码：%d", iRes);
        AfxMessageBox(strError);
        return;
    }

    // 成功处理
    UpdateAxisStatus(iAxisNum);
}
```

#### 日志记录
```cpp
// 调试日志输出
#ifdef _DEBUG
#define DEBUG_LOG(fmt, ...) \
    { \
        CString strLog; \
        strLog.Format(fmt, __VA_ARGS__); \
        TRACE(strLog); \
        OutputDebugString(strLog); \
    }
#else
#define DEBUG_LOG(fmt, ...)
#endif

// 使用示例
DEBUG_LOG("轴%d运动到位置%d\n", iAxisNum, lTargetPos);
```

### 状态机设计模式

#### 轴状态管理
```cpp
// 轴状态枚举
enum AXIS_STATE {
    AXIS_STATE_IDLE = 0,
    AXIS_STATE_MOVING = 1,
    AXIS_STATE_HOMING = 2,
    AXIS_STATE_ERROR = 3
};

// 状态管理函数
void CDemoVCDlg::UpdateAxisState(int iAxisNum)
{
    DWORD dwStatus = m_AllSysStatusData.lAxisStatus[iAxisNum-1];

    AXIS_STATE newState = AXIS_STATE_IDLE;

    if (dwStatus & AXIS_STATUS_SV_ALARM) {
        newState = AXIS_STATE_ERROR;
    } else if (dwStatus & AXIS_STATUS_HOME_RUNNING) {
        newState = AXIS_STATE_HOMING;
    } else if (dwStatus & AXIS_STATUS_RUNNING) {
        newState = AXIS_STATE_MOVING;
    }

    // 状态变化处理
    if (m_AxisState[iAxisNum-1] != newState) {
        OnAxisStateChanged(iAxisNum, m_AxisState[iAxisNum-1], newState);
        m_AxisState[iAxisNum-1] = newState;
    }
}
```

### 配置文件管理

#### 参数持久化
```cpp
// 保存配置到注册表
void CDemoVCDlg::SaveSettings()
{
    HKEY hKey;
    if (RegCreateKeyEx(HKEY_CURRENT_USER, _T("Software\\DemoVC"), 0, NULL,
                      REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS) {

        DWORD dwValue = m_lTargetPos;
        RegSetValueEx(hKey, _T("TargetPos"), 0, REG_DWORD, (LPBYTE)&dwValue, sizeof(dwValue));

        RegCloseKey(hKey);
    }
}

// 从注册表读取配置
void CDemoVCDlg::LoadSettings()
{
    HKEY hKey;
    if (RegOpenKeyEx(HKEY_CURRENT_USER, _T("Software\\DemoVC"), 0,
                     KEY_READ, &hKey) == ERROR_SUCCESS) {

        DWORD dwValue = 0;
        DWORD dwSize = sizeof(dwValue);
        if (RegQueryValueEx(hKey, _T("TargetPos"), NULL, NULL,
                           (LPBYTE)&dwValue, &dwSize) == ERROR_SUCCESS) {
            m_lTargetPos = dwValue;
        }

        RegCloseKey(hKey);
    }
}
```

## 硬件接口

- **通信方式**: 以太网 UDP/IP
- **支持轴数**: 最多 16 轴(可扩展到 32 轴)
- **坐标系数**: 最多 16 个
- **IO 端口**: 16 路输入 + 16 路输出(主卡),可通过扩展卡增加
- **编码器**: 支持 AB 相增量编码器
- **脉冲输出**: 脉冲+方向或 AB 相输出
- **模拟量输入**: 支持ADC采样，用于电压监测
- **比较输出**: 2路独立比较输出，支持位置触发
- **手轮接口**: 支持标准手轮信号输入
- **串口通信**: 多路UART，支持RS232/RS485通信
