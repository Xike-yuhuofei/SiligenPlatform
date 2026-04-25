# 世界领先级无痕内衣视觉点胶机上位机技术差距审查

审查日期：2026-04-24
审查范围：`D:\Projects\SiligenSuite` 仓库当前主线
审查目标：判断当前上位机距离“产品级、工业级、算法先进、长期可演进”的世界领先级工业软件还有哪些差距

---

# 1. 总体结论

这套仓库已经不是“原型级脚本系统”，而是有明确主链、正式契约、runtime-owned preview/traceability、L0-L6 分层测试和 HIL 路径的产品化雏形。最强的部分在 `预览/执行唯一真值`、`DXF 主执行链契约冻结`、`分层测试体系`。最弱的部分在 `统一路径质量评价`、`输入文件级追溯键`、`工艺知识与数据闭环`。

按“世界领先级无痕内衣视觉点胶机上位机”的标尺看，当前更接近“产品级可靠主链已成形，但算法质量门禁和工艺闭环仍明显不足”。如果只允许严格限制 DXF 输入范围，现状可以继续朝产品级推进；如果目标是“路径质量不过关绝不执行、执行前仿真验证、长期工艺知识沉淀与数据驱动优化”，当前仍有实质技术差距。

# 2. 当前成熟度雷达表

| 维度 | 当前等级 | 审查结论 |
|---|---|---|
| 1. DXF 输入治理能力 | L3 | 有正式输入契约、错误码、preview/production readiness，但缺正式 DXF 质量报告与文件级 hash 追溯 |
| 2. 几何与拓扑处理能力 | L3 | 有实质拓扑修复、轮廓重排、交点拆分和回归 fixture，但不是完整几何质量平台 |
| 3. 点胶工艺建模能力 | L2 | 已有 trigger/spacing/补偿/阀时序参数化建模，但没有款式/布料/胶水/效果知识模型 |
| 4. 路径质量评价能力 | L1 | 有分散检查点，没有统一评分器、质量报告、统一 fail-closed gate |
| 5. 运动规划与执行段生成能力 | L3 | `ProcessPath -> MotionTrajectory -> InterpolationData -> MultiCard` 主链清晰且可执行 |
| 6. 预览与执行一致性能力 | L4 | preview binding、snapshot hash、runtime authority 很强，但还不是正式执行前仿真/验证系统 |
| 7. 设备执行安全能力 | L3 | ready/interlock/软硬限位/production_blocked 已存在，但路径越界最终门禁证据不足 |
| 8. 诊断与追溯能力 | L2 | 有 terminal traceability 和 evidence bundle，但缺 file hash、query/archive/audit 平台 |
| 9. 自动化测试与回归防护能力 | L3 | L0-L6 体系、契约测试、golden baseline、HIL/fake harness 已有，但病理矩阵不完整 |
| 10. 工艺数据闭环能力 | L1 | 有 `production_baseline` 与部分性能阈值，但未见工艺知识库与效果反馈闭环 owner |

# 3. 10 个技术维度逐项审查

## 3.1 DXF 输入治理能力

1. 当前能力等级：`L3`
2. 已有证据：`docs/architecture/dxf-input-contract-v1.md:1-5,75-83,373-438`；`modules/dxf-geometry/application/engineering_data/processing/dxf_to_pb.py:18-35,576-611,649-691,735-786`；`tests/contracts/test_engineering_data_compatibility.py:84-223`
3. 缺失能力：缺正式 `DXF 输入质量报告` 对象/文件；缺 `dxf_hash/file_hash` 贯穿 `artifact -> plan -> preview -> job`；病理 DXF 输入矩阵不完整
4. 与目标等级的差距：距离“世界领先”还差输入 profile 管理、机器可读质量报告、输入指纹追溯、异常分级处置和审计闭环
5. P0 / P1 / P2 风险：`P0` 无 file hash；`P1` 无正式质量报告产物；`P2` 病理 fixture 覆盖不完整
6. 最小补齐路径：在 `dxf_to_pb.py` 现有 `import_diagnostics` 上升格出 `DxfInputQualityReport`，增加 `file_hash/profile_id/production_blockers`
7. 需要 Codex 执行的任务：设计并落地 `DxfInputQualityReport` DTO 与导出链；验证方法是 `dxf.artifact.create -> dxf.plan.prepare` 都返回同一份 report id/hash，并在 report 目录落盘
8. 需要新增的测试：新增 contract test，断言 `unsupported version / missing unit / bad INSERT / 3D entity / corrupted file` 都生成稳定 quality report 与 blocking 分类
9. 是否影响产品级可靠性：`是`
10. 是否影响世界领先目标：`是`

