# 服务器创建 / 模组包导入（依赖 download + server + java）
add_library(msm_serverctl STATIC
    createservercontroller.h
    createservercontroller.cpp
    modpackimporter.h
    modpackimporter.cpp
    installtask.h
    installcoordinator.h
    installcoordinator.cpp
)
target_link_libraries(msm_serverctl PUBLIC Qt6::Core Qt6::Network msm_core msm_download msm_server msm_java)
target_include_directories(msm_serverctl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
