# tests

hmi-application 的模块级验证入口应收敛到此目录。

当前最小正式测试面：

- `unit/`
  - `test_launch_state.py`：状态投影与 startup-state
  - `test_preview_session.py`：preview gate 与协议消费链
- `contract/`
  - `test_launch_supervision_contract.py`：`SessionSnapshot` / `SessionStageEvent` 不变量

当前未补：

- presenter/view-model 更细粒度映射样本
- host-to-owner 邻接集成回归
- HMI 启动链的更完整 fault/recovery 矩阵
