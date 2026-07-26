import QtQuick
import QtQuick.Controls
import MinecraftServerManager

// 自绘按钮（绕开 Button 在 Basic 样式下的背景/contentItem 处理）：
// 底色可通过 accentColor 指定（默认 Theme.accent），文字跟随深浅色；hover 时略深。
Item {
    id: root
    property alias text: label.text
    property color accentColor: Theme.accent
    signal clicked()

    implicitWidth: label.implicitWidth + 16
    implicitHeight: 28

    opacity: enabled ? 1 : 0.6

    Rectangle {
        anchors.fill: parent
        radius: 6
        color: ma.containsMouse && root.enabled ? Qt.darker(accentColor, 1.15) : accentColor
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    Label {
        id: label
        anchors.centerIn: parent
        color: Theme.text
        font.pixelSize: 13
    }
    MouseArea {
        id: ma
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ForbiddenCursor
        enabled: root.enabled
        onClicked: root.clicked()
    }
}
