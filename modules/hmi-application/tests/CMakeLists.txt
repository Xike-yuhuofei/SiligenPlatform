cmake_minimum_required(VERSION 3.20)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(HMI_APPLICATION_TEST_FILES
    unit/test_launch_state.py
    unit/test_preview_session.py
    contract/test_launch_supervision_contract.py
    run_hmi_application_tests.py
)

add_custom_target(siligen_hmi_application_tests
    SOURCES ${HMI_APPLICATION_TEST_FILES}
)

add_test(
    NAME siligen_hmi_application_tests
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/run_hmi_application_tests.py"
)
set_tests_properties(siligen_hmi_application_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
