# 服务器运行（ServerManager 数据模型 + ServerController 进程管理）
# ServerManager 用 QQmlListProperty 暴露给 QML，需链接 Qt6::Qml。
add_library(msm_server STATIC
    servermanager.h
    servercontroller.h
    servercontroller.cpp
)
# Psapi 是 Windows 专属库（进程/模块信息查询），仅 Windows 链接。
# 现代 Windows 上 Psapi 函数实际多由 kernel32 导出，但为兼容仍显式链接。
if(WIN32)
    target_link_libraries(msm_server PUBLIC Qt6::Core Qt6::Qml Qt6::Network Psapi)
else()
    target_link_libraries(msm_server PUBLIC Qt6::Core Qt6::Qml Qt6::Network)
endif()
target_include_directories(msm_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
