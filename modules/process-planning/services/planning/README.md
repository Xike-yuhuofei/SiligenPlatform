# planning

承接路径规整、轨迹规划、插补规划和离线工程数据进入运行时前的规划切片。

当前模块内对外暴露的 canonical configuration target：`siligen_process_planning_contracts_public`。

`siligen_process_planning_domain_configuration` 仅保留为 configuration 实现库与历史 include wrapper 的承载面，不再作为 contracts/application/tests 的 public consumer 入口。

更宽的 planning 聚合面仍处于 bridge-backed 收敛阶段。