## 3.2 几何与拓扑处理能力

1. 当前能力等级：`L3`
2. 已有证据：`modules/process-path/domain/trajectory/domain-services/TopologyRepairService.cpp:666-705,856-926`；`modules/process-path/tests/regression/PbFixtureRegressionTest.cpp:61-172`；`modules/process-path/tests/regression/TopologyRepairContourOrderingRegressionTest.cpp:45-136`；`modules/process-path/tests/regression/TopologyRepairNoOpRegressionTest.cpp:55-80`；`shared/contracts/engineering/fixtures/cases/fragmented_chain/README.md:1-20`
3. 缺失能力：未见统一“几何/拓扑质量报告”；对自交、重复顶点、开闭环期望、异常空移的覆盖不完整
4. 与目标等级的差距：还没有世界级 CAD/CAM 前处理应有的完整拓扑分类、修复策略选择、修复后质量评分与可解释报告
5. P0 / P1 / P2 风险：`P1` 某些病理拓扑只能靠局部 repair；`P1` 修复结果未形成统一质量结论；`P2` 复杂几何类别覆盖不足
6. 最小补齐路径：在 `TopologyRepairDiagnostics` 之上补 `topology_verdict / repair_strategy / residual_defects`
7. 需要 Codex 执行的任务：给 `process-path` 增加统一 residual defect 枚举与导出；验证方法是现有 `fragmented_chain / multi_contour_reorder / intersection_split` fixture 全部产出稳定 defect taxonomy
8. 需要新增的测试：新增 `self_intersection / duplicate_vertex / open_loop_expected_closed / tiny island` fixture 回归
9. 是否影响产品级可靠性：`是`
10. 是否影响世界领先目标：`是`

## 3.3 点胶工艺建模能力

1. 当前能力等级：`L2`
2. 已有证据：`modules/dispense-packaging/application/services/dispensing/PlanningAssemblyAuthorityArtifacts.cpp:185-325,379-469`；`modules/dispense-packaging/domain/dispensing/planning/domain-services/AuthorityTriggerLayoutPlanner.cpp:531,597,700-707,812-839`；已有 `target_spacing/min/max/valve_response_ms/safety_margin_ms/compensation_profile/production_baseline`
3. 缺失能力：未见款式、布料、胶水、工艺策略、历史效果的 owner 模型；当前更像“执行参数装配”，不是“工艺知识系统”
4. 与目标等级的差距：距离世界领先差在“可沉淀、可复用、可迁移”的工艺知识层，而不只是一次性的参数集合
5. P0 / P1 / P2 风险：`P1` 不同布料/胶水场景缺少正式工艺模型；`P1` 参数迁移依赖人工；`P2` baseline 只能表达当前窄窗口
6. 最小补齐路径：先定义 `ProcessRecipeSnapshot`，最少覆盖 `material/glue/strategy/baseline/authority parameters`
7. 需要 Codex 执行的任务：在 `dispense-packaging` 与 `runtime-execution` 之间增加工艺快照对象；验证方法是 `plan.prepare / job.start / report` 三处快照一致
8. 需要新增的测试：新增 contract test，断言 `production_baseline` 之外还能稳定携带 `strategy/material/glue` 快照字段
9. 是否影响产品级可靠性：`是（尤其影响扩 SKU/扩布料）`
10. 是否影响世界领先目标：`是`

## 3.4 路径质量评价能力

