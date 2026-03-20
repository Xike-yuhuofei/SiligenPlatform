cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_shared_kernel)
    message(FATAL_ERROR "shared-kernel tests 需要先加载 canonical siligen_shared_kernel target")
endif()

set(SHARED_KERNEL_TEST_SOURCES
    unit/result_test.cpp
    unit/string_manipulator_test.cpp
)

add_executable(siligen_shared_kernel_tests ${SHARED_KERNEL_TEST_SOURCES})

if(TARGET GTest::gtest_main)
    target_link_libraries(siligen_shared_kernel_tests PRIVATE GTest::gtest_main)
elseif(TARGET gtest_main)
    target_link_libraries(siligen_shared_kernel_tests PRIVATE gtest_main)
else()
    message(FATAL_ERROR "GoogleTest target not found")
endif()

target_link_libraries(siligen_shared_kernel_tests PRIVATE
    siligen_shared_kernel
)

target_compile_features(siligen_shared_kernel_tests PRIVATE cxx_std_17)

add_test(NAME siligen_shared_kernel_tests COMMAND siligen_shared_kernel_tests)
