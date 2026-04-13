cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/pc_ppc"
    )
endif()

if(NOT TARGET siligen_module_topology_feature)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../modules/topology-feature"
        "${CMAKE_BINARY_DIR}/_p/pc_tf"
    )
endif()

if(NOT TARGET siligen_app_planner_cli)
    add_library(siligen_app_planner_cli INTERFACE)
endif()

target_link_libraries(siligen_app_planner_cli INTERFACE
    siligen_module_job_ingest
    siligen_dispense_packaging_application_public
    siligen_valve_core
    siligen_workflow_adapters_public
    siligen_recipe_lifecycle_application_public
    siligen_runtime_execution_application_public
    siligen_runtime_process_bootstrap_public
    siligen_process_planning_contracts_public
    siligen_module_topology_feature
    siligen_shared
)
target_compile_features(siligen_app_planner_cli INTERFACE cxx_std_17)

if(
    EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp" AND
    EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CommandLineParser.cpp" AND
    EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CommandHandlers.cpp"
)
    add_executable(siligen_planner_cli
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CommandLineParser.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CommandHandlers.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CommandHandlers.Connection.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CommandHandlers.Motion.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CommandHandlers.Dispensing.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CommandHandlers.Dxf.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/CommandHandlers.Recipe.cpp
    )
    set_target_properties(siligen_planner_cli PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        OUTPUT_NAME "siligen_planner_cli"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    target_include_directories(siligen_planner_cli PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    )

    target_link_libraries(siligen_planner_cli PRIVATE siligen_app_planner_cli)

    set(MULTICARD_DLL_PATH "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll")
    if(EXISTS "${MULTICARD_DLL_PATH}")
        add_custom_command(TARGET siligen_planner_cli POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MULTICARD_DLL_PATH}"
                "$<TARGET_FILE_DIR:siligen_planner_cli>"
            COMMENT "拷贝 MultiCard.dll 到 planner-cli 输出目录"
        )
    endif()

endif()
