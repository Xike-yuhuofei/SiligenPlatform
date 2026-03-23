cmake_minimum_required(VERSION 3.16)

if(WIN32)
    find_program(CLANG_CL_EXE NAMES clang-cl PATHS "C:/Program Files/LLVM/bin" NO_DEFAULT_PATH REQUIRED)
    find_program(LLD_LINK_EXE NAMES lld-link PATHS "C:/Program Files/LLVM/bin" NO_DEFAULT_PATH REQUIRED)
    set(CMAKE_CXX_COMPILER "${CLANG_CL_EXE}" CACHE FILEPATH "Clang-CL编译器" FORCE)
    set(CMAKE_C_COMPILER "${CLANG_CL_EXE}" CACHE FILEPATH "Clang-CL C编译器" FORCE)
    set(CMAKE_LINKER "${LLD_LINK_EXE}" CACHE FILEPATH "LLD链接器" FORCE)

    find_program(CMAKE_MT NAMES mt PATHS
        "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
        "C:/Program Files (x86)/Windows Kits/10/bin/*/x64"
        NO_DEFAULT_PATH REQUIRED)
    if(CMAKE_MT)
        message(STATUS "📍 MT.exe: ${CMAKE_MT}")
    endif()

    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO" CACHE STRING "Flags for Debug linker" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO" CACHE STRING "Flags for Debug shared linker" FORCE)
    set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO "/debug /INCREMENTAL:NO" CACHE STRING "Flags for RelWithDebInfo linker" FORCE)
    set(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO "/debug /INCREMENTAL:NO" CACHE STRING "Flags for RelWithDebInfo shared linker" FORCE)

    if(NOT CLANG_CL_EXE)
        message(FATAL_ERROR "❌ 未找到Clang-CL编译器，请安装LLVM工具链")
    endif()

    add_compile_options(/D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
    message(STATUS "⚠️ 使用 _ALLOW_COMPILER_AND_STL_VERSION_MISMATCH 绕过版本检查")
    message(STATUS "✅ 使用Clang-CL + LLD编译配置")
    message(STATUS "📍 编译器: ${CMAKE_CXX_COMPILER}")
    message(STATUS "📍 链接器: ${CMAKE_LINKER}")
endif()

project(SiligenDispenser
    VERSION 1.0.0
    DESCRIPTION "Siligen工业点胶机控制系统 - 工作区根 canonical C++ superbuild"
    LANGUAGES CXX
)

get_filename_component(SILIGEN_CMAKE_SOURCE_ROOT "${CMAKE_SOURCE_DIR}" ABSOLUTE)
set(SILIGEN_LAYOUT_FILE "${SILIGEN_CMAKE_SOURCE_ROOT}/cmake/workspace-layout.env")
include("${SILIGEN_CMAKE_SOURCE_ROOT}/cmake/LoadWorkspaceLayout.cmake")
siligen_load_workspace_layout(
    "${SILIGEN_CMAKE_SOURCE_ROOT}"
    "${SILIGEN_LAYOUT_FILE}"
)
set(SILIGEN_WORKSPACE_ROOT "${SILIGEN_CMAKE_SOURCE_ROOT}")
if(NOT "${SILIGEN_WORKSPACE_ROOT}" STREQUAL "${CMAKE_SOURCE_DIR}")
    message(FATAL_ERROR
        "workspace root must be canonical superbuild source root: "
        "CMAKE_SOURCE_DIR='${CMAKE_SOURCE_DIR}', resolved='${SILIGEN_WORKSPACE_ROOT}'"
    )
endif()
set(SILIGEN_WORKSPACE_ROOT
    "${SILIGEN_WORKSPACE_ROOT}"
    CACHE INTERNAL "Workspace root used by the canonical build graph"
)
set(SILIGEN_PCH_HEADER
    "${SILIGEN_WORKSPACE_ROOT}/cmake/pch/siligen_pch.h"
    CACHE INTERNAL "Shared PCH header provided by the workspace root"
)
set(SILIGEN_SHARED_COMPAT_INCLUDE_ROOT
    "${SILIGEN_SHARED_KERNEL_DIR}/include"
    CACHE INTERNAL "Compatibility include root for shared/* headers"
)
set(SILIGEN_BOOST_INCLUDE_DIR
    "${SILIGEN_THIRD_PARTY_DIR}"
    CACHE INTERNAL "Explicit Boost include root used by #include <boost/...>"
)
set(SILIGEN_NLOHMANN_INCLUDE_DIR
    "${SILIGEN_THIRD_PARTY_DIR}"
    CACHE INTERNAL "Explicit nlohmann/json include root used by #include \"nlohmann/json.hpp\""
)
set(SILIGEN_SPDLOG_INCLUDE_DIR
    "${SILIGEN_THIRD_PARTY_DIR}/spdlog/include"
    CACHE INTERNAL "Explicit spdlog include root used by traceability-observability"
)
set(SILIGEN_RUCKIG_SOURCE_DIR
    "${SILIGEN_THIRD_PARTY_DIR}/ruckig"
    CACHE INTERNAL "Explicit Ruckig source root used by process-runtime-core"
)
set(SILIGEN_PROTOBUF_SOURCE_DIR
    "${SILIGEN_THIRD_PARTY_DIR}/protobuf"
    CACHE INTERNAL "Explicit Protobuf source root used by process-runtime-core"
)
set(SILIGEN_GTEST_SOURCE_DIR
    "${SILIGEN_THIRD_PARTY_DIR}/googletest"
    CACHE INTERNAL "Explicit GoogleTest source root used by process-runtime-core"
)

if(NOT EXISTS "${SILIGEN_SHARED_KERNEL_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "未找到 canonical shared-kernel package: ${SILIGEN_SHARED_KERNEL_DIR}")
endif()
if(NOT EXISTS "${SILIGEN_MACHINE_CONFIG_FILE}")
    message(FATAL_ERROR "未找到 canonical machine config: ${SILIGEN_MACHINE_CONFIG_FILE}")
endif()
if(NOT EXISTS "${SILIGEN_PCH_HEADER}")
    message(FATAL_ERROR "未找到 workspace root PCH: ${SILIGEN_PCH_HEADER}")
endif()

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

if(MSVC)
    set(CMAKE_CXX_STANDARD_COMPILE_OPTION "/std:c++17")
    set(CMAKE_CXX_STANDARD_OPTION_COMPILE_OPTION "/std:c++17")
    set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "/std:c++17")
    set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "/std:c++17")
    set(CMAKE_CXX_COMPILE_FEATURES "cxx_std_14;cxx_std_17" CACHE STRING "CXX compiler features" FORCE)
    set(CMAKE_CXX14_COMPILE_FEATURES "cxx_std_14" CACHE STRING "CXX14 features" FORCE)
    set(CMAKE_CXX17_COMPILE_FEATURES "cxx_std_17" CACHE STRING "CXX17 features" FORCE)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded CACHE STRING "MSVC debug info format" FORCE)
    message(STATUS "配置 MSVC 编译器特性（GoogleTest 兼容性）")
