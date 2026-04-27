cmake_minimum_required(VERSION 3.20)

function(siligen_job_ingest_link_gtest target_name)
    if(TARGET GTest::gtest_main)
        target_link_libraries(${target_name} PRIVATE GTest::gtest_main)
    elseif(TARGET gtest_main)
        target_link_libraries(${target_name} PRIVATE gtest_main)
    else()
        message(FATAL_ERROR "GoogleTest target not found")
    endif()
endfunction()

function(siligen_job_ingest_add_test target_name)
    add_executable(${target_name} ${ARGN})
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )
    siligen_job_ingest_link_gtest(${target_name})
    target_compile_features(${target_name} PRIVATE cxx_std_17)

    if(COMMAND siligen_add_test_with_runtime_environment)
        siligen_add_test_with_runtime_environment(
            ${target_name}
            ${target_name}
        )
    else()
        add_test(NAME ${target_name} COMMAND ${target_name})
    endif()
endfunction()

siligen_job_ingest_add_test(
    siligen_job_ingest_contract_tests
    contract/UploadContractsTest.cpp
)
target_link_libraries(siligen_job_ingest_contract_tests PRIVATE
    siligen_job_ingest_contracts
)

siligen_job_ingest_add_test(
    siligen_job_ingest_unit_tests
    unit/dispensing/UploadFileUseCaseTest.cpp
)
target_link_libraries(siligen_job_ingest_unit_tests PRIVATE
    siligen_job_ingest_application_public
)

siligen_job_ingest_add_test(
    siligen_job_ingest_golden_tests
    golden/SourceDrawingGoldenTest.cpp
)
target_link_libraries(siligen_job_ingest_golden_tests PRIVATE
    siligen_job_ingest_application_public
)

siligen_job_ingest_add_test(
    siligen_job_ingest_integration_tests
    integration/UploadFileUseCaseIntegrationTest.cpp
)
target_link_libraries(siligen_job_ingest_integration_tests PRIVATE
    siligen_job_ingest_application_public
    siligen_dxf_geometry_application_public
)
