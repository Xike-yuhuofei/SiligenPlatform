# 提交信息约定

推荐格式：

```text
<type>(<scope>): <summary>
```

## type 建议

- `feat`：新功能
- `fix`：缺陷修复
- `refactor`：重构
- `test`：测试相关
- `docs`：文档变更
- `build`：构建系统或脚本变更
- `chore`：杂项维护

## scope 建议

- `domain-motion`
- `domain-dispensing`
- `domain-recipes`
- `application`
- `infrastructure`
- `adapter-cli`
- `adapter-tcp`
- `tests`
- `docs`

## 示例

- `feat(domain-motion): 增加插补约束校验`
- `fix(adapter-tcp): 修复超时响应解析`
- `refactor(infrastructure): 收敛硬件适配器创建逻辑`
