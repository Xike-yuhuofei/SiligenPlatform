cmake_minimum_required(VERSION 3.20)

set(SILIGEN_RUNTIME_EXECUTION_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_APPLICATION_PUBLIC_INCLUDE_DIR}/runtime_execution")
    message(FATAL_ERROR
        "runtime-execution canonical application include root missing: "
        "${SILIGEN_RUNTIME_EXECUTION_APPLICATION_PUBLIC_INCLUDE_DIR}/runtime_execution"
    )
endif()

if(NOT TARGET siligen_runtime_execution_application_headers)
    add_library(siligen_runtime_execution_application_headers INTERFACE)
endif()
target_include_directories(siligen_runtime_execution_application_headers INTERFACE
    ${SILIGEN_RUNTIME_EXECUTION_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_link_libraries(siligen_runtime_execution_application_headers INTERFACE
    siligen_runtime_execution_runtime_contracts
    siligen_domain
    siligen_process_planning_domain_configuration
    siligen_process_path_application_public
    siligen_motion_planning_application_public
    siligen_dispense_packaging_application_public
    siligen_dxf_geometry_application_public
)
target_compile_features(siligen_runtime_execution_application_headers INTERFACE cxx_std_17)

set(SILIGEN_RUNTIME_EXECUTION_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
    ${PROCESS_RUNTIME_CORE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
list(REMOVE_DUPLICATES SILIGEN_RUNTIME_EXECUTION_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS)

if(NOT TARGET siligen_runtime_execution_application)
    add_library(siligen_runtime_execution_application STATIC
        usecases/dispensing/DispensingExecutionUseCase.cpp
        usecases/dispensing/DispensingExecutionUseCase.Setup.cpp
        usecases/dispensing/DispensingExecutionUseCase.Control.cpp
        usecases/dispensing/DispensingExecutionUseCase.Async.cpp
    )
endif()
target_include_directories(siligen_runtime_execution_application PUBLIC
    ${SILIGEN_RUNTIME_EXECUTION_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_runtime_execution_application PRIVATE
    ${SILIGEN_RUNTIME_EXECUTION_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_runtime_execution_application PUBLIC
    siligen_runtime_execution_runtime_contracts
    siligen_domain
    siligen_process_planning_domain_configuration
    siligen_process_path_application_public
    siligen_motion_planning_application_public
    siligen_dispense_packaging_application_public
    siligen_dxf_geometry_application_public
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_domain
    siligen_domain_services
)
target_compile_features(siligen_runtime_execution_application PUBLIC cxx_std_17)
set_target_properties(siligen_runtime_execution_application PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)

if(NOT TARGET siligen_runtime_execution_application_public)
    add_library(siligen_runtime_execution_application_public INTERFACE)
endif()
target_link_libraries(siligen_runtime_execution_application_public INTERFACE
    siligen_runtime_execution_application_headers
    $<LINK_ONLY:siligen_runtime_execution_application>
)
target_compile_features(siligen_runtime_execution_application_public INTERFACE cxx_std_17)
set_target_properties(siligen_runtime_execution_application_public PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)

if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_runtime_execution_application PRIVATE ${SILIGEN_PCH_HEADER})
endif()
