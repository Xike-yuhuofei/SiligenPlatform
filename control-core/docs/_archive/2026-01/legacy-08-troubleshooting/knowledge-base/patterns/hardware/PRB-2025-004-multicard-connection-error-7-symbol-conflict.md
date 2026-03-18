# PRB-2025-004: MultiCard连接错误码7 - 符号冲突和参数错误

**问题ID**: PRB-2025-004
**创建日期**: 2025-12-09
**类别**: hardware/connection
**严重程度**: high
**状态**: 已解决

## 问题概述

### 症状
使用厂商SDK静态链接后，调用MC_Open连接控制卡时返回错误码7（MC_COM_ERR_TIME_OUT = 控制卡无响应）。厂商测试软件能正常连接，说明硬件正常，问题出在软件实现。

### 影响
- 无法连接MultiCard控制卡
- 阻塞所有硬件功能测试
- 影响整个运动控制系统开发

## 环境信息

### 软件环境
- **OS**: Windows 10 MSYS_NT-10.0-26200
- **编译器**: MSVC 2019 (通过CMake)
- **CMake版本**: 3.x
- **Qt版本**: 6.7.3

### 硬件环境
- **控制卡**: MultiCard运动控制卡
- **网络配置**:
  - 控制卡IP: 192.168.0.1
  - PC IP: 192.168.0.200
- **厂商SDK**: MultiCard.dll + MultiCard.lib (静态链接)

### 相关文件
- `src/infrastructure/hardware/MultiCardAdapter.cpp:64-75` - MC_Open调用
- `src/infrastructure/hardware/MultiCardStub.cpp` - 旧的动态加载实现（冲突源）
- `src/infrastructure/hardware/MultiCardStub_Class.cpp` - 新的静态链接存根
- `src/infrastructure/CMakeLists.txt:14-20` - 构建配置
- `config/machine_config.ini:96-106` - 网络配置

## 根本原因分析

### 1. 符号冲突（主要问题）

**直接原因**：
构建系统同时编译了两个冲突的存根文件：
- `MultiCardStub.cpp` - 旧的动态加载实现（尝试GetProcAddress）
- `MultiCardStub_Class.cpp` - 新的静态链接存根（返回-999）

链接器优先选择本地编译的符号，导致程序使用了动态加载实现。

**发现过程**：
```bash
# 日志显示使用了动态加载存根
[MultiCardStub] MC_Open not available

# 发现两个.obj文件同时存在
build/src/infrastructure/siligen_hardware.dir/Release/MultiCardStub.obj
build/src/infrastructure/siligen_hardware.dir/Release/MultiCardStub_Class.obj
```

**深层原因**：
- 从动态加载方案迁移到静态链接时，未删除旧代码
- CMakeLists.txt只明确添加了MultiCardStub_Class.cpp，但旧文件仍存在于源目录
- MSBuild可能自动编译了目录中的所有.cpp文件

### 2. 连接参数错误（次要问题）

**对比厂商示例代码**：

```cpp
// 厂商示例 (DemoVCDlg.cpp:862)
g_MultiCard.MC_StartDebugLog(0);
iRes = g_MultiCard.MC_Open(
    1,                  // 卡号从1开始
    "192.168.0.200",   // PC IP
    60000,             // PC端口
    "192.168.0.1",     // 控制卡IP
    60000              // 控制卡端口（必须与PC端口一致）
);

// 我们的错误实现
multicard_->MC_Open(
    0,                  // 错误：卡号从0开始
    local_ip,
    1025,              // 错误：使用了错误的端口
    card_ip,
    1025
);
// 缺失：未调用MC_StartDebugLog
```

**参数差异**：
1. **卡号**: 应从1开始，不是0
2. **端口**: 厂商建议从60000起始，我们使用的1025可能不正确
3. **调试日志**: 厂商示例中连接前调用MC_StartDebugLog(0)

## 解决方案

### 修复步骤

#### 1. 删除符号冲突源
```bash
# 重命名旧的动态加载文件
mv src/infrastructure/hardware/MultiCardStub.cpp \
   src/infrastructure/hardware/MultiCardStub.cpp.old_dynamic_loading

# 删除旧的.obj文件
rm -rf build/src/infrastructure/siligen_hardware.dir/Release/MultiCardStub.obj
```

#### 2. 修正连接参数

**修改 MultiCardAdapter.cpp**:
```cpp
// 创建MultiCard对象
multicard_ = std::make_unique<MultiCard>();

// 启动调试日志（厂商示例建议）
multicard_->MC_StartDebugLog(0);

// MC_Open参数说明（参考厂商示例）：
// - 卡号从1开始（不是0）
// - PC端和控制卡端口必须一致
// - 建议使用60000起始端口
short result = multicard_->MC_Open(
    1,  // nCardNum - 卡号，从1开始
    local_ip,
    static_cast<unsigned short>(config.port),
    card_ip,
    static_cast<unsigned short>(config.port)
);
```

**修改 machine_config.ini**:
```ini
[Network]
# 控制卡IP地址
control_card_ip=192.168.0.1
# 本机IP地址
local_ip=192.168.0.200
# 控制卡端口 (0=系统自动分配, 或60000起始)
control_card_port=0
# 本机端口 (必须与控制卡端口一致)
local_port=0
# 连接超时 (ms)
timeout_ms=5000
```

**端口配置说明**:
- **端口0**: 系统自动分配，避免端口占用冲突（推荐）
- **60000+**: 厂商示例使用的端口，如60000被占用可尝试端口0
- **PC端和控制卡端口必须保持一致**

