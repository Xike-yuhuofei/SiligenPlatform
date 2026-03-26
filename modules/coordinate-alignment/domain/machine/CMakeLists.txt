# Machine 子域 CMakeLists.txt

add_library(siligen_coordinate_alignment_domain_machine STATIC
    aggregates/DispenserModel.cpp
    domain-services/CalibrationProcess.cpp
)

target_include_directories(siligen_coordinate_alignment_domain_machine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

target_link_libraries(siligen_coordinate_alignment_domain_machine PUBLIC
    siligen_types
    siligen_utils
)

target_compile_features(siligen_coordinate_alignment_domain_machine PUBLIC cxx_std_17)

message(STATUS "[CoordinateAlignment::Domain::Machine] DispenserModel / CalibrationProcess 已添加")
