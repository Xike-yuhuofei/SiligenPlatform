# DXF Dispensing Use Cases

## 职责

基于 DXF 文件的点胶路径规划和执行。

## 命名空间

```cpp
namespace Siligen::Application::UseCases::Dispensing
```

## Use Cases

### UploadFileUseCase（由 `job-ingest` owner）
- **职责**: 上传和验证 DXF 文件的 owner 面已归位到 `modules/job-ingest/`
- **workflow 定位**: workflow 通过 `job-ingest/contracts` 消费 `UploadRequest`、`UploadResponse` 与 `IUploadFilePort`，继续执行 `CreateArtifact -> PreparePlan -> Preview/Job` 编排链

### PlanningUseCase
- **职责**: 编排 `.pb -> process path -> motion plan -> execution package/preview` 生成链
- **使用场景**: 路径预览和编辑
- **补充**: 可选启用 Domain 插补算法，但 live 链已改为通过 `ProcessPathFacade`、`MotionPlanningFacade`、`AuthorityPreviewAssemblyService`、`ExecutionAssemblyService` 组合完成，不再注入 `DispensingPlanner` concrete

### DispensingExecutionUseCase
- **职责**: 执行 DXF 点胶任务
- **使用场景**: 自动点胶流程
- **依赖**: Domain::Dispensing::DomainServices::DispensingProcessService
- **说明**: 硬件插补程序由 Domain 统一入口生成与校验（`InterpolationProgramPlanner` / `ValidatedInterpolationPort`），应用层只做编排与调用
- **触发技术说明**: 采用 **规划位置触发 (Planned Position Triggering)**。
  > 触发点由规划阶段生成并作为执行阶段唯一来源，位置触发不可用时直接失败，不回退到定时触发。
  > 定时触发仅保留在 HMI 设置页的点胶阀单独控制（调试链路），不参与 DXF 执行。

### 点胶均匀性优化策略 (Dispensing Optimization Strategies)

为缓解梯形速度曲线导致的起笔/收笔堆胶问题，系统支持以下优化策略（可通过 `machine_config.ini` 中的 `[Dispensing] strategy` 配置）：

1.  **BASELINE (默认)**
    - **逻辑**: 全程使用固定空间间距触发点，间距由目标匀速与阀门节拍换算。
    - **优点**: 简单，最稳定。
    - **缺点**: 加减速段严重堆胶。

2.  **SEGMENTED (Phase 1)**
    - **逻辑**: 将运动拆分为3段（加速、匀速、减速）。加速/减速段使用平均速度换算触发间距。
    - **优点**: 显著减少堆胶，风险低。
    - **缺点**: 加速段内部仍有轻微不均。

3.  **SUBSEGMENTED (Phase 1.5 - 推荐)**
    - **逻辑**: 将加速/减速段进一步细分为 N 个子段（默认 N=8），每段独立换算触发间距。
    - **优点**: 触发点间距更贴合速度曲线，均匀性更好。
    - **配置**: `subsegment_count` (默认 8)。

### CleanupFilesUseCase
- **职责**: 清理过期的 DXF 文件
- **使用场景**: 存储管理

## 设计原则

1. DXF 上传/归档 owner 面由 `job-ingest` 提供，workflow 只消费上传结果与文件事实
2. 触发点/阀门时序与过程校验由 `DispensingProcessService` 统一实现
3. 插补策略/校验/程序生成由 Domain 统一入口负责，基础设施仅硬件调用
4. 提供点胶进度监控
5. 支持暂停/恢复/取消操作
6. 路径优化仅在轮廓级进行：保持实体内顺序，闭合轮廓只允许起点旋转，开放轮廓允许整体反向

## DXF 预处理边界

### `.pb` 的定位

- `.pb` 是 **运行时中间格式**，由 DXF 预处理管线从 DXF 生成。
- DXF 不是直接被 C++ 规划器消费；规划/预览/执行统一读取 `.pb`。
- DXF 上传 owner 面由 `job-ingest` 承载；workflow 只在预览、规划、执行前消费统一准备好的输入事实，并通过 `DxfPbPreparationService` 确保 `.pb` 已存在、非空且时间戳不落后于源 DXF。
- `DxfPbPreparationService` 默认调用本仓库内的 `scripts/engineering-data/dxf_to_pb.py`；仅在显式设置 `SILIGEN_DXF_PROJECT_ROOT` 且路径有效时才调用外部 DXF 算法仓库，并向其 launcher 传递 canonical 子命令 `engineering-data-dxf-to-pb`。
- `PlanningUseCase` 在主链路中接收 `EnsurePbReady` 返回的 `.pb` 路径，并继续编排 `M6 -> M7 -> M8` owner facade；live 链不再直接消费 `DispensingPlannerService`。
- `ContourAugmenterAdapter` 已下沉到 `infrastructure/adapters/planning/geometry`，应用层不再直接承载几何增广实现。
- `AutoPathSourceAdapter` 的 legacy DXF 自动预处理分支默认禁用；仅在显式设置 `SILIGEN_DXF_AUTOPATH_LEGACY=1` 时允许临时回退。
- 上传阶段的校验不只看扩展名或文件头，还会实际跑预处理；若 DXF 无法解析，或预处理后没有导出任何受支持图元，上传会直接失败。

### 当前支持的 DXF 实体

- `LINE`
- `ARC`
- `CIRCLE`
- `LWPOLYLINE`
- `POLYLINE`
  当前仅按 2D 顶点/`bulge` 链处理，不覆盖 3D polyline / mesh 变体。
- `SPLINE`
- `ELLIPSE`
- `POINT`

### 当前不支持的 DXF 实体

- `INSERT` / 块引用
- `TEXT` / `MTEXT` / `ATTRIB`
- `HATCH`
- `DIMENSION` / `LEADER` / `MLEADER`
- `IMAGE`
- `XLINE` / `RAY`
- `REGION`
- `3DFACE` / `SOLID` / `TRACE`
- 3D `POLYLINE` / polygon mesh / polyface mesh
- 其他未在预处理脚本中显式处理的实体

### 样条当前策略与限制

- 默认情况下，预处理会尽量把 `SPLINE` 保留为样条图元写入 `.pb`。
- 当预处理配置 `approx_splines=true` 时，样条会先离散为折线轮廓再写入 `.pb`。
- 规划请求仍可结合 `approximate_splines`、`spline_max_step_mm`、`spline_max_error_mm` 做进一步近似或采样控制。
- 当前样条精度依赖 `geomdl`/`ezdxf` 展平结果、`spline_samples`、`spline_max_step`、`chordal` 等参数，不保证与原始 NURBS 完全等价。
- 若样条既无法提取控制点/拟合点，也无法成功展平为有效点列，则预处理会把该实体视为不可用；当整个文件最终导出不到任何受支持图元时，请求会失败。

## 备注

- DXFSegmentFilter 已移除，请使用轮廓级优化流程。 
