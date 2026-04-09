cmake_minimum_required(VERSION 3.20)

set(DXF_GEOMETRY_TEST_SOURCES
    contract/DxfPbPreparationServiceContractsTest.cpp
    golden/DxfPbPreparationServiceCommandGoldenTest.cpp
    integration/DxfPbPreparationServiceIntegrationTest.cpp
    unit/adapters/dxf/DXFAdapterFactoryTest.cpp
    unit/services/dxf/DxfPbPreparationServiceTest.cpp
)

add_executable(siligen_dxf_geometry_unit_tests ${DXF_GEOMETRY_TEST_SOURCES})

target_include_directories(siligen_dxf_geometry_unit_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_dxf_geometry_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_dxf_geometry_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_dxf_geometry_unit_tests PRIVATE
    siligen_dxf_geometry_application_public
    siligen_process_planning_contracts_public
    siligen_types
    siligen_utils
)
if(TARGET siligen_parsing_adapter)
    target_link_libraries(siligen_dxf_geometry_unit_tests PRIVATE
        siligen_parsing_adapter
    )
endif()
target_compile_features(siligen_dxf_geometry_unit_tests PRIVATE cxx_std_17)

if(COMMAND siligen_add_test_with_runtime_environment)
    siligen_add_test_with_runtime_environment(
        siligen_dxf_geometry_unit_tests
        siligen_dxf_geometry_unit_tests
    )
else()
    add_test(NAME siligen_dxf_geometry_unit_tests COMMAND siligen_dxf_geometry_unit_tests)
endif()

add_executable(dxf_geometry_pb_path_source_adapter_contract_test
    contract/PbPathSourceAdapterContractTest.cpp
)
target_include_directories(dxf_geometry_pb_path_source_adapter_contract_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
target_link_libraries(dxf_geometry_pb_path_source_adapter_contract_test PRIVATE
    siligen_parsing_adapter
    siligen_types
    siligen_utils
)
target_compile_features(dxf_geometry_pb_path_source_adapter_contract_test PRIVATE cxx_std_17)
add_test(
    NAME PbPathSourceAdapterContractTest
    COMMAND dxf_geometry_pb_path_source_adapter_contract_test
)
