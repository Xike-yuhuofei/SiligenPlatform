cmake_minimum_required(VERSION 3.20)

add_library(siligen_control_gateway INTERFACE)

target_link_libraries(siligen_control_gateway INTERFACE
    siligen_transport_gateway_protocol
)
