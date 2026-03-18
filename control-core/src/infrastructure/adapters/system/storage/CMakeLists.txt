# LocalFileStorageAdapter CMakeLists.txt

set(HEADER_FILES
    ${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/storage/files/LocalFileStorageAdapter.h
)

set(SOURCE_FILES
    ${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/storage/files/LocalFileStorageAdapter.cpp
)

add_library(storage_adapters ${HEADER_FILES} ${SOURCE_FILES})

target_include_directories(storage_adapters
    PUBLIC
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${SILIGEN_RUNTIME_HOST_DIR}/src
)

target_link_libraries(storage_adapters
    PUBLIC
        siligen_types
)

# 设置输出目录
set_target_properties(storage_adapters PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)
