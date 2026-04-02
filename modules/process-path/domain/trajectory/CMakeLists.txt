# Trajectory 子域 CMakeLists.txt

include_directories(BEFORE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

add_library(siligen_process_path_domain_trajectory STATIC
    domain-services/GeometryNormalizer.cpp
    domain-services/PathRegularizer.cpp
    domain-services/ProcessAnnotator.cpp
    domain-services/TrajectoryShaper.cpp
    domain-services/SplineApproximation.cpp
)

target_sources(siligen_process_path_domain_trajectory PRIVATE
    value-objects/Primitive.h
    value-objects/Path.h
    value-objects/ProcessPath.h
    value-objects/ProcessConfig.h
    ports/IPathSourcePort.h
)

target_include_directories(siligen_process_path_domain_trajectory PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

target_link_libraries(siligen_process_path_domain_trajectory PUBLIC
    siligen_shared_boost_headers
    siligen_types
    siligen_geometry
)

target_compile_features(siligen_process_path_domain_trajectory PUBLIC cxx_std_17)

message(STATUS "[ProcessPath::Domain::Trajectory] canonical trajectory target 已添加")
