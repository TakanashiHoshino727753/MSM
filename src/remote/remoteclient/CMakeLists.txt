# 远控端核心（msm_remoteclient）：连接本地端 WebUI 并转发操作（REST 客户端）。
# 远程端唯一的成体量业务模块，与桌面端各 msm_* 领域库结构保持一致，便于独立编译/单点修复。
# RemoteClient 经 setContextProperty("remote", ...) 暴露给 QML，自身仅需 Qt6::Core/Network 编译。

add_library(msm_remoteclient STATIC
    remoteclient.cpp
    remoteclient.h
)

target_link_libraries(msm_remoteclient PUBLIC Qt6::Core Qt6::Network)
target_include_directories(msm_remoteclient PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
