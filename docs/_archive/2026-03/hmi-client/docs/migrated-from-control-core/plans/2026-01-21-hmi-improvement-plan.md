# HMI完善方案计划

> 创建日期: 2026-01-21
> 状态: 待审批
> 参考: CLI客户端功能 (`src/adapters/cli/CommandHandlers.cpp`)

## 1. 背景与目标

### 1.1 当前状态

现有HMI (`src/adapters/hmi/ui/main_window.py`) 实现了基础功能：
- TCP连接/断开
- 硬件初始化
- 状态显示（轴位置、机器状态、点胶阀状态）
- 全轴回零
- X/Y/Z轴点动
- 停止/急停
- 点胶阀开/关

### 1.2 目标

参考CLI客户端功能，完善HMI以达到功能对等，同时遵循六边形架构和`.claude/rules`规范。

## 2. 功能差距分析

### 2.1 CLI vs HMI功能对比

| 功能类别 | CLI功能 | HMI现状 | 优先级 |
|---------|---------|---------|:------:|
| **连接管理** | | | |
| 硬件连接 | `connect` | 有 | - |
| 硬件断开 | `disconnect` | 缺失 | P1 |
| **状态监控** | | | |
| 轴位置/速度/状态 | `status` | 部分（仅位置） | P1 |
| IO状态 | `status` | 缺失 | P2 |
| 阀门状态 | `status` | 部分（仅点胶阀） | P2 |
| **运动控制** | | | |
| 单轴回零 | `home [axis]` | 缺失 | P1 |
| 全轴回零 | `home` | 有 | - |
| 点动 | `jog` | 有（无速度控制） | P1 |
| 步进移动 | `step` | 缺失 | P2 |
| 点位移动 | `move` | 缺失 | P1 |
| 停止 | `stop` | 有 | - |
| 急停 | `estop` | 有 | - |
| **点胶控制** | | | |
| 开始点胶 | `dispenser.start` | 有 | - |
| 暂停点胶 | `dispenser.pause` | 缺失 | P2 |
| 恢复点胶 | `dispenser.resume` | 缺失 | P2 |
| 停止点胶 | `dispenser.stop` | 有 | - |
| 清洗/排胶 | `purge` | 缺失 | P2 |
| 供料阀控制 | `supply.open/close` | 缺失 | P2 |
| **DXF功能** | | | |
| DXF路径规划 | `dxf.plan` | 缺失 | P3 |
| DXF执行 | `dxf.execute` | 缺失 | P3 |

### 2.2 优先级说明

- **P1 (高)**: 基础操作必需，影响日常使用
- **P2 (中)**: 完整功能需要，提升用户体验
- **P3 (低)**: 高级功能，可后续迭代

## 3. 架构合规性要求

### 3.1 六边形架构规则 (HEXAGONAL.md)

```
RULE: HEXAGONAL_DEPENDENCY_DIRECTION
MUST:
  infrastructure_depends_on_domain
  application_depends_on_domain
MUST_NOT:
  domain_depends_on_infrastructure

RULE: HEXAGONAL_LAYER_ORDER
MUST:
  adapter -> application -> domain
FORBIDDEN:
  reverse_dependency
```

**HMI合规要求**:
- HMI作为Driving Adapter，只能调用Application层UseCase
- 不能直接访问Domain层或Infrastructure层
- 通过TCP协议调用后端UseCase

### 3.2 适配器规则 (src/adapters/CLAUDE.md)

```
RULE: ADAPTER_NO_BYPASS_USECASE
FORBIDDEN:
  - Bypass UseCase to call Ports directly
  - Include business logic
MUST:
  - Execute operations through UseCases

RULE: ADAPTER_INPUT_VALIDATION
SCOPE: Adapters
MUST:
  - Basic validation (format, required, range)
  - Friendly error messages
```

**HMI合规要求**:
- 所有操作通过TCP协议调用后端UseCase
- 前端只做基本输入验证（格式、必填、范围）
- 提供友好的错误提示

### 3.3 UI测试规则 (UI_PLAYWRIGHT.md)

```
RULE: UI_TEST_SELECTORS
SCOPE: e2e_tests
MUST_PRIORITY:
  data-testid
  aria-label
  role
FORBIDDEN: fragile_selectors
```

**HMI合规要求**:
- 所有可交互元素必须添加`data-testid`属性
- 使用语义化的`aria-label`
- 避免使用脆弱的选择器（如CSS类名、XPath）

## 4. 实施方案

### 4.1 Phase 1: 协议层扩展 (TCP Server)

**目标**: 扩展TCP协议支持缺失的命令

**新增协议方法**:

| 方法 | 参数 | 说明 |
|------|------|------|
| `disconnect` | `{}` | 断开硬件连接 |
| `dispenser.pause` | `{}` | 暂停点胶 |
| `dispenser.resume` | `{}` | 恢复点胶 |
| `purge` | `{duration_ms}` | 清洗/排胶 |
| `supply.open` | `{}` | 打开供料阀 |
| `supply.close` | `{}` | 关闭供料阀 |
| `dxf.plan` | `{file_path}` | DXF路径规划 |
| `dxf.execute` | `{plan_id}` | 执行DXF点胶 |

