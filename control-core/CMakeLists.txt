cmake_minimum_required(VERSION 3.16)

# 使用Clang-CL + LLD编译配置
if(WIN32)
    # 强制设置Clang-CL编译器
    find_program(CLANG_CL_EXE NAMES clang-cl PATHS "C:/Program Files/LLVM/bin" NO_DEFAULT_PATH REQUIRED)
    find_program(LLD_LINK_EXE NAMES lld-link PATHS "C:/Program Files/LLVM/bin" NO_DEFAULT_PATH REQUIRED)
    set(CMAKE_CXX_COMPILER "${CLANG_CL_EXE}" CACHE FILEPATH "Clang-CL编译器" FORCE)
    set(CMAKE_C_COMPILER "${CLANG_CL_EXE}" CACHE FILEPATH "Clang-CL C编译器" FORCE)
    set(CMAKE_LINKER "${LLD_LINK_EXE}" CACHE FILEPATH "LLD链接器" FORCE)

    # 设置 MT.exe (Manifest Tool) 路径
    find_program(CMAKE_MT NAMES mt PATHS
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64"
        NO_DEFAULT_PATH REQUIRED)
    if(CMAKE_MT)
        message(STATUS "📍 MT.exe: ${CMAKE_MT}")
    endif()

    # 禁用增量链接（lld-link 不完全支持 /INCREMENTAL）
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO")
    # 覆盖 Debug/RelWithDebInfo 配置的默认增量链接设置（使用赋值而非追加）
    # 使用 FORCE 强制覆盖 CMake 平台模块的默认值
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO" CACHE STRING "Flags for Debug linker" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO" CACHE STRING "Flags for Debug shared linker" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/debug /INCREMENTAL:NO" CACHE STRING "Flags for RelWithDebInfo linker" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/debug /INCREMENTAL:NO" CACHE STRING "Flags for RelWithDebInfo shared linker" FORCE)

    # 验证编译器和工具链
    if(NOT CLANG_CL_EXE)
        message(FATAL_ERROR "❌ 未找到Clang-CL编译器，请安装LLVM工具链")
    endif()

    # Clang-CL 与 MSVC STL 版本兼容性设置
    # 如果使用 Clang 编译 MSVC 头文件，可能需要此宏
    add_compile_options(/D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
    message(STATUS "⚠️ 使用 _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH 绕过版本检查")

    message(STATUS "✅ 使用Clang-CL + LLD编译配置")
    message(STATUS "📍 编译器: ${CMAKE_CXX_COMPILER}")
    message(STATUS "📍 链接器: ${CMAKE_LINKER}")
endif()

project(SiligenDispenser
    VERSION 1.0.0
    DESCRIPTION "Siligen工业点胶机控制系统 - GAS运动控制卡集成测试"
    LANGUAGES CXX
)

set(SILIGEN_MULTICARD_VENDOR_DIR
    "${CMAKE_SOURCE_DIR}/../packages/device-adapters/vendor/multicard"
    CACHE INTERNAL "Canonical location of vendor MultiCard SDK binaries"
)
set(SILIGEN_RUNTIME_HOST_DIR
    "${CMAKE_SOURCE_DIR}/../packages/runtime-host"
    CACHE INTERNAL "Canonical location of the runtime-host package sources"
)
set(SILIGEN_PROCESS_RUNTIME_CORE_DIR
    "${CMAKE_SOURCE_DIR}/../packages/process-runtime-core"
    CACHE INTERNAL "Canonical location of the process-runtime-core package sources"
)
set(SILIGEN_SHARED_KERNEL_DIR
    "${CMAKE_SOURCE_DIR}/../packages/shared-kernel"
    CACHE INTERNAL "Canonical location of the shared-kernel package sources"
)
if(NOT EXISTS "${SILIGEN_SHARED_KERNEL_DIR}/include")
    set(SILIGEN_SHARED_KERNEL_DIR
        "${CMAKE_SOURCE_DIR}/modules/shared-kernel"
        CACHE INTERNAL "Fallback location of the shared-kernel sources"
        FORCE
    )
endif()
set(SILIGEN_TRANSPORT_GATEWAY_DIR
    "${CMAKE_SOURCE_DIR}/../packages/transport-gateway"
    CACHE INTERNAL "Canonical location of the transport-gateway package sources"
)
set(SILIGEN_MACHINE_CONFIG_FILE
    "${CMAKE_SOURCE_DIR}/../config/machine/machine_config.ini"
    CACHE INTERNAL "Canonical location of the machine config asset"
)
if(NOT EXISTS "${SILIGEN_MACHINE_CONFIG_FILE}")
    set(SILIGEN_MACHINE_CONFIG_FILE
        "${CMAKE_SOURCE_DIR}/config/machine_config.ini"
        CACHE INTERNAL "Fallback location of the machine config asset"
        FORCE
    )
endif()
if(NOT EXISTS "${SILIGEN_MACHINE_CONFIG_FILE}")
    set(SILIGEN_MACHINE_CONFIG_FILE
        "${CMAKE_SOURCE_DIR}/src/infrastructure/resources/config/files/machine_config.ini"
        CACHE INTERNAL "Legacy location of the machine config asset"
        FORCE
    )
endif()

# ========================================
# 构建信息：Git 提交哈希
# ========================================
find_package(Git QUIET)
if(GIT_FOUND)
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git/HEAD")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/.git/HEAD")
        file(READ "${CMAKE_SOURCE_DIR}/.git/HEAD" SILIGEN_GIT_HEAD_CONTENT)
        if(SILIGEN_GIT_HEAD_CONTENT MATCHES "ref: (.+)")
            set(SILIGEN_GIT_REF_FILE "${CMAKE_SOURCE_DIR}/.git/${CMAKE_MATCH_1}")
            if(EXISTS "${SILIGEN_GIT_REF_FILE}")
                set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SILIGEN_GIT_REF_FILE}")
            endif()
        endif()
    endif()
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE SILIGEN_GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT SILIGEN_GIT_HASH)
    set(SILIGEN_GIT_HASH "unknown")
