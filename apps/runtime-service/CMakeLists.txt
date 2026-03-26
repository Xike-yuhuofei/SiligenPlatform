cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_app_runtime_service)
    add_library(siligen_app_runtime_service INTERFACE)
endif()

target_link_libraries(siligen_app_runtime_service INTERFACE
    siligen_module_runtime_execution
)
target_compile_features(siligen_app_runtime_service INTERFACE cxx_std_17)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(siligen_runtime_service
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    )
    set_target_properties(siligen_runtime_service PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        OUTPUT_NAME "siligen_runtime_service"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_link_libraries(siligen_runtime_service PRIVATE siligen_app_runtime_service)
endif()