1. 当前能力等级：`L1`
2. 已有证据：局部检查分散存在：`modules/dispense-packaging/application/services/dispensing/PlanningAssemblyAuthorityArtifacts.cpp:185-225` 的 `spacing validation`；`modules/dispense-packaging/application/services/dispensing/PreviewSnapshotService.cpp:81-101,371-391` 的短折返清理；`modules/motion-planning/domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.cpp:45-49` 的重复点拒绝；`modules/motion-planning/domain/motion/value-objects/TrajectoryTypes.h:82-91` 的 `PathQualityMetrics`
3. 缺失能力：没有统一 `PathQualityScorer / quality_report / quality_gate`；没有对 `点距异常、重复点、小折返、异常空移` 的统一评分和统一 fail-closed 结论
4. 与目标等级的差距：目标要求“路径质量不过关不允许执行”，当前仓库只证明“有若干局部检查”，没有证明“有统一质量门禁”
5. P0 / P1 / P2 风险：`P0` 无统一质量评分器；`P0` 无路径质量报告；`P1` 局部检测口径分散、不可追溯
6. 最小补齐路径：增加 runtime-owned `PathQualityReport`，把 DXF/import、topology、spacing、duplicate/backtrack、workspace envelope 汇总成单一 verdict
7. 需要 Codex 执行的任务：设计 `PathQualityReport + PathQualityGate`；验证方法是 `dxf.job.start` 在 `quality_verdict!=pass` 时必失败，错误码稳定
8. 需要新增的测试：新增病理 fixture：`duplicate_points / tiny_backtrack / excessive_air_move / uneven_spacing / out_of_range_path`，并断言报告分类与阻断行为
9. 是否影响产品级可靠性：`是，直接影响`
10. 是否影响世界领先目标：`是，直接影响`

## 3.5 运动规划与执行段生成能力

1. 当前能力等级：`L3`
2. 已有证据：`modules/motion-planning/domain/motion/domain-services/interpolation/InterpolationProgramPlanner.cpp:140-279`；`modules/runtime-execution/application/services/dispensing/DispensingProcessService.cpp:1410-1425,1800-1839,2024-2038`；`docs/architecture/topics/dxf/dxf-motion-execution-contract-v1.md:7-20,53-73`；`docs/architecture/topics/dxf/dxf-motion-execution-code-map-v1.md:39-44`
3. 缺失能力：未见正式执行段级质量报告、段级可行性验证、final workspace envelope 校核、执行前数值仿真
4. 与目标等级的差距：主链能跑，但离“高质量运动程序自动验证并可解释”还有差距
5. P0 / P1 / P2 风险：`P1` 只证明能生成和下发，不等于质量领先；`P1` final segment feasibility 证据不足；`P2` 高阶几何降阶后的质量损失未量化
6. 最小补齐路径：在 `BuildProgram` 后增加 `ExecutionSegmentVerificationReport`
7. 需要 Codex 执行的任务：给 `motion-planning/runtime-execution` 增加 segment verification；验证方法是对圆弧、整圆、折线混合样本输出稳定误差/长度/段类型统计
8. 需要新增的测试：新增 `arc retention / full-circle split / segment length drift / preview-vs-interpolation length delta` 回归
9. 是否影响产品级可靠性：`是`
10. 是否影响世界领先目标：`是`

## 3.6 预览与执行一致性能力

1. 当前能力等级：`L4`
2. 已有证据：`modules/dispense-packaging/application/services/dispensing/PreviewSnapshotService.cpp:534-607,633-697`；`modules/runtime-execution/application/usecases/dispensing/DispensingWorkflowUseCase.cpp:463-483,1176-1249,1602-1668`；`apps/hmi-app/src/hmi_client/ui/main_window.py:3479-3492`；`apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py:154-187`；`tests/contracts/test_online_preview_evidence_contract.py:80-142`；`tests/integration/scenarios/first-layer/test_real_preview_snapshot_geometry.py:99-157`
3. 缺失能力：还没有正式“执行前仿真/验证系统”；`controller_ready_trace` 目前主要是日志，不是稳定 machine-readable verdict，见 `modules/runtime-execution/application/services/dispensing/DispensingProcessService.cpp:1994-2035`
4. 与目标等级的差距：当前是“强一致 preview binding + confirm gate”，目标应是“执行前仿真 + 数值差异报告 + 一键阻断”
5. P0 / P1 / P2 风险：`P0` 缺正式 execution verification report；`P1` controller-ready trace 只日志化；`P2` 病理模式一致性矩阵不足
6. 最小补齐路径：把 preview 升级为 `PreviewVerificationReport`，显式比较 `glue_points / motion_preview / interpolation_segments`
7. 需要 Codex 执行的任务：新增 verification report 导出与网关返回；验证方法是 `preview.confirm` 后固定生成 report，并断言三层 hash/长度/点数/绑定一致
8. 需要新增的测试：新增 contract test，断言 report 包含 `snapshot_hash/segment_diff/length_delta/binding_verdict`
9. 是否影响产品级可靠性：`是`
10. 是否影响世界领先目标：`是，且是当前强项向更高台阶跃迁的关键`