endif()

set(SILIGEN_BUILD_TARGET "all" CACHE STRING "构建目标选择: all/tests/clean")
set_property(CACHE SILIGEN_BUILD_TARGET PROPERTY STRINGS all tests clean)
option(SILIGEN_CLEAN_BUILD "清理构建目录" OFF)
option(SILIGEN_BUILD_TESTS "构建单元测试" OFF)
option(SILIGEN_FETCH_GTEST "通过FetchContent下载Google Test" ON)
option(SILIGEN_ENABLE_CLANG_TIDY "启用clang-tidy静态分析" OFF)
option(SILIGEN_ENABLE_COVERAGE "启用代码覆盖率" OFF)
option(SILIGEN_ENABLE_MOCK_MULTICARD "构建 MockMultiCard 支持" ON)
option(SILIGEN_ENABLE_CGAL "启用CGAL支持（DXF轮廓增广）" OFF)
option(SILIGEN_ENABLE_PROTOBUF "启用Protobuf支持" ON)
option(BUILD_SECURITY_MODULE "构建安全模块" OFF)
set(SILIGEN_LOG_LEVEL "ERROR" CACHE STRING "构建日志级别: ERROR/WARNING/INFO")
option(SILIGEN_USE_PCH "使用预编译头加速编译" ON)
option(SILIGEN_USE_UNITY_BUILD "使用Unity Build加速编译(实验性)" OFF)
option(SILIGEN_PARALLEL_COMPILE "启用并行编译" ON)

