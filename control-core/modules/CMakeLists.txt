cmake_minimum_required(VERSION 3.20)

message(STATUS "=== 仓库内模块 ===")

if(NOT TARGET siligen_shared_kernel AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../packages/shared-kernel/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../../packages/shared-kernel"
        "${CMAKE_BINARY_DIR}/packages/shared-kernel"
    )
elseif(NOT TARGET siligen_shared_kernel)
    add_subdirectory(shared-kernel)
endif()

if(TARGET siligen_process_core AND TARGET siligen_motion_core)
    message(STATUS "[Modules] process-core / motion-core 由 packages/process-runtime-core 提供 canonical target")
else()
    add_subdirectory(process-core)
    add_subdirectory(motion-core)
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../packages/device-contracts/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../../packages/device-contracts"
        "${CMAKE_BINARY_DIR}/packages/device-contracts"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../packages/device-adapters/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../../packages/device-adapters"
        "${CMAKE_BINARY_DIR}/packages/device-adapters"
    )
endif()

add_subdirectory(device-hal)
add_subdirectory(control-gateway)
