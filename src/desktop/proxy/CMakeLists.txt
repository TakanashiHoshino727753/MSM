# Velocity 反向代理聚合（多服务器统一入口端口，依赖 server 读取端口/服务器列表）
add_library(msm_proxy STATIC
    proxycontroller.h
    proxycontroller.cpp
    proxymanager.h
    proxymanager.cpp
)
target_link_libraries(msm_proxy PUBLIC Qt6::Core Qt6::Network msm_core msm_server)
target_include_directories(msm_proxy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
