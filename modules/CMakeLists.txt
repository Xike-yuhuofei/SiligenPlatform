cmake_minimum_required(VERSION 3.20)

foreach(module_dir IN ITEMS
    workflow
    job-ingest
    dxf-geometry
    topology-feature
    process-planning
    coordinate-alignment
    process-path
    motion-planning
    dispense-packaging
    runtime-execution
    trace-diagnostics
    hmi-application
)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module_dir}/CMakeLists.txt")
        add_subdirectory(
            "${CMAKE_CURRENT_SOURCE_DIR}/${module_dir}"
            "${CMAKE_BINARY_DIR}/modules/${module_dir}"
        )
    endif()
endforeach()
