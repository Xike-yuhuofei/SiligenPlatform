cmake_minimum_required(VERSION 3.20)

set(WORKFLOW_CONTRACT_TEST_SOURCES
    WorkflowContractsTest.cpp
)

add_executable(workflow_contract_tests ${WORKFLOW_CONTRACT_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(workflow_contract_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(workflow_contract_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(workflow_contract_tests PRIVATE
    siligen_workflow_contracts_public
    siligen_types
    siligen_utils
)
target_compile_features(workflow_contract_tests PRIVATE cxx_std_17)

siligen_add_test_with_runtime_environment(
    workflow_contract_tests
    workflow_contract_tests
)
