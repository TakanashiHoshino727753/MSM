# 外观个性化模块（msm_appearance）：背景图/壁纸轮换/压暗/透明度/背景音乐路径。
# 与本地端 SettingsController + AppController 的壁纸逻辑行为一致（共用同一套 QSettings 键），
# 作为桌面端与远控端共用的唯一实现。含 Q_OBJECT，需 AUTOMOC；依赖 Qt6::Core + Qt6::Multimedia
# （appearancesettings.cpp 引用 QMediaPlayer / QAudioOutput 解析音频格式）。
add_library(msm_appearance STATIC
    appearancesettings.cpp
    appearancesettings.h
)

target_link_libraries(msm_appearance PUBLIC Qt6::Core Qt6::Multimedia)
target_include_directories(msm_appearance PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
