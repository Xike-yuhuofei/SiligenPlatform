# shared-kernel

`shared-kernel` 现在承接稳定共享基础代码，不再把 `control-core/modules/shared-kernel` 作为默认实现来源。

当前真实承接内容：

- `numeric_types.h`
- `axis_types.h`
- `basic_types.h`
- `point2d.h`
- `error.h`
- `result.h` / `result_types.h`
- `strings/string_manipulator.*`

规则：

- 只放稳定、跨包复用、无业务倾向的基础能力
- 不吸收设备、流程、运行时宿主或配方仓储实现
- 不反向依赖任何业务包或 `control-core/modules/*`

当前兼容关系：

- `control-core/modules/CMakeLists.txt` 已优先注册本包的 `siligen_shared_kernel`
- legacy `control-core/modules/shared-kernel` 目录只剩待清理兼容壳，不再是默认 target owner
