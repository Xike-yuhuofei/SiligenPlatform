# DXF 点胶流程全链路分析报告

**日期**: 2026-02-01  
**范围**: DXF 文件加载、解析、轨迹规划至点胶执行的端到端流程  
**代码库版本状态**: 基于 `src/application/usecases/dispensing/dxf` 及相关 Adapter 实现

本文档整合了对 DXF 点胶业务流程的代码静态分析，包含两份时序图：
1.  **全流程概览**：从 TCP 入口到硬件执行的宏观视图。
2.  **规划阶段详解**：`UnifiedTrajectoryPlanner` 内部几何处理与速度规划（前后向扫描）的微观视图。

---

## 1. 全流程概览 (Loading -> Execution)

本阶段展示系统如何响应 `dxf.load` 和 `dxf.execute` 指令，并协调 UseCase、Parser 和 Driver 完成任务。

### 时序图

```mermaid
sequenceDiagram
    autonumber
    
    %% 参与者定义
    actor User as Client (HMI/CLI)
    participant TCP as TcpCommandDispatcher
    participant UC_Load as UploadDXFFileUseCase
    participant UC_Exec as DXFDispensingExecutionUseCase
    participant Planner as UnifiedTrajectoryPlanner
    
    box "Domain Services (算法核心)" #f9f9f9
        participant Geo as GeometryNormalizer
        participant Annot as ProcessAnnotator
        participant Motion as MotionPlanner
    end
    
    box "Infrastructure (硬件适配器)" #e6e6e6
        participant Parser as DXFFileParsingAdapter
        participant Port_Interp as InterpolationAdapter
        participant Port_Trig as TriggerControllerAdapter
        participant Port_Valve as ValveAdapter
    end

    %% ==========================================
    %% 阶段 1: 加载 (Loading)
    %% ==========================================
    Note over User, TCP: 1. 用户上传 DXF 文件
    User->>TCP: dxf.load (filepath)
    TCP->>UC_Load: Execute(request)
    
    activate UC_Load
    UC_Load->>UC_Load: GenerateSafeFilename()
    UC_Load->>Parser: ParseGeometry(filepath)
    activate Parser
    Note right of Parser: 读取文件实体 (LINE/ARC)<br/>转换为 GeometrySegment
    Parser-->>UC_Load: segments
    deactivate Parser
    UC_Load-->>TCP: Loaded (metadata)
    deactivate UC_Load
    TCP-->>User: Success (id, bounds)

    %% ==========================================
    %% 阶段 2: 规划 (Planning)
    %% ==========================================
    Note over User, TCP: 2. 用户请求执行点胶
    User->>TCP: dxf.execute (speed, accel)
    TCP->>UC_Exec: Execute(request)
    
    activate UC_Exec
    UC_Exec->>UC_Exec: ValidateHardwareConnection()
    
    Note right of UC_Exec: 调用核心规划器
    UC_Exec->>Planner: Plan(segments, config)
    activate Planner
    
    Planner->>Geo: Normalize(segments)
    Note right of Geo: 单位缩放 + 连续性对齐（不含旋转/平移）
    
    Planner->>Annot: Annotate(path)
    Note right of Annot: 识别拐角、直线、空跑段
    
    Planner->>Motion: Plan(shaped_path, motion_config)
    activate Motion
    Note right of Motion: ★ 核心算法: 速度上界 + 前后向扫描（梯形）<br/>Jerk 默认仅统计，可选启用 Ruckig 严格约束
    Motion-->>Planner: MotionTrajectory (PVT Points)
    deactivate Motion
    
    Planner-->>UC_Exec: PlanResult
    deactivate Planner

    %% ==========================================
    %% 阶段 3: 执行 (Execution)
    %% ==========================================
    Note right of UC_Exec: 准备硬件环境
    UC_Exec->>Port_Interp: ClearInterpolationBuffer()
    
    Note right of Port_Trig: 设置位置触发 (Position Structured Triggering)
    UC_Exec->>Port_Trig: SetSequenceTrigger(positions)
    
    UC_Exec->>Port_Valve: OpenSupply()
    Note right of Port_Valve: 打开供胶主阀
    
    Note right of UC_Exec: 预填充缓冲区
    UC_Exec->>Port_Interp: AddInterpolationData(initial_batch)
    UC_Exec->>Port_Interp: StartCoordinateSystemMotion()
    
    loop While points remaining
        Note right of UC_Exec: 实时填充循环
        UC_Exec->>Port_Interp: GetInterpolationBufferSpace()
        alt Space Available
            UC_Exec->>Port_Interp: AddInterpolationData(batch)
            UC_Exec->>Port_Interp: FlushInterpolationData()
        else Buffer Full
            UC_Exec->>UC_Exec: Sleep(5ms)
        end
    end
    
    UC_Exec->>UC_Exec: WaitForMotionComplete()
    
    UC_Exec->>Port_Valve: CloseSupply()
    UC_Exec-->>TCP: ExecutionResult (Success)
    deactivate UC_Exec
    
    TCP-->>User: Success
```

### 关键说明
1.  **加载与规划分离**：DXF 解析在加载阶段完成（`ParseGeometry`），而轨迹规划在执行阶段触发。这允许用户调整速度参数而无需重新解析文件。
2.  **缓冲流控**：执行阶段采用“生产者-消费者”模式，`UC_Exec` 负责监控硬件缓冲区状态并动态填充数据。
3.  **位置触发**：点胶阀的开关不是由软件实时控制，而是预先下发位置序列（`SetSequenceTrigger`），由硬件在运动到位时自动触发。

---

## 2. 规划阶段详解 (UnifiedTrajectoryPlanner)

本阶段深入 `UnifiedTrajectoryPlanner` 内部，展示原始几何段如何转化为包含时间戳的运动轨迹。

