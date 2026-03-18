---
Owner: @Xike
Status: active
Last reviewed: 2026-01-11
Scope: 现场/线上故障排查
Project-Binding: Completed (IO映射、错误码、配置参数已补充)
---

# Runbook 排障手册

> 目标: 让任何人按步骤能把机器"先恢复运行",并能留下足够信息定位根因。

## 通用信息(先补齐)
- 控制卡型号/连接方式: 博派MultiCard (网口连接, Ethernet)
- 驱动版本: MultiCard SDK v1.0 (DLL 6.9MB, LIB 137KB)
- 固件版本: v1.0 (博派MultiCard运动控制卡固件)
- 日志位置: `logs/siligen_dispenser.log` (相对可执行文件目录)
- 关键命令/工具:
  - `ping 192.168.0.1` (测试控制卡连通性)
  - 设备管理器 (查看网络适配器)
  - `logs/` 目录 (查看系统日志)
- 现场安全注意:
  - ⚠️ 急停按钮位置: 见现场设备标识
  - ⚠️ 门禁互锁: 安全门打开时禁止运动
  - ⚠️ 高压: 系统使用24V直流电压,无高压危险
  - ⚠️ 运动风险: 工作区域480x480mm, Z轴行程±50mm

---

## 1) 控制卡连接失败

### 现象
- 上位机提示连接失败/超时
- 无法读写 IO/轴状态
- 初始化失败

### 快速确认(30 秒)
- 控制卡是否上电? 指示灯状态: **绿色亮**=正常工作, **熄灭/其他颜色**=故障
- 连接方式: 网口 (Ethernet), 控制卡IP=192.168.0.1, 本机IP=192.168.0.200
- 是否是"这台机器"独有问题? (检查其他机器是否正常)

### 排查步骤(从低成本到高成本)
1. **物理层**: 重新插拔/换线/换端口; 检查接地与屏蔽
2. **供电与指示灯**: 确认电压/电流; 记录灯态
3. **枚举与驱动**: 系统是否识别设备? 驱动版本是否匹配
4. **网络**(如网口): IP/网段/冲突; ping; 端口占用/防火墙
5. **单实例占用**: 是否有另一个程序占用连接?
6. **日志定位**: 搜索关键字: `timeout` / `handshake` / `device not found`

### 临时恢复(先让系统跑起来)
- 断电重启控制卡(记录上电顺序)
- 换线/换端口
- 回滚到已知可用驱动版本

### 必须记录的信息(便于复盘)
- 错误码/报错文案 + 时间点
- 控制卡序列号/固件版本/驱动版本
- 是否更换线缆/端口/电脑

### 项目绑定信息(必须补齐,保证可执行)
> 这部分将Task 5变成"填空题",避免永远停留在"待补充"

- **相关 IO**:
  - DO: `Y2` (端子/线号: `见厂商文档`) 作用: `供胶阀控制`
  - CMP通道: `CMP通道1` (脉冲宽度: `2000μs`) 作用: `点胶阀位置触发`
  - DI: `类型2 (驱动报警)` (X0-X7) 作用: `伺服驱动器状态检测`
- **相关端口/地址**:
  - 控制卡IP: `192.168.0.1:0` (端口0=自动分配)
  - 本机IP: `192.168.0.200:0`
  - 连接超时: `5000ms`
- **关键日志关键字**:
  - `MC_Open` (连接API)
  - `MC_COM_ERR_TIME_OUT` / `-7` (超时错误码)
  - `MC_COM_ERR_CARD_OPEN_FAIL` / `-6` (打开失败)
  - `MultiCard::MC_Open` (C++调用)
  - `connection_failed` (应用层日志)
- **相关API**:
  - `MC_SetCardNo(iCardNum)` - 设置卡号 [1,255], 多卡系统必需
  - `MC_Open(0, "192.168.0.200", cardNum, 0, 0)` - 网口连接, 端口0为系统自动分配
  - `MC_Reset()` - 系统复位, 初始化必需
  - 参数约束: nPCEthernetPort/nCardEthernetPort必须唯一且同网段
- **关联参考**:
  - IO映射: 见 `06_reference.md` 的 "IO映射表"
  - 版本矩阵: 见 `06_reference.md` 的 "版本矩阵"
  - API规范: 见 `06_reference.md` 的 "7.5 板卡操作API参数约束"
  - 网络配置: 见 `config/machine_config.ini` 的 `[Network]` 段
  - 厂商SDK集成: 见 [third_party/vendor/README.md](../../third_party/vendor/README.md)

---

## 2) 电机无响应

