# recording

这里承接仿真事实记录。

职责：

- timeline
- motion_profile
- 关键状态变化 trace
- summary
- 结果导出
- 回归断言辅助

规则：

- 只记录，不参与控制决策
- timeline 合并原始 event 和关键状态变化，作为稳定回归比较面
- motion_profile 按快照顺序和轴顺序生成，保证可回放和可比较
- summary 统一空 timeline、提前 stop、异常失败的终止语义
