# HMI简体中文本地化方案

Created: 2026-01-21
Status: planning
Owner: @claude

## 背景

HMI工具面向中国用户，需要将界面语言从英文切换为简体中文。由于仅需保留简体中文，不需要多语言切换功能，采用直接替换方案。

## 架构考量

### 六边形架构合规性

根据`.claude/rules/HEXAGONAL.md`和`.claude/rules/PORTS_ADAPTERS.md`:

- HMI位于`src/adapters/hmi/`，属于Adapter层
- Adapter层负责外部接口，UI文本属于表现层职责
- 本地化修改仅涉及Adapter层，不影响Domain和Application层
- 符合六边形架构的依赖方向规则

### 修改范围

仅修改Adapter层文件:
- `src/adapters/hmi/ui/main_window.py` - UI文本
- `src/adapters/hmi/ui/styles.py` - 样式注释(可选)

## 方案选择

### 方案A: 直接替换 (推荐)

将所有英文UI字符串直接替换为中文。

优点:
- 实现简单，无额外依赖
- 代码可读性好，中文直接可见
- 维护成本低

缺点:
- 不支持多语言切换(但用户明确不需要)

### 方案B: Qt国际化(QTranslator)

使用Qt的翻译系统，创建.ts/.qm翻译文件。

优点:
- 标准Qt国际化方案
- 支持运行时切换语言

缺点:
- 增加复杂度
- 需要额外的翻译文件
- 用户明确不需要多语言

**结论**: 采用方案A，直接替换。

## 实施计划

### Phase 1: 窗口和Tab标签

修改`main_window.py`:

| 原文 | 中文 |
|------|------|
| Siligen Motion Controller HMI | 思力根运动控制器 |
| Motion Control | 运动控制 |
| Dispenser Control | 点胶控制 |
| DXF Process | DXF工艺 |
| Alarms | 报警 |

### Phase 2: 侧边栏面板

#### CONNECTION面板
| 原文 | 中文 |
|------|------|
| CONNECTION | 连接 |
| TCP: | TCP: |
| HW: | 硬件: |
| Host: | 主机: |
| Port: | 端口: |
| Connect | 连接 |
| Disconnect | 断开 |

#### SYSTEM面板
| 原文 | 中文 |
|------|------|
| SYSTEM | 系统 |
| Initialize Hardware | 初始化硬件 |
| Home All | 全部回零 |
| Home X | X轴回零 |
| Home Y | Y轴回零 |
| STOP | 停止 |
| EMERGENCY STOP | 急停 |

### Phase 3: 状态面板

| 原文 | 中文 |
|------|------|
| STATUS | 状态 |
| Axis | 轴 |
| Position | 位置 |
| Velocity | 速度 |
| Enabled | 使能 |
| Homed | 已回零 |
| IO STATUS | IO状态 |
| X+ Limit | X+限位 |
| X- Limit | X-限位 |
| Y+ Limit | Y+限位 |
| Y- Limit | Y-限位 |
| E-Stop | 急停 |
| Door | 门 |
| State: | 状态: |
| Dispenser: | 点胶阀: |
| Supply: | 供料阀: |
| Unknown | 未知 |
| ON | 开 |
| OFF | 关 |

### Phase 4: 运动控制Tab

| 原文 | 中文 |
|------|------|
| Jog Control | 点动控制 |
| Speed: | 速度: |
| Slow | 慢速 |
| Medium | 中速 |
| Fast | 快速 |
| Home | 回零 |
| Point-to-Point Move | 点对点移动 |
| Move to Position | 移动到位置 |

### Phase 5: 点胶控制Tab

| 原文 | 中文 |
|------|------|
| Dispenser Parameters | 点胶参数 |
| Count: | 次数: |
| Interval: | 间隔: |
| Duration: | 持续时间: |
| Dispenser Valve | 点胶阀 |
| Start | 启动 |
| Pause | 暂停 |
| Resume | 继续 |
| Stop | 停止 |
| Supply Valve | 供料阀 |
| Open Supply | 打开供料 |
| Close Supply | 关闭供料 |
| Purge | 清洗 |
| Execute Purge | 执行清洗 |

### Phase 6: DXF工艺Tab

| 原文 | 中文 |
|------|------|
| DXF File | DXF文件 |
| Select DXF file... | 选择DXF文件... |
| Browse | 浏览 |
| Load DXF | 加载DXF |
| Segments: | 段数: |
| Execution Parameters | 执行参数 |
| Execution | 执行 |
| Segment: | 当前段: |
| Execute | 执行 |

### Phase 7: 报警Tab

| 原文 | 中文 |
|------|------|
| Active Alarms | 活动报警 |
| Clear All | 全部清除 |
| Acknowledge | 确认 |

### Phase 8: 状态栏消息

| 原文 | 中文 |
|------|------|
| Disconnected | 已断开 |
| Connected to {host}:{port} | 已连接到 {host}:{port} |
| Connection failed | 连接失败 |
| Hardware connected | 硬件已连接 |
| Hardware connection failed | 硬件连接失败 |
| Hardware: {msg} | 硬件: {msg} |
| Home: {msg} | 回零: {msg} |
| Homing complete | 回零完成 |
| Homing failed | 回零失败 |
| E-Stop: {msg} | 急停: {msg} |
| Moving... | 移动中... |
| Move failed | 移动失败 |
| Dispenser started | 点胶已启动 |
| Dispenser paused | 点胶已暂停 |
| Dispenser resumed | 点胶已继续 |
| Dispenser stopped | 点胶已停止 |
| Supply valve opened | 供料阀已打开 |
| Supply valve closed | 供料阀已关闭 |
| Purging (timeout: {timeout}ms) | 清洗中 (超时: {timeout}ms) |
| No DXF file selected | 未选择DXF文件 |
| DXF loaded: {count} segments | DXF已加载: {count}段 |
| DXF load failed | DXF加载失败 |
| No DXF loaded | 未加载DXF |
| DXF execution started | DXF执行已启动 |
| DXF execution failed | DXF执行失败 |
| DXF execution stopped | DXF执行已停止 |
| Alarms cleared | 报警已清除 |
| Failed to clear alarms | 清除报警失败 |
| Alarm {id} acknowledged | 报警 {id} 已确认 |
| Failed to acknowledge alarm | 确认报警失败 |
| No alarm selected | 未选择报警 |

### Phase 9: 文件对话框

| 原文 | 中文 |
|------|------|
| Select DXF File | 选择DXF文件 |
| DXF Files (*.dxf) | DXF文件 (*.dxf) |
| All Files (*) | 所有文件 (*) |

## 验证清单

- [ ] 所有UI文本已替换为中文
- [ ] 状态栏消息已本地化
- [ ] 文件对话框标题已本地化
- [ ] 单位后缀保持不变 (mm, mm/s, ms)
- [ ] data-testid属性保持英文(用于自动化测试)
- [ ] 代码注释可选择性保留英文

## 风险评估

- 低风险: 仅涉及字符串替换，不改变逻辑
- 编码: Python文件已使用UTF-8，支持中文
- 字体: PyQt5默认支持中文显示

## 预计工作量

- 修改文件: 1个 (main_window.py)
- 替换字符串: 约80处
- 预计时间: 30分钟
