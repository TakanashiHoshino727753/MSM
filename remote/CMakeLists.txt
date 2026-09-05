# ============================================================================
# MSM 远控端子项目
# ----------------------------------------------------------------------------
# 通过桌面端 WebUI JSON API 远程管理 Minecraft 服务器的原生移动客户端。
#
# 两个平台各有官方构建体系，CMake 不参与实际编译：
#   android/ —— Android Studio + Gradle（Kotlin / Jetpack Compose）
#   ios/     —— Xcode（Swift / SwiftUI）
#
# CMake 在此的作用：让远控端成为父项目里可见、可校验的一项——
# 检查工程入口是否存在、登记源码清单，便于 IDE 聚合展示与 CI 做存在性校验。
# ============================================================================
project(MSMRemote VERSION 1.0 LANGUAGES NONE)

set(MSM_REMOTE_ANDROID_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
set(MSM_REMOTE_IOS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/ios")

# ---- Android：Gradle 工程入口 ----
if(EXISTS "${MSM_REMOTE_ANDROID_DIR}/settings.gradle.kts")
    message(STATUS "[remote] Android 工程就绪：${MSM_REMOTE_ANDROID_DIR}（用 Android Studio 打开）")
else()
    message(WARNING "[remote] 未找到 Android 工程入口：${MSM_REMOTE_ANDROID_DIR}/settings.gradle.kts")
endif()

# ---- iOS：Swift 源码目录 ----
if(IS_DIRECTORY "${MSM_REMOTE_IOS_DIR}")
    message(STATUS "[remote] iOS 源码就绪：${MSM_REMOTE_IOS_DIR}（用 Xcode 打开）")
else()
    message(WARNING "[remote] 未找到 iOS 源码目录：${MSM_REMOTE_IOS_DIR}")
endif()

# 伪目标：始终视为最新，仅作为父项目/IDE 中的可见入口
add_custom_target(MSMRemoteClient
    COMMENT "远控端由 Android Studio(remote/android) / Xcode(remote/ios) 构建，CMake 不参与编译"
)
