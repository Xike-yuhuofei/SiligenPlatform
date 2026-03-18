# CLI命令行参考

## 概述

Siligen工业点胶控制系统提供功能完整的命令行界面（CLI），支持系统配置、运动控制、状态查询等功能。

## 启动CLI

```bash
# Windows环境
cd build\Release
test_mc_lnxy.exe

# 或使用批处理脚本
scripts\build.bat
```

## 主要命令

### 系统控制命令

#### 连接和初始化
```bash
connect IP PORT        # 连接控制卡
disconnect             # 断开连接
reset                 # 复位控制卡
status                # 查询系统状态
```

**示例：**
```bash
connect 192.168.0.1 0
status
```

#### 配置管理
```bash
load config FILE      # 加载配置文件
save config FILE      # 保存配置文件
show config           # 显示当前配置
set param NAME VALUE  # 设置参数
```

### 运动控制命令

#### 基础运动
```bash
home AXIS             # 回零操作
move X Y [Z]         # 直线插补
arc X Y I J DIR      # 圆弧插补
jog AXIS SPEED       # JOG运动
stop [AXIS]          # 停止运动
```

**示例：**
```bash
home 0               # 0轴回零
move 10000 20000     # 移动到(10000, 20000)
arc 20000 0 10000 0 1 # 绘制半圆
jog 0 500            # 0轴JOG运动，速度500
stop                 # 停止所有轴
```

#### 速度和参数设置
```bash
velocity X Y [Z]     # 设置合成速度
acceleration X Y [Z] # 设置加速度
position X Y [Z]     # 设置目标位置
update AXIS          # 更新位置
```

**示例：**
```bash
velocity 1000        # 设置合成速度1000
acceleration 5000    # 设置加速度5000
position 5000 5000   # 设置目标位置
update 0             # 更新0轴位置
```

### 触发控制命令

#### 位置触发
```bash
trigger AXIS POS MASK DIR DELAY    # 位置触发
buffer_trigger AXIS POS MASK LEVEL # 缓冲触发
wait_io MASK LEVEL TIMEOUT        # 等待IO信号
buffer_delay TIME                  # 缓冲延时
```

**示例：**
```bash
trigger 0 10000 0x01 1 0.0         # 0轴10000位置触发OUT0
wait_io 0x01 1 5000                # 等待DI0高电平，超时5秒
buffer_delay 100                   # 插入100ms延时
```

#### IO控制
```bash
set_do MASK VALUE    # 设置数字输出
get_di              # 读取数字输入
get_adc CHANNEL     # 读取模拟输入
```

**示例：**
```bash
set_do 0x03 0x03     # 设置OUT0和OUT1为高
get_di               # 读取所有DI状态
get_adc 0            # 读取ADC通道0
```

### 坐标系命令

```bash
crd_init NUM         # 初始化坐标系
crd_start NUM        # 启动坐标系
crd_stop NUM         # 停止坐标系
crd_clear NUM        # 清空缓冲区
crd_data DATA...     # 发送运动数据
```

**示例：**
```bash
crd_init 1           # 初始化1号坐标系
crd_start 1          # 启动坐标系
crd_data 1 2 1000 5000 0 10000 20000  # 直线运动数据
```

### 状态查询命令

#### 位置和速度
```bash
pos [AXIS]           # 查询位置
vel [AXIS]           # 查询速度
space CRD            # 查询规划空间
crd_status CRD       # 查询坐标系状态
```

**示例：**
```bash
pos                  # 查询所有轴位置
pos 0                # 查询0轴位置
vel                  # 查询所有轴速度
space 1              # 查询1号坐标系剩余空间
```

#### 系统状态
```bash
status AXIS          # 查询轴状态
error                # 查询错误信息
version              # 查询版本信息
help COMMAND         # 查看命令帮助
```

### 配置文件

#### 配置文件格式（INI）

```ini
[System]
ip = 192.168.0.1
port = 0
timeout = 5000

[Axis0]
enabled = true
velocity = 1000
acceleration = 5000
position_limit = 100000
negative_limit = -100000

[Axis1]
enabled = true
velocity = 800
acceleration = 4000
position_limit = 80000
negative_limit = -80000

[CoordinateSystem1]
fifo_size = 500
smooth_time = 20.0
max_acceleration = 5000
```

## 批处理脚本示例

### 基础运动脚本

```batch
@echo off
echo Starting motion sequence...

connect 192.168.0.1 0
if errorlevel 1 goto error

home 0
timeout /t 3

move 10000 0
timeout /t 2

move 10000 10000
timeout /t 2

move 0 10000
timeout /t 2

move 0 0
timeout /t 2

disconnect
echo Motion sequence completed
goto end

:error
echo Error occurred
:end
pause
```

### 点胶应用脚本

```batch
@echo off
echo Dispensing application...

connect 192.168.0.1 0

% 初始化 %
reset
home 0
home 1

% 设置参数 %
velocity 500
acceleration 2000

% 点胶序列 %
move 0 0
trigger 0 5000 0x01 1 0.0
move 10000 0
trigger 0 10000 0x01 0 0.0

move 0 10000
trigger 0 5000 0x01 1 0.0
move 10000 10000
trigger 0 10000 0x01 0 0.0

% 回到原点 %
move 0 0

disconnect
echo Dispensing completed
pause
```

## 错误处理

### 常见错误代码

| 代码 | 含义 | 解决方法 |
|------|------|----------|
| -1 | 连接失败 | 检查IP地址和端口 |
| -2 | 参数错误 | 检查命令参数 |
| -3 | 轴未使能 | 先执行轴使能 |
| -4 | 限位触发 | 检查机械位置 |
| -5 | 运动错误 | 停止运动并检查状态 |

### 错误处理示例

```batch
@echo off
connect 192.168.0.1 0
if errorlevel 1 (
    echo Connection failed
    goto end
)

home 0
if errorlevel 1 (
    echo Homing failed, checking status...
    status 0
    goto end
)

echo All operations completed successfully

:end
pause
```

## 调试模式

启用调试日志：

```bash
debug start FILE      # 开始记录调试日志
debug stop           # 停止记录
debug level LEVEL    # 设置日志级别 (0-3)
```

## 快捷命令

```bash
?                    # 显示帮助列表
q, quit              # 退出程序
cls                  # 清屏
history              # 显示命令历史
repeat COUNT         # 重复上一个命令COUNT次
```

## 注意事项

1. **命令大小写**: 命令不区分大小写
2. **参数分隔**: 参数之间用空格分隔
3. **错误检查**: 每个命令后检查错误状态
4. **单位**: 位置使用脉冲单位，速度使用脉冲/毫秒
5. **安全**: 运动前检查限位和系统状态
6. **端口配置**: 建议使用端口0让系统自动分配，避免端口冲突

## 性能优化

1. **批量操作**: 使用脚本文件批量执行命令
2. **缓冲管理**: 合理设置运动缓冲区大小
3. **查询频率**: 避免过于频繁的状态查询
4. **网络延迟**: 网络环境考虑命令延迟

## 扩展功能

### 自定义命令

通过配置文件可以定义自定义命令组合：

```ini
[CustomCommands]
safe_home = home 0; home 1; status
dispense_sequence = move 0 0; trigger 0 1000 0x01 1 0; move 5000 0; trigger 0 6000 0x01 0 0
```

### 环境变量

使用环境变量简化配置：

```bash
set SILIGEN_IP=192.168.0.1
set SILIGEN_PORT=0
connect %SILIGEN_IP% %SILIGEN_PORT%
```