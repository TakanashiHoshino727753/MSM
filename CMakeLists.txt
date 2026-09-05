cmake_minimum_required(VERSION 3.16)

# ============================================================================
# MSM 父项目（聚合 / Superbuild）
# ----------------------------------------------------------------------------
# 本仓库组织为「一个父项目 + 两个并列子项目」，两端均为 Qt 6 / C++ / QML：
#
#   src/desktop/  —— MSM 桌面端（服务端管理器）：本地 WebUI、QQ 机器人、代理、自动化等
#   src/remote/   —— MSM 远控端（控制客户端）：经桌面端 WebUI JSON API（HTTPS）远程管理服务器
#
# 两端共用同一份统一日志处理器（src/shared/msm_logging.cpp，用 Qt 宏 QT_DEBUG 区分
# Debug/Release 输出行为），以及同一套 WebUI JSON API 契约（见 src/shared/README.md）。
#
# 构建方式：在仓库根执行
#     cmake -S . -B build
#     cmake --build build
# 桌面端产物为 MinecraftServerManager.exe，远控端产物为 MSM_Remote.exe。
# （旧版原生 Android/iOS 远控端已迁移至仓库根 _legacy_remote_native/ 备查。）
# ============================================================================
project(MinecraftServerManagerSuper
    VERSION 1.0
    DESCRIPTION "MSM 父项目：聚合桌面端(designated desktop)与远控端(remote)两个并列子项目"
    LANGUAGES CXX
)

# 共用静态库：统一日志 + 外观个性化（两端链接，须在两端之前构建）
add_subdirectory(src/shared)

# 远控端：Qt/C++ 客户端（src/remote）
add_subdirectory(src/remote)

# 桌面端：Qt/C++ 主程序（自带 project() 与 Qt 依赖探测，src/desktop）
add_subdirectory(src/desktop)
