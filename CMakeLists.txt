cmake_minimum_required(VERSION 3.16)

project(MinecraftServerManager VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 未显式指定构建类型时默认 Release（开启编译器优化，并关闭 QT_DEBUG 控制台回显，
# 避免日志噪声与运行开销）。可在首次 configure 前用 -DCMAKE_BUILD_TYPE=MinSizeRel 覆盖。
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "默认构建类型" FORCE)
endif()

# Qt 元对象系统（moc/rcc/uic）全局开启，子目录的静态库同样受益
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 跳过 Qt6Gui 对 WrapVulkanHeaders 的探测：Vulkan 在本项目是软依赖（找不到也照常构建），
# 而我们只用 OpenGL 渲染后端（见 main.cpp 的 QSG_RHI_BACKEND=opengl），无需 Vulkan 头文件/SDK。
# 设为 TRUE 可静默跳过 configure 阶段的 "Could NOT find WrapVulkanHeaders" 提示。
set(CMAKE_DISABLE_FIND_PACKAGE_WrapVulkanHeaders TRUE)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Quick
    QuickControls2
    Widgets
    Network
    Svg
    WebSockets
    HttpServer
)

qt_standard_project_setup(REQUIRES 6.8)

# 先构建各业务领域的独立静态库（见 src/CMakeLists.txt）
add_subdirectory(src)

# Theme 作为单例在 QML 中通过 import MinecraftServerManager 1.0 访问。
# 必须在 qt_add_qml_module 之前设置，否则属性在模块处理文件时尚未生效，
# 导致 Theme 被注册成普通类型（qmldir 无 singleton 前缀），
# 运行时 Theme.accent 等取不到值、背景色全部丢失。
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
set_source_files_properties(qml/I18n.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# PropsNames.js 含 ".pragma library"，被 QML 以相对路径 import "PropsNames.js" 使用，
# 不依赖 qmldir 条目。必须在 qt_add_qml_module 之前设置，消除 Qt6 qmllint 的 (dev) 警告
# （警告在模块处理文件时即发出，事后设置无效）。
set_source_files_properties(qml/PropsNames.js PROPERTIES QT_QML_SKIP_QMLDIR_ENTRY TRUE)

# 可执行程序：仅包含入口 main.cpp + 资源，链接上述静态库
# WIN32：将 exe 标记为 Windows GUI 子系统，避免启动时弹出空的 cmd/控制台窗口。
# 需要命令行诊断时仍可加 --console 参数，程序会自行 AllocConsole 打开控制台。
qt_add_executable(MinecraftServerManager
    WIN32
    main.cpp
    app.rc
)

# Linux 上可执行文件不带 .exe 后缀，会与 qt_add_qml_module 在 build 根生成的同名
# QML 模块目录(MinecraftServerManager/)冲突，导致链接器"cannot open output file ... Is a directory"。
# 将 exe 输出到 bin/ 子目录规避；Windows 因 .exe 后缀不冲突，保持原样。
if(NOT WIN32)
    set_target_properties(MinecraftServerManager PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# QML 模块：UI 层单独抽离到 qml/，C++ 通过上下文属性注入，不在模块内注册类型
qt_add_qml_module(MinecraftServerManager
    URI MinecraftServerManager
    VERSION 1.0
    QML_FILES
        qml/Theme.qml
        qml/TitleBar.qml
        qml/SideBar.qml
        qml/ServerCard.qml
        qml/ServerPage.qml
        qml/ModalPopup.qml
        qml/MainWindow.qml
        qml/DownloadCenter.qml
        qml/DownloadsPanel.qml
        qml/CreateServerDialog.qml
        qml/ImportModpackDialog.qml
        qml/ControllerSettings.qml
        qml/ProxyPage.qml
        qml/ComboBoxEx.qml
        qml/ComboBoxEditable.qml
        qml/AccentButton.qml
        qml/SubtleButton.qml
        qml/PropsNames.js
        qml/I18n.qml
    RESOURCES
        Resources.qrc
)

target_link_libraries(MinecraftServerManager PRIVATE
    Qt6::Core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
    Qt6::WebSockets
    msm_core
    msm_download
    msm_java
    msm_server
    msm_serverctl
    msm_settings
    msm_webui
    msm_bot
    msm_proxy
    msm_notify
    msm_ops
)

install(TARGETS MinecraftServerManager
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
