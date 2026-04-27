cmake_minimum_required(VERSION 3.20)
project(shared-kernel LANGUAGES CXX)

get_filename_component(SHARED_KERNEL_WORKSPACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(SHARED_KERNEL_THIRD_PARTY_DIR "")
if(DEFINED SILIGEN_THIRD_PARTY_DIR AND EXISTS "${SILIGEN_THIRD_PARTY_DIR}")
    set(SHARED_KERNEL_THIRD_PARTY_DIR "${SILIGEN_THIRD_PARTY_DIR}")
elseif(EXISTS "${SHARED_KERNEL_WORKSPACE_ROOT}/third_party")
    set(SHARED_KERNEL_THIRD_PARTY_DIR "${SHARED_KERNEL_WORKSPACE_ROOT}/third_party")
else()
    message(FATAL_ERROR
        "shared-kernel requires SILIGEN_THIRD_PARTY_DIR or a materialized workspace third_party root. "
        "Run scripts/bootstrap/bootstrap-third-party.ps1 before configuring standalone builds."
    )
endif()

add_library(siligen_shared_kernel STATIC
    src/strings/string_manipulator.cpp
)

target_include_directories(siligen_shared_kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(siligen_shared_kernel PUBLIC cxx_std_17)

add_library(siligen_types STATIC
    src/shared/types/Types.cpp
    src/shared/types/CMPTypes.cpp
    src/shared/types/Point.cpp
    src/shared/types/TraceContext.cpp
    src/shared/types/VisualizationTypes.cpp
)

target_include_directories(siligen_types PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SHARED_KERNEL_THIRD_PARTY_DIR}
)
target_include_directories(siligen_types PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shared/types
)

target_compile_features(siligen_types PUBLIC cxx_std_17)

add_library(siligen_utils STATIC
    src/shared/conversion/TypeConverter.cpp
    src/shared/encoding/EncodingCodec.cpp
    src/shared/errors/ErrorHandler.cpp
    src/shared/hashing/HashCalculator.cpp
    src/shared/hashing/Sha256.cpp
    src/shared/paths/PathOperations.cpp
    src/shared/strings/StringFormatter.cpp
    src/shared/strings/StringManipulator.cpp
    src/shared/strings/StringToolkit.cpp
    src/shared/strings/StringValidator.cpp
)

target_include_directories(siligen_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SHARED_KERNEL_THIRD_PARTY_DIR}
    ${SHARED_KERNEL_THIRD_PARTY_DIR}/spdlog/include
)
target_include_directories(siligen_utils PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shared/conversion
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shared/encoding
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shared/errors
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shared/hashing
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shared/paths
    ${CMAKE_CURRENT_SOURCE_DIR}/include/shared/strings
)

target_link_libraries(siligen_utils PUBLIC
    siligen_types
    siligen_shared_kernel
)
if(WIN32)
    target_link_libraries(siligen_utils PUBLIC bcrypt)
endif()

target_compile_features(siligen_utils PUBLIC cxx_std_17)

add_library(siligen_geometry INTERFACE)

target_include_directories(siligen_geometry INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${SHARED_KERNEL_THIRD_PARTY_DIR}
)

target_link_libraries(siligen_geometry INTERFACE
    siligen_types
)

target_compile_features(siligen_geometry INTERFACE cxx_std_17)

set_target_properties(siligen_shared_kernel PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/shared-kernel"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/kernel"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOTS "shared/kernel;shared/ids;shared/artifacts;shared/messaging;shared/logging"
)
set_target_properties(siligen_types PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/ids"
)
set_target_properties(siligen_geometry PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "shared/artifacts"
)
set_target_properties(siligen_utils PROPERTIES
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOTS "shared/messaging;shared/logging"
)
