# Webhook 通知（崩溃 / 启停 / 玩家进服推送）
add_library(msm_notify STATIC
    notifier.h
    notifier.cpp
)
target_link_libraries(msm_notify PUBLIC Qt6::Core Qt6::Network)
target_include_directories(msm_notify PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
