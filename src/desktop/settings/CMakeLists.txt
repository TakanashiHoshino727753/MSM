# 设置管理
add_library(msm_settings STATIC
    settingscontroller.h
    settingscontroller.cpp
)
target_link_libraries(msm_settings PUBLIC Qt6::Core msm_core)
target_include_directories(msm_settings PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
