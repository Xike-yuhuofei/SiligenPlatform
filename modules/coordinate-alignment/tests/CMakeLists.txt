cmake_minimum_required(VERSION 3.20)

set(COORDINATE_ALIGNMENT_TEST_SOURCES
    unit/CalibrationProcessTest.cpp
    contract/CoordinateTransformContractsTest.cpp
    golden/CoordinateTransformSetGoldenTest.cpp
)

add_executable(siligen_coordinate_alignment_tests ${COORDINATE_ALIGNMENT_TEST_SOURCES})

target_include_directories(siligen_coordinate_alignment_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_coordinate_alignment_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_coordinate_alignment_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_coordinate_alignment_tests PRIVATE
    siligen_module_coordinate_alignment
    siligen_coordinate_alignment_contracts_public
)
target_compile_features(siligen_coordinate_alignment_tests PRIVATE cxx_std_17)

add_test(NAME siligen_coordinate_alignment_tests COMMAND siligen_coordinate_alignment_tests)
