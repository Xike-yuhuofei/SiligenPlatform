# Adapters层 - 外部接口适配器(Driving Adapters)
# 当前 CMake 管理的程序化入口能力集中在 TCP；HMI 作为外部客户端接入

# TCP适配器(HMI服务器)
message(STATUS "[Adapters Layer] TCP适配器已启用")
add_subdirectory(tcp)

# hmi/ 是Python项目，不由CMake管理

message(STATUS "[Adapters Layer] 适配器层配置完成（当前程序化入口能力：TCP）")
