# Trajectory 子域 CMakeLists.txt

add_library(domain_trajectory STATIC
    domain-services/GeometryNormalizer.cpp
    domain-services/PathRegularizer.cpp
    domain-services/ProcessAnnotator.cpp
    domain-services/TrajectoryShaper.cpp
    domain-services/MotionPlanner.cpp
    domain-services/SplineApproximation.cpp
)

target_sources(domain_trajectory PRIVATE
    value-objects/Primitive.h
    value-objects/Path.h
    value-objects/ProcessPath.h
    value-objects/ProcessConfig.h
    value-objects/MotionConfig.h
    value-objects/PlanningReport.h
    value-objects/Trajectory.h
    ports/IPathSourcePort.h
)

target_include_directories(domain_trajectory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)

target_link_libraries(domain_trajectory PUBLIC
    siligen_types
    siligen_geometry
    ruckig
)

target_compile_features(domain_trajectory PUBLIC cxx_std_17)

message(STATUS "[Domain::Trajectory] domain_trajectory 已添加")
