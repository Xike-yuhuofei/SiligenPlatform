cmake_minimum_required(VERSION 3.20)

set(SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application")
    message(FATAL_ERROR
        "workflow internal application skeleton include root missing: "
        "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application"
    )
endif()

foreach(required_dir
    IN ITEMS
        "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application/commands"
        "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application/queries"
        "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application/facade"
)
    if(NOT EXISTS "${required_dir}")
        message(FATAL_ERROR "workflow canonical application slice missing: ${required_dir}")
    endif()
endforeach()
set(WORKFLOW_APPLICATION_FORBIDDEN_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/planning-trigger"
    "${CMAKE_CURRENT_SOURCE_DIR}/phase-control"
    "${CMAKE_CURRENT_SOURCE_DIR}/ports"
    "${CMAKE_CURRENT_SOURCE_DIR}/services"
    "${CMAKE_CURRENT_SOURCE_DIR}/recovery-control"
    "${CMAKE_CURRENT_SOURCE_DIR}/usecases"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/application"
    "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application/planning-trigger"
    "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application/phase-control"
    "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application/ports"
    "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application/services"
    "${SILIGEN_WORKFLOW_APPLICATION_SKELETON_INCLUDE_DIR}/workflow/application/recovery-control"
)
foreach(forbidden_dir IN LISTS WORKFLOW_APPLICATION_FORBIDDEN_DIRS)
    if(EXISTS "${forbidden_dir}")
        message(FATAL_ERROR "workflow application legacy foreign surface must stay deleted: ${forbidden_dir}")
    endif()
endforeach()
message(STATUS "[Workflow::Application] canonical M0 skeleton is internal-only")