if(WIN32)
    message(STATUS "🔧 构建系统: ${CMAKE_GENERATOR}")
    message(STATUS "🏗️ 编译类型: ${CMAKE_BUILD_TYPE}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "===========================================")
message(STATUS "Siligen点胶机控制系统 - 工作区根 CMake 构建")
message(STATUS "===========================================")
message(STATUS "项目版本: ${PROJECT_VERSION}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "编译器路径: ${CMAKE_CXX_COMPILER}")
message(STATUS "工作区根: ${SILIGEN_WORKSPACE_ROOT}")
message(STATUS "===========================================")
message(STATUS "")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(SILIGEN_ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE
        NAMES clang-tidy
        PATHS "C:/Program Files/LLVM/bin"
        NO_DEFAULT_PATH
    )

    if(CLANG_TIDY_EXE)
        message(STATUS "✅ clang-tidy 找到: ${CLANG_TIDY_EXE}")
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
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

if(SILIGEN_ENABLE_COVERAGE)
    if(MSVC)
        message(WARNING "MSVC不支持gcov覆盖率,请使用Visual Studio覆盖率工具")
    else()
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
        message(STATUS "代码覆盖率已启用")
    endif()
endif()

if(NOT MSVC)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

if(SILIGEN_PARALLEL_COMPILE)
    if(MSVC AND NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(/MP)
        add_compile_options(/FS)
        message(STATUS "并行编译已启用: MSVC /MP")
    else()
        message(STATUS "并行编译已启用: 使用构建系统并行能力")
    endif()
endif()

if(SILIGEN_USE_UNITY_BUILD)
    set(CMAKE_UNITY_BUILD ON)
    set(CMAKE_UNITY_BUILD_BATCH_SIZE 16)
    message(STATUS "Unity Build已启用: 批次大小=16")
endif()

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
    add_compile_options(
        /permissive-
        /utf-8
        /EHsc
    )
    if(SILIGEN_WARNING_LEVEL EQUAL "1")
        add_compile_options(/W1)
    elseif(SILIGEN_WARNING_LEVEL GREATER "1")
        add_compile_options(/W4)
    endif()
    add_compile_options(/WX-)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        add_compile_options(/clang:-msse3)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        add_compile_options(/arch:SSE2)
    endif()
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_definitions(UNICODE _UNICODE)
    set(CMAKE_CXX_FLAGS_DEBUG "/MDd /Od /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "/MD /O2 /DNDEBUG")
else()
    add_compile_options(
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
    if(SILIGEN_WARNING_LEVEL EQUAL "0")
        add_compile_options(-w)
    elseif(SILIGEN_WARNING_LEVEL EQUAL "1")
        add_compile_options(-Wall)
    else()
        add_compile_options(-Wall -Wextra -Wpedantic)
    endif()
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
foreach(cfg Debug Release RelWithDebInfo MinSizeRel)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/bin)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${cfg} ${CMAKE_BINARY_DIR}/lib)
endforeach()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/logs)
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/cache)

message(STATUS "")
message(STATUS "=== 第三方库配置 ===")
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

    if(EXISTS "${SILIGEN_THIRD_PARTY_DIR}/protobuf/CMakeLists.txt")
        add_subdirectory("${SILIGEN_THIRD_PARTY_DIR}/protobuf" "${CMAKE_BINARY_DIR}/third_party/protobuf")
        message(STATUS "[ThirdParty] Protobuf 库已添加")
    else()
        message(WARNING "[ThirdParty] Protobuf 库未找到 (${SILIGEN_THIRD_PARTY_DIR}/protobuf)")
    endif()
endif()

set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(BUILD_PYTHON_MODULE OFF CACHE BOOL "" FORCE)
set(BUILD_ONLINE_CLIENT OFF CACHE BOOL "" FORCE)
if(EXISTS "${SILIGEN_THIRD_PARTY_DIR}/ruckig/CMakeLists.txt")
    add_subdirectory("${SILIGEN_THIRD_PARTY_DIR}/ruckig" "${CMAKE_BINARY_DIR}/third_party/ruckig")
    message(STATUS "[ThirdParty] Ruckig 库已添加")
else()
    message(WARNING "[ThirdParty] Ruckig 库未找到 (${SILIGEN_THIRD_PARTY_DIR}/ruckig)")
endif()
message(STATUS "=== 第三方库配置完成 ===")
message(STATUS "")

message(STATUS "")
message(STATUS "=== 六边形架构模块 ===")
add_subdirectory("${SILIGEN_SHARED_KERNEL_DIR}" "${CMAKE_BINARY_DIR}/packages/shared-kernel")
if(NOT TARGET siligen_types OR NOT TARGET siligen_utils OR NOT TARGET siligen_geometry)
    message(FATAL_ERROR "shared compat targets 必须由 packages/shared-kernel 提前注册")
endif()

add_library(siligen_shared INTERFACE)
target_link_libraries(siligen_shared INTERFACE
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_shared_kernel
)

add_subdirectory("${SILIGEN_PROCESS_RUNTIME_CORE_DIR}" "${CMAKE_BINARY_DIR}/packages/process-runtime-core")

include_directories(${SILIGEN_WORKSPACE_ROOT})
include_directories(${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src)
include_directories(${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/modules/process-core/include)
include_directories(${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/modules/motion-core/include)
include_directories(${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT})
include_directories(${SILIGEN_BOOST_INCLUDE_DIR})
include_directories(${SILIGEN_NLOHMANN_INCLUDE_DIR})
include_directories(${SILIGEN_SPDLOG_INCLUDE_DIR})
include_directories(${SILIGEN_WORKSPACE_ROOT}/tests)

add_subdirectory("${SILIGEN_RUNTIME_HOST_DIR}" "${CMAKE_BINARY_DIR}/packages/runtime-host")
add_subdirectory("${SILIGEN_TRANSPORT_GATEWAY_DIR}" "${CMAKE_BINARY_DIR}/packages/transport-gateway")
add_subdirectory(apps)

if(NOT TARGET siligen_domain)
    message(FATAL_ERROR "packages/process-runtime-core 必须提供 siligen_domain")
endif()

message(STATUS "=== 六边形架构模块配置完成 ===")
message(STATUS "")

message(STATUS "=== 构建目标: ${SILIGEN_BUILD_TARGET} ===")
if(SILIGEN_BUILD_TARGET STREQUAL "tests")
    set(SILIGEN_BUILD_TESTS ON CACHE BOOL "构建单元测试" FORCE)
    message(STATUS "[目标] 测试模式 - 单元测试")
elseif(SILIGEN_BUILD_TARGET STREQUAL "clean")
    message(STATUS "[目标] 清理模式 - 准备清理构建目录")
else()
    message(STATUS "[目标] 完整模式 - 构建所有目标")
endif()
message(STATUS "=== 构建目标配置完成 ===")
message(STATUS "")

if(WIN32)
    set(WINDOWS_LIBS user32 kernel32 ws2_32)
endif()

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/config/machine)
configure_file(
    ${SILIGEN_MACHINE_CONFIG_FILE}
    ${CMAKE_BINARY_DIR}/config/machine/machine_config.ini
    COPYONLY
)

install(FILES ${SILIGEN_MACHINE_CONFIG_FILE}
    DESTINATION config/machine
)

if(EXISTS "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll")
    install(FILES ${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll
        DESTINATION bin
    )
endif()

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
        ${SILIGEN_SHARED_KERNEL_DIR}/src/shared/hashing/HashCalculator.cpp
    )

    target_include_directories(security_module PRIVATE
        ${SILIGEN_WORKSPACE_ROOT}
        ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
        ${SILIGEN_RUNTIME_HOST_DIR}/src
        ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
        ${SILIGEN_BOOST_INCLUDE_DIR}
    )

    target_link_libraries(security_module
        siligen_shared_kernel
        ${WINDOWS_LIBS}
        bcrypt
    )
else()
    message(STATUS "安全模块未启用(使用-DBUILD_SECURITY_MODULE=ON启用)")
endif()

if(SILIGEN_BUILD_TESTS)
    message(STATUS "")
    message(STATUS "=== 单元测试配置 ===")

    if(SILIGEN_FETCH_GTEST)
        set(CMAKE_CXX_STANDARD 17)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
        set(CMAKE_CXX_EXTENSIONS OFF)
        set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)

        if(EXISTS "${SILIGEN_THIRD_PARTY_DIR}/googletest/CMakeLists.txt")
            message(STATUS "使用本地Google Test: ${SILIGEN_THIRD_PARTY_DIR}/googletest")
            add_subdirectory("${SILIGEN_THIRD_PARTY_DIR}/googletest" "${CMAKE_BINARY_DIR}/third_party/googletest" EXCLUDE_FROM_ALL)
        else()
            include(FetchContent)
            if(WIN32)
                set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
                set(CMAKE_CXX_COMPILER "${CMAKE_CXX_COMPILER}" CACHE FILEPATH "Compiler for GTest" FORCE)
                set(CMAKE_C_COMPILER "${CMAKE_C_COMPILER}" CACHE FILEPATH "C Compiler for GTest" FORCE)
            endif()
            FetchContent_Declare(
                googletest
                GIT_REPOSITORY https://github.com/google/googletest.git
                GIT_TAG v1.14.0
            )
            FetchContent_MakeAvailable(googletest)
            message(STATUS "Google Test已通过FetchContent配置")
        endif()
    else()
        find_package(GTest REQUIRED)
        message(STATUS "使用系统Google Test")
    endif()

    enable_testing()
    add_subdirectory(tests)
    message(STATUS "=== 单元测试配置完成 ===")
    message(STATUS "")
endif()

add_subdirectory(examples)

add_custom_target(build_info ALL
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "Siligen点胶机控制系统 - 构建完成！"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    COMMAND ${CMAKE_COMMAND} -E echo "可执行文件目录: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
    COMMAND ${CMAKE_COMMAND} -E echo "工作区根 source root: ${SILIGEN_WORKSPACE_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E echo "查看具体可用目标: cmake --build build --target help"
    COMMAND ${CMAKE_COMMAND} -E echo "=========================================="
    VERBATIM
)