## 3.7 设备执行安全能力

1. 当前能力等级：`L3`
2. 已有证据：`docs/architecture/topics/runtime-hmi/hmi-supervisor-min-contract-v1.md:34-39,124-139,194-205,238-239`；`docs/architecture/topics/runtime-hmi/hmi-online-startup-state-machine-v1.md:56-79,159-162`；`apps/runtime-service/runtime/status/RuntimeStatusExportPort.cpp:187-233,240-262,288-391`；`modules/runtime-execution/runtime/host/services/motion/HardLimitMonitorService.cpp:38-65,81-132`；`modules/runtime-execution/runtime/host/services/motion/SoftLimitMonitorService.cpp:88-190`；`modules/runtime-execution/application/usecases/dispensing/DispensingWorkflowUseCase.cpp:515-520,1333-1356,1602-1665`；`tests/e2e/hardware-in-loop/run_real_dxf_production_validation.py:403-456,1049-1073`
3. 缺失能力：`DXF_E_GEOMETRY_OUT_OF_RANGE` 只在 `docs/architecture/dxf-input-contract-v1.md:422` 看到，未在 live 执行主链找到正式路径越界 admission gate；“安全门禁总报告”证据不足
4. 与目标等级的差距：现在更像“在线状态/互锁/限位安全”，还不是“几何质量 + 工作区 + 工艺质量 + 设备状态”的统一执行准入系统
5. P0 / P1 / P2 风险：`P0` 路径越界 fail-closed 证据不足；`P0` 质量门禁与设备门禁未统一成单一 admission artifact；`P1` fake runtime safety matrix 仍不够完整
6. 最小补齐路径：增加 `ExecutionAdmissionReport`，统一 ready、interlock、quality、workspace、profile_compare gate
7. 需要 Codex 执行的任务：在 `runtime-execution` 加 admission builder；验证方法是 `not ready / interlock / quality_fail / out_of_range / production_blocked` 五类负例全部稳定阻断
8. 需要新增的测试：新增 simulated-line 和 HIL 双 lane 的 safety block matrix；每个 case 断言 `job.start` 失败码、report category 和 HMI 按钮禁用态
9. 是否影响产品级可靠性：`是，直接影响`
10. 是否影响世界领先目标：`是`

## 3.8 诊断与追溯能力

1. 当前能力等级：`L2`
2. 已有证据：`modules/runtime-execution/application/usecases/dispensing/DispensingExecutionUseCase.Async.cpp:803-835`；`modules/runtime-execution/application/services/dispensing/DispensingProcessService.cpp:428-537,1950-1977`；`apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.cpp:132-141`；`apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py:388-449`；`modules/trace-diagnostics/README.md:7-10,45-50`；`modules/trace-diagnostics/tests/README.md:5-17`
3. 缺失能力：缺 `dxf_hash/file_hash`；缺可查询/可归档/可审计诊断平台；`task_id` 仅内部存在，不是对外追溯键；工艺参数快照只有 `production_baseline` 级别，不是完整 process snapshot
4. 与目标等级的差距：现有追溯强在“profile_compare terminal trace”，弱在“输入文件级、跨报告级、历史归档级”
5. P0 / P1 / P2 风险：`P0` 无 file hash；`P1` trace-diagnostics 只到 evidence/logging，不到 query/archive；`P2` 对外键集偏窄
6. 最小补齐路径：扩成 `artifact_id + dxf_hash + plan_id + snapshot_hash + job_id + baseline_id + quality_report_id`
7. 需要 Codex 执行的任务：扩展 traceability schema 与 report writer；验证方法是任一 report 都能回溯到唯一 DXF、唯一 baseline、唯一 quality report
8. 需要新增的测试：新增 contract test，断言 `dxf.job.traceability` 与 `preview/prod report` 都带 `dxf_hash/quality_report_id/process_snapshot_id`
9. 是否影响产品级可靠性：`是`
10. 是否影响世界领先目标：`是，差距很大`

## 3.9 自动化测试与回归防护能力

