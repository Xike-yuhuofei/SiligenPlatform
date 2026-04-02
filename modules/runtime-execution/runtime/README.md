# runtime

`runtime/` 负责 `runtime-execution` 的 host core、运行态支撑实现与后续 runtime owner 收口。

当前并不意味着所有 job/session/control 的 live owner 已经落到 `runtime/`；对外 job API 仍以 `application/include/runtime_execution/application/**` 与实际 CMake 编译面为准。

`runtime/host/README.md` 记录当前 `runtime/host` 的第二阶段拆分定稿，包括文件级归类、目标依赖图与迁移顺序。
