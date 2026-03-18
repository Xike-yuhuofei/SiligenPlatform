# 配置文件排查方法论

**文档版本**: 1.0
**创建日期**: 2026-01-08
**作者**: Claude Code
**适用范围**: C++项目配置管理调试

---

## 目录

1. [概述](#概述)
2. [阶段1：正向追踪（配置→代码）](#阶段1正向追踪配置代码)
3. [阶段2：反向追踪（代码→配置）](#阶段2反向追踪代码配置)
4. [阶段3：交叉验证](#阶段3交叉验证)
5. [阶段4：静态分析检查清单](#阶段4静态分析检查清单)
6. [阶段5：动态验证](#阶段5动态验证)
7. [常见模式识别](#常见模式识别)
8. [快速检查命令](#快速检查命令)
9. [实战案例](#实战案例)
10. [排查三原则](#排查三原则)

---

## 概述

本方法论用于系统化排查"配置文件不生效"类问题。这类问题通常表现为：
- 配置文件中设置了某个值
- 但运行时行为不符合配置预期
- 代码可能使用了硬编码而非配置

**核心思想**：双向追踪 + 交叉验证
- **正向追踪**：从配置文件到代码使用
- **反向追踪**：从代码参数到配置来源
- **交叉验证**：对比配置值和实际值

---

## 阶段1：正向追踪（配置→代码）

**目标**：确认配置文件是否被代码读取

### 1.1 找到配置文件位置

```bash
# 查找所有ini配置文件
find . -name "*.ini" -type f

# 定位特定配置项
rg "do_bit_index" . --type ini

# 查看配置文件内容
cat src/infrastructure/configs/files/machine_config.ini
```

### 1.2 追踪配置读取

```bash
# 搜索配置键名在代码中的使用
rg "do_bit_index|ValveSupply" src/ --type cpp --type c

# 搜索配置读取API
rg "GetIniInt|GetPrivateProfileInt|AfxGetProfile" src/

# 搜索配置管理器
rg "class.*ConfigManager|class.*ConfigAdapter" src/
```

**关键检查点**：

```cpp
// ✅ 正确：读取配置
int do_bit = config_->GetInt("ValveSupply", "do_bit_index", 0);

// ❌ 错误：硬编码
int do_bit = 0;  // 没有读取配置
```

### 1.3 验证配置加载路径

```bash
# 追踪配置文件如何被加载
rg "machine_config\.ini" src/
rg "LoadConfiguration|ReadConfig" src/ --type cpp

# 查看配置管理器实现
rg "class.*ConfigManager" src/ -A 20
```

---

## 阶段2：反向追踪（代码→配置）

**目标**：确认代码中的参数从哪里来

### 2.1 追踪参数来源

```bash
# 找到适配器的构造函数
rg "ValveAdapter::ValveAdapter|new ValveAdapter" src/ -A 10

# 查看ApplicationContainer如何创建适配器
rg "ValveAdapter" src/application/container/ -B 5 -A 5
```

### 2.2 检查魔法数字

```bash
# 查找可疑的硬编码数字
rg "CallMC_SetExtDoBit.*\b0\b" src/ -B 2 -A 2

# 查找所有字面量0、1、2
rg "\b(0|1|2)\b.*bitIndex|bitIndex.*\b(0|1|2)\b" src/

# 查找硬编码字符串
rg "\"Y0\"|\"Y1\"|\"Y2\"" src/
```

**关键检查点**：

```cpp
// ❌ 疑似硬编码
CallMC_SetExtDoBit(0, 1);  // 这些数字从哪来？

// ✅ 使用配置变量
CallMC_SetExtDoBit(config_.do_card_index, config_.do_bit_index);
```

### 2.3 追溯调用栈

```
ValveAdapter::OpenSupply()
  └─> CallMC_SetExtDoBit(0, 1)  // 参数0来自？
        └─> 检查：是成员变量？参数？还是魔法数字？
```

**验证方法**：
```bash
# 查看成员变量定义
rg "do_bit_index|do_card_index" src/infrastructure/adapters/hardware/ValveAdapter.h

# 查看构造函数初始化
rg "ValveAdapter::ValveAdapter" src/infrastructure/adapters/hardware/ValveAdapter.cpp -A 10
```

---

## 阶段3：交叉验证

**目标**：对比配置值和实际值

### 3.1 创建验证表

| 检查项 | 配置文件 | 代码实际使用 | 一致性 |
|--------|----------|--------------|--------|
| 卡索引 | `do_card_index=0` | `CallMC_SetExtDoBit(0, ...)` | ✅ |
| 位索引 | `do_bit_index=2` | `CallMC_SetExtDoBit(..., 0, ...)` | ❌ **不一致** |
| 输出值 | 无配置 | `CallMC_SetExtDoBit(..., 1)` | N/A |

### 3.2 使用grep批量验证

```bash
# 提取配置文件中的值
rg "do_(card|bit)_index" src/infrastructure/configs/files/machine_config.ini

# 提取代码中使用的值
rg "MC_SetExtDoBit\(" src/infrastructure/adapters/hardware/ValveAdapter.cpp -A 1

# 对比差异
diff <(rg "do_bit_index" configs/) <(rg "bitIndex.*=" src/)
```

---

## 阶段4：静态分析检查清单

使用这个检查清单逐项验证：

```
□ 配置文件是否存在？
  □ 位置：configs/files/machine_config.ini
  □ 格式：[ValveSupply] 段是否存在
  □ 键名：do_bit_index 是否存在

□ 配置是否被读取？
  □ rg "do_bit_index" 是否有匹配？
  □ 配置管理器是否加载该段？
  □ 代码中是否有 GetInt/GetString 调用？

□ 配置是否传递到构造函数？
  □ ValveAdapter构造函数参数是否包含config？
  □ ApplicationContainer创建时是否传入配置？
  □ 适配器是否保存配置到成员变量？

□ 配置是否被使用？
  □ CallMC_SetExtDoBit的参数是变量还是字面量？
  □ 变量的值是否等于配置值？
  □ 是否有配置到API的映射逻辑？

□ 是否有硬编码？
  □ 代码中是否有魔法数字？
  □ 注释和实际值是否一致？
  □ 是否有 #define 或 const 替代配置？

□ 配置路径是否正确？
  □ 相对路径 vs 绝对路径
  □ 运行目录是否正确
  □ 开发环境 vs 生产环境路径差异
```

---

## 阶段5：动态验证

**目标**：运行时验证配置是否生效

### 5.1 添加调试日志

```cpp
// 在适配器构造函数中
ValveAdapter::ValveAdapter(...) {
    std::cout << "[ValveAdapter] Config loaded:" << std::endl;
    std::cout << "  do_card_index = " << config_.do_card_index << std::endl;
    std::cout << "  do_bit_index = " << config_.do_bit_index << std::endl;
}

// 在使用配置的函数中
Result<SupplyValveState> ValveAdapter::OpenSupply() noexcept {
    std::cout << "[ValveAdapter::OpenSupply] Calling MC_SetExtDoBit with: "
              << "cardIndex=" << config_.do_card_index
              << ", bitIndex=" << config_.do_bit_index
              << ", value=1" << std::endl;

    int result = CallMC_SetExtDoBit(config_.do_card_index,
                                     config_.do_bit_index,
                                     1);
}
```

### 5.2 运行时检查

```bash
# 编译并运行
cd build
ninja
./bin/HardwareHttpServer.exe

# 观察日志输出
# 应该看到：
# [ValveAdapter] Config loaded:
#   do_card_index = 0
#   do_bit_index = 2
# [ValveAdapter::OpenSupply] Calling MC_SetExtDoBit with: cardIndex=0, bitIndex=2, value=1
```

### 5.3 使用调试器

```bash
# 使用GDB或LLDB设置断点
gdb ./bin/HardwareHttpServer.exe
(gdb) break ValveAdapter::ValveAdapter
(gdb) break ValveAdapter::OpenSupply
(gdb) run

# 查看变量值
(gdb) print config_.do_bit_index
(gdb) print this->do_card_index_
```

---

## 常见模式识别

### 模式1：配置文件存在但未读取

**症状**：
```bash
rg "config_key" configs/  # 有匹配
rg "config_key" src/      # 无匹配
```

**诊断**：配置存在但从未被代码读取

**修复**：
```cpp
// 添加配置读取代码
auto config = container->Resolve<IConfigurationPort>();
int value = config->GetInt("Section", "config_key", default_value);
```

### 模式2：配置读取但未传递

**症状**：
```bash
rg "config_key" src/application/     # 有匹配（应用层读取了）
rg "config_key" src/infrastructure/  # 无匹配（适配器没收到）
```

**诊断**：配置断层 - 应用层读取了但没传给基础设施层

**修复**：
```cpp
// 修改适配器构造函数接收配置参数
ValveAdapter(hardware, config);

// 修改ApplicationContainer传递配置
auto valveAdapter = std::make_shared<ValveAdapter>(hardware, config);
```

### 模式3：配置存在但有硬编码

**症状**：
```bash
rg "MC_SetExtDoBit.*\b\d+\b" src/  # 字面量参数
rg "MC_SetExtDoBit.*config_" src/  # 无匹配（不是变量）
```

**诊断**：硬编码覆盖了配置

**修复**：
```cpp
// 替换硬编码为配置变量
// ❌ CallMC_SetExtDoBit(0, 1);
// ✅ CallMC_SetExtDoBit(config_.do_card_index, config_.do_bit_index);
```

### 模式4：注释误导

**症状**：
```cpp
// 注释说Y2
std::cout << "打开供胶阀 Y2" << std::endl;

// 实际控制Y0
CallMC_SetExtDoBit(0, 1);  // bitIndex=0 是Y0，不是Y2
```

**诊断**：注释与代码不一致

**修复**：更新注释或修正代码

---

## 快速检查命令

### 一键检查脚本

```bash
#!/bin/bash
# config-check.sh - 快速检查配置是否被使用

echo "=== 配置文件排查脚本 ==="
echo ""

CONFIG_KEY="do_bit_index"
CONFIG_SECTION="ValveSupply"

echo "1️⃣  检查配置文件..."
rg "$CONFIG_KEY" src/infrastructure/configs/ --type ini
if [ $? -ne 0 ]; then
    echo "❌ 配置文件中未找到 $CONFIG_KEY"
    exit 1
fi
echo "✅ 配置文件存在"
echo ""

echo "2️⃣  检查配置是否被读取..."
rg "$CONFIG_KEY" src/ --type cpp
if [ $? -ne 0 ]; then
    echo "❌ 代码中未找到 $CONFIG_KEY"
    echo "⚠️  配置未被读取！"
else
    echo "✅ 代码中找到配置引用"
fi
echo ""

echo "3️⃣  检查构造函数..."
rg "ValveAdapter\(" src/infrastructure/adapters/hardware/ValveAdapter.h -A 3
echo ""

echo "4️⃣  检查实际调用..."
rg "MC_SetExtDoBit" src/infrastructure/adapters/hardware/ValveAdapter.cpp -B 2 -A 1
echo ""

echo "5️⃣  检查适配器创建..."
rg "make_shared.*ValveAdapter" src/application/container/ -A 3
echo ""

echo "=== 检查完成 ==="
```

### 使用方法

```bash
# 保存为 scripts/config-check.sh
chmod +x scripts/config-check.sh
./scripts/config-check.sh
```

---

## 实战案例

### 案例：供胶阀配置不生效

**问题描述**：
- 配置文件设置 `do_bit_index=2`（Y2）
- WebUI显示"供胶阀控制（Y2输出）"
- 实际硬件控制的是 Y0 而非 Y2

**排查过程**：

```bash
# 步骤1：查找配置
rg "do_bit_index" src/infrastructure/configs/ --type ini
# ✅ 找到：do_bit_index=2

# 步骤2：追踪配置读取
rg "do_bit_index" src/ --type cpp
# ❌ 无匹配 → 配置未被读取！

# 步骤3：检查构造函数
rg "ValveAdapter::ValveAdapter" src/ -A 5
# ❌ 参数只有hardware，没有config

# 步骤4：检查硬编码
rg "CallMC_SetExtDoBit" src/infrastructure/adapters/hardware/ValveAdapter.cpp -B 2 -A 1
# ❌ 硬编码：CallMC_SetExtDoBit(0, 1)

# 步骤5：验证ApplicationContainer
rg "make_shared.*ValveAdapter" src/application/container/ -A 2
# ❌ 只传入hardware，没有传入配置

# 结论：配置存在但未被使用，代码硬编码bitIndex=0
```

**问题定位**：

```cpp
// ValveAdapter.cpp:406
int result = CallMC_SetExtDoBit(0, 1);
//                            ^ 硬编码0，应该是config_.do_bit_index（值为2）

// ApplicationContainer.cpp:110
auto valveAdapter = std::make_shared<Infrastructure::Adapters::ValveAdapter>(
    std::static_pointer_cast<Infrastructure::Hardware::IMultiCardWrapper>(multiCard_)
);  // ❌ 缺少配置参数
```

**影响**：
- 配置文件设置无效
- 无法通过配置调整硬件引脚
- 代码可维护性差

**修复方案**：
1. 修改 ValveAdapter 接收配置参数
2. 修改 ApplicationContainer 传递配置
3. 使用配置值替代硬编码

---

## 排查三原则

### 原则1：不信任原则

**不假设配置被使用，必须验证**

常见错误：
- ❌ "配置文件里有，肯定会被使用"
- ❌ "注释说Y2，实际就是Y2"
- ✅ 每一步都要验证

### 原则2：双向追踪

**配置→代码 + 代码→配置**

```
配置文件 → 读取代码 → 传递链 → 使用点
                ↓
          验证每一步
```

### 原则3：交叉验证

**对比三个来源的值**

1. 配置文件值
2. 代码变量值
3. 实际API调用值

只有三者一致，配置才真正生效。

---

## 工具推荐

### 静态分析工具
- **ripgrep (rg)**: 快速代码搜索
- **ctags**: 代码符号跳转
- **clang-query**: C++代码查询

### 动态分析工具
- **GDB/LLDB**: 调试器
- **Valgrind**: 内存检查
- **strace/ltrace**: 系统调用跟踪

### 可视化工具
- **PlantUML**: 绘制调用链
- **Graphviz**: 依赖关系图

---

## 附录：检查清单模板

```markdown
# 配置排查检查清单

项目名称：[项目名]
配置项：[配置键名]
问题描述：[问题描述]

## 检查项

- [ ] 配置文件存在
- [ ] 配置格式正确
- [ ] 配置被读取
- [ ] 配置被传递到适配器
- [ ] 配置被实际使用
- [ ] 无硬编码覆盖
- [ ] 注释与代码一致
- [ ] 运行时验证通过

## 发现的问题

1. [问题描述]
2. [问题描述]

## 修复建议

1. [修复方案]
2. [修复方案]

## 验证方法

[如何验证修复有效]
```

---

**文档结束**

如有问题或改进建议，请更新本文档。
