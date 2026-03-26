# hmi-application

`modules/hmi-application/` 是 `M11 hmi-application` 的 canonical owner 入口。

## Owner 范围

- HMI 任务接入、审批与状态查询的业务语义收敛。
- 人机展示对象与交互状态聚合规则。
- HMI 域 owner 专属 contracts 与宿主调用边界。

## Owner 入口

- 构建入口：`modules/hmi-application/CMakeLists.txt`（target：`siligen_module_hmi_application`）。
- 模块契约入口：`modules/hmi-application/contracts/README.md`。

## 边界约束

- `apps/hmi-app/`、`apps/control-cli/` 仅承担宿主/命令入口，不承载 `M11` 终态 owner 事实。
- 跨模块稳定应用契约维护在 `shared/contracts/application/`。
- `M11` 仅承载 HMI 业务 owner 语义，不直接越权承担设备实现或运行时执行 owner 职责。

## 当前协作面

- `apps/hmi-app/`
- `shared/contracts/application/`
- `modules/hmi-application/contracts/`

## 统一骨架状态

- 已补齐 module.yaml、domain、services、application、adapters、tests 与 examples 子目录。
- 终态实现必须只落在该模块 canonical 骨架内，不再引入桥接目录或旁路 owner 面。

