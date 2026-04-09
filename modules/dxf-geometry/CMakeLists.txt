cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../process-planning/contracts"
        "${CMAKE_BINARY_DIR}/modules/process-planning/contracts"
    )
endif()

set(SILIGEN_DXF_GEOMETRY_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/application/include")
if(NOT EXISTS "${SILIGEN_DXF_GEOMETRY_APPLICATION_PUBLIC_INCLUDE_DIR}/dxf_geometry")
    message(FATAL_ERROR
        "dxf-geometry canonical application include root missing: "
        "${SILIGEN_DXF_GEOMETRY_APPLICATION_PUBLIC_INCLUDE_DIR}/dxf_geometry"
    )
endif()

if(NOT TARGET siligen_dxf_geometry_application_headers)
    add_library(siligen_dxf_geometry_application_headers INTERFACE)
endif()
target_include_directories(siligen_dxf_geometry_application_headers INTERFACE
    ${SILIGEN_DXF_GEOMETRY_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_dxf_geometry_application_headers INTERFACE
    siligen_process_planning_contracts_public
)
target_compile_features(siligen_dxf_geometry_application_headers INTERFACE cxx_std_17)

set(SILIGEN_DXF_GEOMETRY_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS
    ${SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS}
    ${SILIGEN_WORKSPACE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/application
)
list(REMOVE_DUPLICATES SILIGEN_DXF_GEOMETRY_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS)

add_library(siligen_dxf_geometry_application STATIC
    application/services/dxf/DxfPbPreparationService.cpp
    application/services/dxf/PbCommandResolver.cpp
    application/services/dxf/PbProcessRunner.cpp
)
target_include_directories(siligen_dxf_geometry_application PUBLIC
    ${SILIGEN_DXF_GEOMETRY_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_dxf_geometry_application PRIVATE
    ${SILIGEN_DXF_GEOMETRY_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_dxf_geometry_application PUBLIC
    siligen_process_planning_contracts_public
    siligen_types
    siligen_utils
)
target_compile_features(siligen_dxf_geometry_application PUBLIC cxx_std_17)

if(NOT TARGET siligen_dxf_geometry_application_public)
    add_library(siligen_dxf_geometry_application_public INTERFACE)
endif()
target_link_libraries(siligen_dxf_geometry_application_public INTERFACE
    siligen_dxf_geometry_application_headers
    $<LINK_ONLY:siligen_dxf_geometry_application>
)
target_compile_features(siligen_dxf_geometry_application_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_module_dxf_geometry)
    add_library(siligen_module_dxf_geometry INTERFACE)
endif()
target_link_libraries(siligen_module_dxf_geometry INTERFACE
    siligen_dxf_geometry_application_public
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adapters/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/adapters"
        "${CMAKE_BINARY_DIR}/modules/dxf-geometry/adapters"
    )
endif()

if(TARGET siligen_dxf_geometry_pb_path_source_adapter)
    target_link_libraries(siligen_module_dxf_geometry INTERFACE siligen_dxf_geometry_pb_path_source_adapter)
endif()

target_compile_features(siligen_module_dxf_geometry INTERFACE cxx_std_17)

set_target_properties(siligen_module_dxf_geometry PROPERTIES
    SILIGEN_MODULE_ID "M2"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/dxf-geometry"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/dxf-geometry/application;modules/dxf-geometry/adapters;modules/dxf-geometry/tests"
)
