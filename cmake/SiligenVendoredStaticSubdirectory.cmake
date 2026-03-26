macro(siligen_add_static_vendor_subdirectory source_dir binary_dir)
    if(DEFINED BUILD_SHARED_LIBS)
        set(_siligen_saved_build_shared_libs_defined TRUE)
        set(_siligen_saved_build_shared_libs_value "${BUILD_SHARED_LIBS}")
    else()
        set(_siligen_saved_build_shared_libs_defined FALSE)
    endif()

    if(DEFINED CACHE{BUILD_SHARED_LIBS})
        set(_siligen_saved_build_shared_libs_cache_defined TRUE)
    else()
        set(_siligen_saved_build_shared_libs_cache_defined FALSE)
    endif()

    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
    add_subdirectory("${source_dir}" "${binary_dir}")

    if(_siligen_saved_build_shared_libs_cache_defined)
        set(
            BUILD_SHARED_LIBS
            "${_siligen_saved_build_shared_libs_value}"
            CACHE BOOL "Build shared libraries" FORCE
        )
    else()
        unset(BUILD_SHARED_LIBS CACHE)
    endif()

    if(_siligen_saved_build_shared_libs_defined)
        set(BUILD_SHARED_LIBS "${_siligen_saved_build_shared_libs_value}")
    else()
        unset(BUILD_SHARED_LIBS)
    endif()

    unset(_siligen_saved_build_shared_libs_defined)
    unset(_siligen_saved_build_shared_libs_value)
    unset(_siligen_saved_build_shared_libs_cache_defined)
endmacro()
