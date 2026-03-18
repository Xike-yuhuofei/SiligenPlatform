# HMI优化方案

> 创建日期: 2026-01-21
> 状态: 已实施
> 作者: Claude

## 1. 背景

基于UI设计师视角的分析，结合项目六边形架构和`.claude/rules`规范，制定HMI优化方案。

### 1.1 架构约束

根据`.claude/rules/HEXAGONAL.md`和`src/adapters/CLAUDE.md`：

```
RULE: HEXAGONAL_LAYER_ORDER
MUST: adapter -> application -> domain

RULE: ADAPTER_NO_BYPASS_USECASE
FORBIDDEN: Bypass UseCase to call Ports directly
MUST: Execute operations through UseCases

RULE: ADAPTER_INPUT_VALIDATION
MUST: Basic validation (format, required, range)
```

**HMI合规要求**:
- HMI作为Driving Adapter，只能通过TCP协议调用后端UseCase
- 前端只做基本输入验证（格式、必填、范围）
- 不能直接访问Domain层或Infrastructure层

### 1.2 UI测试约束

根据`.claude/rules/UI_PLAYWRIGHT.md`：

```
RULE: UI_TEST_SELECTORS
MUST_PRIORITY: data-testid, aria-label, role
FORBIDDEN: fragile_selectors
```

## 2. 模块划分

基于用户操作流程，HMI分为6个核心模块：

| 模块 | 职责 | 优先级 |
|------|------|:------:|
| 连接管理 | TCP连接/断开、硬件初始化 | P1 |
| 状态监控 | 轴状态、IO状态、阀门状态 | P1 |
| 运动控制 | 回零、点动、点位移动 | P1 |
| 点胶控制 | 阀门控制、清洗/排胶 | P2 |
| 工艺管理 | DXF导入、路径规划、执行 | P3 |
| 报警/日志 | 异常提示、操作记录 | P3 |

## 3. 实施方案

### Phase 1: 连接管理模块完善

**目标**: 完善连接管理功能

**当前状态**:
- [x] TCP连接
- [x] 硬件初始化
- [x] 断开连接按钮
- [x] 连接状态指示器

**修改内容**:

1. `src/adapters/hmi/ui/main_window.py`:
   - 添加断开连接按钮（复用现有`_on_connect`逻辑）
   - 添加连接状态LED指示器

2. `src/adapters/hmi/client/protocol.py`:
   - 添加`disconnect_hardware()`方法

**data-testid规范**:
```python
"btn-disconnect"          # 断开连接按钮
"indicator-tcp-status"    # TCP连接状态指示器
"indicator-hw-status"     # 硬件连接状态指示器
```

---

### Phase 2: 状态监控模块增强

**目标**: 完善状态显示功能

**当前状态**:
- [x] 轴位置显示
- [x] 轴速度显示
- [x] 轴使能/回零状态
- [x] 点胶阀状态
- [x] 供料阀状态
- [x] IO状态显示（限位、急停、门禁）

**修改内容**:

1. `src/adapters/hmi/ui/main_window.py`:
   - 在状态面板添加IO状态区域
   - 显示限位开关状态（X+/X-/Y+/Y-/Z+/Z-）
   - 显示急停按钮状态
   - 显示门禁状态

2. `src/adapters/hmi/client/protocol.py`:
   - 扩展`MachineStatus`数据类，添加IO状态字段
   - 修改`get_status()`解析IO数据

**UI布局**:
```
+------------------------------------------+
| IO状态                                    |
| +--------------------+-------------------+|
| | X+ 限位: (*)       | X- 限位: ( )      ||
| | Y+ 限位: ( )       | Y- 限位: ( )      ||
| | 急停: (!)          | 门禁: ( )         ||
| +--------------------+-------------------+|
+------------------------------------------+
注: (*) = LED亮, ( ) = LED灭, (!) = 报警LED
```

**data-testid规范**:
```python
"io-limit-x-pos", "io-limit-x-neg"
"io-limit-y-pos", "io-limit-y-neg"
"io-estop", "io-door"
```

---

### Phase 3: 运动控制模块完善

**目标**: 完善手动控制功能

