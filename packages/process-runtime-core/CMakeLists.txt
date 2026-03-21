cmake_minimum_required(VERSION 3.20)

set(PROCESS_RUNTIME_CORE_PACKAGE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
get_filename_component(
    PROCESS_RUNTIME_CORE_WORKSPACE_ROOT
    "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/../.."
    ABSOLUTE
)

set(PROCESS_RUNTIME_CORE_STANDALONE OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(PROCESS_RUNTIME_CORE_STANDALONE ON)

    if(WIN32)
        find_program(CLANG_CL_EXE NAMES clang-cl PATHS "C:/Program Files/LLVM/bin" NO_DEFAULT_PATH REQUIRED)
        find_program(LLD_LINK_EXE NAMES lld-link PATHS "C:/Program Files/LLVM/bin" NO_DEFAULT_PATH REQUIRED)
        set(CMAKE_CXX_COMPILER "${CLANG_CL_EXE}" CACHE FILEPATH "Clang-CL compiler for process-runtime-core standalone build" FORCE)
        set(CMAKE_C_COMPILER "${CLANG_CL_EXE}" CACHE FILEPATH "Clang-CL C compiler for process-runtime-core standalone build" FORCE)
        set(CMAKE_LINKER "${LLD_LINK_EXE}" CACHE FILEPATH "LLD linker for process-runtime-core standalone build" FORCE)

        find_program(CMAKE_MT NAMES mt PATHS
            "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
            "C:/Program Files (x86)/Windows Kits/10/bin/*/x64"
            NO_DEFAULT_PATH REQUIRED)

        add_compile_options(/D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /INCREMENTAL:NO")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /INCREMENTAL:NO")
        set(CMAKE_EXE_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO" CACHE STRING "Flags for Debug linker" FORCE)
        set(CMAKE_SHARED_LINKER_FLAGS_DEBUG "/debug /INCREMENTAL:NO" CACHE STRING "Flags for Debug shared linker" FORCE)
    endif()

    project(
        process_runtime_core
        VERSION 1.0.0
        LANGUAGES CXX
    )

    include(CTest)

    set(CMAKE_CXX_STANDARD 17)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)

    if(MSVC)
        set(CMAKE_CXX_STANDARD_COMPILE_OPTION "/std:c++17")
        set(CMAKE_CXX_STANDARD_OPTION_COMPILE_OPTION "/std:c++17")
        set(CMAKE_CXX17_STANDARD_COMPILE_OPTION "/std:c++17")
        set(CMAKE_CXX17_EXTENSION_COMPILE_OPTION "/std:c++17")
        set(CMAKE_CXX_COMPILE_FEATURES "cxx_std_14;cxx_std_17" CACHE STRING "CXX compiler features" FORCE)
        set(CMAKE_CXX14_COMPILE_FEATURES "cxx_std_14" CACHE STRING "CXX14 features" FORCE)
        set(CMAKE_CXX17_COMPILE_FEATURES "cxx_std_17" CACHE STRING "CXX17 features" FORCE)
    endif()

    option(SILIGEN_ENABLE_CGAL "启用CGAL支持（DXF轮廓增广）" OFF)
    option(SILIGEN_ENABLE_PROTOBUF "启用Protobuf支持" ON)
    option(SILIGEN_FETCH_GTEST "通过FetchContent下载Google Test" ON)
    option(SILIGEN_USE_PCH "使用预编译头加速编译" OFF)

    if(NOT DEFINED SILIGEN_WORKSPACE_ROOT)
        set(
            SILIGEN_WORKSPACE_ROOT
            "${PROCESS_RUNTIME_CORE_WORKSPACE_ROOT}"
            CACHE PATH "Siligen workspace root used by process-runtime-core standalone configure"
        )
    endif()

    if(NOT DEFINED SILIGEN_PCH_HEADER)
        set(
            SILIGEN_PCH_HEADER
            "${SILIGEN_WORKSPACE_ROOT}/cmake/pch/siligen_pch.h"
            CACHE FILEPATH "Shared PCH header used by process-runtime-core"
        )
    endif()

    if(NOT DEFINED SILIGEN_SHARED_COMPAT_INCLUDE_ROOT)
        set(
            SILIGEN_SHARED_COMPAT_INCLUDE_ROOT
            "${SILIGEN_WORKSPACE_ROOT}/packages/shared-kernel/include"
            CACHE PATH "Compatibility include root that serves shared/* headers"
        )
    endif()

    if(NOT DEFINED SILIGEN_THIRD_PARTY_DIR)
        set(
            SILIGEN_THIRD_PARTY_DIR
            "${SILIGEN_WORKSPACE_ROOT}/third_party"
            CACHE PATH "Workspace third_party root"
        )
    endif()

    if(NOT DEFINED SILIGEN_BOOST_INCLUDE_DIR)
        set(
            SILIGEN_BOOST_INCLUDE_DIR
            "${SILIGEN_THIRD_PARTY_DIR}/boost"
            CACHE PATH "Workspace Boost include root"
        )
    endif()

    if(NOT DEFINED SILIGEN_PROCESS_RUNTIME_CORE_DIR)
        set(
            SILIGEN_PROCESS_RUNTIME_CORE_DIR
            "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}"
            CACHE PATH "Canonical process-runtime-core package root"
        )
    endif()

    if(NOT DEFINED SILIGEN_SHARED_KERNEL_DIR)
        set(
            SILIGEN_SHARED_KERNEL_DIR
            "${SILIGEN_WORKSPACE_ROOT}/packages/shared-kernel"
            CACHE PATH "Canonical shared-kernel package root"
        )
    endif()

    if(NOT TARGET siligen_shared_kernel AND EXISTS "${SILIGEN_SHARED_KERNEL_DIR}/CMakeLists.txt")
        add_subdirectory(
            "${SILIGEN_SHARED_KERNEL_DIR}"
            "${CMAKE_BINARY_DIR}/packages/shared-kernel"
        )
    endif()

    if(NOT TARGET ruckig)
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
        set(BUILD_PYTHON_MODULE OFF CACHE BOOL "" FORCE)
        set(BUILD_ONLINE_CLIENT OFF CACHE BOOL "" FORCE)

        if(EXISTS "${SILIGEN_THIRD_PARTY_DIR}/ruckig/CMakeLists.txt")
            add_subdirectory(
                "${SILIGEN_THIRD_PARTY_DIR}/ruckig"
                "${CMAKE_BINARY_DIR}/third_party/ruckig"
            )
        else()
            message(FATAL_ERROR "未找到 Ruckig 依赖：${SILIGEN_THIRD_PARTY_DIR}/ruckig")
        endif()
    endif()

endif()

set(PROCESS_RUNTIME_CORE_SRC_DIR
    "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/src"
)
set(PROCESS_RUNTIME_CORE_MODULES_DIR
    "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/modules"
)
set(PROCESS_RUNTIME_CORE_SOURCE_ROOT_INCLUDE_DIRS
    ${PROCESS_RUNTIME_CORE_SRC_DIR}
    ${PROCESS_RUNTIME_CORE_MODULES_DIR}/process-core/include
    ${PROCESS_RUNTIME_CORE_MODULES_DIR}/motion-core/include
)

set(PROCESS_RUNTIME_CORE_SHARED_COMPAT_INCLUDE_ROOT "")
if(DEFINED SILIGEN_SHARED_COMPAT_INCLUDE_ROOT AND EXISTS "${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}/shared")
    set(PROCESS_RUNTIME_CORE_SHARED_COMPAT_INCLUDE_ROOT
        "${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}"
    )
endif()

if(NOT PROCESS_RUNTIME_CORE_SHARED_COMPAT_INCLUDE_ROOT)
    message(FATAL_ERROR "process-runtime-core 需要上层显式提供 SILIGEN_SHARED_COMPAT_INCLUDE_ROOT，禁止回落到 CMAKE_SOURCE_DIR/src")
endif()

set(PROCESS_RUNTIME_CORE_THIRD_PARTY_DIR "")
if(DEFINED SILIGEN_THIRD_PARTY_DIR AND EXISTS "${SILIGEN_THIRD_PARTY_DIR}")
    set(PROCESS_RUNTIME_CORE_THIRD_PARTY_DIR
        "${SILIGEN_THIRD_PARTY_DIR}"
    )
endif()

if(NOT PROCESS_RUNTIME_CORE_THIRD_PARTY_DIR)
    message(FATAL_ERROR "process-runtime-core 需要上层显式提供 SILIGEN_THIRD_PARTY_DIR，禁止回落到 CMAKE_SOURCE_DIR/third_party")
endif()

if(NOT EXISTS "${PROCESS_RUNTIME_CORE_SRC_DIR}/domain/CMakeLists.txt")
    message(FATAL_ERROR "process-runtime-core 缺少 package-local domain CMake 定义")
endif()

if(NOT DEFINED SILIGEN_SHARED_KERNEL_DIR)
    set(SILIGEN_SHARED_KERNEL_DIR "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/../shared-kernel")
endif()

if(NOT TARGET siligen_shared_kernel AND EXISTS "${SILIGEN_SHARED_KERNEL_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${SILIGEN_SHARED_KERNEL_DIR}"
        "${CMAKE_BINARY_DIR}/packages/shared-kernel"
    )
endif()

if(NOT TARGET siligen_types OR NOT TARGET siligen_utils OR NOT TARGET siligen_geometry)
    message(FATAL_ERROR "process-runtime-core 需要由 packages/shared-kernel 提供 siligen_types/siligen_utils/siligen_geometry")
endif()

set(PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS
    ${PROCESS_RUNTIME_CORE_SOURCE_ROOT_INCLUDE_DIRS}
    ${PROCESS_RUNTIME_CORE_SHARED_COMPAT_INCLUDE_ROOT}
    ${PROCESS_RUNTIME_CORE_THIRD_PARTY_DIR}
)
list(REMOVE_DUPLICATES PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS)

add_library(siligen_domain INTERFACE)
target_include_directories(siligen_domain INTERFACE
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)
target_compile_features(siligen_domain INTERFACE cxx_std_17)

add_subdirectory(
    "${PROCESS_RUNTIME_CORE_MODULES_DIR}/process-core"
    "${CMAKE_BINARY_DIR}/packages/process-runtime-core/process-core"
)
add_subdirectory(
    "${PROCESS_RUNTIME_CORE_MODULES_DIR}/motion-core"
    "${CMAKE_BINARY_DIR}/packages/process-runtime-core/motion-core"
)
add_subdirectory(
    "${PROCESS_RUNTIME_CORE_SRC_DIR}/domain"
    "${CMAKE_BINARY_DIR}/packages/process-runtime-core/domain"
)

target_link_libraries(siligen_domain INTERFACE
    siligen_motion
    siligen_triggering
    siligen_configuration
    siligen_domain_services
    siligen_process_core
    siligen_motion_core
    domain_trajectory
    domain_recipes
    domain_machine
    domain_dispensing
)

if(NOT TARGET siligen_recipe_json_codec)
    add_subdirectory(
        "${PROCESS_RUNTIME_CORE_SRC_DIR}/recipes/serialization"
        "${CMAKE_BINARY_DIR}/packages/process-runtime-core/recipes-serialization"
    )
endif()

if(NOT TARGET siligen_parsing_adapter)
    add_subdirectory(
        "${PROCESS_RUNTIME_CORE_SRC_DIR}/infrastructure/adapters/planning/dxf"
        "${CMAKE_BINARY_DIR}/packages/process-runtime-core/infrastructure-adapters-planning-dxf"
    )
endif()

if(NOT TARGET siligen_contour_augment_adapter)
    add_subdirectory(
        "${PROCESS_RUNTIME_CORE_SRC_DIR}/infrastructure/adapters/planning/geometry"
        "${CMAKE_BINARY_DIR}/packages/process-runtime-core/infrastructure-adapters-planning-geometry"
    )
endif()

if(NOT TARGET siligen_redundancy_storage_adapter)
    add_subdirectory(
        "${PROCESS_RUNTIME_CORE_SRC_DIR}/infrastructure/adapters/redundancy"
        "${CMAKE_BINARY_DIR}/packages/process-runtime-core/infrastructure-adapters-redundancy"
    )
endif()

add_subdirectory(
    "${PROCESS_RUNTIME_CORE_SRC_DIR}/application"
    "${CMAKE_BINARY_DIR}/packages/process-runtime-core/application"
)

function(process_runtime_core_stage_runtime_dependency target_name dependency_target)
    if(WIN32 AND TARGET ${dependency_target})
        add_custom_command(
            TARGET ${target_name}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${dependency_target}>"
                "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM
        )
    endif()
endfunction()

function(process_runtime_core_add_test_with_runtime_environment test_name test_target)
    if(NOT TARGET ${test_target})
        message(FATAL_ERROR "无法为不存在的 target 注册测试运行环境: ${test_target}")
    endif()

    add_test(
        NAME ${test_name}
        COMMAND $<TARGET_FILE:${test_target}>
    )

    if(WIN32)
        set(test_runtime_path_entries
            "PATH=path_list_prepend:$<TARGET_FILE_DIR:${test_target}>"
            "PATH=path_list_prepend:${CMAKE_BINARY_DIR}"
        )

        if(TARGET gtest)
            list(APPEND test_runtime_path_entries
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:gtest>"
            )
        endif()

        if(TARGET gtest_main)
            list(APPEND test_runtime_path_entries
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:gtest_main>"
            )
        endif()

        if(TARGET abseil_dll)
            list(APPEND test_runtime_path_entries
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:abseil_dll>"
            )
        endif()

        if(TARGET utf8_validity)
            list(APPEND test_runtime_path_entries
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:utf8_validity>"
            )
        endif()

        if(TARGET ruckig)
            list(APPEND test_runtime_path_entries
                "PATH=path_list_prepend:$<TARGET_FILE_DIR:ruckig>"
            )
        endif()

        list(REMOVE_DUPLICATES test_runtime_path_entries)
        set_tests_properties(
            ${test_name}
            PROPERTIES ENVIRONMENT_MODIFICATION "${test_runtime_path_entries}"
        )
    endif()
endfunction()

function(process_runtime_core_enable_standalone_tests)
    if(NOT TARGET protobuf::libprotobuf-lite)
        if(NOT SILIGEN_ENABLE_PROTOBUF)
            message(FATAL_ERROR "process-runtime-core standalone tests 需要启用 SILIGEN_ENABLE_PROTOBUF")
        endif()

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
            add_subdirectory(
                "${SILIGEN_THIRD_PARTY_DIR}/protobuf"
                "${CMAKE_BINARY_DIR}/third_party/protobuf"
            )
        else()
            message(FATAL_ERROR "未找到 Protobuf 依赖：${SILIGEN_THIRD_PARTY_DIR}/protobuf")
        endif()
    endif()

    if(NOT TARGET GTest::gtest_main AND NOT TARGET gtest_main)
        if(SILIGEN_FETCH_GTEST)
            if(EXISTS "${SILIGEN_THIRD_PARTY_DIR}/googletest/CMakeLists.txt")
                add_subdirectory(
                    "${SILIGEN_THIRD_PARTY_DIR}/googletest"
                    "${CMAKE_BINARY_DIR}/third_party/googletest"
                    EXCLUDE_FROM_ALL
                )
            else()
                include(FetchContent)
                set(gtest_disable_pthreads ON CACHE BOOL "" FORCE)
                if(WIN32)
                    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
                endif()
                FetchContent_Declare(
                    googletest
                    GIT_REPOSITORY https://github.com/google/googletest.git
                    GIT_TAG v1.14.0
                )
                FetchContent_MakeAvailable(googletest)
            endif()
        else()
            find_package(GTest REQUIRED)
        endif()
    endif()

    add_subdirectory(
        "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/tests"
        "${CMAKE_BINARY_DIR}/packages/process-runtime-core-tests"
    )
endfunction()

if(BUILD_TESTING AND EXISTS "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/tests/unit/motion/DeterministicPathExecutionUseCaseTest.cpp")
    add_executable(process_runtime_core_deterministic_path_execution_test
        tests/unit/motion/DeterministicPathExecutionUseCaseTest.cpp
    )
    target_include_directories(process_runtime_core_deterministic_path_execution_test PRIVATE
        ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
        ${PROCESS_RUNTIME_CORE_SRC_DIR}
    )
    target_link_libraries(process_runtime_core_deterministic_path_execution_test PRIVATE
        siligen_application_motion
        siligen_types
        siligen_utils
    )
    target_compile_features(process_runtime_core_deterministic_path_execution_test PRIVATE cxx_std_17)
    process_runtime_core_stage_runtime_dependency(process_runtime_core_deterministic_path_execution_test ruckig)
    process_runtime_core_add_test_with_runtime_environment(
        process_runtime_core_deterministic_path_execution_test
        process_runtime_core_deterministic_path_execution_test
    )
endif()

if(BUILD_TESTING AND EXISTS "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/tests/unit/motion/MotionRuntimeAssemblyFactoryTest.cpp")
    add_executable(process_runtime_core_motion_runtime_assembly_test
        tests/unit/motion/MotionRuntimeAssemblyFactoryTest.cpp
    )
    target_include_directories(process_runtime_core_motion_runtime_assembly_test PRIVATE
        ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
        ${PROCESS_RUNTIME_CORE_SRC_DIR}
    )
    target_link_libraries(process_runtime_core_motion_runtime_assembly_test PRIVATE
        siligen_application_motion
        siligen_types
        siligen_utils
    )
    target_compile_features(process_runtime_core_motion_runtime_assembly_test PRIVATE cxx_std_17)
    process_runtime_core_stage_runtime_dependency(process_runtime_core_motion_runtime_assembly_test ruckig)
    process_runtime_core_add_test_with_runtime_environment(
        process_runtime_core_motion_runtime_assembly_test
        process_runtime_core_motion_runtime_assembly_test
    )
endif()

if(BUILD_TESTING AND EXISTS "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/tests/unit/infrastructure/adapters/planning/dxf/PbPathSourceAdapterContractTest.cpp")
    add_executable(process_runtime_core_pb_path_source_adapter_test
        tests/unit/infrastructure/adapters/planning/dxf/PbPathSourceAdapterContractTest.cpp
    )
    target_include_directories(process_runtime_core_pb_path_source_adapter_test PRIVATE
        ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
        ${PROCESS_RUNTIME_CORE_SRC_DIR}
    )
    target_link_libraries(process_runtime_core_pb_path_source_adapter_test PRIVATE
        siligen_parsing_adapter
        siligen_types
        siligen_utils
    )
    target_compile_features(process_runtime_core_pb_path_source_adapter_test PRIVATE cxx_std_17)
    process_runtime_core_add_test_with_runtime_environment(
        process_runtime_core_pb_path_source_adapter_test
        process_runtime_core_pb_path_source_adapter_test
    )
endif()

function(siligen_process_runtime_core_interface target_name)
    add_library(${target_name} INTERFACE)
    target_include_directories(${target_name} INTERFACE
        ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
    )
    target_compile_features(${target_name} INTERFACE cxx_std_17)
endfunction()

siligen_process_runtime_core_interface(siligen_process_runtime_core_process)
target_link_libraries(siligen_process_runtime_core_process INTERFACE
    siligen_process_core
    siligen_configuration
)

siligen_process_runtime_core_interface(siligen_process_runtime_core_planning)
target_link_libraries(siligen_process_runtime_core_planning INTERFACE
    domain_trajectory
    siligen_motion
)

siligen_process_runtime_core_interface(siligen_process_runtime_core_execution)
target_link_libraries(siligen_process_runtime_core_execution INTERFACE
    siligen_triggering
    siligen_motion_execution_services
    siligen_dispensing_execution_services
)

siligen_process_runtime_core_interface(siligen_process_runtime_core_machine)
target_link_libraries(siligen_process_runtime_core_machine INTERFACE
    domain_machine
)

siligen_process_runtime_core_interface(siligen_process_runtime_core_safety)
target_link_libraries(siligen_process_runtime_core_safety INTERFACE
    siligen_motion_core
    siligen_safety_domain_services
)

siligen_process_runtime_core_interface(siligen_process_runtime_core_recipes)
target_link_libraries(siligen_process_runtime_core_recipes INTERFACE
    domain_recipes
    siligen_process_core
    siligen_recipe_domain_services
    siligen_recipe_core
)

siligen_process_runtime_core_interface(siligen_process_runtime_core_dispensing)
target_link_libraries(siligen_process_runtime_core_dispensing INTERFACE
    domain_dispensing
    siligen_triggering
    siligen_dispensing_execution_services
    siligen_valve_core
)

siligen_process_runtime_core_interface(siligen_process_runtime_core_diagnostics_domain)
target_link_libraries(siligen_process_runtime_core_diagnostics_domain INTERFACE
    siligen_diagnostics_domain_services
)

siligen_process_runtime_core_interface(siligen_process_runtime_core_application)
target_link_libraries(siligen_process_runtime_core_application INTERFACE
    siligen_control_application
)

siligen_process_runtime_core_interface(siligen_process_runtime_core)
target_link_libraries(siligen_process_runtime_core INTERFACE
    siligen_process_runtime_core_process
    siligen_process_runtime_core_planning
    siligen_process_runtime_core_execution
    siligen_process_runtime_core_machine
    siligen_process_runtime_core_safety
    siligen_process_runtime_core_recipes
    siligen_process_runtime_core_dispensing
    siligen_process_runtime_core_diagnostics_domain
    siligen_process_runtime_core_application
)

if(PROCESS_RUNTIME_CORE_STANDALONE AND BUILD_TESTING)
    process_runtime_core_enable_standalone_tests()
    if(TARGET process_runtime_core_tests AND TARGET process_runtime_core_deterministic_path_execution_test)
        add_dependencies(
            process_runtime_core_tests
            process_runtime_core_deterministic_path_execution_test
        )
    endif()
    if(TARGET process_runtime_core_tests AND TARGET process_runtime_core_motion_runtime_assembly_test)
        add_dependencies(
            process_runtime_core_tests
            process_runtime_core_motion_runtime_assembly_test
        )
    endif()
    if(TARGET process_runtime_core_tests AND TARGET process_runtime_core_pb_path_source_adapter_test)
        add_dependencies(
            process_runtime_core_tests
            process_runtime_core_pb_path_source_adapter_test
        )
    endif()
endif()
