cmake_minimum_required(VERSION 3.20)

get_filename_component(MOTION_PLANNING_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(MOTION_PLANNING_MODULES_DIR "${MOTION_PLANNING_MODULE_DIR}/.." ABSOLUTE)

if(NOT DEFINED SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR)
    set(SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR
        "${MOTION_PLANNING_MODULE_DIR}"
    )
endif()

if(NOT TARGET siligen_process_runtime_core_boost_headers)
    add_library(siligen_process_runtime_core_boost_headers INTERFACE)
    target_include_directories(siligen_process_runtime_core_boost_headers INTERFACE
        ${SILIGEN_BOOST_INCLUDE_DIR}
    )
    target_compile_features(siligen_process_runtime_core_boost_headers INTERFACE cxx_std_17)
endif()

if(NOT TARGET siligen_process_path_domain_trajectory)
    add_subdirectory(
        "${MOTION_PLANNING_MODULES_DIR}/process-path/domain/trajectory"
        "${CMAKE_BINARY_DIR}/modules/process-path/domain/trajectory"
    )
endif()

add_library(siligen_motion STATIC
    ../../../process-path/domain/trajectory/domain-services/MotionPlanner.cpp
    domain-services/interpolation/TrajectoryInterpolatorBase.cpp
    domain-services/interpolation/LinearInterpolator.cpp
    domain-services/interpolation/ArcInterpolator.cpp
    domain-services/interpolation/ArcGeometryMath.cpp
    domain-services/interpolation/SplineInterpolator.cpp
    domain-services/interpolation/SplineGeometryMath.cpp
    domain-services/interpolation/InterpolationCommandValidator.cpp
    domain-services/interpolation/InterpolationProgramPlanner.cpp
    domain-services/interpolation/ValidatedInterpolationPort.cpp
    domain-services/GeometryBlender.cpp
    domain-services/SpeedPlanner.cpp
    domain-services/TrajectoryPlanner.cpp
    domain-services/TriggerCalculator.cpp
    domain-services/TimeTrajectoryPlanner.cpp
    domain-services/VelocityProfileService.cpp
    domain-services/SevenSegmentSCurveProfile.cpp
    BSplineCalculator.cpp
    BezierCalculator.cpp
    CircleCalculator.cpp
    CMPValidator.cpp
    CMPCompensation.cpp
    CMPCoordinatedInterpolator.cpp
)

target_include_directories(siligen_motion BEFORE PUBLIC
    ${MOTION_PLANNING_MODULES_DIR}/process-path
    ${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_include_directories(siligen_motion PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/domain-services/interpolation
)

target_link_libraries(siligen_motion PUBLIC
    siligen_process_runtime_core_boost_headers
    siligen_types
    siligen_utils
    siligen_process_path_domain_trajectory
)

target_compile_features(siligen_motion PUBLIC cxx_std_17)

message(STATUS "[MotionPlanning::Domain::Motion] canonical motion target 已添加")
