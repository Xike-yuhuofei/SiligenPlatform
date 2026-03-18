# ============================================================
# 共享接口层 CMakeLists.txt
# ============================================================

# 共享类型库
add_library(siligen_types STATIC
    types/Types.cpp
    types/CMPTypes.cpp
    types/Point.cpp
    types/VisualizationTypes.cpp
    types/TraceContext.cpp
)

target_include_directories(siligen_types PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/types
    ${CMAKE_SOURCE_DIR}/third_party
)

# 共享工具库
add_library(siligen_utils STATIC
    Strings/StringToolkit.cpp
    Strings/StringFormatter.cpp
    Strings/StringManipulator.cpp
    Strings/StringValidator.cpp
    Conversion/TypeConverter.cpp
    Encoding/EncodingCodec.cpp
    Hashing/HashCalculator.cpp
    Paths/PathOperations.cpp
)

target_include_directories(siligen_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/types
    ${CMAKE_SOURCE_DIR}/src
    ${SILIGEN_SHARED_KERNEL_DIR}/include
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
)

target_link_libraries(siligen_utils
    siligen_types
)

# 几何工具库 (Boost.Geometry 适配, header-only)
add_library(siligen_geometry INTERFACE)

target_include_directories(siligen_geometry INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Geometry
    ${CMAKE_SOURCE_DIR}/third_party
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(siligen_geometry INTERFACE
    siligen_types
)

# 为共享层库添加预编译头
if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_types PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)
    target_precompile_headers(siligen_utils PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)

    message(STATUS "[Shared Layer] 预编译头已应用于所有共享库")
endif()

# 导出共享库目标供其他模块使用
set(SHARED_LIBRARIES
    siligen_types
    siligen_utils
    siligen_geometry
)

message(STATUS "共享接口层库配置完成")

