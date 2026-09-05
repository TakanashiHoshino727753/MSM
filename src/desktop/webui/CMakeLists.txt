# Web 管理界面（汇总所有模块）
add_library(msm_webui STATIC
    webuiserver.h
    webuiserver.cpp
    qrimageprovider.h
    qrimageprovider.cpp
    qrcode_lib.h
)
# webuiserver_spa.inc 是被 webuiserver.cpp 包含的片段，不作为独立编译单元
set_source_files_properties(webuiserver_spa.inc PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(msm_webui PRIVATE webuiserver_spa.inc)
# qrcode_lib.h 为内嵌的第三方 QR 码生成库（含 JS 与 C++ 实现），被 webuiserver.cpp /
# qrimageprovider.cpp 直接 #include，登记进目标以便在 IDE 项目树中可见（非独立编译单元）。
# 其配套的 qrcode_lib.inc 已无引用，属死文件，已删除。
# crypt32 / advapi32 是 Windows CryptoAPI 专属库（自签证书），仅 Windows 链接。
# MinGW 的 #pragma comment(lib, "crypt32.lib") 对此库不可靠，故用 CMake 按平台选择，
# Linux 不链这些库，避免链接器找不到 -lcrypt32/-ladvapi32。
# Qt6::Qml / Qt6::Quick 供 QrImageProvider（QQuickImageProvider + QJSEngine）使用。
if(WIN32)
    target_link_libraries(msm_webui PUBLIC Qt6::Core Qt6::Network Qt6::Gui Qt6::Qml Qt6::Quick crypt32 advapi32
        msm_core msm_download msm_server msm_serverctl msm_java msm_settings msm_bot)
else()
    target_link_libraries(msm_webui PUBLIC Qt6::Core Qt6::Network Qt6::Gui Qt6::Qml Qt6::Quick
        msm_core msm_download msm_server msm_serverctl msm_java msm_settings msm_bot)
endif()
target_include_directories(msm_webui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