**修改文件**:
- `src/adapters/tcp/TcpCommandDispatcher.cpp` - 添加命令处理器
- `src/adapters/hmi/client/protocol.py` - 添加客户端方法

### 4.2 Phase 2: 状态面板增强

**目标**: 完善状态监控功能

**UI改进**:

```
+------------------------------------------+
|              状态监控面板                  |
+------------------------------------------+
| 轴状态                                    |
| +------+----------+--------+------+-----+|
| | 轴   | 位置     | 速度   | 使能 | 回零 ||
| +------+----------+--------+------+-----+|
| | X    | 100.000  | 0.0    | [*]  | [*] ||
| | Y    | 200.000  | 0.0    | [*]  | [*] ||
| | Z    | 50.000   | 0.0    | [*]  | [ ] ||
| | U    | 0.000    | 0.0    | [ ]  | [ ] ||
| +------+----------+--------+------+-----+|
|                                          |
| IO状态                                    |
| +--------------------+-------------------+|
| | X+ 限位: [ ]       | X- 限位: [ ]      ||
| | Y+ 限位: [ ]       | Y- 限位: [ ]      ||
| | Z+ 限位: [ ]       | Z- 限位: [ ]      ||
| | 急停按钮: [ ]      | 门禁: [ ]         ||
| +--------------------+-------------------+|
|                                          |
| 阀门状态                                  |
| +--------------------+-------------------+|
| | 点胶阀: [关闭]     | 供料阀: [关闭]    ||
| +--------------------+-------------------+|
+------------------------------------------+
```

**data-testid规范**:
```python
# 轴状态
"axis-position-X", "axis-position-Y", "axis-position-Z", "axis-position-U"
"axis-velocity-X", "axis-velocity-Y", "axis-velocity-Z", "axis-velocity-U"
"axis-enabled-X", "axis-enabled-Y", "axis-enabled-Z", "axis-enabled-U"
"axis-homed-X", "axis-homed-Y", "axis-homed-Z", "axis-homed-U"

# IO状态
"io-limit-x-pos", "io-limit-x-neg"
"io-limit-y-pos", "io-limit-y-neg"
"io-limit-z-pos", "io-limit-z-neg"
"io-estop", "io-door"

# 阀门状态
"valve-dispenser", "valve-supply"
```

**修改文件**:
- `src/adapters/hmi/ui/main_window.py` - 重构状态面板

### 4.3 Phase 3: 控制面板增强

**目标**: 完善手动控制功能

**UI改进**:

```
+------------------------------------------+
|              手动控制面板                  |
+------------------------------------------+
| 回零控制                                  |
| +--------------------------------------+ |
| | [全轴回零] [X回零] [Y回零] [Z回零]   | |
| +--------------------------------------+ |
|                                          |
| 点动控制                                  |
| +--------------------------------------+ |
| | 速度: [====|====] 20.0 mm/s          | |
| |                                      | |
| |        [Y+]                          | |
| |   [X-]      [X+]    [Z+]             | |
| |        [Y-]         [Z-]             | |
| +--------------------------------------+ |
|                                          |
| 点位移动                                  |
| +--------------------------------------+ |
| | X: [_______] Y: [_______]            | |
| | Z: [_______] 速度: [_______] mm/s    | |
| | [移动到位置]                          | |
| +--------------------------------------+ |
|                                          |
| 停止控制                                  |
| +--------------------------------------+ |
| | [停止] [急停]                         | |
| +--------------------------------------+ |
+------------------------------------------+
```

**data-testid规范**:
```python
# 回零
"btn-home-all", "btn-home-X", "btn-home-Y", "btn-home-Z"

# 点动
"slider-jog-speed", "label-jog-speed"
"btn-jog-X-plus", "btn-jog-X-minus"
"btn-jog-Y-plus", "btn-jog-Y-minus"
"btn-jog-Z-plus", "btn-jog-Z-minus"

# 点位移动
"input-move-X", "input-move-Y", "input-move-Z", "input-move-speed"
"btn-move-to-position"

# 停止
"btn-stop", "btn-estop"
```

**修改文件**:
- `src/adapters/hmi/ui/main_window.py` - 重构控制面板

### 4.4 Phase 4: 点胶控制面板

**目标**: 完善点胶控制功能

**UI改进**:

```
+------------------------------------------+
|              点胶控制面板                  |
+------------------------------------------+
| 点胶阀控制                                |
| +--------------------------------------+ |
| | [开始] [暂停] [恢复] [停止]           | |
| +--------------------------------------+ |
|                                          |
| 供料阀控制                                |
| +--------------------------------------+ |
| | [打开供料阀] [关闭供料阀]             | |
| +--------------------------------------+ |
|                                          |
| 清洗/排胶                                 |
| +--------------------------------------+ |
| | 时长: [_______] ms  [执行清洗]        | |
| +--------------------------------------+ |
+------------------------------------------+
```

