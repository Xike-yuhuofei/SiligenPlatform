cmake_minimum_required(VERSION 3.20)

get_filename_component(SILIGEN_TESTS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(SILIGEN_WORKSPACE_ROOT_CANDIDATE "${SILIGEN_TESTS_SOURCE_DIR}/.." ABSOLUTE)
set(SILIGEN_LAYOUT_FILE "${SILIGEN_WORKSPACE_ROOT_CANDIDATE}/cmake/workspace-layout.env")
include("${SILIGEN_WORKSPACE_ROOT_CANDIDATE}/cmake/LoadWorkspaceLayout.cmake")
siligen_load_workspace_layout(
    "${SILIGEN_WORKSPACE_ROOT_CANDIDATE}"
    "${SILIGEN_LAYOUT_FILE}"
)
set(SILIGEN_WORKSPACE_ROOT "${SILIGEN_WORKSPACE_ROOT_CANDIDATE}")
if(NOT "${SILIGEN_WORKSPACE_ROOT}" STREQUAL "${SILIGEN_WORKSPACE_ROOT_CANDIDATE}")
    message(FATAL_ERROR
        "tests workspace root mismatch: "
        "resolved='${SILIGEN_WORKSPACE_ROOT}', candidate='${SILIGEN_WORKSPACE_ROOT_CANDIDATE}'"
    )
endif()
if(NOT "${SILIGEN_WORKSPACE_ROOT}" STREQUAL "${CMAKE_SOURCE_DIR}")
    message(FATAL_ERROR
        "tests root must resolve to canonical superbuild source root: CMAKE_SOURCE_DIR='${CMAKE_SOURCE_DIR}', "
        "resolved='${SILIGEN_WORKSPACE_ROOT}'"
    )
endif()

set(SILIGEN_REPO_VALIDATION_SURFACE_READMES
    "${SILIGEN_TESTS_SOURCE_DIR}/integration/README.md"
    "${SILIGEN_TESTS_SOURCE_DIR}/e2e/README.md"
    "${SILIGEN_TESTS_SOURCE_DIR}/performance/README.md"
)
foreach(SILIGEN_VALIDATION_SURFACE_README IN LISTS SILIGEN_REPO_VALIDATION_SURFACE_READMES)
    if(NOT EXISTS "${SILIGEN_VALIDATION_SURFACE_README}")
        message(FATAL_ERROR "未找到仓库级验证承载面说明: ${SILIGEN_VALIDATION_SURFACE_README}")
    endif()
endforeach()

add_custom_target(
    siligen_repo_validation_surface_docs
    SOURCES ${SILIGEN_REPO_VALIDATION_SURFACE_READMES}
)

set(SILIGEN_CANONICAL_TEST_ROOTS
    "${SILIGEN_SHARED_ROOT}/kernel/tests"
    "${SILIGEN_MODULES_ROOT}/workflow/tests"
    "${SILIGEN_MODULES_ROOT}/recipe-lifecycle/tests"
    "${SILIGEN_MODULES_ROOT}/dxf-geometry/tests"
    "${SILIGEN_MODULES_ROOT}/job-ingest/tests"
    "${SILIGEN_MODULES_ROOT}/topology-feature/tests"
    "${SILIGEN_MODULES_ROOT}/process-planning/tests"
    "${SILIGEN_MODULES_ROOT}/coordinate-alignment/tests"
    "${SILIGEN_MODULES_ROOT}/trace-diagnostics/tests"
    "${SILIGEN_MODULES_ROOT}/hmi-application/tests"
    "${SILIGEN_MODULES_ROOT}/process-path/tests"
    "${SILIGEN_MODULES_ROOT}/motion-planning/tests"
    "${SILIGEN_MODULES_ROOT}/dispense-packaging/tests"
    "${SILIGEN_APPS_ROOT}/runtime-service/tests"
    "${SILIGEN_APPS_ROOT}/planner-cli/tests"
    "${SILIGEN_MODULES_ROOT}/runtime-execution/tests"
)

foreach(SILIGEN_CANONICAL_TEST_ROOT IN LISTS SILIGEN_CANONICAL_TEST_ROOTS)
    if(NOT EXISTS "${SILIGEN_CANONICAL_TEST_ROOT}/CMakeLists.txt")
        message(FATAL_ERROR "missing canonical test root: ${SILIGEN_CANONICAL_TEST_ROOT}/CMakeLists.txt")
    endif()
endforeach()

foreach(SILIGEN_CANONICAL_TEST_ROOT IN LISTS SILIGEN_CANONICAL_TEST_ROOTS)
    file(RELATIVE_PATH SILIGEN_REL_TEST_ROOT "${SILIGEN_WORKSPACE_ROOT}" "${SILIGEN_CANONICAL_TEST_ROOT}")
    string(REPLACE "/" "-" SILIGEN_TEST_BIN_SUFFIX "${SILIGEN_REL_TEST_ROOT}")
    add_subdirectory(
        "${SILIGEN_CANONICAL_TEST_ROOT}"
        "${CMAKE_BINARY_DIR}/tests/${SILIGEN_TEST_BIN_SUFFIX}"
    )
endforeach()
