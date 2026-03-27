# Recipe Core - 可独立沉淀的纯业务用例库
# 约束:
# - 仅包含内存内业务编排与端口协作
# - 不包含文件路径/文件系统/宿主运行时逻辑

add_library(siligen_recipe_core STATIC
    CreateRecipeUseCase.cpp
    UpdateRecipeUseCase.cpp
    CreateDraftVersionUseCase.cpp
    UpdateDraftVersionUseCase.cpp
    RecipeCommandUseCase.cpp
    RecipeQueryUseCase.cpp
    CreateVersionFromPublishedUseCase.cpp
    CompareRecipeVersionsUseCase.cpp
    ExportRecipeBundlePayloadUseCase.cpp
    ImportRecipeBundlePayloadUseCase.cpp
)

target_include_directories(siligen_recipe_core PUBLIC
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)
target_include_directories(siligen_recipe_core PRIVATE
    ${PROCESS_RUNTIME_CORE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(siligen_recipe_core PUBLIC
    siligen_types
    siligen_domain
    siligen_domain_services
)

target_compile_features(siligen_recipe_core PUBLIC cxx_std_17)

if(SILIGEN_USE_PCH)
    message(STATUS "[Application::Recipes] siligen_recipe_core 跳过预编译头以规避 Windows/MSBuild 下的 PCH 文件锁竞争")
endif()

message(STATUS "[Application::Recipes] 独立库 siligen_recipe_core 配置完成")
