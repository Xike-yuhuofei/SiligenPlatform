# Machine 子域 CMakeLists.txt

add_library(domain_machine STATIC
    aggregates/DispenserModel.cpp
    domain-services/CalibrationProcess.cpp
)

target_include_directories(domain_machine PUBLIC
    ${SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS}
)
target_include_directories(domain_machine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SILIGEN_WORKSPACE_IMPLEMENTATION_INCLUDE_DIRS}
)

target_link_libraries(domain_machine PUBLIC
    siligen_types
    siligen_utils
)

target_compile_features(domain_machine PUBLIC cxx_std_17)

message(STATUS "[Domain::Machine] DispenserModel / CalibrationProcess (domain_machine) 已添加")