### 现象
- 下发运动指令后不动
- 位置反馈不变
- 伺服驱动器可能报警

### 快速确认(30 秒)
- 单轴还是全轴?
- 伺服是否 Enable? 是否有报警码?
- 急停/安全门/限位是否触发?

### 排查步骤
1. **安全链路**: 急停复位、门禁、限位(硬/软)检查
2. **驱动器状态**: 读取报警码; 确认使能信号到位
3. **输出链路**: 脉冲/方向或总线通讯是否正常; 交换通道验证
4. **电源链路**: 主电源/控制电源; 抱闸是否释放
5. **软件状态机**: 是否未上电/未回零/被互锁拦截

### 临时恢复
- 清报警 → 重新使能 → 回零
- 必要时在安全允许下绕过非关键互锁(必须记录)

### 必须记录的信息
- 哪个轴、报警码、当时互锁状态
- 指令是否下发成功(日志截图/片段)

### 项目绑定信息(必须补齐,保证可执行)
- **相关 IO**:
  - DO: `伺服使能` (端子: `见厂商文档轴控制`) 作用: `电机使能信号`
  - DI: `类型0 (正限位)` (X0-X7) 作用: `正向软限位检测`
  - DI: `类型1 (负限位)` (X0-X7) 作用: `负向软限位检测`
  - DI: `类型2 (驱动报警)` (X0-X7) 作用: `伺服驱动器报警,标星号优先检查`
  - DI: `类型3 (原点开关)` (X0-X7) 作用: `回零原点信号`
- **关键日志关键字**:
  - `AXIS_STATUS_SV_ALARM` / `0x00000002` (驱动报警状态位)
  - `AXIS_STATUS_ESTOP` / `0x00000001` (急停状态位)
  - `AXIS_STATUS_ENABLE` / `0x00000200` (使能状态位)
  - `MC_AxisOn` / `MC_AxisOff` (轴控制API)
  - `MC_GetDiRaw` (DI读取API)
  - `enable_failed` (应用层使能失败日志)
- **相关API**:
  - `MC_GetSts(nAxisNum, &status)` - 读取轴状态, 检查频率建议10ms, 响应时间<1ms
  - `MC_EmgStop(axisMask)` - 急停控制, 响应时间<1ms, axisMask为轴位掩码
  - `MC_ClrSts(nAxisNum, statusBit)` - 清除状态, 急停清除0x1000, 报警清除0x002/0x004
  - `MC_AlmStsReset(axisMask)` - 复位报警, axisMask∈[0,2³²-1], 前置条件:报警原因已排除
  - 状态位: 0x200(运行), 0x400(到位), 0x1000(急停), 0x002/0x004(报警)
- **关联参考**:
  - IO映射: 见 `06_reference.md` 的 "IO映射表"
  - 状态位定义: 见 `modules/device-hal/src/drivers/multicard/MultiCardCPP.h`
  - API规范: 见 `06_reference.md` 的 "7.1 安全机制API规范"
- 轴状态位定义: 见 `modules/device-hal/src/drivers/multicard/MultiCardCPP.h`

---

## 3) 点胶阀无响应

### 现象
- 点胶指令触发后无出胶
- 阀体不动作
- 驱动灯无变化

### 快速确认(30 秒)
- 阀类型: 24V 电磁阀 / 压电阀 / 带驱动器?
- 气源压力是否达标?
- 同类阀是否正常(对照组)

### 排查步骤
1. **气源/压力**: 气压范围; 调压阀/过滤器; 漏气
2. **电气**: DO 是否输出; 端子电压; 线圈电阻
3. **阀本体**: 卡滞/污染/回流; 手动触发验证
4. **软件互锁**: 未到位/压力不足禁止点胶; 是否进入点胶时序

### 临时恢复
- 手动模式验证阀动作(隔离软件问题)
- 更换线圈/更换 DO 通道
- 清洗或更换阀

### 项目绑定信息(必须补齐,保证可执行)
- **相关 IO**:
  - CMP: `CMP通道1` (脉冲类型: `0=脉冲`, 脉冲宽度: `2000μs`) 作用: `点胶阀位置触发`
  - 配置参数: `abs_position_flag=1` (绝对位置模式,DXF点胶使用)
  - 位置触发比较源: 规划位置（Profile Position），编码器反馈关闭
  - DXF 执行仅规划位置触发，不使用定时触发回退
  - 定时触发仅用于 HMI 设置页的点胶阀单独控制（调试链路）