1. 当前能力等级：`L3`
2. 已有证据：`docs/architecture/validation/layered-test-system-v2.md:11-17,23-45,87-89`；`docs/architecture/build-and-test.md:29-35,47-50,71-85`；`tests/contracts/test_online_preview_evidence_contract.py:80-142`；`apps/runtime-gateway/transport-gateway/tests/test_transport_gateway_compatibility.py:80-187,388-449`；`tests/baselines/baseline-manifest.json:24-237`；`tests/e2e/hardware-in-loop/runtime_gateway_harness.py:246-299`；`tests/e2e/hardware-in-loop/run_hil_closed_loop.py:46,137,238`
3. 缺失能力：黄金样本有，但病理 DXF fixture 仍不够系统；缺“路径质量 gate”相关测试，因为 gate 本身尚不存在
4. 与目标等级的差距：当前测试体系强在“层次完整”，弱在“工业质量病理矩阵”和“算法质量 regression”
5. P0 / P1 / P2 风险：`P1` 病理 DXF 集合不完整；`P1` safety/quality 负例矩阵不闭；`P2` 某些性能/质量指标仍靠人工解释
6. 最小补齐路径：在现有 L2/L3/L4/L5 上新增 path quality、admission、traceability key 三类正式 gate
7. 需要 Codex 执行的任务：把新 gate 接到 `quick-gate/full-offline-gate/limited-hil`；验证方法是故意注入坏 fixture 时 gate 必红且退出码稳定
8. 需要新增的测试：`test_path_quality_gate_contract.py`、`test_execution_admission_contract.py`、`test_traceability_keys_contract.py`、HIL safety matrix
9. 是否影响产品级可靠性：`是`
10. 是否影响世界领先目标：`是`

## 3.10 工艺数据闭环能力

1. 当前能力等级：`L1`
2. 已有证据：现有闭环只到 `production_baseline` 和部分 performance threshold：`tests/e2e/hardware-in-loop/runtime_gateway_harness.py:246-299`；`tests/contracts/test_online_preview_evidence_contract.py:80-142`；`tests/e2e/hardware-in-loop/run_real_dxf_preview_snapshot.py:675-884`；`tests/e2e/hardware-in-loop/run_real_dxf_production_validation.py:963-1078`；`tests/baselines/performance/dxf-preview-profile-thresholds.json`；`docs/architecture/build-and-test.md:52-62`
3. 缺失能力：证据不足，未见款式/布料/胶水/历史效果/效果反馈 owner 模型；未见参数推荐、闭环调参、历史工艺知识库
4. 与目标等级的差距：距离“世界领先级工艺数据驱动优化”差距最大
5. P0 / P1 / P2 风险：`P1` 当前只能靠 baseline 管窄窗口；`P1` 无工艺知识沉淀；`P2` 性能阈值不等于工艺效果闭环
6. 最小补齐路径：从 `process snapshot + outcome snapshot + historical effect store` 三件套起步
7. 需要 Codex 执行的任务：定义 `ProcessSnapshot / OutcomeSnapshot / ProcessKnowledgeRecord`；验证方法是一次执行后能把 DXF、baseline、quality、traceability、outcome 关联成单条历史记录
8. 需要新增的测试：新增 integration test，断言执行完成后历史记录含 `dxf_hash/baseline/process snapshot/outcome verdict`
9. 是否影响产品级可靠性：`是（主要影响扩展与持续改进）`
10. 是否影响世界领先目标：`是，核心短板`

# 4. P0 风险清单

- 没有统一 `PathQualityGate` 与 `PathQualityReport`，无法严格证明“路径质量不过关，不允许执行”。
- `DXF_E_GEOMETRY_OUT_OF_RANGE` 只有文档口径，未见 live 执行前路径越界门禁实现证据。
- 预览虽强，但尚未升级为正式 `执行前仿真/验证系统`；`controller_ready_trace` 仍主要是日志。
- live 追溯链缺 `dxf_hash/file_hash`，无法做到输入文件级召回、复盘和历史归档。
- 安全门禁、质量门禁、workspace 门禁、profile_compare gate 还没有合成单一 `ExecutionAdmissionReport`。

# 5. P1 风险清单

- 工艺建模停留在参数装配层，缺款式/布料/胶水/策略知识模型。
- `trace-diagnostics` 只覆盖 logging/bootstrap/evidence bundle，不是 query/archive/audit 平台。
- 病理 DXF fixture 不够系统，尤其是 `bad INSERT / duplicate_vertex / self-intersection / out_of_range / abnormal air move`。
- 现有 golden baseline 偏 preview/simulation；针对“路径质量”和“执行准入”的 golden 仍缺。
- 对外 process snapshot 不完整，`production_baseline` 还不足以表达完整工艺上下文。

