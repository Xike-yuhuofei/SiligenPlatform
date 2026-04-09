cmake_minimum_required(VERSION 3.20)

get_filename_component(MOTION_PLANNING_MODULE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(MOTION_PLANNING_CONTRACTS_INCLUDE_DIR "${MOTION_PLANNING_MODULE_DIR}/contracts/include")

if(NOT TARGET siligen_process_path_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../process-path/contracts"
        "${CMAKE_BINARY_DIR}/_p/mp_ppc"
    )
endif()
if(NOT TARGET siligen_runtime_execution_runtime_contracts)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../runtime-execution/contracts/runtime"
        "${CMAKE_BINARY_DIR}/_p/mp_rerc"
    )
endif()

if(NOT TARGET siligen_workspace_boost_headers)
    add_library(siligen_workspace_boost_headers INTERFACE)
    target_include_directories(siligen_workspace_boost_headers INTERFACE
        ${SILIGEN_BOOST_INCLUDE_DIR}
    )
    target_compile_features(siligen_workspace_boost_headers INTERFACE cxx_std_17)
endif()

if(NOT TARGET siligen_process_path_contracts_public)
    message(FATAL_ERROR
        "motion-planning requires siligen_process_path_contracts_public "
        "as the canonical M6 consumer surface")
endif()

add_library(siligen_motion STATIC
    domain-services/MotionPlanner.cpp
    domain-services/interpolation/TrajectoryInterpolatorBase.cpp
    domain-services/interpolation/LinearInterpolator.cpp
    domain-services/interpolation/ArcInterpolator.cpp
    domain-services/interpolation/ArcGeometryMath.cpp
    domain-services/interpolation/SplineInterpolator.cpp
    domain-services/interpolation/SplineGeometryMath.cpp
    domain-services/interpolation/InterpolationCommandValidator.cpp
    domain-services/interpolation/InterpolationProgramPlanner.cpp
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
    ${MOTION_PLANNING_CONTRACTS_INCLUDE_DIR}
)
target_include_directories(siligen_motion PRIVATE
    ${MOTION_PLANNING_MODULE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/domain-services/interpolation
)

target_link_libraries(siligen_motion PUBLIC
    siligen_workspace_boost_headers
    siligen_types
    siligen_utils
    siligen_process_path_contracts_public
)
if(TARGET siligen_runtime_execution_runtime_contracts)
    target_link_libraries(siligen_motion PRIVATE
        siligen_runtime_execution_runtime_contracts
    )
endif()

target_compile_features(siligen_motion PUBLIC cxx_std_17)

message(STATUS "[MotionPlanning::Domain::Motion] canonical motion target 已添加")