**当前状态**:
- [x] 全轴回零
- [x] 单轴回零（X/Y）
- [x] 点动控制（带速度滑块）
- [x] 点位移动（X/Y/Speed）
- [x] 停止/急停

**修改内容**:

1. `src/adapters/hmi/ui/main_window.py`:
   - 点动控制改为D-Pad十字键布局
   - 按钮尺寸增大至60x60px（触摸屏友好）
   - 添加按压反馈QSS样式
   - 速度滑块增粗

**UI布局（D-Pad）**:
```
+------------------------------------------+
| 点动控制                                  |
| +--------------------------------------+ |
|           [Y+]                           |
|    [X-]   [Home]   [X+]                  |
|           [Y-]                           |
| +--------------------------------------+ |
| 速度: [慢] [中] [快]  [====*====] 20 mm/s |
+------------------------------------------+
```

**data-testid规范**:
```python
# 回零
"btn-home-all", "btn-home-X", "btn-home-Y"

# 点动 (D-Pad)
"btn-jog-X-plus", "btn-jog-X-minus"
"btn-jog-Y-plus", "btn-jog-Y-minus"
"btn-jog-home-xy"                    # D-Pad中心Home按钮

# 速度控制
"slider-jog-speed", "label-jog-speed"
"btn-speed-slow", "btn-speed-medium", "btn-speed-fast"

# 点位移动
"input-move-X", "input-move-Y", "input-move-speed"
"btn-move-to-position"

# 停止
"btn-stop", "btn-estop"
```

---

### Phase 4: 点胶控制模块完善

**目标**: 完善点胶控制功能

**当前状态**:
- [x] 点胶阀开始/暂停/恢复/停止
- [x] 供料阀打开/关闭
- [x] 清洗/排胶（带时长设置）
- [x] 点胶参数设置（次数、间隔、时长）

**修改内容**:

1. `src/adapters/hmi/ui/main_window.py`:
   - 点胶阀控制区添加参数输入（次数、间隔、时长）
   - 修改`_on_dispenser_start()`传递参数

2. `src/adapters/hmi/client/protocol.py`:
   - 修改`dispenser_start()`方法签名，添加参数

**UI布局**:
```
+------------------------------------------+
| 点胶参数                                  |
| +--------------------------------------+ |
| | 次数: [____]  间隔: [____] ms        | |
| | 时长: [____] ms                      | |
| +--------------------------------------+ |
|                                          |
| 点胶阀控制                                |
| +--------------------------------------+ |
| | [开始] [暂停] [恢复] [停止]           | |
| +--------------------------------------+ |
+------------------------------------------+
```

**data-testid规范**:
```python
# 点胶参数
"input-dispenser-count"
"input-dispenser-interval"
"input-dispenser-duration"

# 点胶阀（已有）
"btn-dispenser-start", "btn-dispenser-pause"
"btn-dispenser-resume", "btn-dispenser-stop"

# 供料阀（已有）
"btn-supply-open", "btn-supply-close"

# 清洗（已有）
"input-purge-duration", "btn-purge"
```

---

### Phase 5: 工艺管理模块（DXF）

**目标**: 添加DXF路径规划和执行功能

**当前状态**:
- [x] DXF文件选择
- [x] 路径规划
- [x] 执行控制

**修改内容**:

1. `src/adapters/hmi/ui/main_window.py`:
   - 添加DXF功能Tab页
   - 文件选择对话框
   - 规划参数设置
   - 执行进度显示

2. `src/adapters/hmi/client/protocol.py`:
   - 添加`dxf_load(filepath)`方法
   - 添加`dxf_execute()`方法

3. `src/adapters/tcp/TcpCommandDispatcher.cpp`:
   - 确认`dxf.load`和`dxf.execute`命令已实现

**UI布局**:
```
+------------------------------------------+
| DXF功能                                   |
+------------------------------------------+
| 文件选择                                  |
| +--------------------------------------+ |
| | [浏览...] path/to/file.dxf           | |
| +--------------------------------------+ |
|                                          |
| 规划参数                                  |
| +--------------------------------------+ |
| | 最大速度: [____] mm/s                 | |
| | 最大加速度: [____] mm/s^2             | |
| | 路径优化: [x]                         | |
| +--------------------------------------+ |
|                                          |
| 执行控制                                  |
| +--------------------------------------+ |
| | [规划] [执行] [停止]                  | |
| | 进度: [=========>          ] 45%     | |
| +--------------------------------------+ |
+------------------------------------------+
```

