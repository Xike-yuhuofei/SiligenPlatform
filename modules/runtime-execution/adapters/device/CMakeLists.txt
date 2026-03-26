cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_shared_kernel AND EXISTS "${SILIGEN_WORKSPACE_ROOT}/shared/kernel/CMakeLists.txt")
    add_subdirectory(
        "${SILIGEN_WORKSPACE_ROOT}/shared/kernel"
        "${CMAKE_BINARY_DIR}/shared/kernel"
    )
endif()

set(SILIGEN_DEVICE_CONTRACTS_CANONICAL_DIR "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/contracts/device")
if(NOT TARGET siligen_device_contracts AND EXISTS "${SILIGEN_DEVICE_CONTRACTS_CANONICAL_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${SILIGEN_DEVICE_CONTRACTS_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/contracts/device"
    )
endif()

set(SILIGEN_DEVICE_ADAPTERS_INCLUDE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(SILIGEN_DEVICE_ADAPTERS_SHARED_COMPAT_DIR "${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}")
set(SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/adapters/device/vendor/multicard")
if(DEFINED SILIGEN_MULTICARD_VENDOR_DIR AND EXISTS "${SILIGEN_MULTICARD_VENDOR_DIR}")
    set(SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR "${SILIGEN_MULTICARD_VENDOR_DIR}")
endif()

if(NOT EXISTS "${SILIGEN_DEVICE_ADAPTERS_INCLUDE_ROOT}/siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h")
    message(FATAL_ERROR
        "runtime-execution canonical device adapters require include/siligen/device/adapters/* public headers."
    )
endif()

if(NOT EXISTS "${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/drivers/multicard/error_mapper.cpp")
    message(FATAL_ERROR
        "runtime-execution canonical device adapters require src/drivers/multicard/error_mapper.cpp"
    )
endif()

if(NOT EXISTS "${SILIGEN_DEVICE_ADAPTERS_SHARED_COMPAT_DIR}/shared")
    message(FATAL_ERROR
        "device-adapters public adapters require SILIGEN_SHARED_COMPAT_INCLUDE_ROOT to expose shared/* compatibility headers."
    )
endif()

set(SILIGEN_DEVICE_ADAPTERS_SOURCES
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/drivers/multicard/error_mapper.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/drivers/multicard/MockMultiCard.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/drivers/multicard/MockMultiCardWrapper.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/motion/multicard_motion_device.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/io/multicard_digital_io_adapter.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/fake/fake_motion_device.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/fake/fake_dispenser_device.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.Connection.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.Jog.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.Homing.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.IO.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.Trigger.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.Cmp.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.Interpolation.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.Diagnostics.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.Safety.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/diagnostics/health/testing/HardwareTestAdapter.Helpers.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/connection/HardwareConnectionAdapter.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/control/MultiCardMotionAdapter.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/control/MultiCardMotionAdapter.Status.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/control/MultiCardMotionAdapter.Motion.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/control/MultiCardMotionAdapter.Settings.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/control/MultiCardMotionAdapter.Helpers.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/homing/HomingPortAdapter.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/interpolation/InterpolationAdapter.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/internal/UnitConverter.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/runtime/MotionRuntimeConnectionAdapter.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/motion/controller/runtime/MotionRuntimeFacade.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/dispensing/dispenser/ValveAdapter.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/dispensing/dispenser/ValveAdapter.Dispenser.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/dispensing/dispenser/ValveAdapter.Supply.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/dispensing/dispenser/ValveAdapter.Hardware.cpp
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/adapters/dispensing/dispenser/triggering/TriggerControllerAdapter.cpp
)

set(SILIGEN_DEVICE_ADAPTERS_LEGACY_BRIDGE_SOURCES
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/legacy/drivers/multicard/MultiCardAdapter.cpp
)

add_library(siligen_device_adapters STATIC
    ${SILIGEN_DEVICE_ADAPTERS_SOURCES}
    ${SILIGEN_DEVICE_ADAPTERS_LEGACY_BRIDGE_SOURCES}
)

if(EXISTS "${SILIGEN_DEVICE_ADAPTERS_VENDOR_DIR}/MultiCard.lib")
    set(SILIGEN_HAS_REAL_MULTICARD 1)
else()
    set(SILIGEN_HAS_REAL_MULTICARD 0)
endif()

if(SILIGEN_HAS_REAL_MULTICARD)
    target_sources(siligen_device_adapters PRIVATE
        ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/drivers/multicard/RealMultiCardWrapper.cpp
    )
else()
    target_sources(siligen_device_adapters PRIVATE
        ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}/legacy/drivers/multicard/MultiCardStub_Class.cpp
    )
endif()

target_include_directories(siligen_device_adapters PUBLIC
    ${SILIGEN_DEVICE_ADAPTERS_INCLUDE_ROOT}
    ${SILIGEN_DEVICE_ADAPTERS_SHARED_COMPAT_DIR}
)
target_include_directories(siligen_device_adapters PRIVATE
    ${SILIGEN_DEVICE_ADAPTERS_SOURCE_ROOT}
)

target_link_libraries(siligen_device_adapters PUBLIC
    siligen_device_contracts
    siligen_shared_kernel
    siligen_workflow_domain_public
)
if(TARGET siligen_types)
    target_link_libraries(siligen_device_adapters PRIVATE siligen_types)
endif()

if(TARGET siligen_utils)
    target_link_libraries(siligen_device_adapters PRIVATE siligen_utils)
endif()

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
set_target_properties(siligen_device_adapters PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/device-adapters"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)

if(NOT TARGET siligen_device_hal)
    add_library(siligen_device_hal INTERFACE)
    target_include_directories(siligen_device_hal INTERFACE
        ${SILIGEN_DEVICE_ADAPTERS_INCLUDE_ROOT}
    )
    target_link_libraries(siligen_device_hal INTERFACE
        siligen_device_adapters
        siligen_device_contracts
    )
    target_compile_features(siligen_device_hal INTERFACE cxx_std_17)
endif()
