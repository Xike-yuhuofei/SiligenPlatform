# services

workflow 的模块级编排服务已在此目录落成最小 skeleton。

- `lifecycle/`：创建、推进、重试、成功/失败投影
- `rollback/`：回退申请、执行、影响分析
- `archive/`：归档握手与完成
- `projection/`：domain -> contracts DTO / timeline 投影

禁止把 compat shell、foreign planning/execution concrete 塞到这里。
