# 统一日志模块（msm_logging）：desktop / remote 两端共用的消息处理器。
# 仅依赖 Qt6::Core（QCoreApplication / QFile / QDateTime 等），无网络/多媒体依赖。
add_library(msm_logging STATIC
    msm_logging.cpp
    msm_logging.h
)

target_link_libraries(msm_logging PUBLIC Qt6::Core)
target_include_directories(msm_logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