**data-testid规范**:
```python
# 点胶阀
"btn-dispenser-start", "btn-dispenser-pause"
"btn-dispenser-resume", "btn-dispenser-stop"

# 供料阀
"btn-supply-open", "btn-supply-close"

# 清洗
"input-purge-duration", "btn-purge"
```

**修改文件**:
- `src/adapters/hmi/ui/main_window.py` - 添加点胶控制面板

### 4.5 Phase 5: DXF功能面板 (P3)

**目标**: 添加DXF路径规划和执行功能

**UI改进**:

```
+------------------------------------------+
|              DXF功能面板                   |
+------------------------------------------+
| 文件选择                                  |
| +--------------------------------------+ |
| | [浏览...] path/to/file.dxf           | |
| +--------------------------------------+ |
|                                          |
| 路径规划                                  |
| +--------------------------------------+ |
| | [规划路径]                            | |
| | 状态: 已规划 / 未规划                  | |
| | 路径数: 12                            | |
| | 总长度: 1234.5 mm                     | |
| +--------------------------------------+ |
|                                          |
| 执行控制                                  |
| +--------------------------------------+ |
| | [开始执行] [暂停] [停止]              | |
| | 进度: [=========>          ] 45%     | |
| +--------------------------------------+ |
+------------------------------------------+
```

**data-testid规范**:
```python
# 文件选择
"btn-dxf-browse", "label-dxf-path"

# 路径规划
"btn-dxf-plan", "label-plan-status"
"label-path-count", "label-total-length"

# 执行控制
"btn-dxf-execute", "btn-dxf-pause", "btn-dxf-stop"
"progress-dxf-execution"
```

**修改文件**:
- `src/adapters/hmi/ui/main_window.py` - 添加DXF面板

## 5. 文件修改清单

### 5.1 TCP Server端 (C++)

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `src/adapters/tcp/TcpCommandDispatcher.cpp` | 修改 | 添加新命令处理器 |
| `src/adapters/tcp/TcpCommandDispatcher.h` | 修改 | 添加新方法声明 |

### 5.2 HMI端 (Python)

| 文件 | 修改类型 | 说明 |
|------|---------|------|
| `src/adapters/hmi/client/protocol.py` | 修改 | 添加新协议方法 |
| `src/adapters/hmi/ui/main_window.py` | 重构 | 完善UI布局和功能 |

### 5.3 新增文件

无需新增文件，所有改动在现有文件中完成。

## 6. 实施顺序

```
Phase 1 (协议扩展)
    │
    ├── 1.1 TCP Server添加disconnect命令
    ├── 1.2 TCP Server添加dispenser.pause/resume命令
    ├── 1.3 TCP Server添加purge命令
    ├── 1.4 TCP Server添加supply.open/close命令
    └── 1.5 Python client添加对应方法
    │
    v
Phase 2 (状态面板)
    │
    ├── 2.1 添加轴详细状态显示（速度、使能、回零）
    ├── 2.2 添加IO状态显示
    └── 2.3 添加供料阀状态显示
    │
    v
Phase 3 (控制面板)
    │
    ├── 3.1 添加单轴回零按钮
    ├── 3.2 添加点动速度滑块
    └── 3.3 添加点位移动功能
    │
    v
Phase 4 (点胶面板)
    │
    ├── 4.1 添加暂停/恢复按钮
    ├── 4.2 添加供料阀控制
    └── 4.3 添加清洗功能
    │
    v
Phase 5 (DXF面板) [P3 - 可选]
    │
    ├── 5.1 添加文件选择
    ├── 5.2 添加路径规划
    └── 5.3 添加执行控制
```

## 7. 验收标准

### 7.1 功能验收

- [ ] 所有P1功能实现并可用
- [ ] 所有P2功能实现并可用
- [ ] 状态显示实时更新（200ms刷新）
- [ ] 错误提示友好且准确

### 7.2 架构验收

- [ ] 所有操作通过TCP协议调用后端
- [ ] 无直接Domain/Infrastructure依赖
- [ ] 输入验证在前端完成

### 7.3 UI测试验收

- [ ] 所有可交互元素有`data-testid`
- [ ] 选择器符合UI_PLAYWRIGHT.md规范
- [ ] 无脆弱选择器使用

## 8. 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| TCP协议扩展复杂 | 开发延迟 | 复用现有命令处理模式 |
| UI布局调整大 | 用户体验变化 | 保持核心操作位置不变 |
| 状态刷新性能 | UI卡顿 | 使用异步更新，避免阻塞 |

## 9. 备注

- 本计划仅涉及规划，不包含实施
- 实施前需用户审批
- P3功能可根据实际需求调整优先级
