# adapters

hmi-application 的外部依赖适配实现终态应收敛到此目录。
当前 live runtime-concrete 已先在包内 `application/hmi_application/adapters/` 做 stage-2 split；
顶层 `modules/hmi-application/adapters/` 仍为非 live 顶层占位目录，不在当前 `SILIGEN_TARGET_IMPLEMENTATION_ROOTS` 中。
