# test-kit

`packages/test-kit` 现在承接工作区级验证运行器、结果模型和报告输出能力。

## 负责什么

- 统一单测 / 集成 / 仿真-HIL 三层验证的运行模型
- 收口命令执行、状态归类、已知失败识别与报告输出
- 为根级 `test.ps1`、`ci.ps1` 和 `integration/` 场景提供共享工具

## 当前入口

- Python 包：`D:\Projects\SiligenSuite\packages\test-kit\src\test_kit`
- 工作区验证主入口：`python -m test_kit.workspace_validation`

## 状态语义

- `passed`：执行成功
- `failed`：出现新的失败或回归
- `known_failure`：当前已知未收口问题，单独计入报告
- `skipped`：当前环境不具备执行条件
