cmake_minimum_required(VERSION 3.20)

# Reserved for future workflow-owned facade integration cases only.
if(NOT TARGET workflow_integration_tests)
    add_custom_target(workflow_integration_tests)
endif()
