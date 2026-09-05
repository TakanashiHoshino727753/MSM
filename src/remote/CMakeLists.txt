# ============================================================================
# MSM 远控端子项目（Qt / C++ / QML 客户端）
# ----------------------------------------------------------------------------
# 与本地端（desktop）通过 WebUI JSON API（HTTPS + Bearer 令牌）通信，实现
# 远程管理 Minecraft 服务器：连接/列表/启停/发指令/一键优化模组安装。
# 与 desktop 共用同一份统一日志处理器（src/shared/msm_logging.cpp），由 main.cpp 安装。
# ============================================================================
cmake_minimum_required(VERSION 3.16)

project(MSM_Remote VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT DEFINED QT_VERSION_MAJOR)
    set(QT_VERSION_MAJOR 6)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Quick QuickControls2 Network Multimedia Gui Widgets)

qt_standard_project_setup()

# 先构建远控端各业务领域的独立静态库（见 remoteclient/CMakeLists.txt）
add_subdirectory(remoteclient)

# msm_appearance（appearancesettings）的 moc 由该静态库统一生成并编译；
# 远控端 main.cpp 仅引用该类，跳过其 AUTOMOC 扫描，避免重复生成 moc 导致链接多重定义。
set_source_files_properties(main.cpp PROPERTIES SKIP_AUTOMOC ON)

set(PROJECT_SOURCES
    main.cpp
)

# QML / 资源打进 qrc，跨平台统一从 qrc 加载
qt_add_executable(MSM_Remote
    ${PROJECT_SOURCES}
    qml.qrc
)

set_target_properties(MSM_Remote PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(MSM_Remote PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Network
    Qt6::Multimedia
    Qt6::Gui
    Qt6::Widgets
    msm_remoteclient
    msm_logging
    msm_appearance
)

# 共享模块（src/shared/）头文件搜索路径
target_include_directories(MSM_Remote PRIVATE ${CMAKE_SOURCE_DIR}/src/shared)

# 安卓 / iOS 部署设置
if(ANDROID)
    set_property(TARGET MSM_Remote APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
        ${CMAKE_CURRENT_SOURCE_DIR}/android)
endif()
