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

target_link_libraries(siligen_recipe_core PUBLIC
    siligen_types
    siligen_domain
    siligen_domain_services
)

target_compile_features(siligen_recipe_core PUBLIC cxx_std_17)

if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_recipe_core PRIVATE ${SILIGEN_PCH_HEADER})
    message(STATUS "[Application::Recipes] 预编译头已应用于 siligen_recipe_core")
endif()

message(STATUS "[Application::Recipes] 独立库 siligen_recipe_core 配置完成")
