# Java 运行环境管理（按 MC 版本选择 / 下载 / 安装 JDK）
# 仅依赖 downloadlistmodel（注入），不依赖整个 DownloadManager，避免循环依赖。
add_library(msm_java STATIC
    javamanager.h
    javamanager.cpp
)
target_link_libraries(msm_java PUBLIC Qt6::Core Qt6::Network Qt6::Gui msm_core)
target_include_directories(msm_java PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                   # DownloadManager 为纯头文件内联实现（无 downloadmanager.cpp），
                                   # 此处仅提供 include 目录供 javamanager.cpp 编译，不建立 target 链接依赖，
                                   # 避免与 msm_download（已反向依赖 msm_java）形成循环依赖。
                                   PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../download)