**data-testid规范**:
```python
# 文件选择
"btn-dxf-browse", "label-dxf-path"

# 规划参数
"input-dxf-max-velocity"
"input-dxf-max-acceleration"
"checkbox-dxf-optimize"

# 执行控制
"btn-dxf-plan", "btn-dxf-execute", "btn-dxf-stop"
"progress-dxf-execution"
```

---

### Phase 6: 报警/日志模块

**目标**: 添加报警和日志功能

**当前状态**:
- [x] StatusBar消息显示
- [x] 报警列表
- [x] 操作日志

**修改内容**:

1. `src/adapters/hmi/ui/main_window.py`:
   - 底部添加报警/日志面板
   - 报警列表（最新置顶）
   - 清除报警按钮

**UI布局**:
```
+------------------------------------------+
| 报警/日志                                 |
| +--------------------------------------+ |
| | [!] 2026-01-21 10:30:15 X轴限位触发   | |
| | [!] 2026-01-21 10:28:03 通信超时       | |
| +--------------------------------------+ |
| [清除报警]                                |
+------------------------------------------+
```

**data-testid规范**:
```python
"panel-alarm-log"
"list-alarms"
"btn-clear-alarms"
```

## 4. 文件修改清单

### 4.1 Python HMI端

| 文件 | 修改类型 | Phase |
|------|---------|:-----:|
| `src/adapters/hmi/ui/main_window.py` | 修改 | 1-6 |
| `src/adapters/hmi/client/protocol.py` | 修改 | 1-5 |

### 4.2 C++ TCP Server端

| 文件 | 修改类型 | Phase |
|------|---------|:-----:|
| `src/adapters/tcp/TcpCommandDispatcher.cpp` | 确认/修改 | 1,5 |

## 5. 实施顺序

```
Phase 1 (连接管理) ─────────────────────────────────────
    │
    ├── 1.1 添加断开连接按钮
    └── 1.2 添加连接状态指示器
    │
    v
Phase 2 (状态监控) ─────────────────────────────────────
    │
    ├── 2.1 添加IO状态显示区域
    └── 2.2 扩展protocol.py解析IO数据
    │
    v
Phase 3 (运动控制) ─────────────────────────────────────
    │
    └── 3.1 点位移动添加Z轴支持
    │
    v
Phase 4 (点胶控制) ─────────────────────────────────────
    │
    └── 4.1 添加点胶参数输入
    │
    v
Phase 5 (DXF功能) [P3] ─────────────────────────────────
    │
    ├── 5.1 添加DXF Tab页
    ├── 5.2 文件选择和参数设置
    └── 5.3 执行控制和进度显示
    │
    v
Phase 6 (报警/日志) [P3] ───────────────────────────────
    │
    └── 6.1 添加报警/日志面板
```

## 6. 验收标准

### 6.1 功能验收

- [x] Phase 1: 连接/断开功能正常，状态指示器正确
- [x] Phase 2: IO状态实时更新（200ms刷新）
- [x] Phase 3: 点动控制D-Pad布局、速度预设
- [x] Phase 4: 点胶参数可配置
- [x] Phase 5: DXF文件可加载、预览信息、空跑、执行
- [x] Phase 6: 报警信息正确显示

### 6.2 架构验收

- [x] 所有操作通过TCP协议调用后端UseCase
- [x] 无直接Domain/Infrastructure依赖
- [x] 输入验证在前端完成

### 6.3 UI测试验收

- [x] 所有可交互元素有`data-testid`
- [x] 选择器符合UI_PLAYWRIGHT.md规范
- [x] 无脆弱选择器使用

### 6.4 补充功能验收

- [x] 生产统计功能（已完成/目标/UPH/周期时间）
- [x] 配方管理功能（加载/保存/新建）
- [x] 权限管理功能（操作员/技术员/工程师）
- [x] 维护提醒功能（针头计数/清洗提醒）

