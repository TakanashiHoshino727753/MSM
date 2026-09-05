# 低层：下载任务列表模型（无外部依赖，被 download / java 等共用，打破循环依赖）
add_library(msm_core STATIC
    downloadlistmodel.h
    httpclient.h
)
target_link_libraries(msm_core PUBLIC Qt6::Core Qt6::Network)
target_include_directories(msm_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
