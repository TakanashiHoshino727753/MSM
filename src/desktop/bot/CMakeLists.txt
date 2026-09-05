# QQ 机器人控制（独立管理 NapCat / NoneBot 外部进程，与 WebUI 解耦）
add_library(msm_bot STATIC
    botcontroller.h
    botcontroller.cpp
)
target_link_libraries(msm_bot PUBLIC Qt6::Core Qt6::Network Qt6::HttpServer msm_core msm_server msm_settings msm_download msm_serverctl)
target_include_directories(msm_bot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
