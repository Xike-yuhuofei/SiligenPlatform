# Dispensing 子域 CMakeLists.txt
# S2-A: workflow 不再承载 M8 dispensing/planning live implementation。

if(NOT TARGET domain_dispensing)
    add_library(domain_dispensing INTERFACE)
endif()

if(TARGET siligen_dispense_packaging_domain_dispensing)
    target_link_libraries(domain_dispensing INTERFACE
        siligen_dispense_packaging_domain_dispensing
    )
endif()

target_include_directories(domain_dispensing INTERFACE
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)
target_compile_features(domain_dispensing INTERFACE cxx_std_17)

message(STATUS "[Domain::Dispensing] workflow compatibility target now forwards to M8 owner")
