cmake_minimum_required(VERSION 3.16)

# ============================================================================
# MSM 父项目（聚合 / Superbuild）
# ----------------------------------------------------------------------------
# 本仓库组织为「一个父项目 + 两个并列子项目」：
#
#   desktop/  —— MSM 桌面端：Qt 6 / C++ / QML 主程序（CMake 直接构建）
#   remote/   —— MSM 远控端：Android / iOS 原生客户端（Gradle / Xcode 构建，
#                CMake 不参与编译，仅登记以便统一管理）
#
# 两端之间没有编译期代码耦合：桌面端实现 WebUI JSON API，远控端按契约调用。
# 真正的「共用物」是这份契约本身，统一放在 shared/ 下（见 shared/README.md）。
#
# 构建方式：在仓库根执行
#     cmake -S . -B build
#     cmake --build build
# 桌面端产物由 desktop/ 子项目生成；远控端请用 Android Studio / Xcode 打开
# remote/android 与 remote/ios 自行构建。
# ============================================================================
project(MinecraftServerManagerSuper
    VERSION 1.0
    DESCRIPTION "MSM 父项目：聚合桌面端(designated desktop)与远控端(remote)两个并列子项目"
    LANGUAGES CXX
)

# 远控端：不参与 C++ 编译，仅登记
add_subdirectory(remote)

# 桌面端：Qt/C++ 主程序（自带 project() 与 Qt 依赖探测）
add_subdirectory(desktop)
