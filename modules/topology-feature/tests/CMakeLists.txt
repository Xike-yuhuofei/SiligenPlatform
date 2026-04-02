cmake_minimum_required(VERSION 3.20)

set(TOPOLOGY_FEATURE_TEST_SOURCES
    unit/ContourAugmentConfigDefaultsTest.cpp
    contract/ContourAugmentContractsTest.cpp
    golden/ContourAugmentConfigGoldenTest.cpp
    integration/ContourAugmenterAdapterIntegrationTest.cpp
)

add_executable(siligen_topology_feature_tests ${TOPOLOGY_FEATURE_TEST_SOURCES})

target_include_directories(siligen_topology_feature_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_topology_feature_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_topology_feature_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_topology_feature_tests PRIVATE
    siligen_module_topology_feature
    siligen_topology_feature_contracts_public
    siligen_topology_feature_contour_augment_adapter
    siligen_types
    siligen_utils
)
target_compile_features(siligen_topology_feature_tests PRIVATE cxx_std_17)

add_test(NAME siligen_topology_feature_tests COMMAND siligen_topology_feature_tests)
