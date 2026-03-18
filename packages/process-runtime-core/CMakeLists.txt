cmake_minimum_required(VERSION 3.20)

set(PROCESS_RUNTIME_CORE_PACKAGE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}"
)
set(PROCESS_RUNTIME_CORE_SRC_DIR
    "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/src"
)
set(PROCESS_RUNTIME_CORE_MODULES_DIR
    "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/modules"
)
set(PROCESS_RUNTIME_CORE_CONTROL_CORE_DIR
    "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/../../control-core"
)
if(EXISTS "${PROCESS_RUNTIME_CORE_CONTROL_CORE_DIR}/src")
    set(PROCESS_RUNTIME_CORE_CONTROL_CORE_SRC_DIR
        "${PROCESS_RUNTIME_CORE_CONTROL_CORE_DIR}/src"
    )
else()
    set(PROCESS_RUNTIME_CORE_CONTROL_CORE_SRC_DIR
        "${CMAKE_SOURCE_DIR}/src"
    )
endif()
if(EXISTS "${PROCESS_RUNTIME_CORE_CONTROL_CORE_DIR}/third_party")
    set(PROCESS_RUNTIME_CORE_CONTROL_CORE_THIRD_PARTY_DIR
        "${PROCESS_RUNTIME_CORE_CONTROL_CORE_DIR}/third_party"
    )
else()
    set(PROCESS_RUNTIME_CORE_CONTROL_CORE_THIRD_PARTY_DIR
        "${CMAKE_SOURCE_DIR}/third_party"
    )
endif()

if(NOT EXISTS "${PROCESS_RUNTIME_CORE_SRC_DIR}/domain/CMakeLists.txt")
    message(FATAL_ERROR "process-runtime-core 缺少 package-local domain CMake 定义")
endif()

if(NOT TARGET siligen_shared_kernel AND EXISTS "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/../shared-kernel/CMakeLists.txt")
    add_subdirectory(
        "${PROCESS_RUNTIME_CORE_PACKAGE_DIR}/../shared-kernel"
        "${CMAKE_BINARY_DIR}/packages/shared-kernel"
    )
endif()

set(PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS
    ${PROCESS_RUNTIME_CORE_SRC_DIR}
    ${PROCESS_RUNTIME_CORE_MODULES_DIR}/process-core/include
    ${PROCESS_RUNTIME_CORE_MODULES_DIR}/motion-core/include
    ${PROCESS_RUNTIME_CORE_CONTROL_CORE_SRC_DIR}
    ${PROCESS_RUNTIME_CORE_CONTROL_CORE_THIRD_PARTY_DIR}
)

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

add_subdirectory(
    "${PROCESS_RUNTIME_CORE_SRC_DIR}/application"
    "${CMAKE_BINARY_DIR}/packages/process-runtime-core/application"
)

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
