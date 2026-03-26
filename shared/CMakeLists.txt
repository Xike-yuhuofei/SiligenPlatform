cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(shared LANGUAGES CXX)
endif()

get_filename_component(SILIGEN_SHARED_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(SILIGEN_SHARED_WORKSPACE_ROOT_CANDIDATE "${SILIGEN_SHARED_SOURCE_DIR}/.." ABSOLUTE)

if(NOT DEFINED SILIGEN_WORKSPACE_ROOT OR NOT EXISTS "${SILIGEN_WORKSPACE_ROOT}/cmake/LoadWorkspaceLayout.cmake")
    set(SILIGEN_LAYOUT_FILE "${SILIGEN_SHARED_WORKSPACE_ROOT_CANDIDATE}/cmake/workspace-layout.env")
    include("${SILIGEN_SHARED_WORKSPACE_ROOT_CANDIDATE}/cmake/LoadWorkspaceLayout.cmake")
    siligen_load_workspace_layout(
        "${SILIGEN_SHARED_WORKSPACE_ROOT_CANDIDATE}"
        "${SILIGEN_LAYOUT_FILE}"
    )
    set(SILIGEN_WORKSPACE_ROOT "${SILIGEN_SHARED_WORKSPACE_ROOT_CANDIDATE}")
endif()

if(NOT "${SILIGEN_WORKSPACE_ROOT}" STREQUAL "${SILIGEN_SHARED_WORKSPACE_ROOT_CANDIDATE}")
    message(FATAL_ERROR
        "shared root must resolve to workspace root: "
        "resolved='${SILIGEN_WORKSPACE_ROOT}', candidate='${SILIGEN_SHARED_WORKSPACE_ROOT_CANDIDATE}'"
    )
endif()

foreach(shared_subdir IN ITEMS kernel contracts testing)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${shared_subdir}/CMakeLists.txt")
        message(FATAL_ERROR "未找到 shared/${shared_subdir} 构建入口: ${CMAKE_CURRENT_SOURCE_DIR}/${shared_subdir}/CMakeLists.txt")
    endif()

    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/${shared_subdir}"
        "${CMAKE_BINARY_DIR}/shared/${shared_subdir}"
    )
endforeach()

set(SILIGEN_SHARED_PUBLIC_TARGETS "")
foreach(shared_target IN ITEMS
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_shared_kernel
)
    if(NOT TARGET ${shared_target})
        message(FATAL_ERROR "shared root 需要先解析 target: ${shared_target}")
    endif()
    list(APPEND SILIGEN_SHARED_PUBLIC_TARGETS "${shared_target}")
endforeach()

if(NOT TARGET siligen_shared)
    add_library(siligen_shared INTERFACE)
    target_link_libraries(siligen_shared INTERFACE
        ${SILIGEN_SHARED_PUBLIC_TARGETS}
    )
    target_compile_features(siligen_shared INTERFACE cxx_std_17)
endif()
