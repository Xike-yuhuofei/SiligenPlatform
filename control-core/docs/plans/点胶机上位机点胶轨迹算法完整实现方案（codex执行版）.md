# 点胶机上位机点胶轨迹算法完整实现方案（用于指导 Codex 执行）

> 本文档是一份**可直接交付给 Codex / 自动代码生成系统执行的工程级方案**，目标不是讲原理，而是**明确模块边界、数据结构、处理流程与实现顺序**。

---

## 1. 设计目标与总原则

### 1.1 总目标

- 在上位机（PC）侧完成**点胶轨迹的离线/准实时生成**
- 输出 **稳定、可复现、可调试** 的轨迹数据
- 支持真实工业点胶工艺，而非理论插补

### 1.2 核心设计原则（Codex 必须遵守）

1. **几何、工艺、运动三者严格分层**
2. **任何算法不得跨层读取/修改不属于本层的数据**
3. **所有参数必须配置化，不允许硬编码工艺规则**
4. **所有中间结果必须可序列化、可回放**

---

## 2. 总体架构与数据流

```
[PathSource]
     ↓
[GeometryNormalizer]
     ↓
[ProcessAnnotator]
     ↓
[TrajectoryShaper]
     ↓
[MotionPlanner]
     ↓
[ControllerAdapter]
```

Codex 需按 **自上而下模块化实现**，禁止反向依赖。

---

## 3. 核心数据模型（必须先实现）

### 3.1 基础数学结构

```python
class Vec2:
    x: float
    y: float

class Vec3:
    x: float
    y: float
    z: float
```

---

### 3.2 原始几何元素（Path Definition Layer）

```python
class Primitive: pass

class Line(Primitive):
    start: Vec2
    end: Vec2

class Arc(Primitive):
    center: Vec2
    radius: float
    start_angle: float
    end_angle: float
    cw: bool
```

约束：
- 此层 **禁止** 出现速度、时间、胶量字段

---

### 3.3 规范化路径段（Geometry Layer）

```python
class Segment:
    type: str              # 'line' | 'arc'
    length: float
    geom: object           # Line / Arc
```

```python
class Path:
    segments: list[Segment]
    closed: bool
```

---

### 3.4 工艺语义节点（Process Layer） ⭐

```python
class ProcessNode:
    pos: Vec3
    dispense_on: bool
    flow_rate: float
    tag: str               # 'normal' | 'corner' | 'start' | 'end'
```

```python
class ProcessPath:
    nodes: list[ProcessNode]
```

---

### 3.5 轨迹点（Motion Layer）

```python
class TrajectoryPoint:
    t: float
    pos: Vec3
    vel: Vec3
    dispense_on: bool
    flow_rate: float
```

```python
class Trajectory:
    points: list[TrajectoryPoint]
```

---

## 4. 各模块职责与实现要求

---

## 4.1 PathSource（路径来源模块）

### 职责

- 提供 `Primitive` 列表

### Codex 实现要求

- 至少实现：
  - 手动构造 API
  - 文件接口占位（DXF / Gerber 可后续扩展）

---

## 4.2 GeometryNormalizer（几何规范化）

### 输入

- `list[Primitive]`

### 输出

- `Path`

### 必须实现的处理

1. 坐标系与单位统一
2. 零长度段剔除
3. 连续性检查（端点误差阈值）
4. 计算每段长度

> 禁止：
> - 插补为点序列
> - 引入时间/速度

---

## 4.3 ProcessAnnotator（工艺标注） ⭐ 核心模块

### 输入

- `Path`
- `ProcessConfig`

```python
class ProcessConfig:
    default_flow: float
    lead_on_dist: float
    lead_off_dist: float
    corner_slowdown: bool
```

### 输出

- `ProcessPath`

### 必须实现逻辑

1. 起点前插入 **提前开胶点**
2. 终点前插入 **提前关胶点**
3. 标注 corner 节点
4. 支持 dispense_on = False 的空移段

---

## 4.4 TrajectoryShaper（轨迹连续化）

### 输入

- `ProcessPath`

### 输出

- `ProcessPath`（几何连续性增强）

### 必须实现

- 拐角圆角（基于最小半径）
- G1 连续性保证
- 圆角段继承工艺语义

---

## 4.5 MotionPlanner（运动与时间规划）

### 输入

- `ProcessPath`
- `MotionConfig`

```python
class MotionConfig:
    vmax: float
    amax: float
    jmax: float
    sample_dt: float
```

### 输出

- `Trajectory`

### 必须实现

1. 路径弧长参数化 s
2. 梯形或 S 曲线速度规划
3. 按 tag 限速（corner / start / end）
4. 胶量与速度同步（线性或表驱动）

---

## 4.6 ControllerAdapter（控制器适配）

### 输入

- `Trajectory`

### 输出

- 控制器指令序列（占位）

### 要求

- 核心轨迹结构 **不允许修改**
- 仅做格式转换

---

## 5. Codex 执行顺序（强制）

1. 实现 **所有数据结构**（不写算法）
2. 实现 GeometryNormalizer（含单元测试）
3. 实现 ProcessAnnotator（可打印节点）
4. 实现 TrajectoryShaper（只处理几何）
5. 实现 MotionPlanner（先 1D 再 2D）
6. 最后实现 ControllerAdapter

---

## 6. 验证与调试要求

Codex 必须提供：

- 任意 Path → Trajectory 的 JSON 导出
- 轨迹可视化接口（matplotlib 占位）
- 每一层的中间结果可单独运行

---

## 7. 明确禁止事项（重要）

- ❌ 在 Geometry 层引入时间
- ❌ 在 Motion 层修改工艺开关逻辑
- ❌ 在 Adapter 层重新规划轨迹
- ❌ 写“为了方便”的跨层访问

---

## 8. 交付定义

当 Codex 完成以下内容，视为方案成功落地：

- 输入一条折线 + 圆弧路径
- 输出连续、不停胶的点胶轨迹
- 能清楚指出：
  - 哪一段是工艺
  - 哪一段是运动

---

**本方案结束。后续任何新增需求，必须标明属于哪一层。**