## 7. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| TCP协议扩展复杂 | 开发延迟 | 复用现有命令处理模式 |
| UI布局调整大 | 用户体验变化 | 保持核心操作位置不变 |
| 状态刷新性能 | UI卡顿 | 使用异步更新，避免阻塞 |

## 8. 补充改进（基于UX评审）

### 8.1 视觉反馈增强

**问题**: 动作按钮缺乏执行状态反馈，可能导致重复点击

**解决方案**:
1. 所有动作按钮执行时添加`disabled`状态
2. 添加全局状态指示条（顶部），显示当前操作状态
3. 按钮点击后立即变色，等待响应

**data-testid规范**:
```python
"indicator-global-status"    # 全局状态指示条
"label-current-operation"    # 当前操作标签
```

### 8.2 报警分级与弹窗

**问题**: 报警仅在底部显示，严重错误可能被忽略

**解决方案**:
1. 报警分级：Info(蓝)/Warning(黄)/Error(红)
2. Error级别触发模态弹窗，阻断操作直到确认
3. 急停按钮保持在侧边栏，始终可见

**data-testid规范**:
```python
"dialog-error-alert"         # 错误弹窗
"btn-error-acknowledge"      # 错误确认按钮
```

### 8.3 DXF预览与空跑

**问题**: DXF执行前缺乏预览，可能导致位置错误

**解决方案**:
1. 添加路径预览区，显示Bounding Box和轨迹
2. 添加"空跑"按钮（只走轨迹不出胶）
3. 显示路径统计信息（总长度、预计时间）

**UI布局更新**:
```
+------------------------------------------+
| DXF功能                                   |
+------------------------------------------+
| 文件选择                                  |
| +--------------------------------------+ |
| | [浏览...] path/to/file.dxf           | |
| +--------------------------------------+ |
|                                          |
| 路径预览                                  |
| +--------------------------------------+ |
| |  ┌─────────────────┐                 | |
| |  │    轨迹图形      │  总长度: 123mm  | |
| |  │   (Canvas)      │  预计: 45s      | |
| |  └─────────────────┘  段数: 12       | |
| +--------------------------------------+ |
|                                          |
| 执行控制                                  |
| +--------------------------------------+ |
| | [规划] [空跑] [执行] [停止]           | |
| | 进度: [=========>          ] 45%     | |
| +--------------------------------------+ |
+------------------------------------------+
```

**data-testid规范**:
```python
"canvas-dxf-preview"         # 路径预览画布
"btn-dxf-dry-run"            # 空跑按钮
"label-dxf-total-length"     # 总长度标签
"label-dxf-estimated-time"   # 预计时间标签
```

### 8.4 速度预设

**问题**: 纯输入框在快速操作时不便

**解决方案**:
1. 点动速度添加预设按钮（慢/中/快）
2. 保留滑块和输入框用于精确调节

**UI布局更新**:
```
速度: [慢] [中] [快]  [====●====] 20.0 mm/s
```

**data-testid规范**:
```python
"btn-speed-slow"             # 慢速预设 (10 mm/s)
"btn-speed-medium"           # 中速预设 (30 mm/s)
"btn-speed-fast"             # 快速预设 (50 mm/s)
```

### 8.5 工作流导向布局（重要）

**定位澄清**:
- **HMI** = 生产工具（操作员使用）
- **CLI** = 调试工具（工程师使用）

**问题**: 当前布局是功能堆砌，不适合生产环境

**解决方案**: 采用Tab分页，按工作流组织

**布局重构**:
```
┌─────────────────────────────────────────────────────────────┐
│  [状态指示条]  运行中: DXF点胶  进度: 45%     [急停]        │
├─────────────────────────────────────────────────────────────┤
│  [生产] [设置] [文件]                                        │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                                                     │   │
│  │              Tab 内容区域                            │   │
│  │                                                     │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│  [报警/日志面板]                                             │
└─────────────────────────────────────────────────────────────┘
```

**Tab 1: 生产 (Production)**
- 大号 [开始] [暂停] [停止] 按钮
- 当前文件名和路径预览
- 产量计数（已完成/目标）
- 简化状态显示

