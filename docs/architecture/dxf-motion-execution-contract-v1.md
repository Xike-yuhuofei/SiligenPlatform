# DXF Motion Execution Contract V1

更新时间：`2026-03-22`

## 1. 目的与适用范围

本文冻结 `DXF -> PB -> Domain Primitive -> process_path / motion_trajectory -> interpolation_segments -> MultiCard` 主链中的执行契约，目标是消除以下歧义：

- `process_path`、`motion_trajectory`、`interpolation_segments` 三者各自代表什么
- 上位机与控制卡分别对哪些语义拥有决定权
- “内部时间化轨迹”与“控制卡实际执行程序”之间是否等价

本文适用于：

- `modules/dxf-geometry`
- `modules/process-path`
- `modules/motion-planning`
- `modules/dispense-packaging`
- `modules/runtime-execution`
- `shared/contracts/engineering`

本文不覆盖：

- HMI 预览显示策略
- 胶量/工艺效果一致性
- 真实设备轨迹精度验收结论

## 2. 术语冻结

### 2.1 `PB 几何原语`

指 `PathBundle.pb` 中的 `LINE / ARC / CIRCLE / SPLINE / ELLIPSE / POINT / CONTOUR` 等几何表达。它是导入交换格式，不直接等于执行程序。

### 2.2 `Domain Primitive`

指 runtime 从 PB 恢复出的运行时几何对象。它承接几何实体语义，但仍不是最终执行程序。

### 2.3 `process_path`

指面向几何执行与工艺语义的路径段集合。它回答“走什么几何、按什么段类型走、段上挂什么工艺语义”。

### 2.4 `motion_trajectory`

指上位机内部的时间律/速度轨迹表示。它回答“沿既定路径如何分配速度、如何处理段间速度过渡、末速度约束如何落位”。

### 2.5 `interpolation_segments`

指板卡可执行的段级插补程序。它回答“最终下发给坐标系插补接口的命令序列是什么”。

## 3. 总体数据流

```text
DXF
  -> dxf_to_pb.py
  -> PathBundle.pb
  -> PbPathSourceAdapter
  -> Domain Primitive
  -> DispensingPlannerService
     -> process_path
     -> motion_trajectory
  -> InterpolationProgramPlanner
     -> interpolation_segments
  -> DispensingProcessService
  -> InterpolationAdapter
  -> MultiCard SDK
  -> 控制卡坐标系插补执行
```

当前路径真值说明：

- DXF / PB 契约 authority 以 `shared/contracts/engineering/` 与 `data/schemas/engineering/dxf/v1/` 为准。
- DXF preprocess 与 PB 准备链当前 owner 以 `modules/dxf-geometry/` 为准。
- `process_path`、`motion_trajectory`、`interpolation_segments` 三层对象的 live surface 已切到 `modules/*`，不再以 package-era 路径作为默认代码入口。

## 4. 三层契约

### 4.1 `[5]` 规划结果层

输入：

- `Domain Primitive`
- 工艺与设备约束
- 起点、边界、速度/加速度/Jerk 参数

输出：

- `process_path`
- `motion_trajectory`

契约：

- `process_path` 是几何执行语义权威源。
- `process_path` 必须稳定表达段类型、段终点、圆弧中心/方向、工艺 tag。
- `motion_trajectory` 是时间律参考源，不是板卡原生输入。
- `motion_trajectory` 可用于速度映射、触发推导、仿真评估、长度/时长估算。
- `motion_trajectory` 不应被定义为“设备实际最终执行轨迹”。

禁止事项：

- 不得把 `motion_trajectory` 直接宣称为板卡执行程序。
- 不得在该层隐式决定板卡缓冲策略或 SDK 调用顺序。

### 4.2 `[6]` 控制程序合成层

输入：

- `process_path`
- `motion_trajectory`
- 段级加速度/末速度约束

输出：

- `interpolation_segments`

契约：

- `InterpolationProgramPlanner` 的职责是执行语义合成，不是简单格式转换。
- 合成过程必须同时保留：
  - `process_path` 的几何段类型语义
  - `motion_trajectory` 的速度分布与端速度语义
- 输出段至少应包含：
  - `type`
  - `positions`
  - `velocity`
  - `end_velocity`
  - `acceleration`
  - 圆弧所需参数（如圆心、平面）

允许的适配：

- 为适配板卡能力对单段做有限拆分，例如整圆拆成两段圆弧。

禁止事项：

- 不得改变全局路径顺序。
- 不得把圆弧、直线等几何语义无依据降阶为“无语义点流”。
- 不得丢失 `process_path` 到执行段的可追溯关系。

### 4.3 `[7]` 板卡执行层

输入：

- `interpolation_segments`
- 坐标系配置
- 缓冲区与启动控制命令

输出：

- SDK 调用序列
- 控制卡坐标系实际执行

契约：

- `InterpolationAdapter` 是协议适配层，不是重新规划层。
- 该层负责：
  - 坐标系配置
  - 段写入 FIFO / lookahead
  - flush / start / stop / status 查询
- 该层不负责：
  - 重排路径顺序
  - 重新定义段类型
  - 重新决定工艺 tag 语义

执行边界：

