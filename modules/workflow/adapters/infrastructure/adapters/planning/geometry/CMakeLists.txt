# Contour augmentation adapter (infrastructure)

add_library(siligen_contour_augment_adapter STATIC)

target_sources(siligen_contour_augment_adapter PRIVATE
    $<$<BOOL:${SILIGEN_ENABLE_CGAL}>:${CMAKE_CURRENT_SOURCE_DIR}/ContourAugmenterAdapter.cpp>
    $<$<NOT:$<BOOL:${SILIGEN_ENABLE_CGAL}>>:${CMAKE_CURRENT_SOURCE_DIR}/ContourAugmenterAdapter.stub.cpp>
)

target_include_directories(siligen_contour_augment_adapter PUBLIC
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)
target_include_directories(siligen_contour_augment_adapter PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROCESS_RUNTIME_CORE_IMPLEMENTATION_INCLUDE_DIRS}
)

target_link_libraries(siligen_contour_augment_adapter PUBLIC
    siligen_types
    siligen_utils
    siligen_geometry
)

if(SILIGEN_ENABLE_CGAL)
    find_package(CGAL REQUIRED COMPONENTS Core)
    target_link_libraries(siligen_contour_augment_adapter PUBLIC
        CGAL::CGAL
        CGAL::CGAL_Core
    )
    target_compile_definitions(siligen_contour_augment_adapter PRIVATE SILIGEN_ENABLE_CGAL=1)
    message(STATUS "[Infrastructure] CGAL enabled: ContourAugmenterAdapter")
else()
    target_compile_definitions(siligen_contour_augment_adapter PRIVATE SILIGEN_ENABLE_CGAL=0)
    message(STATUS "[Infrastructure] CGAL disabled: ContourAugmenterAdapter stub")
endif()

target_compile_features(siligen_contour_augment_adapter PUBLIC cxx_std_17)
