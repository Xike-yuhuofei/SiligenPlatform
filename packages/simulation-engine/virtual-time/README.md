# virtual-time

这里承接确定性时间模型。

职责：

- 固定步长时钟
- 显式 tick 推进
- deterministic tick 调度器
- 暂停、恢复、超时判定
- 保证同输入下的可重复执行

约束：

- 默认步长 `1 ms`
- 只使用逻辑时间
- 时间推进只能由 `application/session` 显式触发
- 不依赖真实 sleep、线程定时器或宿主时钟
