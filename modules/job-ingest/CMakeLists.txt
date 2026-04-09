cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/ji_ppc"
    )
endif()

if(NOT TARGET siligen_process_planning_domain_configuration)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../process-planning/domain/configuration"
        "${CMAKE_BINARY_DIR}/_p/ji_pdc"
    )
endif()

if(NOT TARGET siligen_job_ingest_contracts AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/contracts/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts"
        "${CMAKE_BINARY_DIR}/modules/job-ingest/contracts"
    )
endif()

set(SILIGEN_JOB_INGEST_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/application/include")
if(NOT EXISTS "${SILIGEN_JOB_INGEST_APPLICATION_PUBLIC_INCLUDE_DIR}/application")
    message(FATAL_ERROR
        "job-ingest canonical application include root missing: "
        "${SILIGEN_JOB_INGEST_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
    )
endif()

if(NOT TARGET siligen_job_ingest_application_headers)
    add_library(siligen_job_ingest_application_headers INTERFACE)
endif()
target_include_directories(siligen_job_ingest_application_headers INTERFACE
    ${SILIGEN_JOB_INGEST_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_job_ingest_application_headers INTERFACE
    siligen_process_planning_domain_configuration
    siligen_process_planning_contracts_public
    siligen_job_ingest_contracts
)
target_compile_features(siligen_job_ingest_application_headers INTERFACE cxx_std_17)

set(SILIGEN_JOB_INGEST_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS
    ${SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS}
    ${SILIGEN_WORKSPACE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/application
)
list(REMOVE_DUPLICATES SILIGEN_JOB_INGEST_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS)

add_library(siligen_job_ingest_application STATIC
    application/usecases/dispensing/UploadFileUseCase.cpp
)
target_include_directories(siligen_job_ingest_application PUBLIC
    ${SILIGEN_JOB_INGEST_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_job_ingest_application PRIVATE
    ${SILIGEN_JOB_INGEST_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_job_ingest_application PUBLIC
    siligen_process_planning_domain_configuration
    siligen_process_planning_contracts_public
    siligen_job_ingest_contracts
    siligen_dxf_geometry_application_public
    siligen_types
    siligen_utils
)
target_compile_features(siligen_job_ingest_application PUBLIC cxx_std_17)

if(NOT TARGET siligen_job_ingest_application_public)
    add_library(siligen_job_ingest_application_public INTERFACE)
endif()
target_link_libraries(siligen_job_ingest_application_public INTERFACE
    siligen_job_ingest_application_headers
    siligen_job_ingest_contracts
    $<LINK_ONLY:siligen_job_ingest_application>
)
target_compile_features(siligen_job_ingest_application_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_module_job_ingest)
    add_library(siligen_module_job_ingest INTERFACE)
endif()
target_link_libraries(siligen_module_job_ingest INTERFACE
    siligen_job_ingest_application_public
)
if(TARGET siligen_job_ingest_contracts)
    target_link_libraries(siligen_module_job_ingest INTERFACE siligen_job_ingest_contracts)
endif()

if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_module_job_ingest INTERFACE siligen_application_contracts)
endif()

target_compile_features(siligen_module_job_ingest INTERFACE cxx_std_17)

set_target_properties(siligen_module_job_ingest PROPERTIES
    SILIGEN_MODULE_ID "M1"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/job-ingest"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/job-ingest/domain;modules/job-ingest/services;modules/job-ingest/application;modules/job-ingest/adapters;modules/job-ingest/tests;modules/job-ingest/examples"
)
