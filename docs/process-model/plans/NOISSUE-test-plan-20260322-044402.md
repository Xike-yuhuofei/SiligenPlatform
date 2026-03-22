# NOISSUE 测试计划 - 轨迹预览点状化与交点小尾巴修复

- 状态：Updated (As-Built)
- 日期：2026-03-22
- 分支：feat/hmi/NOISSUE-dispense-trajectory-preview-v2
- 关联范围文档：NOISSUE-scope-20260322-042851.md
- 关联架构文档：NOISSUE-arch-plan-20260322-044402.md

## 1. 关键路径

1. 关键路径 A：真实链路预览几何正确性
   - 流程：artifact.create -> plan.prepare -> preview.snapshot -> HMI 渲染
   - 目标：rect_diag.dxf 显示为点状矩形+对角线，交点无“小尾巴”
2. 关键路径 B：门禁不回归
   - 流程：preview.confirm -> dxf.job.start
   - 目标：未确认或哈希不一致时仍阻断启动
3. 关键路径 C：高密度点集真实性
   - 流程：snapshot 高点数 -> HMI 渲染
   - 目标：HMI 不做显示层点过滤，渲染点数与 runtime `polyline_point_count` 一致
4. 关键路径 D：胶点工艺参数一致性
   - 流程：plan.prepare -> preview.snapshot -> HMI 点绘制
   - 目标：圆心距 3.0 mm（算法采样）；点径 1.5 mm（渲染尺寸）

## 2. 边界场景

1. max_polyline_points = 2（最小边界）
2. trajectory_polyline 含重复点 / 近重复点
3. 含角点与交点的折线（rect_diag.dxf）
4. 大点数场景（接近 4000 点）
5. 空或异常 payload（缺字段/类型异常）
6. 点数一致性场景（`sampled_points == polyline_point_count`）
7. 固定间距场景（直线段相邻点距离约 3.0 mm）

## 3. 回归基线

1. 几何基线
   - 样例：examples/dxf/rect_diag.dxf
   - 判据：点状可辨、无交点小尾巴、外包框不越界
2. 行为基线
   - preview confirm 与 snapshot hash 门禁保持原语义
3. 兼容基线
   - gateway 兼容测试持续通过，不新增协议破坏

## 4. 测试分层与命令（优先根级命令）

### 4.1 单元测试

1. Runtime 预览点集处理（新增/更新）
   - 命令：out/build-runtime-preview/bin/siligen_unit_tests.exe --gtest_filter="DispensingWorkflowUseCaseTest.*"
   - 预期：新增“去尾巴/角点保真/3mm 固定间距/保底回退”测试全部通过

2. HMI 预览契约与渲染策略
   - 命令：apps/hmi-app/scripts/test.ps1
   - 预期：预览相关单测通过，点状渲染契约与错误处理不回归，且无 UI 点过滤模块残留

3. Gateway 兼容
   - 命令：python packages/transport-gateway/tests/test_transport_gateway_compatibility.py
   - 预期：snapshot 兼容面与上限夹断断言通过

### 4.2 集成测试

1. Apps 套件回归
   - 命令：.\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure
   - 预期：apps 套件通过，无新增失败

2. Runtime 相关联回归
   - 命令：out/build-runtime-preview/bin/siligen_unit_tests.exe --gtest_filter="DispensingExecutionUseCaseProgressTest.*:DispensingWorkflowUseCaseTest.*:MotionMonitoringUseCaseTest.*:RedundancyGovernanceUseCasesTest.*"
   - 预期：既有关键用例与新用例全绿

### 4.3 端到端（真实链路）

1. 启动真实后端
   - 命令：apps/control-tcp-server/run.ps1
   - 预期：监听 9527 成功

2. 启动 HMI 在线模式
   - 命令：apps/hmi-app/run.ps1 -Mode online
   - 预期：启动状态 ready，无初始化失败

3. 人工验证流程
   - 加载 rect_diag.dxf -> 生成预览 -> 观察点状几何与交点 -> 预览确认 -> 启动任务
   - 预期：
     - 预览为离散点，不呈连续线带
     - 交点无可见“小尾巴”
     - `tcp_server.log` 中 `polyline_point_count` 与 `hmi_ui.log` 中 `sampled_points` 一致
     - 直线段相邻点圆心距约 3.0 mm；点径按 1.5 mm 映射显示
     - 门禁行为正确

## 5. 通过/阻断标准

1. 以下任一失败则阻断合并：
   - 交点小尾巴仍可复现
   - 点状渲染回退为连续线段视觉
   - preview gate 行为回归
   - 单元/集成命令存在红灯
2. 文档产物必须补齐：
   - arch-plan、test-plan、qa 回归报告

## 6. 风险跟踪与验收证据

1. 证据项
   - 关键测试命令输出摘要
   - rect_diag.dxf 预览截图（真实链路）
   - 预览点数统计（源点数/输出点数）
2. 风险项
   - 若阈值调优仍不稳定，需在 QA 报告中记录场景与参数，并明确后续优化票据