- 控制卡拥有段内插补执行权。
- 控制卡可以在段内实现自己的插补细节、缓冲调度、速度覆盖。
- 控制卡不应被假定与上位机 `motion_trajectory` 逐点等价。

## 5. 五项冻结契约

### 5.1 谁是几何权威源

冻结结论：

- `process_path` 是几何执行权威源。
- `interpolation_segments` 必须从 `process_path` 继承几何段语义。
- `motion_trajectory` 不是几何权威源。

### 5.2 谁是速度权威源

冻结结论：

- `motion_trajectory` 是时间律与速度分布权威源。
- `InterpolationProgramPlanner` 负责把 `motion_trajectory` 投影为段级 `velocity / end_velocity / acceleration`。
- 控制卡只是在既定段参数下执行，不反向定义上位机速度语义。

### 5.3 谁是最终可执行程序权威源

冻结结论：

- `interpolation_segments` 是板卡下发前唯一真源。
- `DispensingProcessService` 执行时应以 `plan.interpolation_segments` 为准。

### 5.4 板卡允许改写的边界

冻结结论：

- 允许：段内插补实现、FIFO 调度、速度覆盖、状态机运行。
- 不允许：重排段顺序、篡改段类型、改写工艺 tag、把执行程序回写为新的上位机规划真源。

### 5.5 工艺 tag 如何贯穿到执行段

冻结结论：

- `process_path` 是工艺 tag 权威源。
- 从 `process_path` 到 `interpolation_segments` 必须存在稳定可追溯映射。
- 触发、诊断、回放、QA 取证不得只依赖板卡返回状态，而丢失上位机段来源。

## 6. 当前实现下的明确判断

### 6.1 成立的判断

- 当前主链不是“同一最终轨迹先在上位机时间化，再原样交给控制卡重复插补”。
- 当前主链是“几何/工艺语义 + 时间律语义 -> 控制程序合成 -> 控制卡段内插补执行”。
- 控制卡最终接收的是段命令参数，而不是仅有点坐标的无语义点列。

### 6.2 必须保留的保留意见

- `motion_trajectory` 不应直接称为“设备实际执行轨迹”。
- 实际执行轨迹以控制卡对 `interpolation_segments` 的执行结果为准。
- 若系统需要声明“轨迹一致性闭合”，必须额外验证板卡执行结果与上位机时间律参考之间的偏差。

## 7. 当前代码上的边界观察

### 7.1 已经收清的边界

- `DispensingPlannerService` 同时保留 `process_path`、`motion_trajectory`、`interpolation_segments`。
- `InterpolationProgramPlanner` 从 `process_path + motion_trajectory` 合成执行段，而不是只从点序列拼接。
- `InterpolationAdapter` 支持线段与圆弧段下发。

### 7.2 尚未完全收清的边界

- `dxf_to_pb.py` 已经做了样条近似、`CONTOUR` 组织等导入期拓扑处理。
- 规划层仍会做交点拆分、连通重建、轮廓优化。
- 这说明“导入规范化”和“可执行拓扑重建”仍有职责交叠，后续需要继续收敛。

## 8. 失败模式与补救

### 8.1 失败模式：把 `motion_trajectory` 误当最终执行程序

后果：

- 文档、调试、回放口径全部失真。

补救：

- 对外统一使用“内部时间律表示”术语，不使用“最终执行轨迹”术语。

### 8.2 失败模式：程序合成层退化为简单点转段

后果：

- 圆弧语义丢失
- 速度语义与几何语义耦合混乱

补救：

- 评审时必须核查 `process_path` 与 `motion_trajectory` 是否共同参与程序合成。

### 8.3 失败模式：板卡层隐式改写执行语义

后果：

- 回放与诊断无法解释
- 工艺 tag 失去追溯链

补救：

- 明确限制适配层职责，只允许协议表达与执行控制，不允许重新规划。

## 9. 评审检查项

评审时至少逐项确认以下问题：

1. `process_path` 是否仍是几何与工艺 tag 的唯一权威源
2. `motion_trajectory` 是否只承担时间律参考职责
3. `interpolation_segments` 是否是执行前唯一真源
4. `InterpolationProgramPlanner` 是否仍是“程序合成层”而非“点格式转换层”
5. 适配层是否存在越权重排或语义改写

## 10. 对应验证路径

建议验证命令：

- `.\test.ps1 -Profile CI -Suite apps -FailOnKnownFailure`
- `ctest --test-dir build --output-on-failure -R "Dispensing|Motion|Interpolation|DXF"`

建议补充的定向验证：

1. 线段路径
   - 期望：生成 `LINEAR` 执行段，速度与末速度来自 `motion_trajectory`
2. 圆弧路径
   - 期望：生成 `CIRCULAR_CW/CCW` 执行段，并在适配层映射为 `MC_ArcXYC`
3. 触发链路
   - 期望：触发位置可追溯到 `process_path` / `motion_trajectory` 的来源，不依赖板卡隐式推断

## 11. 当前版本结论

当前版本冻结结论如下：

- `process_path`：几何/工艺权威源
- `motion_trajectory`：内部时间律参考源
- `interpolation_segments`：执行前唯一真源
- `InterpolationProgramPlanner`：执行语义合成层
- 控制卡：段内插补执行层，不是上位机规划结果的替代权威源
