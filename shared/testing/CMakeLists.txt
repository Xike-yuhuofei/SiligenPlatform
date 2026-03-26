cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    project(shared-testing-entry LANGUAGES CXX)
endif()

get_filename_component(SILIGEN_SHARED_TESTING_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(SILIGEN_SHARED_TESTING_WORKSPACE_ROOT_CANDIDATE "${SILIGEN_SHARED_TESTING_SOURCE_DIR}/../.." ABSOLUTE)

if(NOT DEFINED SILIGEN_WORKSPACE_ROOT OR NOT EXISTS "${SILIGEN_WORKSPACE_ROOT}/cmake/LoadWorkspaceLayout.cmake")
    set(SILIGEN_LAYOUT_FILE "${SILIGEN_SHARED_TESTING_WORKSPACE_ROOT_CANDIDATE}/cmake/workspace-layout.env")
    include("${SILIGEN_SHARED_TESTING_WORKSPACE_ROOT_CANDIDATE}/cmake/LoadWorkspaceLayout.cmake")
    siligen_load_workspace_layout(
        "${SILIGEN_SHARED_TESTING_WORKSPACE_ROOT_CANDIDATE}"
        "${SILIGEN_LAYOUT_FILE}"
    )
    set(SILIGEN_WORKSPACE_ROOT "${SILIGEN_SHARED_TESTING_WORKSPACE_ROOT_CANDIDATE}")
endif()

if(NOT "${SILIGEN_WORKSPACE_ROOT}" STREQUAL "${SILIGEN_SHARED_TESTING_WORKSPACE_ROOT_CANDIDATE}")
    message(FATAL_ERROR
        "shared/testing root mismatch: "
        "resolved='${SILIGEN_WORKSPACE_ROOT}', candidate='${SILIGEN_SHARED_TESTING_WORKSPACE_ROOT_CANDIDATE}'"
    )
endif()

set(SILIGEN_TEST_KIT_BRIDGE_DIR "")
if(DEFINED SILIGEN_TEST_KIT_DIR AND EXISTS "${SILIGEN_TEST_KIT_DIR}/pyproject.toml")
    set(SILIGEN_TEST_KIT_BRIDGE_DIR "${SILIGEN_TEST_KIT_DIR}")
elseif(EXISTS "${SILIGEN_WORKSPACE_ROOT}/shared/testing/test-kit/pyproject.toml")
    set(SILIGEN_TEST_KIT_BRIDGE_DIR "${SILIGEN_WORKSPACE_ROOT}/shared/testing/test-kit")
endif()

if(NOT SILIGEN_TEST_KIT_BRIDGE_DIR)
    message(FATAL_ERROR "未找到 shared/testing bridge source: ${SILIGEN_WORKSPACE_ROOT}/shared/testing/test-kit/pyproject.toml")
endif()

set(SILIGEN_TEST_KIT_SOURCE_DIR "${SILIGEN_TEST_KIT_BRIDGE_DIR}/src")
if(NOT EXISTS "${SILIGEN_TEST_KIT_SOURCE_DIR}/test_kit/workspace_validation.py")
    message(FATAL_ERROR
        "shared/testing bridge 缺少 workspace validation 入口: "
        "${SILIGEN_TEST_KIT_SOURCE_DIR}/test_kit/workspace_validation.py"
    )
endif()

if(NOT TARGET siligen_shared_testing)
    add_library(siligen_shared_testing INTERFACE)
    target_compile_features(siligen_shared_testing INTERFACE cxx_std_17)
endif()

set_target_properties(siligen_shared_testing PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/test-kit"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/testing"
    SILIGEN_SHARED_TESTING_BRIDGE_DIR "${SILIGEN_TEST_KIT_BRIDGE_DIR}"
    SILIGEN_SHARED_TESTING_PYTHONPATH "${SILIGEN_TEST_KIT_SOURCE_DIR}"
    SILIGEN_SHARED_TESTING_VALIDATION_ENTRYPOINT "python -m test_kit.workspace_validation"
)

if(NOT TARGET siligen_test_kit)
    add_library(siligen_test_kit ALIAS siligen_shared_testing)
endif()