endif()
add_compile_definitions(SILIGEN_GIT_HASH="${SILIGEN_GIT_HASH}")

# ========================================
# MSVC 编译器特性设置（GoogleTest 兼容性）
# ========================================
# 必须在 project() 调用之后，因为 MSVC 变量在 project() 中设置
if(MSVC)
    # GoogleTest 需要 cxx_std_14 特性，但 MSVC 不原生支持 CMake 编译特性
    # 提供假的特性映射，让 GoogleTest 的 target_compile_features 调用成功
    set(CMAKE_CXX_STANDARD_COMPILE_OPTION "/std:c++17")
    set(CMAKE_CXX_STANDARD_OPTION_COMPILE_OPTION "/std:c++17")
    set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "/std:c++17")
    set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "/std:c++17")

    # 关键：提供 GoogleTest 需要的编译器特性
    set(CMAKE_CXX_COMPILE_FEATURES "cxx_std_14;cxx_std_17" CACHE STRING "CXX compiler features" FORCE)
    set(CMAKE_CXX14_COMPILE_FEATURES "cxx_std_14" CACHE STRING "CXX14 features" FORCE)
    set(CMAKE_CXX17_COMPILE_FEATURES "cxx_std_17" CACHE STRING "CXX17 features" FORCE)

    # 使用编译器内嵌调试信息，减少/避免并发写入PDB带来的冲突
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "MSVC debug info format" FORCE)

    message(STATUS "配置 MSVC 编译器特性（GoogleTest 兼容性）")
endif()

# ========================================
# 统一构建系统选项
# ========================================

# 构建目标选择 (新增)
set(SILIGEN_BUILD_TARGET "all" CACHE STRING "构建目标选择: all/tests/clean")
set_property(CACHE SILIGEN_BUILD_TARGET PROPERTY STRINGS all tests clean)
option(SILIGEN_CLEAN_BUILD "清理构建目录" OFF)

