cmake_minimum_required(VERSION 3.20)

set(SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/application/include"
)
set(SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/domain/include"
)
set(SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/adapters/include"
)
set(SILIGEN_WORKFLOW_DOMAIN_IMPL_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/domain"
)
set(SILIGEN_WORKFLOW_PROCESS_CORE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/domain/process-core"
)
set(SILIGEN_WORKFLOW_MOTION_CORE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/domain/motion-core"
)
set(SILIGEN_WORKFLOW_APPLICATION_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/application"
)
set(SILIGEN_WORKFLOW_ADAPTERS_IMPL_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/adapters"
)
set(SILIGEN_PROCESS_PATH_APPLICATION_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../process-path/application/include"
)
set(SILIGEN_MOTION_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../motion-planning/application/include"
)
set(SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../dispense-packaging/application/include"
)
if(NOT DEFINED SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR)
    set(SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../motion-planning"
    )
endif()

set(PROCESS_RUNTIME_CORE_SOURCE_ROOT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_IMPL_DIR}
    ${SILIGEN_WORKFLOW_APPLICATION_DIR}
    ${SILIGEN_WORKFLOW_ADAPTERS_IMPL_DIR}
    ${SILIGEN_WORKFLOW_PROCESS_CORE_DIR}/include
    ${SILIGEN_WORKFLOW_MOTION_CORE_DIR}/include
)
set(PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS
    ${SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../job-ingest/application/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dxf-geometry/application/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime-execution/application/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime-execution/contracts/runtime/include
    ${SILIGEN_PROCESS_PATH_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_MOTION_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_PROCESS_CORE_DIR}/include
    ${SILIGEN_WORKFLOW_MOTION_CORE_DIR}/include
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
set(PROCESS_RUNTIME_CORE_IMPLEMENTATION_INCLUDE_DIRS
    ${PROCESS_RUNTIME_CORE_SOURCE_ROOT_INCLUDE_DIRS}
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)
list(REMOVE_DUPLICATES PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS)
list(REMOVE_DUPLICATES PROCESS_RUNTIME_CORE_IMPLEMENTATION_INCLUDE_DIRS)

foreach(required_dir
    IN ITEMS
        "${SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
        "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/domain"
        "${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}/infrastructure"
        "${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}/recipes"
        "${SILIGEN_PROCESS_PATH_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
        "${SILIGEN_MOTION_PLANNING_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
        "${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
        "${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}/domain/motion"
)
    if(NOT EXISTS "${required_dir}")
        message(FATAL_ERROR "workflow canonical public include root missing: ${required_dir}")
    endif()
endforeach()

if(NOT TARGET siligen_process_runtime_core_boost_headers)
    add_library(siligen_process_runtime_core_boost_headers INTERFACE)
    target_include_directories(siligen_process_runtime_core_boost_headers INTERFACE
        ${SILIGEN_BOOST_INCLUDE_DIR}
    )
    target_compile_features(siligen_process_runtime_core_boost_headers INTERFACE cxx_std_17)
endif()

if(NOT TARGET siligen_process_runtime_core_json_headers)
    add_library(siligen_process_runtime_core_json_headers INTERFACE)
    target_include_directories(siligen_process_runtime_core_json_headers INTERFACE
        ${SILIGEN_NLOHMANN_INCLUDE_DIR}
    )
    target_compile_features(siligen_process_runtime_core_json_headers INTERFACE cxx_std_17)
endif()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/domain" "${CMAKE_BINARY_DIR}/modules/workflow/domain")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/adapters" "${CMAKE_BINARY_DIR}/modules/workflow/adapters")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/application" "${CMAKE_BINARY_DIR}/modules/workflow/application")

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
if(TARGET siligen_process_path_application_public)
    target_link_libraries(siligen_process_runtime_core_planning INTERFACE
        siligen_process_path_application_public
    )
else()
    target_link_libraries(siligen_process_runtime_core_planning INTERFACE
        siligen_process_path_domain_trajectory
    )
endif()
if(TARGET siligen_motion_planning_application_public)
    target_link_libraries(siligen_process_runtime_core_planning INTERFACE
        siligen_motion_planning_application_public
    )
else()
    target_link_libraries(siligen_process_runtime_core_planning INTERFACE
        siligen_motion
    )
endif()

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
if(TARGET siligen_dispense_packaging_application_public)
    target_link_libraries(siligen_process_runtime_core_dispensing INTERFACE
        siligen_dispense_packaging_application_public
    )
else()
    target_link_libraries(siligen_process_runtime_core_dispensing INTERFACE
        siligen_dispense_packaging_domain_dispensing
    )
endif()
target_link_libraries(siligen_process_runtime_core_dispensing INTERFACE
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
set_target_properties(siligen_process_runtime_core_application PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/workflow"
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
set_target_properties(siligen_process_runtime_core PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOTS
        "modules/workflow;modules/job-ingest;modules/dxf-geometry;modules/topology-feature;modules/process-planning;modules/coordinate-alignment;modules/process-path;modules/motion-planning;modules/dispense-packaging"
)

if(NOT TARGET siligen_workflow_domain_public)
    add_library(siligen_workflow_domain_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_domain_public INTERFACE
    siligen_workflow_domain_headers
)
if(TARGET siligen_domain)
    target_link_libraries(siligen_workflow_domain_public INTERFACE
        $<LINK_ONLY:siligen_domain>
    )
endif()
target_compile_features(siligen_workflow_domain_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_workflow_application_public)
    add_library(siligen_workflow_application_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_application_public INTERFACE
    siligen_workflow_domain_public
    siligen_workflow_application_headers
)
if(TARGET siligen_control_application)
    target_link_libraries(siligen_workflow_application_public INTERFACE
        $<LINK_ONLY:siligen_control_application>
    )
endif()
target_compile_features(siligen_workflow_application_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_workflow_adapters_public)
    add_library(siligen_workflow_adapters_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_adapters_public INTERFACE
    siligen_workflow_domain_public
    siligen_workflow_adapters_headers
)
foreach(adapter_target
    IN ITEMS
        siligen_parsing_adapter
        siligen_contour_augment_adapter
        siligen_redundancy_storage_adapter
)
    if(TARGET ${adapter_target})
        target_link_libraries(siligen_workflow_adapters_public INTERFACE
            $<LINK_ONLY:${adapter_target}>
        )
    endif()
endforeach()
target_compile_features(siligen_workflow_adapters_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_workflow_recipe_domain_public)
    add_library(siligen_workflow_recipe_domain_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_recipe_domain_public INTERFACE
    siligen_workflow_domain_headers
    domain_recipes
)
if(TARGET siligen_recipe_domain_services)
    target_link_libraries(siligen_workflow_recipe_domain_public INTERFACE
        $<LINK_ONLY:siligen_recipe_domain_services>
    )
endif()
target_compile_features(siligen_workflow_recipe_domain_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_workflow_recipe_application_public)
    add_library(siligen_workflow_recipe_application_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_recipe_application_public INTERFACE
    siligen_workflow_recipe_domain_public
    siligen_workflow_application_headers
)
if(TARGET siligen_recipe_core)
    target_link_libraries(siligen_workflow_recipe_application_public INTERFACE
        $<LINK_ONLY:siligen_recipe_core>
    )
endif()
target_compile_features(siligen_workflow_recipe_application_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_workflow_recipe_serialization_public)
    add_library(siligen_workflow_recipe_serialization_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_recipe_serialization_public INTERFACE
    siligen_workflow_recipe_domain_public
    siligen_workflow_adapters_headers
)
if(TARGET siligen_recipe_json_codec)
    target_link_libraries(siligen_workflow_recipe_serialization_public INTERFACE
        $<LINK_ONLY:siligen_recipe_json_codec>
    )
endif()
target_compile_features(siligen_workflow_recipe_serialization_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_workflow_runtime_consumer_public)
    add_library(siligen_workflow_runtime_consumer_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_runtime_consumer_public INTERFACE
    siligen_workflow_domain_public
    siligen_workflow_application_public
    siligen_workflow_adapters_public
)
target_compile_features(siligen_workflow_runtime_consumer_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_module_workflow)
    add_library(siligen_module_workflow INTERFACE)
endif()
if(TARGET siligen_workflow_application_public)
    target_link_libraries(siligen_module_workflow INTERFACE siligen_workflow_application_public)
elseif(TARGET siligen_process_runtime_core_application)
    target_link_libraries(siligen_module_workflow INTERFACE siligen_process_runtime_core_application)
elseif(TARGET siligen_process_runtime_core)
    target_link_libraries(siligen_module_workflow INTERFACE siligen_process_runtime_core)
endif()
foreach(workflow_public_target
    IN ITEMS
        siligen_workflow_recipe_application_public
        siligen_workflow_recipe_serialization_public
)
    if(TARGET ${workflow_public_target})
        target_link_libraries(siligen_module_workflow INTERFACE ${workflow_public_target})
    endif()
endforeach()
target_compile_features(siligen_module_workflow INTERFACE cxx_std_17)

set_target_properties(siligen_module_workflow PROPERTIES
    SILIGEN_MODULE_ID "M0"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/workflow"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/workflow/domain;modules/workflow/services;modules/workflow/application;modules/workflow/adapters;modules/workflow/tests;modules/workflow/examples"
)
