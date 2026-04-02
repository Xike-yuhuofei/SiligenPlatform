cmake_minimum_required(VERSION 3.20)

set(SILIGEN_JOB_INGEST_CONTRACTS_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_JOB_INGEST_CONTRACTS_PUBLIC_INCLUDE_DIR}/job_ingest/contracts/dispensing/UploadContracts.h")
    message(FATAL_ERROR
        "job-ingest canonical contracts header missing: "
        "${SILIGEN_JOB_INGEST_CONTRACTS_PUBLIC_INCLUDE_DIR}/job_ingest/contracts/dispensing/UploadContracts.h"
    )
endif()
if(NOT EXISTS "${SILIGEN_JOB_INGEST_CONTRACTS_PUBLIC_INCLUDE_DIR}/job_ingest/contracts/storage/IFileStoragePort.h")
    message(FATAL_ERROR
        "job-ingest canonical storage contracts header missing: "
        "${SILIGEN_JOB_INGEST_CONTRACTS_PUBLIC_INCLUDE_DIR}/job_ingest/contracts/storage/IFileStoragePort.h"
    )
endif()

if(NOT TARGET siligen_job_ingest_contracts)
    add_library(siligen_job_ingest_contracts INTERFACE)
endif()
target_include_directories(siligen_job_ingest_contracts INTERFACE
    ${SILIGEN_JOB_INGEST_CONTRACTS_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_job_ingest_contracts INTERFACE
        siligen_application_contracts
    )
endif()
target_compile_features(siligen_job_ingest_contracts INTERFACE cxx_std_17)