# 代码质量工具配置选项
option(SILIGEN_BUILD_TESTS "构建单元测试" OFF)
option(SILIGEN_FETCH_GTEST "通过FetchContent下载Google Test" ON)
option(SILIGEN_ENABLE_CLANG_TIDY "启用clang-tidy静态分析" OFF)
option(SILIGEN_ENABLE_COVERAGE "启用代码覆盖率" OFF)
option(SILIGEN_ENABLE_MOCK_MULTICARD "构建 MockMultiCard 支持" ON)
option(SILIGEN_ENABLE_CGAL "启用CGAL支持（DXF轮廓增广）" OFF)
option(BUILD_SECURITY_MODULE "构建安全模块" OFF)

# 日志级别控制 (ERROR/WARNING/INFO)
set(SILIGEN_LOG_LEVEL "ERROR" CACHE STRING "构建日志级别: ERROR/WARNING/INFO")

# 编译性能优化选项 (Phase 7 - T088)
option(SILIGEN_USE_PCH "使用预编译头加速编译" ON)
option(SILIGEN_USE_UNITY_BUILD "使用Unity Build加速编译(实验性)" OFF)
option(SILIGEN_PARALLEL_COMPILE "启用并行编译" ON)

# 编译器配置已在项目开始处强制设置
# 此处仅显示确认信息
if(WIN32)
    message(STATUS "🔧 构建系统: ${CMAKE_GENERATOR}")
    message(STATUS "🏗️ 编译类型: ${CMAKE_BUILD_TYPE}")
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 显示构建信息
message(STATUS "===========================================")
message(STATUS "Siligen点胶机控制系统 - CMake构建")
message(STATUS "===========================================")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "编译器路径: ${CMAKE_CXX_COMPILER}")
message(STATUS "===========================================")
message(STATUS "")

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ========================================
# clang-tidy 静态分析配置
# ========================================
if(SILIGEN_ENABLE_CLANG_TIDY)
    # 查找 clang-tidy (LLVM 安装路径)
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy
        PATHS "C:/Program Files/LLVM/bin"
        NO_DEFAULT_PATH
    )

    if(CLANG_TIDY_EXE)
        message(STATUS "✅ clang-tidy 找到: ${CLANG_TIDY_EXE}")

        # 设置 CMake 使用 clang-tidy
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
        
        # 添加 clang-tidy 检查选项
        list(APPEND CMAKE_CXX_CLANG_TIDY
            "-checks=-*"
            "-checks=-modernize-use-trailing-return-type"
            "-checks=-cppcoreguidelines-avoid-c-arrays"
            "-checks=-cppcoreguidelines-pro-bounds-pointer-arithmetic"
        )
    else()
        message(WARNING "⚠️  clang-tidy 未找到，静态分析已禁用")
        message(WARNING "   请安装 LLVM: https://llvm.org/builds/")
    endif()
endif()

# 代码覆盖率配置
if(SILIGEN_ENABLE_COVERAGE)
    if(MSVC)
        message(WARNING "MSVC不支持gcov覆盖率,请使用Visual Studio覆盖率工具")
    else()
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
        message(STATUS "代码覆盖率已启用")
    endif()
endif()

# 导出compile_commands.json供clang-tidy使用(仅Clang/GCC)
if(NOT MSVC)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

# 编译性能优化配置 (Phase 7 - T088)
if(SILIGEN_PARALLEL_COMPILE)
    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # MSVC并行编译: /MP[processMax]
        add_compile_options(/MP)
        # 允许并发写入PDB，避免并行编译的PDB写入冲突
        add_compile_options(/FS)
        message(STATUS "并行编译已启用: MSVC /MP")
    else()
        # Clang-CL和GCC使用构建系统并行能力
        message(STATUS "并行编译已启用: 使用构建系统并行能力")
    endif()
endif()

# Unity Build全局配置 (可选)
if(SILIGEN_USE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 16)
    message(STATUS "Unity Build已启用: 批次大小=16")
endif()

# 编译选项
# 根据日志级别设置警告级别
if(SILIGEN_LOG_LEVEL STREQUAL "ERROR")
    set(SILIGEN_WARNING_LEVEL "0")
    message(STATUS "日志级别: ERROR (仅显示错误)")
elseif(SILIGEN_LOG_LEVEL STREQUAL "WARNING")
    set(SILIGEN_WARNING_LEVEL "1")
    message(STATUS "日志级别: WARNING (显示警告和错误)")