**Tab 2: 设置 (Setup)**
- 手动控制（回零、点动、点位移动）
- IO状态监控
- 点胶参数设置
- 速度预设

**Tab 3: 文件 (Recipe)**
- DXF文件选择
- 路径预览画布
- 规划参数
- 空跑/执行控制

**data-testid规范**:
```python
"tab-production"             # 生产Tab
"tab-setup"                  # 设置Tab
"tab-recipe"                 # 文件Tab
"btn-production-start"       # 生产开始按钮（大号）
"btn-production-pause"       # 生产暂停按钮（大号）
"btn-production-stop"        # 生产停止按钮（大号）
"label-production-count"     # 产量计数
"label-current-recipe"       # 当前配方名称
```

**生产Tab设计原则**:
1. 操作员在生产时**不应看到**点动按钮，防止误触
2. 只显示必要的状态和控制
3. 按钮足够大，适合触摸屏操作
4. 颜色编码：绿色=运行，黄色=暂停，红色=停止/急停

## 9. UI美化规范（赛博工业风）

### 9.1 色彩系统（ISA-101合规）

| 用途 | 颜色 | 色值 | ISA-101语义 |
|------|------|------|-------------|
| 背景色（深） | 深灰 | #1E1E1E | 正常状态背景 |
| 背景色（中） | 中灰 | #2D2D2D | 面板背景 |
| 强调色/信息 | 蓝色 | #2196F3 | 信息/选中 |
| 警示色/停止 | 珊瑚红 | #FF5722 | 停止/故障/危险 |
| 就绪/运行 | 翠绿 | #4CAF50 | 运行/正常/允许 |
| 警告/暂停 | 琥珀色 | #FFC107 | 警告/注意/暂停 |
| 文字（主） | 白色 | #FFFFFF | - |
| 文字（次） | 灰色 | #888888 | - |
| 信息提示 | 青色 | #00BCD4 | 辅助信息（非主强调） |

**ISA-101颜色保留原则**:
- 红色仅用于：停止、故障、危险、急停
- 绿色仅用于：运行、正常、允许
- 黄色仅用于：警告、注意、暂停
- 蓝色用于：信息、选中、强调（非安全相关）

### 9.2 字体规范

```css
/* 数值显示 - 防止数字跳动 */
.readout {
    font-family: 'Consolas', 'Monaco', monospace;
    font-size: 16px;
}

/* 标签文字 */
.label {
    font-family: 'Microsoft YaHei', sans-serif;
    font-size: 14px;
}
```

### 9.3 组件规范

#### LED指示器组件 (QLedIndicator)

```python
# 状态样式
LED_OFF = "background: #333; border: 1px solid #555; border-radius: 8px;"
LED_ON_GREEN = "background: #4CAF50; box-shadow: 0 0 5px #4CAF50; border-radius: 8px;"
LED_ON_RED = "background: #FF5722; box-shadow: 0 0 5px #FF5722; border-radius: 8px;"
LED_ON_YELLOW = "background: #FFC107; box-shadow: 0 0 5px #FFC107; border-radius: 8px;"
```

#### 按钮规范

```css
/* 基础按钮 - 60x60px触摸友好 */
QPushButton {
    min-width: 60px;
    min-height: 60px;
    border-radius: 8px;
    font-size: 14px;
}

/* 按压反馈 */
QPushButton:pressed {
    background-color: #1a1a1a;
    margin-top: 2px;
    margin-left: 2px;
}

/* 幽灰按钮（未激活） */
QPushButton[role="ghost"] {
    background: transparent;
    border: 2px solid #2196F3;
    color: #2196F3;
}

/* 实心按钮（激活） */
QPushButton[role="primary"] {
    background: #2196F3;
    border: none;
    color: #FFFFFF;
}

/* 危险按钮 */
QPushButton[role="danger"] {
    background: #FF5722;
    border: none;
    color: #FFFFFF;
}
```

#### 滑块规范（触摸友好）

```css
QSlider::groove:horizontal {
    height: 12px;
    background: #333;
    border-radius: 6px;
}

QSlider::handle:horizontal {
    width: 24px;
    height: 24px;
    background: #2196F3;
    border-radius: 12px;
    margin: -6px 0;
}
```

