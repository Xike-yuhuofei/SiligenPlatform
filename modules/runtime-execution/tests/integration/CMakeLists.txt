cmake_minimum_required(VERSION 3.20)

if(NOT TARGET runtime_execution_integration_host_bootstrap_smoke)
    if(NOT TARGET runtime_service_integration_host_bootstrap_smoke)
        message(FATAL_ERROR
            "runtime-execution integration compatibility target requires apps/runtime-service/tests to be configured first"
        )
    endif()

    add_custom_target(runtime_execution_integration_host_bootstrap_smoke
        DEPENDS runtime_service_integration_host_bootstrap_smoke
    )
endif()

if(NOT TARGET runtime_execution_integration_tests)
    add_custom_target(runtime_execution_integration_tests)
endif()

add_dependencies(runtime_execution_integration_tests
    runtime_execution_integration_host_bootstrap_smoke
)
