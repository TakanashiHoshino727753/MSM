# 按业务领域拆分出的独立静态库。
# 依赖严格单向、无环，便于“单模块独立编译 / 单点修复不影响其他模块”。
# 每个库通过 PUBLIC include 目录把自己的头文件对外暴露，调用方无需关心物理路径。

# ---- 低层：下载任务列表模型（无外部依赖，被 download / java 等共用，打破循环依赖）----
add_library(msm_core STATIC
    core/downloadlistmodel.h
    core/httpclient.h
)
target_link_libraries(msm_core PUBLIC Qt6::Core Qt6::Network)
target_include_directories(msm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core)

# ---- Java 运行环境管理（按 MC 版本选择 / 下载 / 安装 JDK）----
# 仅依赖 downloadlistmodel（注入），不依赖整个 DownloadManager，避免循环依赖。
add_library(msm_java STATIC
    java/javamanager.h
    java/javamanager.cpp
)
target_link_libraries(msm_java PUBLIC Qt6::Core Qt6::Network Qt6::Gui msm_core)
target_include_directories(msm_java PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/java
                                   # DownloadManager 为纯头文件内联实现（无 downloadmanager.cpp），
                                   # 此处仅提供 include 目录供 javamanager.cpp 编译，不建立 target 链接依赖，
                                   # 避免与 msm_download（已反向依赖 msm_java）形成循环依赖。
                                   PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/download)

# ---- 下载引擎 + 下载目录（资源/模组/Java 目录）----
# 依赖 downloadlistmodel 与 java（目录轮询 Java 状态）。
add_library(msm_download STATIC
    download/downloadmanager.h
    download/downloadcatalog.h
    download/downloadcatalog.cpp
)
target_link_libraries(msm_download PUBLIC Qt6::Core Qt6::Network msm_core msm_java)
target_include_directories(msm_download PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/download)

# ---- 服务器运行（ServerManager 数据模型 + ServerController 进程管理）----
# ServerManager 用 QQmlListProperty 暴露给 QML，需链接 Qt6::Qml。
add_library(msm_server STATIC
    server/servermanager.h
    server/servercontroller.h
    server/servercontroller.cpp
)
# Psapi 是 Windows 专属库（进程/模块信息查询），仅 Windows 链接。
# 现代 Windows 上 Psapi 函数实际多由 kernel32 导出，但为兼容仍显式链接。
if(WIN32)
    target_link_libraries(msm_server PUBLIC Qt6::Core Qt6::Qml Qt6::Network Psapi)
else()
    target_link_libraries(msm_server PUBLIC Qt6::Core Qt6::Qml Qt6::Network)
endif()
target_include_directories(msm_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/server)

# ---- 服务器创建 / 模组包导入（依赖 download + server + java）----
add_library(msm_serverctl STATIC
    serverctl/createservercontroller.h
    serverctl/createservercontroller.cpp
    serverctl/modpackimporter.h
    serverctl/modpackimporter.cpp
    serverctl/installtask.h
    serverctl/installcoordinator.h
    serverctl/installcoordinator.cpp
)
target_link_libraries(msm_serverctl PUBLIC Qt6::Core Qt6::Network msm_core msm_download msm_server msm_java)
target_include_directories(msm_serverctl PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/serverctl)

# ---- 设置管理 ----
add_library(msm_settings STATIC
    settings/settingscontroller.h
    settings/settingscontroller.cpp
)
target_link_libraries(msm_settings PUBLIC Qt6::Core msm_core)
target_include_directories(msm_settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/settings)

# ---- Webhook 通知（崩溃 / 启停 / 玩家进服推送）----
add_library(msm_notify STATIC
    notify/notifier.h
    notify/notifier.cpp
)
target_link_libraries(msm_notify PUBLIC Qt6::Core Qt6::Network)
target_include_directories(msm_notify PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/notify)

# ---- 运维自动化（定时备份 / 定时启停 / 一键更新 jar，依赖 server 解析服务器路径）----
add_library(msm_ops STATIC
    ops/backupcontroller.h
    ops/backupcontroller.cpp
    ops/schedulercontroller.h
    ops/schedulercontroller.cpp
    ops/updatecontroller.h
    ops/updatecontroller.cpp
    ops/portmapper.h
    ops/portmapper.cpp
)
target_link_libraries(msm_ops PUBLIC Qt6::Core Qt6::Network msm_core msm_server)
target_include_directories(msm_ops PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/ops
                                   ${CMAKE_CURRENT_SOURCE_DIR}/server)

# ---- Velocity 反向代理聚合（多服务器统一入口端口，依赖 server 读取端口/服务器列表）----
add_library(msm_proxy STATIC
    proxy/proxycontroller.h
    proxy/proxycontroller.cpp
    proxy/proxymanager.h
    proxy/proxymanager.cpp
)
target_link_libraries(msm_proxy PUBLIC Qt6::Core Qt6::Network msm_core msm_server)
target_include_directories(msm_proxy PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/proxy)

# ---- QQ 机器人控制（独立管理 NapCat / NoneBot 外部进程，与 WebUI 解耦）----
add_library(msm_bot STATIC
    bot/botcontroller.h
    bot/botcontroller.cpp
)
target_link_libraries(msm_bot PUBLIC Qt6::Core Qt6::Network Qt6::HttpServer msm_core msm_server msm_settings msm_download msm_serverctl)
target_include_directories(msm_bot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/bot)

# ---- Web 管理界面（汇总所有模块）----
add_library(msm_webui STATIC
    webui/webuiserver.h
    webui/webuiserver.cpp
)
# webuiserver_spa.inc 是被 webuiserver.cpp 包含的片段，不作为独立编译单元
set_source_files_properties(webui/webuiserver_spa.inc PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(msm_webui PRIVATE webui/webuiserver_spa.inc)
# crypt32 / advapi32 是 Windows CryptoAPI 专属库（自签证书），仅 Windows 链接。
# MinGW 的 #pragma comment(lib, "crypt32.lib") 对此库不可靠，故用 CMake 按平台选择，
# Linux 不链这些库，避免链接器找不到 -lcrypt32/-ladvapi32。
if(WIN32)
    target_link_libraries(msm_webui PUBLIC Qt6::Core Qt6::Network Qt6::Gui crypt32 advapi32
        msm_core msm_download msm_server msm_serverctl msm_java msm_settings msm_bot)
else()
    target_link_libraries(msm_webui PUBLIC Qt6::Core Qt6::Network Qt6::Gui
        msm_core msm_download msm_server msm_serverctl msm_java msm_settings msm_bot)
endif()
target_include_directories(msm_webui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/webui)


