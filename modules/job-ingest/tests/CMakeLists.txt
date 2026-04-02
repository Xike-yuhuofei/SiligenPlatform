cmake_minimum_required(VERSION 3.20)

set(JOB_INGEST_TEST_SOURCES
    contract/UploadContractsTest.cpp
    golden/UploadResponseGoldenTest.cpp
    integration/UploadFileUseCaseIntegrationTest.cpp
    unit/dispensing/UploadFileUseCaseTest.cpp
)

add_executable(siligen_job_ingest_unit_tests ${JOB_INGEST_TEST_SOURCES})

target_include_directories(siligen_job_ingest_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_job_ingest_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_job_ingest_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_job_ingest_unit_tests PRIVATE
    siligen_job_ingest_application_public
    siligen_job_ingest_contracts
    siligen_dxf_geometry_application_public
    siligen_types
    siligen_utils
)
target_compile_features(siligen_job_ingest_unit_tests PRIVATE cxx_std_17)

if(COMMAND siligen_add_test_with_runtime_environment)
    siligen_add_test_with_runtime_environment(
        siligen_job_ingest_unit_tests
        siligen_job_ingest_unit_tests
    )
else()
    add_test(NAME siligen_job_ingest_unit_tests COMMAND siligen_job_ingest_unit_tests)
endif()
