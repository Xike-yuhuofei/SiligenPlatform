function(siligen_load_workspace_layout workspace_root layout_file)
    if(NOT EXISTS "${layout_file}")
        message(FATAL_ERROR "未找到 workspace layout 清单: ${layout_file}")
    endif()

    file(STRINGS "${layout_file}" SILIGEN_LAYOUT_LINES REGEX "^[A-Z0-9_]+=.+$")
    foreach(layout_line IN LISTS SILIGEN_LAYOUT_LINES)
        string(REGEX MATCH "^([A-Z0-9_]+)=(.+)$" _ "${layout_line}")
        set(layout_key "${CMAKE_MATCH_1}")
        set(layout_value "${CMAKE_MATCH_2}")

        if(layout_value STREQUAL ".")
            set(layout_abs "${workspace_root}")
        elseif(IS_ABSOLUTE "${layout_value}")
            set(layout_abs "${layout_value}")
        else()
            set(layout_abs "${workspace_root}/${layout_value}")
        endif()

        set(${layout_key} "${layout_abs}" CACHE INTERNAL "Loaded from ${layout_file}" FORCE)
        set(${layout_key}_RELATIVE "${layout_value}" CACHE INTERNAL "Relative value loaded from ${layout_file}" FORCE)
    endforeach()

    set(_siligen_canonical_root_keys "")
    foreach(canonical_root_key IN ITEMS
        SILIGEN_APPS_ROOT
        SILIGEN_MODULES_ROOT
        SILIGEN_SHARED_ROOT
        SILIGEN_DOCS_ROOT
        SILIGEN_SAMPLES_ROOT
        SILIGEN_TESTS_ROOT
        SILIGEN_SCRIPTS_ROOT
        SILIGEN_CONFIG_ROOT
        SILIGEN_DATA_ROOT
        SILIGEN_DEPLOY_ROOT
    )
        if(DEFINED ${canonical_root_key})
            list(APPEND _siligen_canonical_root_keys "${canonical_root_key}")
        endif()
    endforeach()
    set(
        SILIGEN_CANONICAL_ROOT_KEYS
        "${_siligen_canonical_root_keys}"
        CACHE INTERNAL "Canonical workspace roots loaded from ${layout_file}"
        FORCE
    )
    set(
        SILIGEN_TEMPLATE_ROOT_KEYS
        "${_siligen_canonical_root_keys}"
        CACHE INTERNAL "Backward-compatible alias of SILIGEN_CANONICAL_ROOT_KEYS"
        FORCE
    )
endfunction()
