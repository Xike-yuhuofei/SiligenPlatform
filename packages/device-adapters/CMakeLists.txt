cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_shared_kernel AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../shared-kernel/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../shared-kernel"
        "${CMAKE_BINARY_DIR}/packages/shared-kernel"
    )
endif()

if(NOT TARGET siligen_device_contracts AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/../device-contracts/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_LIST_DIR}/../device-contracts"
        "${CMAKE_BINARY_DIR}/packages/device-contracts"
    )
endif()

set(SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/multicard")
set(SILIGEN_DEVICE_ADAPTERS_THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
if(NOT EXISTS "${SILIGEN_DEVICE_ADAPTERS_THIRD_PARTY_DIR}")
    set(SILIGEN_DEVICE_ADAPTERS_THIRD_PARTY_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../../control-core/third_party"
    )
endif()

set(SILIGEN_HAS_REAL_MULTICARD 0)
if(EXISTS "${SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR}/MultiCard.lib")
    set(SILIGEN_HAS_REAL_MULTICARD 1)
endif()

add_library(siligen_device_adapters STATIC
    src/drivers/multicard/error_mapper.cpp
    src/drivers/multicard/MockMultiCard.cpp
    src/drivers/multicard/MockMultiCardWrapper.cpp
    src/motion/multicard_motion_device.cpp
    src/io/multicard_digital_io_adapter.cpp
    src/fake/fake_motion_device.cpp
    src/fake/fake_dispenser_device.cpp
)

if(SILIGEN_HAS_REAL_MULTICARD)
    target_sources(siligen_device_adapters PRIVATE
        src/drivers/multicard/RealMultiCardWrapper.cpp
    )
endif()

target_include_directories(siligen_device_adapters PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(EXISTS "${SILIGEN_DEVICE_ADAPTERS_THIRD_PARTY_DIR}")
    target_include_directories(siligen_device_adapters PRIVATE
        ${SILIGEN_DEVICE_ADAPTERS_THIRD_PARTY_DIR}
    )
endif()

target_link_libraries(siligen_device_adapters PUBLIC
    siligen_device_contracts
    siligen_shared_kernel
)

if(SILIGEN_HAS_REAL_MULTICARD)
    target_link_libraries(siligen_device_adapters PUBLIC
        "${SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR}/MultiCard.lib"
    )
endif()

target_compile_definitions(siligen_device_adapters PUBLIC
    HAS_REAL_MULTICARD=${SILIGEN_HAS_REAL_MULTICARD}
)

target_compile_features(siligen_device_adapters PUBLIC cxx_std_17)
