cmake_minimum_required(VERSION 3.20)

set(SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/application/include"
)
set(SILIGEN_WORKFLOW_CONTRACTS_PUBLIC_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/contracts/include"
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

set(WORKFLOW_MODULE_SOURCE_ROOT_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_IMPL_DIR}
    ${SILIGEN_WORKFLOW_APPLICATION_DIR}
    ${SILIGEN_WORKFLOW_ADAPTERS_IMPL_DIR}
    ${SILIGEN_WORKFLOW_PROCESS_CORE_DIR}/include
    ${SILIGEN_WORKFLOW_MOTION_CORE_DIR}/include
)
set(WORKFLOW_MODULE_PUBLIC_INCLUDE_DIRS
    ${SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_CONTRACTS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../job-ingest/application/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../dxf-geometry/application/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime-execution/application/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../runtime-execution/contracts/runtime/include
    ${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_PROCESS_CORE_DIR}/include
    ${SILIGEN_WORKFLOW_MOTION_CORE_DIR}/include
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
set(WORKFLOW_MODULE_IMPLEMENTATION_INCLUDE_DIRS
    ${WORKFLOW_MODULE_SOURCE_ROOT_INCLUDE_DIRS}
    ${WORKFLOW_MODULE_PUBLIC_INCLUDE_DIRS}
)
list(REMOVE_DUPLICATES WORKFLOW_MODULE_PUBLIC_INCLUDE_DIRS)
list(REMOVE_DUPLICATES WORKFLOW_MODULE_IMPLEMENTATION_INCLUDE_DIRS)

foreach(required_dir
    IN ITEMS
        "${SILIGEN_WORKFLOW_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
        "${SILIGEN_WORKFLOW_CONTRACTS_PUBLIC_INCLUDE_DIR}/workflow/contracts"
        "${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}/domain"
        "${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}/infrastructure"
        "${SILIGEN_WORKFLOW_ADAPTERS_PUBLIC_INCLUDE_DIR}/workflow/adapters/recipes"
        "${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}/domain/motion"
)
    if(NOT EXISTS "${required_dir}")
        message(FATAL_ERROR "workflow canonical public include root missing: ${required_dir}")
    endif()
endforeach()

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/domain" "${CMAKE_BINARY_DIR}/modules/workflow/domain")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/adapters" "${CMAKE_BINARY_DIR}/modules/workflow/adapters")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/application" "${CMAKE_BINARY_DIR}/modules/workflow/application")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/contracts" "${CMAKE_BINARY_DIR}/modules/workflow/contracts")

function(siligen_stage_runtime_dependency target_name dependency_target)
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

function(siligen_add_test_with_runtime_environment test_name test_target)
    if(NOT TARGET ${test_target})
        message(FATAL_ERROR "无法为不存在的 target 注册测试运行环境: ${test_target}")
    endif()

    get_target_property(siligen_test_output_name ${test_target} OUTPUT_NAME)
    if(NOT siligen_test_output_name OR siligen_test_output_name STREQUAL "siligen_test_output_name-NOTFOUND")
        set(siligen_test_output_name ${test_target})
    endif()

    if(WIN32 AND CMAKE_CONFIGURATION_TYPES)
        if(NOT DEFINED SILIGEN_DEFAULT_TEST_CONFIGURATION)
            set(SILIGEN_DEFAULT_TEST_CONFIGURATION "Debug")
        endif()
        set(siligen_test_bin_dir "${CMAKE_BINARY_DIR}/bin/${SILIGEN_DEFAULT_TEST_CONFIGURATION}")
        set(siligen_test_lib_dir "${CMAKE_BINARY_DIR}/lib/${SILIGEN_DEFAULT_TEST_CONFIGURATION}")
        add_test(
            NAME ${test_name}
            COMMAND "${siligen_test_bin_dir}/${siligen_test_output_name}.exe"
        )
    else()
        add_test(
            NAME ${test_name}
            COMMAND $<TARGET_FILE:${test_target}>
        )
    endif()

    if(WIN32)
        if(CMAKE_CONFIGURATION_TYPES)
            set(test_runtime_path_entries
                "PATH=path_list_prepend:${siligen_test_bin_dir}"
                "PATH=path_list_prepend:${CMAKE_BINARY_DIR}"
                "PATH=path_list_prepend:${siligen_test_lib_dir}"
            )
        else()
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
        endif()

        list(REMOVE_DUPLICATES test_runtime_path_entries)
        set_tests_properties(
            ${test_name}
            PROPERTIES ENVIRONMENT_MODIFICATION "${test_runtime_path_entries}"
        )
    endif()
endfunction()

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

if(NOT TARGET siligen_workflow_contracts_public)
    add_library(siligen_workflow_contracts_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_contracts_public INTERFACE
    siligen_workflow_contracts_headers
)
target_compile_features(siligen_workflow_contracts_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_workflow_application_public)
    add_library(siligen_workflow_application_public INTERFACE)
endif()
target_link_libraries(siligen_workflow_application_public INTERFACE
    siligen_workflow_contracts_public
    siligen_workflow_domain_public
    siligen_workflow_application_headers
)
if(TARGET siligen_control_application)
    target_link_libraries(siligen_workflow_application_public INTERFACE
        $<LINK_ONLY:siligen_control_application>
    )
endif()
target_compile_features(siligen_workflow_application_public INTERFACE cxx_std_17)

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

if(NOT TARGET siligen_module_workflow)
    add_library(siligen_module_workflow INTERFACE)
endif()
target_link_libraries(siligen_module_workflow INTERFACE
    siligen_workflow_application_public
)
target_compile_features(siligen_module_workflow INTERFACE cxx_std_17)

set_target_properties(siligen_module_workflow PROPERTIES
    SILIGEN_MODULE_ID "M0"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/workflow"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/workflow/domain;modules/workflow/services;modules/workflow/application;modules/workflow/adapters;modules/workflow/tests;modules/workflow/examples"
)
