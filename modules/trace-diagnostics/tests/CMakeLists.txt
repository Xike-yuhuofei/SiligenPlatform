cmake_minimum_required(VERSION 3.20)

set(TRACE_DIAGNOSTICS_TEST_SOURCES
    contract/ValidationEvidenceContractsTest.cpp
    evidence/ValidationEvidenceBundleGoldenTest.cpp
    observability/LoggingServiceObservabilityTest.cpp
)

add_executable(siligen_trace_diagnostics_tests ${TRACE_DIAGNOSTICS_TEST_SOURCES})

target_include_directories(siligen_trace_diagnostics_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_trace_diagnostics_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_trace_diagnostics_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_trace_diagnostics_tests PRIVATE
    siligen_module_trace_diagnostics
    siligen_trace_diagnostics_contracts_public
)
target_compile_features(siligen_trace_diagnostics_tests PRIVATE cxx_std_17)

add_test(NAME siligen_trace_diagnostics_tests COMMAND siligen_trace_diagnostics_tests)
