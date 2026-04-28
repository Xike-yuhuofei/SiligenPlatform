# Demo-1.dxf 路径碎片化提示根因分析（2026-04-27）

> 说明：本文处于【根因定位模式】，仅做原因分析与证据归因，不包含修复建议或实现方案。

## 1. 现象

HMI 导入 `D:\Projects\SiligenSuite\uploads\dxf\archive\Demo-1.dxf` 后提示：

- 标题：`路径碎片化提示`
- 内容：`路径较碎，当前结果已按例外规则放行，可继续预览与执行。这通常意味着 DXF 几何连通性较差，或导入顺序导致路径被拆成多段。`

## 2. 触发链路（代码证据）

该提示由后端诊断码驱动，链路为：

1. HMI 展示条件：`preview_diagnostic_code == "process_path_fragmentation"`
   - `modules/hmi-application/application/hmi_application/services/preview_preflight.py:90`
2. 诊断码赋值：
   - `prepared.preview_diagnostic_code = process_path_result.topology_diagnostics.fragmentation_suspected ? "process_path_fragmentation" : ""`
   - `modules/dispense-packaging/application/usecases/dispensing/PlanningUseCase.cpp:1022`
3. `fragmentation_suspected` 来源（两类）
   - 拓扑修复后仍存在断链/修复未生效：
     - `discontinuity_after > 0` 或 `(discontinuity_before > 0 && !repair_applied)`
     - `modules/process-path/domain/trajectory/domain-services/TopologyRepairService.cpp:923`
   - 过程路径统计表现为碎片化：
     - `dispense_fragment_count > 1 || rapid_segment_count > 0`
     - `modules/process-path/application/services/process_path/ProcessPathFacade.cpp:58`

## 3. 候选根因与置信度

### 候选根因 A：DXF 几何连通性较差（主根因）

- 置信度：**0.86（高）**
- 触发依据：拓扑修复后仍有连续性问题，直接置位 `fragmentation_suspected`。
- 证据：`TopologyRepairService.cpp:923-925`
- 与提示文案一致性：高（对应“DXF 几何连通性较差”）。

### 候选根因 B：图元顺序/拓扑组织导致路径被拆分

- 置信度：**0.74（中高）**
- 触发依据：路径存在多个 dispense 片段或 rapid 插入，也会触发碎片化判定。
- 证据：
  - `ProcessPathFacade.cpp:39-60`
  - `modules/process-path/tests/unit/application/services/process_path/ProcessPathFacadeTest.cpp:543`（repair off 场景下 rapid>0、fragment>1）
- 证据属性：该项为统计侧强相关证据，不是显式“顺序错误码”。

### 候选根因 C：Demo-1.dxf 已是稳定负样例

- 置信度：**0.71（中高）**
- 证据：
  - `docs/session-handoffs/2026-04-25-1419-p0-path-02-session-handoff.md:24`
  - 文档明确该样例会命中 `process_path_fragmentation` 和 `path_discontinuity`。
- 含义：当前现象与既有基线一致，倾向于“已知问题重现”而非新引入回归。

## 4. 触发条件汇总

满足任一条件即可触发“路径碎片化提示”相关诊断：

- `discontinuity_after > 0`
- `discontinuity_before > 0 && repair_applied == false`
- `dispense_fragment_count > 1`
- `rapid_segment_count > 0`

另：修复策略受 `optimize_path` 开关影响（Auto/Off）：
- `modules/dispense-packaging/application/usecases/dispensing/PlanningUseCase.cpp:582`

## 5. 影响范围

- 预览层：HMI 展示“路径碎片化提示”横幅（提示级）。
  - `preview_preflight.py:90-99`
- 运行门禁层：相同诊断码进入 `path_quality` reason codes，并在生产启动前参与阻断。
  - `modules/runtime-execution/application/usecases/dispensing/DispensingWorkflowUseCase.cpp:1720`
  - `modules/runtime-execution/application/usecases/dispensing/DispensingWorkflowUseCase.cpp:612`

## 6. 证据链完整性评估

- **完整闭环（高）**：
  - process-path 判定 → planning 赋码 → payload 传递 → HMI 渲染。
- **间接证据（中）**：
  - “导入顺序导致拆段”依赖路径统计特征推断，并非单独错误码直接标记。
- **场景佐证（中高）**：
  - `Demo-1.dxf` 的负样例定位已有历史文档支撑。

---

当前文档仅用于根因定位结论记录；是否进入【方案设计模式】需由用户确认。