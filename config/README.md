# config

根级 `config/` 是工作区级配置资产的 canonical 落位。

当前约定：

- `config/machine/`：机器配置的 canonical 源
- `config/machine_config.ini`：旧调用方兼容桥接
- `config/build/`、`config/ci/`、`config/environments/`：保留给工作区级模板与治理配置

边界说明：

- `build/config/*` 属于构建输出，不是手工维护源文件
- `control-core/src/infrastructure/resources/config/files/*` 属于 legacy fallback，不再作为首选入口
