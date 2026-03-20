# S1-S9 首轮评审记录总索引页

## 文档定位

本页用于统一管理 `S1-S9` 首轮正式评审的单场景记录入口。
它解决两个问题：

- 让记录人和评审人能快速定位每个场景的记录页
- 让 [首层结论汇总](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-summary.md) 与 [阻断结论总表](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-blocker-matrix.md) 能回溯到单场景记录

## 使用规则

- 每个场景应至少对应一套单场景评审记录
- 记录完成后，应在“当前状态”中标记为 `已完成` / `进行中` / `未开始`
- 若场景结论涉及阻断项，应在“是否关联阻断项”中标记为 `是`
- 若场景需要重审，应在“是否需要重审”中标记为 `是`，并在备注中写明进入条件
- 单场景结论只允许填写：`通过` / `不通过` / `有条件通过` / `待补证据`
- 各记录页中的判定语义，统一以《验收口径稿》为准

## 记录总表

| 场景 | 场景名称 | 记录页链接 | 当前状态 | 单场景结论 | 是否关联阻断项 | 是否需要重审 | 备注 |
| --- | --- | --- | --- | --- | --- | --- |
| S1 | 主干成功闭环 | [S1 记录页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-s1.md) | 进行中 | 待补证据 | 是 | 是 | 待补主干闭环运行证据与重复验证 |
| S2 | 前置条件阻断与有效反馈 | [S2 记录页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-s2.md) | 进行中 | 待补证据 | 是 | 是 | 待补配方/版本阻断证据 |
| S3 | 前置条件阻断与有效反馈 | [S3 记录页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-s3.md) | 进行中 | 待补证据 | 是 | 是 | 待补设备就绪前置判断证据 |
| S4 | 前置条件阻断与有效反馈 | [S4 记录页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-s4.md) | 进行中 | 待补证据 | 是 | 是 | 待补输入前置校验证据 |
| S5 | 关键操作链路切换 | [S5 记录页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-s5.md) | 进行中 | 待补证据 | 是 | 是 | 待补操作链路状态流转证据 |
| S6 | 关键交互时序异常 | [S6 记录页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-s6.md) | 进行中 | 待补证据 | 是 | 是 | 待补跨边界时序异常证据 |
| S7 | 高频或高风险异常与恢复 | [S7 记录页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-s7.md) | 进行中 | 待补证据 | 是 | 是 | 待补工艺异常与恢复证据 |
| S8 | 高频或高风险异常与恢复 | [S8 记录页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-s8.md) | 进行中 | 待补证据 | 是 | 是 | 待补急停锁定与复位链证据 |
| S9 | 关键数据组合对照 | [S9 记录页](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-s9.md) | 进行中 | 待补证据 | 是 | 是 | 待补成组对照与重复验证证据 |

## 建议记录命名

建议每个场景记录页按以下格式命名：

- `first-layer-review-record-s1.md`
- `first-layer-review-record-s2.md`
- `first-layer-review-record-s3.md`
- `first-layer-review-record-s4.md`
- `first-layer-review-record-s5.md`
- `first-layer-review-record-s6.md`
- `first-layer-review-record-s7.md`
- `first-layer-review-record-s8.md`
- `first-layer-review-record-s9.md`

## 会后回填要求

- 单场景结论确定后，应同步更新本页状态
- 若整体结论发生变化，应同步核对 [首层结论汇总](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-summary.md)
- 若新增或解除阻断项，应同步核对 [阻断结论总表](/D:/Projects/SiligenSuite/docs/validation/first-layer-review-blocker-matrix.md)
- 若某场景被判 `有条件通过` 或 `待补证据`，应同步写明重审进入条件或补证据要求
