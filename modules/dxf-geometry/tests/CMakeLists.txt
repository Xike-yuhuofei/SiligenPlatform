cmake_minimum_required(VERSION 3.20)

function(siligen_dxf_geometry_add_gtest test_target source_file test_name)
    add_executable(${test_target} ${source_file})

    target_include_directories(${test_target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    )

    if(TARGET GTest::gtest_main)
        target_link_libraries(${test_target} PRIVATE GTest::gtest_main)
    elseif(TARGET gtest_main)
        target_link_libraries(${test_target} PRIVATE gtest_main)
    else()
        message(FATAL_ERROR "GoogleTest target not found")
    endif()

    target_link_libraries(${test_target} PRIVATE
        siligen_dxf_geometry_application_public
        siligen_process_planning_contracts_public
        siligen_types
        siligen_utils
    )
    target_compile_features(${test_target} PRIVATE cxx_std_17)

    if(COMMAND siligen_add_test_with_runtime_environment)
        siligen_add_test_with_runtime_environment(${test_name} ${test_target})
    else()
        add_test(NAME ${test_name} COMMAND ${test_target})
    endif()
endfunction()

siligen_dxf_geometry_add_gtest(
    siligen_dxf_geometry_unit_tests
    unit/services/dxf/DxfPbPreparationServiceTest.cpp
    siligen_dxf_geometry_unit_tests
)

siligen_dxf_geometry_add_gtest(
    siligen_dxf_geometry_contract_tests
    contract/DxfPbPreparationServiceContractsTest.cpp
    siligen_dxf_geometry_contract_tests
)

siligen_dxf_geometry_add_gtest(
    siligen_dxf_geometry_golden_tests
    golden/DxfPbPreparationServiceCommandGoldenTest.cpp
    siligen_dxf_geometry_golden_tests
)

siligen_dxf_geometry_add_gtest(
    siligen_dxf_geometry_integration_tests
    integration/DxfPbPreparationServiceIntegrationTest.cpp
    siligen_dxf_geometry_integration_tests
)

add_executable(dxf_geometry_pb_path_source_adapter_contract_test
    contract/PbPathSourceAdapterContractTest.cpp
)
target_include_directories(dxf_geometry_pb_path_source_adapter_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(dxf_geometry_pb_path_source_adapter_contract_test PRIVATE
    siligen_dxf_geometry_pb_path_source_adapter
    siligen_types
    siligen_utils
)
target_compile_features(dxf_geometry_pb_path_source_adapter_contract_test PRIVATE cxx_std_17)
add_test(
    NAME PbPathSourceAdapterContractTest
    COMMAND dxf_geometry_pb_path_source_adapter_contract_test
)
