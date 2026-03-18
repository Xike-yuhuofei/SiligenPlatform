# 提交信息约定

推荐格式：

```text
<type>(<scope>): <summary>
```

## type 建议

- `feat`：新界面功能或工作流
- `fix`：界面缺陷或协议调用问题
- `refactor`：结构重整
- `test`：测试相关
- `docs`：文档变更
- `build`：打包或运行脚本变更
- `chore`：杂项维护

## scope 建议

- `ui`
- `viewmodel`
- `workflow`
- `client`
- `integration-control-core`
- `integration-dxf-editor`
- `tests`
- `docs`

## 示例

- `feat(ui): 增加配方列表筛选面板`
- `fix(client): 修复 TCP 重连状态处理`
- `refactor(workflow): 拆分启动流程状态机`
