# machine

owner: `packages/runtime-host` + control app / HIL 运行入口消费者

canonical：

- `config/machine/machine_config.ini`

bridge：

- `config/machine_config.ini`

注意：

- 当前 INI 仍含工作站相关绝对路径，后续应继续模板化
- 构建期会把 canonical 配置复制到 `build/config/machine/machine_config.ini` 与 bridge `build/config/machine_config.ini`
- `control-core/config/machine_config.ini` 与 `control-core/src/infrastructure/resources/config/files/machine_config.ini` 已退出默认解析链路
