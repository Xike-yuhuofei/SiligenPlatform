cmake_minimum_required(VERSION 3.20)

set(DISPENSE_PACKAGING_TEST_SOURCES
    unit/domain/dispensing/DispensePackagingBoundaryTest.cpp
    unit/domain/dispensing/TriggerPlannerTest.cpp
    unit/domain/dispensing/DispensingControllerTest.cpp
    unit/domain/dispensing/PathArcLengthLocatorTest.cpp
    unit/domain/dispensing/UnifiedTrajectoryPlannerServiceTest.cpp
    unit/domain/dispensing/TrajectoryTriggerUtilsTest.cpp
    unit/application/services/dispensing/DispensePlanningFacadeTest.cpp
    unit/application/services/dispensing/PreviewSnapshotServiceTest.cpp
)

set(DISPENSE_PACKAGING_OPTIONAL_TEST_SOURCES
    unit/domain/dispensing/AuthorityTriggerLayoutPlannerTest.cpp
)

foreach(test_source IN LISTS DISPENSE_PACKAGING_OPTIONAL_TEST_SOURCES)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${test_source}")
        list(APPEND DISPENSE_PACKAGING_TEST_SOURCES "${test_source}")
    endif()
endforeach()

add_executable(siligen_dispense_packaging_unit_tests ${DISPENSE_PACKAGING_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_dispense_packaging_unit_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_dispense_packaging_unit_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_dispense_packaging_unit_tests PRIVATE
    siligen_dispense_packaging_application_public
    siligen_types
    siligen_utils
)
target_compile_features(siligen_dispense_packaging_unit_tests PRIVATE cxx_std_17)

if(COMMAND process_runtime_core_add_test_with_runtime_environment)
    process_runtime_core_add_test_with_runtime_environment(
        siligen_dispense_packaging_unit_tests
        siligen_dispense_packaging_unit_tests
    )
else()
    add_test(NAME siligen_dispense_packaging_unit_tests COMMAND siligen_dispense_packaging_unit_tests)
endif()
