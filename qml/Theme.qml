pragma Singleton
import QtQuick

// 全局主题（单例）。默认深色；可在「控制器设置」里运行时修改主色调/深浅色，
// 修改会持久化（见 main.cpp 的 QSettings），下次启动由 themeDark/themeAccent 上下文属性下发、加载即生效。
QtObject {
    id: theme
    // 初始化即读取 C++ 下发的持久化主题（themeDark/themeAccent），确保加载时文字/背景色一次到位，
    // 不再依赖 Component.onCompleted 的时机。
    property bool dark: (typeof themeDark !== 'undefined' && themeDark !== null) ? themeDark : true
    property color accent: (typeof themeAccent !== 'undefined' && themeAccent !== null) ? themeAccent : "#4f8cff" // 主题色（运行时可改，背景/标题栏/侧边栏跟随）

    // 中性基色（未着色），随深浅色切换
    readonly property color baseBg: dark ? "#1b1c21" : "#f2f3f5"
    readonly property color basePanel: dark ? "#24262e" : "#ffffff"
    readonly property color basePanelAlt: dark ? "#2c2f39" : "#e9eaee"

    // 把主色调按比例混入中性基色，实现“主色调改整个窗口颜色”
    function blend(base, tint, amount) {
        return Qt.rgba(base.r * (1 - amount) + tint.r * amount,
                       base.g * (1 - amount) + tint.g * amount,
                       base.b * (1 - amount) + tint.b * amount,
                       1)
    }

    property color bg: blend(baseBg, accent, 0.06)           // 主背景（跟随主色调）
    property color panel: blend(basePanel, accent, 0.12)     // 面板/标题栏/侧边栏（跟随主色调）
    property color panelAlt: blend(basePanelAlt, accent, 0.16) // 卡片/次级面板（跟随主色调）
    property color accentSoft: blend(basePanel, accent, 0.22)   // 明显的淡主色染色（页签条/内容区整体跟随主色调，浅色下也清晰可见）
    property color text: dark ? "#e8e8ec" : "#1b1c21"        // 主文字（随深浅色）
    property color textMuted: dark ? "#9a9ca8" : "#6b6e7a"   // 次要文字（随深浅色）
    property color border: blend(dark ? "#383b47" : "#d2d4da", accent, 0.14) // 边框/分隔线（跟随主色调）
    property color accentHover: Qt.lighter(accent, 1.2)     // 由 accent 派生
    property color danger: "#e0533d"                         // 危险操作（停止/删除）
    property color dangerHover: "#ef6a53"
    property color success: "#3ec46d"
    property int radius: 8
    property int controlHeight: 34
    property int titleBarHeight: 42
}
