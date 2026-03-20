# 首轮复审输入包总索引页

## 文档定位

本页用于统一管理 `S1-S9` 首轮复审输入包的落盘位置与回填入口。

它只解决两个问题：

- 每个场景的复审包放在哪里
- 复审完成后应回填到哪些总表

## 使用规则

- 每个场景应至少对应一套复审输入包目录
- 复审输入包应至少包含：
  - `inputs.md`
  - `run-record.md`
  - `outputs.md`
  - `repeat-check.md`
  - `rereview-summary.md`
- 若场景没有真实新增证据，不得进入复审判定
- 复审完成后，应同步回填单场景记录页、记录总索引页、结论汇总和阻断总表

## 场景索引

| 场景 | 场景名称 | 复审包目录 | 当前状态 | 目标阻断项 | 回填目标 |
| --- | --- | --- | --- | --- | --- |
| S1 | 主干成功闭环 | [S1 复审包](D:/Projects/SiligenSuite/docs/validation/rereview-packages/S1/README.md) | 待补证据 | B1、B3 | 单场景记录页 / 结论汇总 / 阻断总表 |
| S2 | 前置条件阻断与有效反馈 | [S2 复审包](D:/Projects/SiligenSuite/docs/validation/rereview-packages/S2/README.md) | 待补证据 | B3 | 单场景记录页 / 结论汇总 / 阻断总表 |
| S3 | 前置条件阻断与有效反馈 | [S3 复审包](D:/Projects/SiligenSuite/docs/validation/rereview-packages/S3/README.md) | 待补证据 | B3 | 单场景记录页 / 结论汇总 / 阻断总表 |
| S4 | 前置条件阻断与有效反馈 | [S4 复审包](D:/Projects/SiligenSuite/docs/validation/rereview-packages/S4/README.md) | 待补证据 | B3 | 单场景记录页 / 结论汇总 / 阻断总表 |
| S5 | 关键操作链路切换 | [S5 复审包](D:/Projects/SiligenSuite/docs/validation/rereview-packages/S5/README.md) | 待补证据 | B1、B3 | 单场景记录页 / 结论汇总 / 阻断总表 |
| S6 | 关键交互时序异常 | [S6 复审包](D:/Projects/SiligenSuite/docs/validation/rereview-packages/S6/README.md) | 待补证据 | B2、B3 | 单场景记录页 / 结论汇总 / 阻断总表 |
| S7 | 高频或高风险异常与恢复 | [S7 复审包](D:/Projects/SiligenSuite/docs/validation/rereview-packages/S7/README.md) | 待补证据 | B2、B3 | 单场景记录页 / 结论汇总 / 阻断总表 |
| S8 | 高频或高风险异常与恢复 | [S8 复审包](D:/Projects/SiligenSuite/docs/validation/rereview-packages/S8/README.md) | 待补证据 | B2、B3 | 单场景记录页 / 结论汇总 / 阻断总表 |
| S9 | 关键数据组合对照 | [S9 复审包](D:/Projects/SiligenSuite/docs/validation/rereview-packages/S9/README.md) | 待补证据 | B1、B3 | 单场景记录页 / 结论汇总 / 阻断总表 |

## 回填顺序

建议每个场景复审完成后按以下顺序回填：

1. 先更新场景目录内的 `rereview-summary.md`
2. 再更新对应单场景记录页
3. 再更新 [S1-S9 首轮评审记录总索引页](D:/Projects/SiligenSuite/docs/validation/first-layer-review-record-index.md)
4. 再更新 [首层结论汇总](D:/Projects/SiligenSuite/docs/validation/first-layer-review-summary.md)
5. 最后更新 [阻断结论总表](D:/Projects/SiligenSuite/docs/validation/first-layer-review-blocker-matrix.md)

## 本页结论

当前 `S1-S9` 的复审输入包入口已经可以统一管理。  
后续执行重点应转为场景证据落盘、复审摘要回填和阻断项消减。