#### 3. 重新编译
```bash
cd build
cmake --build . --config Release --target siligen_hardware --clean-first
cmake --build . --config Release --target siligen_gui
```

### 验证方法
1. 启动新编译的siligen_gui.exe
2. 日志中不应出现"[MultiCardStub] MC_Open not available"
3. 点击"连接控制卡"按钮
4. 应返回错误码0（连接成功）

### 验证结果
```
连接成功（错误码：0）
```

## 技术要点

### MultiCard SDK链接方式

**厂商SDK的导出方式**：
- MultiCard.dll导出C++类（不是纯C函数）
- MultiCard.lib是导入库（137KB，包含符号引用信息）
- 应使用静态链接方式（编译时链接.lib，运行时加载.dll）

**链接器符号优先级**：
```
本地编译的.obj文件 > 外部.lib文件
```

因此，本地定义的MultiCard类实现会覆盖厂商.lib中的实现。

### 正确的混合链接模式

```cpp
// MultiCardStub_Class.cpp - 正确的实现

// C++类：完全不定义，由厂商MultiCard.lib提供
// （不要定义MultiCard::构造函数、MC_Open等任何成员函数）

// C API存根：可以定义，用于测试代码
#ifdef __cplusplus
extern "C" {
#endif

short MC_PrfJog(short cardNum, short axis) {
    fprintf(stderr, "[MultiCard Stub] MC_PrfJog stub (C API)\n");
    return -999;
}
// ... 其他C API存根

#ifdef __cplusplus
}
#endif
```

**关键区别**：
```cpp
// C++类成员函数（厂商SDK，用于MultiCardAdapter）
class MultiCard {
    int MC_PrfJog(short axis);  // 由MultiCard.lib提供
};

// 纯C函数（存根，用于HardwareTestAdapter）
extern "C" {
    short MC_PrfJog(short cardNum, short axis);  // 不同的符号
}
```

### MC_Open连接参数规范

根据厂商示例代码和文档：

1. **卡号（nCardNum）**:
   - 从1开始计数
   - 多卡系统中每个卡使用不同卡号（1, 2, 3...）

2. **端口号**:
   - PC端和控制卡端必须一致
   - 建议从60000起始
   - 多卡系统建议递增（60000, 60001, 60002...）

3. **调试日志**:
   - 连接前调用MC_StartDebugLog(0)
   - 有助于诊断连接问题

4. **错误码**:
   ```cpp
   #define MC_COM_SUCCESS           (0)   // 执行成功
   #define MC_COM_ERR_TIME_OUT      (-7)  // 无响应
   #define MC_COM_ERR_CARD_OPEN_FAIL (-6) // 打开失败
   ```

## 预防措施

### 代码层面
1. **清理旧代码**: 重构时彻底删除废弃文件，不要仅重命名
2. **明确构建规则**: 在CMakeLists.txt中明确列出所有源文件，避免通配符
3. **符号隔离**: 使用命名空间或前缀区分不同实现
4. **参考厂商示例**: 连接参数严格按厂商示例设置

### 流程层面
1. **架构迁移检查清单**:
   - [ ] 删除旧实现文件
   - [ ] 清理构建目录
   - [ ] 验证符号链接
   - [ ] 对比厂商示例

2. **连接参数验证**:
   - 对比厂商示例代码
   - 检查IP和端口配置
   - 验证卡号设置

### 监控机制
1. **启动日志检查**: 观察是否有"Stub"相关日志
2. **错误码映射**: 实现完整的MultiCard错误码到可读消息的映射
3. **连接诊断**: 提供详细的连接失败诊断信息

## 相关问题

- PRB-2025-001: MultiCard动态加载无网络活动
- PRB-2025-002: MultiCard C++导出与动态加载冲突
- PRB-2025-003: MultiCard静态链接链接器错误

## 参考资料

### 厂商示例代码
```
C:\Users\xike\我的云端硬盘\Document\BopaiMotionController\MultiCardDemoVC\DemoVC\DemoVCDlg.cpp:860-880
```

关键代码段：
```cpp
void CDemoVCDlg::OnBnClickedButton7() {
    int iRes = 0;

    g_MultiCard.MC_StartDebugLog(0);
    // 5个参数依次为板卡号、PC端IP地址、PC端端口号，板卡端IP地址，板卡端口号
    // PC端端口号和板卡端口号要保持一致

    iRes = g_MultiCard.MC_Open(1,"192.168.0.200",60000,"192.168.0.1",60000);

    if(iRes) {
        MessageBox("Open Card Fail,Please turn off wifi ,check PC IP address or connection!");
    } else {
        g_MultiCard.MC_Reset();
        MessageBox("Open Card Successful!");
    }
}
```

### 技术文档
- MultiCard API头文件: `infrastructure/hardware/MultiCardCPP.h`
- 错误码定义: MultiCardCPP.h:17-24

## 经验总结

### 问题诊断方法
1. **日志分析**: 通过stderr/stdout日志识别使用了哪个实现
2. **符号检查**: 检查构建目录中的.obj文件
3. **对比验证**: 与能正常工作的厂商软件对比参数

### 架构迁移教训
1. 重构时必须彻底删除旧代码，不能留有残留
2. 验证链接器实际使用的符号来源
3. 严格遵循厂商API使用规范

### 最佳实践
1. **参考官方示例**: 连接参数、调用顺序、错误处理都应参考厂商示例
2. **符号命名规范**: C API和C++类API使用不同的命名或签名
3. **构建验证**: 重大重构后验证链接器行为是否符合预期

---

**修复完成日期**: 2025-12-09
**验证人**: AI Assistant + 用户测试
**修复提交**: [构建输出确认连接成功]
