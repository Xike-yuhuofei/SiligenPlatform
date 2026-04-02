cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_device_contracts AND EXISTS "${SILIGEN_WORKSPACE_ROOT}/shared/contracts/device/CMakeLists.txt")
    add_subdirectory(
        "${SILIGEN_WORKSPACE_ROOT}/shared/contracts/device"
        "${CMAKE_BINARY_DIR}/_p/rea_sdc"
    )
endif()

if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/rea_ppc"
    )
endif()

set(SILIGEN_RUNTIME_EXECUTION_WORKFLOW_CONTRACTS_INCLUDE_DIR
    "${CMAKE_SOURCE_DIR}/modules/workflow/contracts/include"
)
if(DEFINED SILIGEN_WORKSPACE_ROOT AND EXISTS "${SILIGEN_WORKSPACE_ROOT}/modules/workflow/contracts/include")
    set(SILIGEN_RUNTIME_EXECUTION_WORKFLOW_CONTRACTS_INCLUDE_DIR
        "${SILIGEN_WORKSPACE_ROOT}/modules/workflow/contracts/include"
    )
endif()

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
    ${CMAKE_SOURCE_DIR}/modules/runtime-execution/contracts/device/include
    ${SILIGEN_RUNTIME_EXECUTION_WORKFLOW_CONTRACTS_INCLUDE_DIR}
)
target_link_libraries(siligen_runtime_execution_application_headers INTERFACE
    siligen_runtime_execution_runtime_contracts
    siligen_dispense_packaging_contracts_public
    siligen_domain
    siligen_process_planning_contracts_public
)
if(TARGET siligen_workflow_contracts_public)
    target_link_libraries(siligen_runtime_execution_application_headers INTERFACE
        siligen_workflow_contracts_public
    )
endif()
if(TARGET siligen_device_contracts)
    target_link_libraries(siligen_runtime_execution_application_headers INTERFACE
        siligen_device_contracts
    )
endif()
target_compile_features(siligen_runtime_execution_application_headers INTERFACE cxx_std_17)

set(SILIGEN_RUNTIME_EXECUTION_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS
    ${WORKFLOW_MODULE_PUBLIC_INCLUDE_DIRS}
    ${WORKFLOW_MODULE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
list(REMOVE_DUPLICATES SILIGEN_RUNTIME_EXECUTION_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS)

if(NOT TARGET siligen_runtime_execution_motion_services)
    add_library(siligen_runtime_execution_motion_services STATIC
        services/motion/MotionBufferController.cpp
        services/motion/JogController.cpp
        services/motion/HomingProcess.cpp
        services/motion/ReadyZeroDecisionService.cpp
    )
endif()
target_include_directories(siligen_runtime_execution_motion_services PUBLIC
    ${SILIGEN_RUNTIME_EXECUTION_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_runtime_execution_motion_services PRIVATE
    ${SILIGEN_RUNTIME_EXECUTION_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_runtime_execution_motion_services PUBLIC
    siligen_runtime_execution_runtime_contracts
    siligen_types
    siligen_utils
    siligen_motion
    siligen_motion_core
)
target_compile_features(siligen_runtime_execution_motion_services PUBLIC cxx_std_17)
set_target_properties(siligen_runtime_execution_motion_services PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)

if(NOT TARGET siligen_runtime_execution_machine_model)
    add_library(siligen_runtime_execution_machine_model STATIC
        system/LegacyDispenserModel.cpp
    )
endif()
target_include_directories(siligen_runtime_execution_machine_model PUBLIC
    ${SILIGEN_RUNTIME_EXECUTION_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_runtime_execution_machine_model PRIVATE
    ${SILIGEN_RUNTIME_EXECUTION_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_runtime_execution_machine_model PUBLIC
    siligen_types
    siligen_utils
)
target_compile_features(siligen_runtime_execution_machine_model PUBLIC cxx_std_17)
set_target_properties(siligen_runtime_execution_machine_model PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)

if(NOT TARGET siligen_runtime_execution_application)
    add_library(siligen_runtime_execution_application STATIC
        services/motion/MotionControlServiceImpl.cpp
        services/motion/MotionStatusServiceImpl.cpp
        services/calibration/CalibrationWorkflowService.cpp
        usecases/motion/homing/HomeAxesUseCase.cpp
        usecases/motion/homing/EnsureAxesReadyZeroUseCase.cpp
        usecases/motion/manual/ManualMotionControlUseCase.cpp
        usecases/motion/monitoring/MotionMonitoringUseCase.cpp
        usecases/motion/MotionControlUseCase.cpp
        usecases/dispensing/DispensingExecutionUseCase.cpp
        usecases/dispensing/DispensingExecutionUseCase.Setup.cpp
        usecases/dispensing/DispensingExecutionUseCase.Control.cpp
        usecases/dispensing/DispensingExecutionUseCase.Async.cpp
    )
endif()
target_include_directories(siligen_runtime_execution_application PUBLIC
    ${SILIGEN_RUNTIME_EXECUTION_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${CMAKE_SOURCE_DIR}/modules/runtime-execution/contracts/device/include
    ${SILIGEN_RUNTIME_EXECUTION_WORKFLOW_CONTRACTS_INCLUDE_DIR}
)
target_include_directories(siligen_runtime_execution_application PRIVATE
    ${SILIGEN_RUNTIME_EXECUTION_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_runtime_execution_application PUBLIC
    siligen_runtime_execution_runtime_contracts
    siligen_dispense_packaging_contracts_public
    siligen_runtime_execution_motion_services
    siligen_runtime_execution_machine_model
    siligen_domain
    siligen_process_planning_contracts_public
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_domain
    siligen_domain_services
)
if(TARGET siligen_workflow_contracts_public)
    target_link_libraries(siligen_runtime_execution_application PUBLIC
        siligen_workflow_contracts_public
    )
endif()
if(TARGET siligen_device_contracts)
    target_link_libraries(siligen_runtime_execution_application PUBLIC
        siligen_device_contracts
    )
endif()
target_compile_definitions(siligen_runtime_execution_application PRIVATE SILIGEN_TEST_HOOKS=1)
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