else()
    set(SILIGEN_WARNING_LEVEL "4")
    message(STATUS "日志级别: INFO (显示所有信息)")
endif()

if(MSVC)
    # MSVC编译器选项 - 根据日志级别动态设置警告级别
    add_compile_options(
        /permissive-                    # 严格标准符合性
        /utf-8                          # UTF-8源码和执行字符集
        /EHsc                           # Enable C++ exceptions
    )
    if(SILIGEN_WARNING_LEVEL EQUAL "1")
        add_compile_options(/W1)        # 警告级别1
    elseif(SILIGEN_WARNING_LEVEL GREATER "1")
        add_compile_options(/W4)        # 警告级别4
    endif()
    add_compile_options(/WX-)           # 警告不视为错误
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(/clang:-msse3)  # Clang-CL 启用 SSE3
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_compile_options(/arch:SSE2)     # MSVC x86 使用 SSE2
    endif()
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    # 设置控制台编码为UTF-8
    add_compile_definitions(UNICODE _UNICODE)
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Od /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /DNDEBUG")
else()
    # GCC/Clang等其他编译器选项 - 根据日志级别动态设置警告级别
    add_compile_options(
        -finput-charset=UTF-8           # UTF-8输入编码
        -fexec-charset=UTF-8            # UTF-8执行编码
    )
    if(SILIGEN_WARNING_LEVEL EQUAL "0")
        add_compile_options(-w)         # 关闭所有警告
    elseif(SILIGEN_WARNING_LEVEL EQUAL "1")
        add_compile_options(-Wall)      # 基本警告
    else()
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

# ========================================
# 统一构建系统配置
# ========================================

# 统一输出目录到单一构建目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
# 多配置生成器（如 VS）也统一输出到 build/bin 与 build/lib，避免生成 build/bin/Release 等重复路径
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/lib)
endforeach()

# 构建日志和缓存目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/cache)

