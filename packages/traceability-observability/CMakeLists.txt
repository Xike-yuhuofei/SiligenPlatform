cmake_minimum_required(VERSION 3.20)

add_library(siligen_spdlog_adapter STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/logging/spdlog/SpdlogLoggingAdapter.cpp
)

target_include_directories(siligen_spdlog_adapter PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    ${SILIGEN_THIRD_PARTY_DIR}/spdlog/include
)

target_link_libraries(siligen_spdlog_adapter PRIVATE
    siligen_types
)

target_compile_features(siligen_spdlog_adapter PUBLIC cxx_std_17)
