# 下载引擎 + 下载目录（资源/模组/Java 目录）
# 依赖 downloadlistmodel 与 java（目录轮询 Java 状态）。
add_library(msm_download STATIC
    downloadmanager.h
    downloadcatalog.h
    downloadcatalog.cpp
)
target_link_libraries(msm_download PUBLIC Qt6::Core Qt6::Network msm_core msm_java)
target_include_directories(msm_download PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
