# ============================================================================
# MSM 共用区域（src/shared/）
# ----------------------------------------------------------------------------
# 不再是一个大杂烩 msm_shared，而是按功能拆成独立静态库模块，由 desktop / remote 各自链接：
#   - logging/     统一日志处理器（msm_logging）
#   - appearance/  外观个性化：背景图/壁纸轮换/压暗/透明度（msm_appearance）
# 每个模块是独立静态库，接口通过各自 PUBLIC include 目录对链接方可见；两端子项目各自
# project() 并 find_package(Qt6)，此处显式声明所需 Qt 组件（已找到时幂等复用），
# 并开启 AUTOMOC 供含 Q_OBJECT 的模块生成 moc。
# ============================================================================
cmake_minimum_required(VERSION 3.16)

find_package(Qt6 REQUIRED COMPONENTS Core Multimedia)

# 含 Q_OBJECT 的模块（appearance）需要 AUTOMOC 生成 moc；显式开启，避免依赖
# 超级项目作用域的继承设置未生效。
set(CMAKE_AUTOMOC ON)

add_subdirectory(logging)
add_subdirectory(appearance)
