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

set(SILIGEN_DEVICE_ADAPTERS_PROCESS_RUNTIME_CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../process-runtime-core")
if(DEFINED SILIGEN_PROCESS_RUNTIME_CORE_DIR AND EXISTS "${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src")
    set(SILIGEN_DEVICE_ADAPTERS_PROCESS_RUNTIME_CORE_DIR "${SILIGEN_PROCESS_RUNTIME_CORE_DIR}")
endif()

set(SILIGEN_DEVICE_ADAPTERS_SHARED_COMPAT_DIR "")
if(DEFINED SILIGEN_SHARED_COMPAT_INCLUDE_ROOT AND EXISTS "${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}/shared")
    set(SILIGEN_DEVICE_ADAPTERS_SHARED_COMPAT_DIR "${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../shared-kernel/include/shared")
    set(SILIGEN_DEVICE_ADAPTERS_SHARED_COMPAT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../shared-kernel/include")
endif()

set(SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/multicard")

option(
    SILIGEN_ENABLE_DEVICE_ADAPTERS_LEGACY_BRIDGES
    "Build package-owned legacy bridges for runtime-host/control-runtime integration."
    ON
)

set(SILIGEN_HAS_REAL_MULTICARD 0)
if(EXISTS "${SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR}/MultiCard.lib")
    set(SILIGEN_HAS_REAL_MULTICARD 1)
endif()

set(SILIGEN_DEVICE_ADAPTERS_SOURCES
    src/drivers/multicard/error_mapper.cpp
    src/drivers/multicard/MockMultiCard.cpp
    src/drivers/multicard/MockMultiCardWrapper.cpp
    src/motion/multicard_motion_device.cpp
    src/io/multicard_digital_io_adapter.cpp
    src/fake/fake_motion_device.cpp
    src/fake/fake_dispenser_device.cpp
)

set(SILIGEN_DEVICE_ADAPTERS_LEGACY_BRIDGE_SOURCES
    src/legacy/drivers/multicard/MultiCardAdapter.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.Connection.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.Jog.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.Homing.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.IO.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.Trigger.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.Cmp.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.Interpolation.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.Diagnostics.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.Safety.cpp
    src/legacy/adapters/diagnostics/health/testing/HardwareTestAdapter.Helpers.cpp
    src/legacy/adapters/motion/controller/connection/HardwareConnectionAdapter.cpp
    src/legacy/adapters/motion/controller/control/MultiCardMotionAdapter.cpp
    src/legacy/adapters/motion/controller/control/MultiCardMotionAdapter.Status.cpp
    src/legacy/adapters/motion/controller/control/MultiCardMotionAdapter.Motion.cpp
    src/legacy/adapters/motion/controller/control/MultiCardMotionAdapter.Settings.cpp
    src/legacy/adapters/motion/controller/control/MultiCardMotionAdapter.Helpers.cpp
    src/legacy/adapters/motion/controller/homing/HomingPortAdapter.cpp
    src/legacy/adapters/motion/controller/interpolation/InterpolationAdapter.cpp
    src/legacy/adapters/motion/controller/internal/UnitConverter.cpp
    src/legacy/adapters/motion/controller/runtime/MotionRuntimeConnectionAdapter.cpp
    src/legacy/adapters/motion/controller/runtime/MotionRuntimeFacade.cpp
    src/legacy/adapters/dispensing/dispenser/ValveAdapter.cpp
    src/legacy/adapters/dispensing/dispenser/ValveAdapter.Dispenser.cpp
    src/legacy/adapters/dispensing/dispenser/ValveAdapter.Supply.cpp
    src/legacy/adapters/dispensing/dispenser/ValveAdapter.Hardware.cpp
    src/legacy/adapters/dispensing/dispenser/triggering/TriggerControllerAdapter.cpp
)

add_library(siligen_device_adapters STATIC
    ${SILIGEN_DEVICE_ADAPTERS_SOURCES}
)

if(SILIGEN_HAS_REAL_MULTICARD)
    target_sources(siligen_device_adapters PRIVATE
        src/drivers/multicard/RealMultiCardWrapper.cpp
    )
else()
    target_sources(siligen_device_adapters PRIVATE
        src/legacy/drivers/multicard/MultiCardStub_Class.cpp
    )
endif()

target_include_directories(siligen_device_adapters PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(siligen_device_adapters PUBLIC
    siligen_device_contracts
    siligen_shared_kernel
)

if(SILIGEN_HAS_REAL_MULTICARD)
    target_link_libraries(siligen_device_adapters PUBLIC
        "${SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR}/MultiCard.lib"
    )

    if(EXISTS "${SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR}/MultiCard.dll")
        add_custom_command(TARGET siligen_device_adapters POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR}/MultiCard.dll"
                "$<TARGET_FILE_DIR:siligen_device_adapters>"
            COMMENT "拷贝 MultiCard.dll 到输出目录"
        )
    endif()
endif()

target_compile_definitions(siligen_device_adapters PUBLIC
    HAS_REAL_MULTICARD=${SILIGEN_HAS_REAL_MULTICARD}
    SILIGEN_ENABLE_MOCK_MULTICARD=$<BOOL:${SILIGEN_ENABLE_MOCK_MULTICARD}>
)

target_compile_features(siligen_device_adapters PUBLIC cxx_std_17)

if(NOT TARGET siligen_device_hal)
    add_library(siligen_device_hal INTERFACE)
    target_include_directories(siligen_device_hal INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    target_link_libraries(siligen_device_hal INTERFACE
        siligen_device_adapters
        siligen_device_contracts
    )
    target_compile_features(siligen_device_hal INTERFACE cxx_std_17)
endif()

if(SILIGEN_ENABLE_DEVICE_ADAPTERS_LEGACY_BRIDGES)
    if(NOT EXISTS "${SILIGEN_DEVICE_ADAPTERS_PROCESS_RUNTIME_CORE_DIR}/src")
        message(FATAL_ERROR
            "device-adapters legacy bridges require a reachable process-runtime-core/src include root."
        )
    endif()

    if(NOT EXISTS "${SILIGEN_DEVICE_ADAPTERS_SHARED_COMPAT_DIR}/shared")
        message(FATAL_ERROR
            "device-adapters legacy bridges require SILIGEN_SHARED_COMPAT_INCLUDE_ROOT to expose shared/* compatibility headers."
        )
    endif()

    add_library(siligen_device_adapters_legacy_bridges STATIC
        ${SILIGEN_DEVICE_ADAPTERS_LEGACY_BRIDGE_SOURCES}
    )

    target_include_directories(siligen_device_adapters_legacy_bridges PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${SILIGEN_DEVICE_ADAPTERS_PROCESS_RUNTIME_CORE_DIR}/src
        ${SILIGEN_DEVICE_ADAPTERS_SHARED_COMPAT_DIR}
    )

    target_link_libraries(siligen_device_adapters_legacy_bridges PUBLIC
        siligen_device_adapters
    )

    if(TARGET siligen_types)
        target_link_libraries(siligen_device_adapters_legacy_bridges PRIVATE siligen_types)
    endif()

    if(TARGET siligen_utils)
        target_link_libraries(siligen_device_adapters_legacy_bridges PRIVATE siligen_utils)
    endif()

    if(TARGET domain_dispensing)
        target_link_libraries(siligen_device_adapters_legacy_bridges PRIVATE domain_dispensing)
    endif()

    target_compile_definitions(siligen_device_adapters_legacy_bridges PUBLIC
        HAS_REAL_MULTICARD=${SILIGEN_HAS_REAL_MULTICARD}
        SILIGEN_ENABLE_MOCK_MULTICARD=$<BOOL:${SILIGEN_ENABLE_MOCK_MULTICARD}>
    )

    target_compile_features(siligen_device_adapters_legacy_bridges PUBLIC cxx_std_17)
endif()