### 时序图

```mermaid
sequenceDiagram
    autonumber
    
    participant UC as DXFDispensingExecutionUseCase
    participant Unified as UnifiedTrajectoryPlanner
    
    box "Step 1: 几何预处理" #f0f0f5
        participant Geo as GeometryNormalizer
    end
    
    box "Step 2: 路径整形与修补" #e6f2ff
        participant Shaper as TrajectoryShaper
        participant Approx as SplineApproximation
    end
    
    box "Step 3: 工艺与约束" #fff0e6
        participant Annot as ProcessAnnotator
    end
    
    box "Step 4: 运动规划 (核心算法)" #e6ffe6
        participant Motion as MotionPlanner
    end

    %% 入口
    UC->>Unified: Plan(primitives, config)
    activate Unified

    %% ---------------------------------------------------------
    %% Step 1: 几何归一化 (Geometry Normalization)
    %% ---------------------------------------------------------
    Unified->>Geo: Normalize(primitives)
    activate Geo
    Note right of Geo: 1. 应用单位缩放 (Scale)<br/>2. 连续性校正（段首对齐）
    Geo-->>Unified: Normalized Path
    deactivate Geo

    %% ---------------------------------------------------------
    %% Step 2: 路径整形 (Trajectory Shaping)
    %% ---------------------------------------------------------
    Unified->>Annot: Annotate(normalized_path) 
    %% (注: 代码中 Annotate 在 Shape 之前，根据 UnifiedTrajectoryPlanner.cpp)
    activate Annot
    Note right of Annot: 1. 识别 Segment 类型 (Line/Arc)<br/>2. 标记 Start/End/Corner
    Annot-->>Unified: Process Path (Annotated)
    deactivate Annot

    Unified->>Shaper: Shape(process_path)
    activate Shaper
    Shaper->>Approx: ApproximateSplines(if needed)
    activate Approx
    Note right of Approx: 将复杂样条曲线(Spline)<br/>离散化为微小线段或圆弧
    Approx-->>Shaper: Approximated Segments
    deactivate Approx
    Shaper-->>Unified: Shaped Path
    deactivate Shaper

    %% ---------------------------------------------------------
    %% Step 3: 运动规划 (Motion Planning) - Trapezoidal Core
    %% ---------------------------------------------------------
    Unified->>Motion: Plan(shaped_path, motion_config)
    activate Motion
    
    Note right of Motion: A. 路径离散化 (Discretization)
    Motion->>Motion: Compute Total Length & Segment Lengths
    Motion->>Motion: Generate S-Samples (path distance points)
    
    Note right of Motion: B. 速度限制计算 (Velocity Limits)
    loop For each sample point
        Motion->>Motion: Calculate Curvature (1/Radius)
        Motion->>Motion: V_limit = min(V_max, sqrt(A_max / Curvature))
        Note right of Motion: 限制: 拐弯处必须减速<br/>以满足向心加速度约束
    end
    
    Note right of Motion: C. 双向扫描 (Bi-directional Scan)
    loop Forward Pass (Accel)
        Motion->>Motion: V[i] = min(V[i], sqrt(V[i-1]^2 + 2*Accel_max*ds))
    end
    loop Backward Pass (Decel)
        Motion->>Motion: V[i] = min(V[i], sqrt(V[i+1]^2 + 2*Accel_max*ds))
        Note right of Motion: 确保能够安全减速停车
    end
    
    Note right of Motion: D. 时间积分 (Time Integration)
    loop Integration
        Motion->>Motion: t[i] = t[i-1] + (ds / V_avg)
    end
    
    Note right of Motion: E. 轨迹点生成 (Generation)
    loop Interpolation
        Motion->>Motion: Interpolate (Pos, Vel) at time t
        Motion->>Motion: Calculate FlowRate (Glue Volume)
    end

    Motion-->>Unified: MotionTrajectory (PVT Points)
    deactivate Motion

    %% 返回结果
    Unified-->>UC: PlanResult
    deactivate Unified
```

### 算法细节解析

1.  **ProcessAnnotator (工艺标注)**
    *   **输入**: 几何路径 (GeometrySegment列表)。
    *   **输出**: 带标签的路径 (ProcessPath)。
    *   **核心逻辑**: 识别轨迹中的关键特征。例如，将非点胶的连接移动标记为 `Rapid` (空跑)，将两段线夹角较小的连接点标记为 `Corner` (拐角)。这些标签直接影响后续的速度规划策略（例如 Corner 处强制减速）。

2.  **SplineApproximation (样条拟合)**
    *   **目的**: 将 DXF 中的 NURBS 样条曲线转换为机器控制器能理解的简单几何图元（通常是微直线段或圆弧）。
    *   **实现**: 使用误差步长与自适应步长进行离散化；无递归分割。

3.  **MotionPlanner (运动规划)**
    *   **策略**: 采用“路径-速度解耦”法。先规划几何路径，再在路径上规划速度分布。
    *   **速度曲线**: 以速度上界为基础，通过双向扫描进行加速度可达性裁剪（梯形规划）。若外部提供 S-Curve 剖面，也会被裁剪以满足路径约束。
    *   **Jerk 约束**: 默认仅统计，不参与速度规划；可选启用 Ruckig 严格约束（`enforce_jerk_limit`）。
    *   **约束条件**:
        *   **最大速度 (V_max)**: 机器物理限制或工艺限制。
        *   **向心加速度 (A_cent)**: `V^2 / R <= A_max`，限制过弯速度。
        *   **最大加速度 (A_max)**: 电机扭矩限制。

---
**Note**: 本文档由 Gemini CLI 自动生成，基于 `src` 目录下的源代码分析。