# 包含目录
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src)
include_directories(${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/modules/process-core/include)
include_directories(${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/modules/motion-core/include)
include_directories(src)
include_directories(include)
include_directories(third_party)
include_directories(tests)

# 包含编译优化模块 (Phase 7 - T088)
# DISABLED: Missing file after directory restructure - include(cmake/PrecompiledHeaders.cmake)

# ============================================================
# 第三方库配置
# ============================================================
message(STATUS "")
message(STATUS "=== 第三方库配置 ===")

# Protobuf (PB path source)
option(SILIGEN_ENABLE_PROTOBUF "启用Protobuf支持" ON)
if(SILIGEN_ENABLE_PROTOBUF)
    set(protobuf_INSTALL OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_CONFORMANCE OFF CACHE BOOL "" FORCE)
    set(protobuf_BUILD_LIBPROTOC ON CACHE BOOL "" FORCE)
    set(protobuf_BUILD_PROTOBUF_BINARIES ON CACHE BOOL "" FORCE)
    set(protobuf_BUILD_PROTOC_BINARIES ON CACHE BOOL "" FORCE)
    set(protobuf_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(protobuf_MSVC_STATIC_RUNTIME OFF CACHE BOOL "" FORCE)
    set(protobuf_WITH_ZLIB OFF CACHE BOOL "" FORCE)

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/protobuf/CMakeLists.txt")
        add_subdirectory(third_party/protobuf)
        message(STATUS "[ThirdParty] Protobuf 库已添加")
    else()
        message(WARNING "[ThirdParty] Protobuf 库未找到 (third_party/protobuf)")
    endif()
endif()

# Ruckig 轨迹生成库 (MIT)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON_MODULE OFF CACHE BOOL "" FORCE)
set(BUILD_ONLINE_CLIENT OFF CACHE BOOL "" FORCE)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ruckig/CMakeLists.txt")
    add_subdirectory(third_party/ruckig)
    message(STATUS "[ThirdParty] Ruckig 库已添加")
else()
    message(WARNING "[ThirdParty] Ruckig 库未找到 (third_party/ruckig)")
endif()

message(STATUS "=== 第三方库配置完成 ===")
message(STATUS "")

# ============================================================
# 六边形架构模块 (新架构)
# ============================================================
message(STATUS "")
message(STATUS "=== 六边形架构模块 ===")

# 添加六边形架构各层
add_subdirectory(src/shared)         # 共享接口层
add_subdirectory("${SILIGEN_PROCESS_RUNTIME_CORE_DIR}" "${CMAKE_BINARY_DIR}/packages/process-runtime-core")
add_subdirectory(src/infrastructure) # 基础设施层

add_subdirectory(src/adapters)       # 适配器层

# ============================================================
# 仓库内模块与 app 注册
# ============================================================
# 说明：
# - `packages/process-runtime-core` 已承接 canonical core 实现
# - `apps/*` 与 `modules/*` 继续承接 runtime / device / gateway 主体能力
add_subdirectory("${SILIGEN_TRANSPORT_GATEWAY_DIR}" "${CMAKE_BINARY_DIR}/packages/transport-gateway")
add_subdirectory(modules)
add_subdirectory("${SILIGEN_RUNTIME_HOST_DIR}" "${CMAKE_BINARY_DIR}/packages/runtime-host")
add_subdirectory(apps)

# 创建聚合库以简化链接
add_library(siligen_shared INTERFACE)
target_link_libraries(siligen_shared INTERFACE
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_shared_kernel
)

if(NOT TARGET siligen_domain)
    message(FATAL_ERROR "packages/process-runtime-core 必须提供 siligen_domain")
endif()

message(STATUS "=== 六边形架构模块配置完成 ===")
message(STATUS "")

# ========================================
# 统一构建目标处理
# ========================================

message(STATUS "=== 构建目标: ${SILIGEN_BUILD_TARGET} ===")

# 根据构建目标自动设置选项
if(SILIGEN_BUILD_TARGET STREQUAL "tests")
    set(SILIGEN_BUILD_TESTS ON CACHE BOOL "构建单元测试" FORCE)
    message(STATUS "[目标] 测试模式 - 单元测试")
elseif(SILIGEN_BUILD_TARGET STREQUAL "clean")
    message(STATUS "[目标] 清理模式 - 准备清理构建目录")
    # 清理模式将由构建脚本处理
else()
    message(STATUS "[目标] 完整模式 - 构建所有目标")
endif()

message(STATUS "=== 构建目标配置完成 ===")
message(STATUS "")

# Windows系统库(直接链接,不使用find_library)
if(WIN32)
    set(WINDOWS_LIBS user32 kernel32 ws2_32)
endif()

# ========== 开发构建时复制配置文件 ==========
# 同时保留 canonical 与 bridge 路径，避免旧入口在迁移期断裂。
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/config/machine)
configure_file(
    ${SILIGEN_MACHINE_CONFIG_FILE}
    ${CMAKE_BINARY_DIR}/config/machine/machine_config.ini
    COPYONLY
)
configure_file(
    ${SILIGEN_MACHINE_CONFIG_FILE}
    ${CMAKE_BINARY_DIR}/config/machine_config.ini
    COPYONLY
)

# ========== 安装规则 ==========
install(FILES ${SILIGEN_MACHINE_CONFIG_FILE}
    DESTINATION config/machine
)
install(FILES ${SILIGEN_MACHINE_CONFIG_FILE}
    DESTINATION config
)

# 安装硬件库文件
install(FILES ${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll
    DESTINATION bin
)

# 安全模块库
if(BUILD_SECURITY_MODULE)
    message(STATUS "安全模块已启用")

    add_library(security_module STATIC
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/AuditLogger.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/NetworkWhitelist.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/NetworkAccessControl.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/SafetyLimitsValidator.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/SessionService.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/PermissionService.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/UserService.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/SecurityService.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/InterlockMonitor.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/config/SecurityConfigLoader.cpp
        ${SILIGEN_RUNTIME_HOST_DIR}/src/security/config/ConfigurationVersionService.cpp
        src/shared/Hashing/HashCalculator.cpp
    )

    target_include_directories(security_module PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
        src
        ${SILIGEN_RUNTIME_HOST_DIR}/src
        ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/modules/motion-core/include
        ${SILIGEN_SHARED_KERNEL_DIR}/include
        third_party
    )

    target_link_libraries(security_module
        ${WINDOWS_LIBS}
        bcrypt
    )

    # T066: 审计日志硬件集成测试
    set(SECURITY_AUDIT_TEST_SOURCE "${CMAKE_SOURCE_DIR}/tests/integration/test_audit_log_hardware.cpp")
    if(EXISTS "${SECURITY_AUDIT_TEST_SOURCE}")
        add_executable(test_audit_log_hardware
            ${SECURITY_AUDIT_TEST_SOURCE}
        )

        target_link_libraries(test_audit_log_hardware
            security_module
            ${WINDOWS_LIBS}
        )

        target_include_directories(test_audit_log_hardware PRIVATE
            ${CMAKE_SOURCE_DIR}
            src
            ${SILIGEN_RUNTIME_HOST_DIR}/src
            third_party
        )
    else()
        message(WARNING "安全模块审计日志测试源缺失，跳过 test_audit_log_hardware 目标")
    endif()
else()
    message(STATUS "安全模块未启用(使用-DBUILD_SECURITY_MODULE=ON启用)")
endif()

# ============================================================
# 硬件限位诊断测试程序 (已移除)
# ============================================================

# ============================================================
# 编译优化 - 预编译头配置 (Phase 7 - T088)
# ============================================================
# Note: PCH support temporarily disabled due to missing siligen_add_pch function
# To re-enable, create cmake/PrecompiledHeaders.cmake with the required function

# ============================================================
# Google Test单元测试框架 (T061)
# ============================================================
if(SILIGEN_BUILD_TESTS)
    message(STATUS "")
    message(STATUS "=== 单元测试配置 ===")

    # 使用FetchContent下载Google Test
    if(SILIGEN_FETCH_GTEST)
        # Set C++17 standard for all targets including GoogleTest
        set(CMAKE_CXX_STANDARD 17)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
        set(CMAKE_CXX_EXTENSIONS OFF)

        # Disable GoogleTest's automatic feature detection
        set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)

        # 检查是否存在本地Google Test
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest/CMakeLists.txt")
            message(STATUS "使用本地Google Test: ${CMAKE_CURRENT_SOURCE_DIR}/third_party/googletest")
            add_subdirectory(third_party/googletest EXCLUDE_FROM_ALL)
        else()
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG v1.14.0
            )

            # Windows平台配置 - 确保GTest使用与项目相同的编译器
            if(WIN32)
                set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
                # 传递编译器设置到GTest子项目
                set(CMAKE_CXX_COMPILER "${CMAKE_CXX_COMPILER}" CACHE FILEPATH "Compiler for GTest" FORCE)
                set(CMAKE_C_COMPILER "${CMAKE_C_COMPILER}" CACHE FILEPATH "C Compiler for GTest" FORCE)
            endif()

            FetchContent_MakeAvailable(googletest)
            message(STATUS "Google Test已通过FetchContent配置")
        endif()
    else()
        # 使用系统安装的Google Test
        find_package(GTest REQUIRED)
        message(STATUS "使用系统Google Test")
    endif()

    enable_testing()
    add_subdirectory(tests)

    message(STATUS "=== 单元测试配置完成 ===")
    message(STATUS "")
endif()
# ============================================================
# Bug 检测系统示例程序
# ============================================================
message(STATUS "")
message(STATUS "=== Bug 检测系统示例 ===")

# 添加示例子目录
add_subdirectory(examples)

message(STATUS "=== 示例程序配置完成 ===")
message(STATUS "")

# 构建完成后的信息
add_custom_target(build_info ALL
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "Siligen点胶机控制系统 - 构建完成！"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "可执行文件目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E echo ""
    if(BUILD_SECURITY_MODULE)
    COMMAND ${CMAKE_COMMAND} -E echo "可用测试程序: test_audit_log_hardware (审计日志)"
    endif()
    COMMAND ${CMAKE_COMMAND} -E echo "查看具体可用目标: cmake --build build --target help"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    VERBATIM
)



