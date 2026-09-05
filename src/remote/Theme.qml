// Theme.qml —— 暗色主题（与原 MSM 风格统一）
pragma Singleton
import QtQuick 2.15

QtObject {
    readonly property color bg: "#15161c"
    readonly property color surface: "#1f2230"
    readonly property color surfaceAlt: "#272b3d"
    readonly property color border: "#33374a"
    readonly property color text: "#e8eaf2"
    readonly property color textMuted: "#8a90a6"
    readonly property color accent: "#4f8cff"
    readonly property color accentHover: "#6ba0ff"
    readonly property color danger: "#ff5a6a"
    readonly property color ok: "#3ddc84"
    readonly property int radius: 10
    readonly property int radiusSm: 6
}
