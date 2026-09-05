# 运维自动化（定时备份 / 定时启停 / 一键更新 jar，依赖 server 解析服务器路径）
add_library(msm_ops STATIC
    backupcontroller.h
    backupcontroller.cpp
    schedulercontroller.h
    schedulercontroller.cpp
    updatecontroller.h
    updatecontroller.cpp
    portmapper.h
    portmapper.cpp
)
target_link_libraries(msm_ops PUBLIC Qt6::Core Qt6::Network msm_core msm_server)
target_include_directories(msm_ops PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
                                   ${CMAKE_CURRENT_SOURCE_DIR}/../server)
