# ============================================================
# vcpkg 包管理器工具链配置文件
# ============================================================
# 用法: cmake -DCMAKE_TOOLCHAIN_FILE=cmake/vcpkg-toolchain.cmake ..
#
# 此文件会自动检测 vcpkg 安装并配置 CMake 使用它
# ============================================================

# 检测 vcpkg 安装路径
if(NOT DEFINED VCPKG_ROOT)
    # 优先级 1: 环境变量
    if(DEFINED ENV{VCPKG_ROOT})
        set(VCPKG_ROOT "$ENV{VCPKG_ROOT}" CACHE PATH "vcpkg installation directory")

    # 优先级 2: D:/Downloads/vcpkg (本次安装位置)
    elseif(EXISTS "D:/Downloads/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(VCPKG_ROOT "D:/Downloads/vcpkg" CACHE PATH "vcpkg installation directory")

    # 优先级 3: C:/vcpkg
    elseif(EXISTS "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "vcpkg installation directory")

    # 优先级 4: 项目根目录下的 vcpkg
    elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../vcpkg/scripts/buildsystems/vcpkg.cmake")
        set(VCPKG_ROOT "${CMAKE_CURRENT_LIST_DIR}/../vcpkg" CACHE PATH "vcpkg installation directory")
    else()
        message(WARNING "未检测到 vcpkg 安装。OpenTelemetry 等依赖将不可用。")
        message(WARNING "如需使用 vcpkg，请安装到以下位置之一：")
        message(WARNING "  - 设置环境变量: VCPKG_ROOT=<vcpkg 路径>")
        message(WARNING "  - 安装到: D:/Downloads/vcpkg")
        message(WARNING "  - 安装到: C:/vcpkg")
        message(WARNING "  - 安装到: <项目根目录>/vcpkg")
        return()
    endif()
endif()

# 设置 vcpkg 工具链文件路径
set(VCPKG_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")

if(NOT EXISTS "${VCPKG_TOOLCHAIN_FILE}")
    message(WARNING "vcpkg 工具链文件不存在: ${VCPKG_TOOLCHAIN_FILE}")
    return()
endif()

# 包含 vcpkg 的工具链配置
message(STATUS "📦 使用 vcpkg: ${VCPKG_ROOT}")
message(STATUS "📦 工具链文件: ${VCPKG_TOOLCHAIN_FILE}")

include("${VCPKG_TOOLCHAIN_FILE}")
