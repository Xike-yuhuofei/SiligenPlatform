# NOISSUE 架构方案 - 轨迹预览点状化与交点小尾巴修复

- 状态：Implemented (As-Built)
- 日期：2026-03-22
- 分支：feat/hmi/NOISSUE-dispense-trajectory-preview-v2
- 关联范围文档：NOISSUE-scope-20260322-042851.md
- 范围结论：以 NOISSUE-scope-20260322-042851.md 为冻结基线

## 1. 当前代码状态复核

1. 工作树存在并行改动（HMI、runtime、gateway、docs 多处文件已修改），本需求实现需严格限制改动面，避免范围污染。
2. 真实链路已可用（transport-gateway + runtime-host 在 9527 端口可连通），后续验证必须以真实链路为主，不使用 mock 作为验收依据。
3. 预览点状几何以 runtime 产出的 `trajectory_polyline` 为唯一真值；HMI 仅做坐标映射与点绘制，不允许引入额外显示层点过滤。

## 2. 受影响目录与文件类型

1. apps
   - apps/hmi-app/src/hmi_client/ui/main_window.py（Python）
   - apps/hmi-app/tests/unit/*.py（Python 单测）
2. packages
   - packages/process-runtime-core/src/application/usecases/dispensing/DispensingWorkflowUseCase.cpp（C++）
   - packages/process-runtime-core/src/application/usecases/dispensing/PlanningUseCase.cpp（C++，仅用于统计与诊断验证，不改执行语义）
   - packages/process-runtime-core/tests/unit/dispensing/*.cpp（C++ 单测）
   - packages/transport-gateway/src/tcp/TcpCommandDispatcher.cpp（C++，契约透传一致性复核）
   - packages/transport-gateway/tests/*.py（兼容性回归）
3. docs
   - docs/process-model/plans/*.md
   - docs/process-model/reviews/*.md

## 3. 技术方案（组件、依赖、边界、回滚）

### 3.1 总体策略

采用“算法层收敛 + 显示层直显”方案，目标是几何真实性与可追溯一致性：

1. 数据层（runtime 预览 polyline）
   - 在 DispensingWorkflowUseCase 的预览点集构建阶段新增轻量清洗与采样守护：
   - 连续重复点去重（epsilon）
   - 短程 ABA 回折尾巴抑制（A≈C 且 AB 为短回折）
   - 胶点圆心固定间距采样：3.0 mm（沿轨迹弧长）
   - 角点保真采样（防止简单步长抽样吞角）
   - 保底机制：若清洗后点数异常下降，回退到原始采样路径

2. 表现层（HMI 点状渲染）
   - 保持“仅点元素”主轨迹表达，不引入线段连线
   - 不做屏幕空间二次降采样/过滤，直接 1:1 渲染 runtime 返回点集
   - 胶点直径按 1.5 mm 映射为画布像素半径，仅影响点大小，不改变点位
   - 保留当前点样式和视口映射逻辑，但不改变点集拓扑

3. 兼容边界
   - 不新增协议命令
   - 不修改 preview confirm / snapshot hash / start gate 语义
   - max_polyline_points 上限夹断保持现有策略

### 3.2 数据流与调用流

DXF 文件
  -> PlanningUseCase::Execute
  -> PlanningResponse.trajectory_points
  -> DispensingWorkflowUseCase::BuildPreviewSnapshotResponse
     -> BuildPreviewPolyline（新增去重/去尾巴/角点保真采样）
  -> transport-gateway dxf.preview.snapshot
  -> HMI protocol.dxf_preview_snapshot
  -> main_window._extract_preview_points
  -> main_window._render_runtime_preview_html（仅坐标映射 + 点绘制，不做点过滤）
  -> 预览画布（点状几何）

控制流保持不变：
preview_confirm -> snapshot_hash 校验 -> dxf.job.start gate

### 3.3 失败模式与补救策略

1. 失败模式：尾巴抑制误删真实几何短段
   - 补救：仅在满足 A≈C + AB/BC 均低于短回折阈值时触发；加入最小保留点保护
2. 失败模式：显示层误改点集导致“看起来正确但不是算法真实结果”
   - 补救：明确禁止 UI 层过滤；以 `polyline_point_count` 与 HMI 渲染点数一致性作为验收项
3. 失败模式：点太密导致视觉线带感
   - 补救：通过 runtime 点集质量（去重、去尾巴、角点保真）优化，而非 UI 降采样
4. 失败模式：性能退化（大点集）
   - 补救：所有新增算法保持 O(n)，不引入全局高复杂度计算
5. 失败模式：门禁回归
   - 补救：把 gate 相关测试列为阻断项，任何失败一律不放行

### 3.4 性能、稳定性与安全风险

1. 性能
   - 风险：高点数下渲染 CPU 增加
   - 控制：保持线性复杂度；复用现有 max_polyline_points=4000 上限
2. 稳定性
   - 风险：清洗阈值不当引发形状漂移
   - 控制：阈值常量显式化 + 单测覆盖矩形/对角/角点场景
3. 安全
   - 风险：无新增外部接口，但输入点集仍来自外部文件
   - 控制：保持参数边界检查与空集/异常格式防护，不放宽任何启动门禁

### 3.5 发布与回滚策略

1. 发布策略
   - 先合入 runtime 点集清洗与单测
   - 再合入 HMI 直显校准（移除显示层点过滤）与单测
   - 最后执行真实链路回归与基线更新
2. 回滚策略
   - 若几何失真：回滚 runtime 点集清洗提交，保留 HMI 直显
   - 若仅视觉异常：优先调整点样式/视口映射，不引入点过滤
   - 若门禁异常：整组提交回滚到修复前 tag/commit
3. 观测项（发布后）
   - preview 输入点数、polyline_source_point_count、polyline_point_count、尾巴抑制计数
   - HMI 预览日志 `sampled_points` 与 `polyline_point_count` 一致性
   - 启动门禁拦截次数与原因码分布