- **关键日志关键字**:
  - `MC_CmpBufData` / `MC_CmpPluse` (CMP配置API)
  - `MC_CMP` / `CMP` (CMP相关日志)
  - `position_trigger` (位置触发日志)
  - `abs_position_flag` (绝对位置标志)
  - `DispenseValve` / `ValveAdapter` (点胶阀适配器)
- **相关API**:
  - `MC_CmpBufData(encoderNum, 2, 0, 2000, buf, len, 1, 0)` - 配置CMP触发, 脉冲精度±1pulse, 最大频率1kHz
  - `MC_CompareEnable(axis, channel)` - 启用CMP, axis∈[1,1024], channel∈[1,4], 启用时间<1ms
  - `MC_CompareDisable(axis, channel)` - 禁用CMP, 状态复位立即生效, 系统关闭时必须调用
  - 参数约束: nTime(脉冲宽度)≥1ms, nBufLen1∈[1,1024], nAbsPosFlag=1(绝对位置)
  - 性能边界: 触发精度±1pulse, 最小脉宽1ms, 最大触发频率1kHz
- **关联参考**:
  - IO映射: 见 `06_reference.md` 的 "IO映射表" (CMP通道1配置)
  - API规范: 见 `06_reference.md` 的 "7.2 比较输出(CMP)API规范"
  - CMP配置: 见 `config/machine_config.ini` 的 `[CMP]` 段
  - CMP API: 见 [third_party/vendor/03-高级功能/03-比较输出飞拍API.md](../../third_party/vendor/03-高级功能/03-比较输出飞拍API.md)
  - 阀控制实现: 见 `modules/device-hal/src/adapters/dispensing/dispenser/ValveAdapter.cpp`

---

## 4) 供胶阀无响应

### 现象
- 供胶开启无效果
- 压力不上升
- 泵/阀不动作

### 快速确认(30 秒)
- 供胶方式: 压力桶/螺杆泵/齿轮泵?
- 是否存在低液位/低压/门禁互锁?

### 排查步骤
1. **源头**: 液位/桶压/泵电源与报警; 过滤器/单向阀堵塞
2. **阀与驱动**: DO 输出是否存在; 驱动模块是否保护
3. **管路**: 堵塞/固化; 旁通是否打开导致不上压
4. **传感器/互锁**: 压力/液位传感器是否异常

### 临时恢复
- 手动开阀/旁通验证机械侧
- 切换备用供胶路径(如有)

### 项目绑定信息(必须补齐,保证可执行)
- **相关 IO**:
  - DO: `Y2` (DO位索引: `2`, 卡索引: `0`) 作用: `供胶阀开关`
  - 失败安全模式: `fail_closed=true` (超时/失败时自动关闭)
  - 超时保护: `timeout_ms=5000` (5秒超时)
- **关键日志关键字**:
  - `MC_SetExtDoBit` (DO位控制API)
  - `ValveSupply` / `SupplyValve` (供胶阀组件)
  - `do_bit_index=2` (配置中的DO位索引)
  - `timeout_ms` (超时参数)
  - `fail_closed` (失败安全模式)
- **相关API**:
  - `MC_SetExtDoBit(nCardIndex, nBitIndex, nValue)` - 设置DO位, nCardIndex∈[0,63], nBitIndex∈[0,31]
  - DO位索引2对应Y2输出, nValue=0(关闭)/1(打开)
  - 参数约束: 必须先调用MC_SetCardNo设置卡号, DO输出实时生效(<1ms)
  - 失败安全: 超时(timeout_ms=5000)或失败时自动关闭(fail_closed=true)
- **关联参考**:
  - IO映射: 见 `06_reference.md` 的 "IO映射表" (Y2供胶阀)
  - IO操作API: 见 [third_party/vendor/01-基础功能/03-IO操作API.md](../../third_party/vendor/01-基础功能/03-IO操作API.md)
  - 供胶阀配置: 见 `config/machine_config.ini` 的 `[ValveSupply]` 段
  - DO API: 见 [third_party/vendor/01-基础功能/03-IO操作API.md](../../third_party/vendor/01-基础功能/03-IO操作API.md)
  - 阀控制实现: 见 `modules/device-hal/src/adapters/dispensing/dispenser/ValveAdapter.cpp`

---

## 附录: 统一故障记录模板

每次故障都记录以下信息:

- **时间**: YYYY-MM-DD HH:mm
- **设备编号/工位**: ...
- **现象**: ...
- **最近改动**(软件/参数/硬件): ...
- **日志片段**(关键字): ...
- **采取动作**: ...
- **结果**: 恢复/未恢复
- **初步根因**: ...
- **后续动作**(创建 Issue/ADR/补充 reference): ...
