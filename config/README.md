# config

根级 `config/` 是工作区级配置资产的 canonical 落位。

当前约定：

- `config/machine/`：机器配置 canonical 源
- `config/build/`、`config/ci/`、`config/environments/`：工作区模板与治理配置

边界说明：

- `build/config/*` 属于构建输出，不是手工维护源文件
- 运行时代码与 `control-core/CMakeLists.txt` 已不再回退到 `control-core/config/*`
- `control-core/src/infrastructure/resources/config/files/*` 不再是默认配置来源