#### 输入框规范（下划线风格）

```css
QLineEdit, QSpinBox, QDoubleSpinBox {
    background: transparent;
    border: none;
    border-bottom: 2px solid #555;
    padding: 8px 4px;
    color: #FFFFFF;
}

QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus {
    border-bottom: 2px solid #2196F3;
}
```

### 9.4 DXF预览画布规范

```python
# 画布背景
CANVAS_BG = "#000000"
CANVAS_GRID = "#333333"
CANVAS_GRID_SPACING = 10  # mm对应像素

# 路径颜色
PATH_RAPID = "#555555"      # 空移 - 灰色虚线
PATH_DISPENSE = "#2196F3"   # 点胶 - 蓝色实线
PATH_CURRENT = "#FFC107"    # 当前点 - 黄色高亮
```

### 9.5 通知系统

#### Toast通知（非关键信息）

```python
# 位置: 右下角
# 动画: 淡入淡出 (QPropertyAnimation)
# 持续: 3秒后自动消失
# 样式:
TOAST_STYLE = """
    background: rgba(45, 45, 45, 0.9);
    border-radius: 8px;
    padding: 12px 20px;
    color: #FFFFFF;
"""
```

#### 报警横幅（严重错误）

```python
# 位置: 顶部全宽
# 行为: 阻断操作直到确认
# 样式:
ALARM_BANNER_STYLE = """
    background: #FF5722;
    padding: 16px;
    font-weight: bold;
    font-size: 16px;
    color: #FFFFFF;
"""
```

### 9.6 卡片式布局

```css
/* 面板卡片 */
QGroupBox {
    background: #2D2D2D;
    border-radius: 8px;
    padding: 16px;
    margin-top: 8px;
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.3);
}

QGroupBox::title {
    color: #888888;
    font-size: 12px;
    text-transform: uppercase;
    letter-spacing: 1px;
}
```

## 10. 生产统计功能（新增）

### 10.1 背景

生产型HMI需要让操作员实时了解生产进度，这是调试工具与生产工具的核心区别。

### 10.2 功能需求

**生产Tab顶部统计区**:
```
┌─────────────────────────────────────────────────────────────┐
│ 生产统计                                                     │
│ ┌───────────────┬───────────────┬───────────────────────────┐│
│ │ 已完成        │ 目标          │ 完成率                    ││
│ │   127         │   200         │ ████████░░░░ 63.5%        ││
│ └───────────────┴───────────────┴───────────────────────────┘│
│ 周期时间: 12.3s  |  UPH: 292  |  运行时间: 2h15m  |  良率: 99.2% │
└─────────────────────────────────────────────────────────────┘
```

### 10.3 数据定义

| 指标 | 计算方式 | 更新频率 |
|------|----------|----------|
| 已完成 | 累计完成的点胶周期数 | 每周期 |
| 目标 | 用户设定的目标产量 | 手动设置 |
| 完成率 | 已完成 / 目标 * 100% | 每周期 |
| 周期时间 | 单次点胶周期耗时 | 每周期 |
| UPH | 3600 / 平均周期时间 | 每分钟 |
| 运行时间 | 累计运行时间（不含暂停） | 每秒 |
| 良率 | (总数 - 异常数) / 总数 * 100% | 每周期 |

### 10.4 data-testid规范

```python
# 生产统计
"label-completed-count"      # 已完成数量
"label-target-count"         # 目标数量
"progress-completion"        # 完成率进度条
"label-completion-rate"      # 完成率百分比
"label-cycle-time"           # 周期时间
"label-uph"                  # 每小时产量
"label-run-time"             # 运行时间
"label-yield-rate"           # 良率

# 目标设置
"input-target-count"         # 目标数量输入
"btn-reset-counter"          # 重置计数器
```

### 10.5 后端支持

需要在TCP协议中添加以下命令：

| 命令 | 参数 | 返回 |
|------|------|------|
| `production.status` | - | 生产统计数据 |
| `production.set_target` | `count: int` | 成功/失败 |
| `production.reset` | - | 成功/失败 |

---

## 11. 配方管理功能（新增）