# 6. P2 风险清单

- `PathQualityMetrics` 已有基础数据结构，但尚未接入 live gate，存在“有对象、无治理”的悬空风险。
- 一些质量检查目前散落在 importer、preview、topology、interpolator 中，维护成本会上升。
- HIL/模拟链路已经存在，但某些工业负例仍依赖人工解释而非稳定 schema verdict。

# 7. 当前最短补齐路径

1. 先做 `DXF 输入质量报告 + file hash`，把输入治理从“stderr/diagnostics”提升到“正式可归档对象”。
2. 再做 `PathQualityReport + PathQualityGate`，统一 topology、spacing、duplicate/backtrack、air-move、workspace。
3. 把 preview 升级为 `ExecutionVerificationReport`，把 `glue_points / motion_preview / interpolation_segments` 的差异显式化。
4. 合成 `ExecutionAdmissionReport`，统一 ready、interlock、quality、workspace、production_blocked。
5. 最后补 `process snapshot + outcome snapshot + historical knowledge record`，进入工艺闭环阶段。

# 8. Codex 可执行任务清单

- 新增 `DxfInputQualityReport` 契约与落盘链。验证：`dxf.artifact.create`、`dxf.plan.prepare`、report 目录的 `report_id/file_hash` 一致。
- 新增 `PathQualityReport` 和 `PathQualityGate`。验证：坏 fixture 触发 `dxf.job.start` fail-closed，错误码稳定。
- 新增 `ExecutionVerificationReport`。验证：`preview.confirm` 后产出 `snapshot_hash/segment_diff/length_delta/binding_verdict`。
- 新增 `ExecutionAdmissionReport`。验证：`not_ready / interlock / quality_fail / out_of_range / production_blocked` 五类负例全部阻断。
- 扩展 traceability key 集。验证：`preview report / production report / dxf.job.traceability` 能互相关联到同一 `dxf_hash`。
- 引入 `ProcessSnapshot / OutcomeSnapshot`。验证：一次执行后生成完整历史记录并可回查。
- 建立病理 DXF fixture 矩阵。验证：每个 fixture 都有稳定 expected verdict，不允许人工口头解释替代。

# 9. 测试与 CI 补强清单

- `L2`：新增 `tests/contracts/test_dxf_input_quality_report_contract.py`，断言 report shape、hash、blocking taxonomy。
- `L2`：新增 `tests/contracts/test_path_quality_gate_contract.py`，断言 `duplicate_points / tiny_backtrack / abnormal_air_move / out_of_range` 的分类和阻断。
- `L3`：新增 `tests/integration/scenarios/first-layer/test_execution_verification_report.py`，断言 `planned_glue_snapshot`、`execution_trajectory_snapshot`、`interpolation_segments` 差异报告稳定。
- `L3`：新增 `tests/integration/scenarios/first-layer/test_traceability_keys_roundtrip.py`，断言 `dxf_hash -> plan_id -> snapshot_hash -> job_id -> report` 全链一致。
- `L4`：在 simulated-line 增加 admission matrix，断言五类阻断场景不能误启动。
- `L5`：在 HIL 增加 safety block matrix，断言未 ready、门开、急停、production_blocked、quality_fail 都不能进入真实执行。
- `L6`：新增 path quality/perf 趋势阈值，把“质量 gate 耗时”和“verification report 生成耗时”纳入 nightly。

# 10. 90 天技术路线图

- `0-30 天`：完成 `DXF 输入质量报告`、`file hash`、病理 fixture 首批、`PathQualityGate` v1。
- `31-60 天`：完成 `ExecutionVerificationReport`、`ExecutionAdmissionReport`、preview 升级为执行前验证。
- `61-90 天`：完成 traceability key 扩展、HIL safety matrix、CI 正式 blocking，把“坏路径不能执行”变成稳定 gate。

# 11. 12 个月世界领先路线图

- `0-3 个月`：先把产品级可靠性补齐，形成单轨 `input quality -> path quality -> execution verification -> admission gate`。
- `3-6 个月`：做算法质量领先，补强异常空移/折返/点距优化、几何病理处理、segment 级验证与自动调优。
- `6-9 个月`：建立工艺知识层，沉淀 `款式/布料/胶水/策略/效果` 的 owner 模型与历史记录。
- `9-12 个月`：进入数据闭环优化，基于历史效果做参数推荐、相似工艺复用、质量趋势分析和异常预警。
