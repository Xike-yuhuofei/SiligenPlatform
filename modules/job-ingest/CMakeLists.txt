add_library(siligen_module_job_ingest INTERFACE)

if(TARGET siligen_process_runtime_core_application)
    target_link_libraries(siligen_module_job_ingest INTERFACE siligen_process_runtime_core_application)
endif()

if(TARGET siligen_application_contracts)
    target_link_libraries(siligen_module_job_ingest INTERFACE siligen_application_contracts)
endif()

target_compile_features(siligen_module_job_ingest INTERFACE cxx_std_17)

set_target_properties(siligen_module_job_ingest PROPERTIES
    SILIGEN_MODULE_ID "M1"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/job-ingest"
    SILIGEN_TARGET_IMPLEMENTATION_ROOTS "modules/job-ingest/domain;modules/job-ingest/services;modules/job-ingest/application;modules/job-ingest/adapters;modules/job-ingest/tests;modules/job-ingest/examples"
)
