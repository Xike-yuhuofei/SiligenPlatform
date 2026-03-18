# TCP 适配能力聚合层（JSON/TCP）
# 当前默认程序化入口是 apps/control-tcp-server；本目录保留 TCP 能力库与兼容聚合说明

cmake_minimum_required(VERSION 3.20)

message(STATUS "[TCP Adapter] TCP服务器适配能力配置")

add_library(siligen_control_gateway_tcp_adapter INTERFACE)
add_library(siligen_tcp_adapter ALIAS siligen_control_gateway_tcp_adapter)

# 链接库
target_link_libraries(siligen_control_gateway_tcp_adapter INTERFACE
    siligen_transport_gateway
)

message(STATUS "[TCP Adapter] 默认能力库目标: siligen_transport_gateway（兼容 alias: siligen_control_gateway_tcp_adapter / siligen_tcp_adapter）")
message(STATUS "[TCP Adapter] 配置完成")