### 11.1 背景

配方（Recipe）是点胶机的核心概念，包含完成一个产品所需的全部参数。没有配方管理就无法实现"一键换产"。

### 11.2 配方数据结构

```python
@dataclass
class Recipe:
    name: str                    # 配方名称
    version: str                 # 版本号
    dxf_path: str               # DXF文件路径

    # 运动参数
    max_velocity: float         # 最大速度 (mm/s)
    max_acceleration: float     # 最大加速度 (mm/s^2)

    # 点胶参数
    dispense_speed: float       # 点胶速度 (mm/s)
    dispense_height: float      # 点胶高度 (mm)

    # 阀门参数
    valve_on_delay: int         # 开阀延迟 (ms)
    valve_off_delay: int        # 关阀延迟 (ms)

    # 工艺参数
    purge_interval: int         # 清洗间隔 (次)
    purge_duration: int         # 清洗时长 (ms)

    # 元数据
    created_at: str             # 创建时间
    modified_at: str            # 修改时间
    author: str                 # 作者
```

### 11.3 UI布局

**文件Tab配方管理区**:
```
┌─────────────────────────────────────────────────────────────┐
│ 配方管理                                                     │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ 当前配方: PCB-001-v2.3                                   │ │
│ │ 最后修改: 2026-01-20 14:30  作者: 张三                   │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│ [加载配方] [保存配方] [另存为] [配方列表]                     │
│                                                             │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ 配方参数                                                 │ │
│ │ ┌─────────────────┬─────────────────────────────────┐   │ │
│ │ │ 最大速度        │ [____] mm/s                     │   │ │
│ │ │ 最大加速度      │ [____] mm/s^2                   │   │ │
│ │ │ 点胶速度        │ [____] mm/s                     │   │ │
│ │ │ 点胶高度        │ [____] mm                       │   │ │
│ │ │ 开阀延迟        │ [____] ms                       │   │ │
│ │ │ 关阀延迟        │ [____] ms                       │   │ │
│ │ │ 清洗间隔        │ [____] 次                       │   │ │
│ │ └─────────────────┴─────────────────────────────────┘   │ │
│ └─────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### 11.4 data-testid规范

```python
# 配方信息
"label-recipe-name"          # 当前配方名称
"label-recipe-modified"      # 最后修改时间
"label-recipe-author"        # 作者

# 配方操作
"btn-recipe-load"            # 加载配方
"btn-recipe-save"            # 保存配方
"btn-recipe-save-as"         # 另存为
"btn-recipe-list"            # 配方列表

# 配方参数
"input-recipe-max-velocity"
"input-recipe-max-acceleration"
"input-recipe-dispense-speed"
"input-recipe-dispense-height"
"input-recipe-valve-on-delay"
"input-recipe-valve-off-delay"
"input-recipe-purge-interval"
```

### 11.5 配方存储

配方以JSON格式存储在本地目录：
```
recipes/
├── PCB-001-v2.3.json
├── PCB-002-v1.0.json
└── TEST-001.json
```

### 11.6 后端支持

| 命令 | 参数 | 返回 |
|------|------|------|
| `recipe.load` | `name: str` | 配方数据 |
| `recipe.save` | `recipe: Recipe` | 成功/失败 |
| `recipe.list` | - | 配方名称列表 |
| `recipe.delete` | `name: str` | 成功/失败 |

---

## 12. 权限管理功能（新增）

### 12.1 背景

生产环境需要权限分级，防止操作员误触高级功能导致设备损坏或生产中断。

### 12.2 角色定义

| 角色 | 权限级别 | 可访问功能 |
|------|:--------:|-----------|
| 操作员 | 1 | 生产Tab（开始/暂停/停止）、配方选择、产量查看 |
| 技术员 | 2 | 设置Tab（手动控制、参数调整）、配方编辑 |
| 工程师 | 3 | 全部功能 + 系统设置 + 配方删除 |

### 12.3 UI布局

**顶部用户栏**:
```
┌─────────────────────────────────────────────────────────────┐
│ [用户图标] 张三 (操作员)                    [切换用户] [登出] │
└─────────────────────────────────────────────────────────────┘
```

**登录对话框**:
```
┌─────────────────────────────────────────────────────────────┐
│                        用户登录                              │
│ ┌─────────────────────────────────────────────────────────┐ │
│ │ 用户名: [________________]                               │ │
│ │ 密码:   [________________]                               │ │
│ └─────────────────────────────────────────────────────────┘ │
│                                                             │
│                    [登录]  [取消]                            │
└─────────────────────────────────────────────────────────────┘
```

### 12.4 权限控制逻辑

```python
# 功能权限映射
PERMISSION_MAP = {
    # 生产Tab - 操作员可用
    "btn-production-start": 1,
    "btn-production-pause": 1,
    "btn-production-stop": 1,
    "btn-recipe-load": 1,

    # 设置Tab - 技术员可用
    "btn-jog-*": 2,
    "btn-home-*": 2,
    "input-recipe-*": 2,
    "btn-recipe-save": 2,

    # 高级功能 - 工程师可用
    "btn-recipe-delete": 3,
    "btn-system-settings": 3,
}

def check_permission(widget_id: str, user_level: int) -> bool:
    required_level = PERMISSION_MAP.get(widget_id, 1)
    return user_level >= required_level
```

### 12.5 data-testid规范

```python
# 用户栏
"label-current-user"         # 当前用户名
"label-user-role"            # 用户角色
"btn-switch-user"            # 切换用户
"btn-logout"                 # 登出

# 登录对话框
"dialog-login"               # 登录对话框
"input-username"             # 用户名输入
"input-password"             # 密码输入
"btn-login"                  # 登录按钮
"btn-login-cancel"           # 取消按钮
```

### 12.6 简化实现（MVP）

初期可采用简化方案：
- 不需要数据库，用户信息硬编码或存储在配置文件
- 密码使用简单PIN码（4-6位数字）
- 自动登出：无操作5分钟后自动降级为操作员

---

## 13. 维护提醒功能（新增）

### 13.1 背景

点胶机需要定期维护，预防性维护提醒可以减少故障停机。

### 13.2 维护项目

| 维护项 | 触发条件 | 提醒方式 |
|--------|----------|----------|
| 针头更换 | 点胶次数 >= 10000 | 黄色警告 |
| 定期清洗 | 距上次清洗 >= 2小时 | 黄色警告 |
| 胶水更换 | 距上次更换 >= 8小时 | 黄色警告 |
| 滤芯更换 | 运行时间 >= 500小时 | 黄色警告 |

### 13.3 UI布局

**状态栏维护提醒**:
```
┌─────────────────────────────────────────────────────────────┐
│ [!] 维护提醒: 针头使用 9856/10000 次，建议更换              │
└─────────────────────────────────────────────────────────────┘
```

### 13.4 data-testid规范

```python
# 维护提醒
"alert-maintenance"          # 维护提醒条
"label-needle-count"         # 针头使用次数
"label-last-purge-time"      # 上次清洗时间
"label-glue-change-time"     # 上次换胶时间
"btn-reset-needle-count"     # 重置针头计数
"btn-reset-purge-timer"      # 重置清洗计时
```

---

## 14. 备注

- 本计划仅涉及规划，不包含实施
- 实施前需用户审批
- P3功能可根据实际需求调整优先级
- 所有修改需遵循六边形架构规则
- **HMI定位为生产工具**，CLI为调试工具
- **轴配置**: 仅X/Y两轴（无Z轴）
- **UI风格**: 赛博工业风（ISA-101合规）
- **颜色规范**: 遵循ISA-101颜色保留原则
- **新增功能**: 生产统计、配方管理、权限管理、维护提醒

## 15. 参考资料

- [ISA-101 HMI标准](https://pipecom.com/2024/06/20/revolutionizing-operator-efficiency-embracing-isa-101-standards-in-hmi-design/)
- [ANSI/ISA-101.01-2015](https://www.isa.org/products/ansi-isa-101-01-2015-human-machine-interfaces-for)
- [Nordson EFD点胶系统](https://www.nordson.com/en/products/efd-products/automated-dispensing-systems)
- [PyQtGraph实时可视化](https://github.com/pyqtgraph/pyqtgraph)
