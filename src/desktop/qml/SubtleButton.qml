import QtQuick
import QtQuick.Controls
import MinecraftServerManager

// 自绘次级按钮：背景用 panelAlt/accentSoft，文字跟随深浅色。
Item {
    id: root
    property alias text: label.text
    property bool small: false
    signal clicked()

    implicitWidth: small ? label.implicitWidth + 6 : label.implicitWidth + 12
    implicitHeight: small ? 15 : 28

    opacity: enabled ? 1 : 0.6

    Rectangle {
        anchors.fill: parent
        radius: small ? 3 : 6
        color: ma.containsMouse && root.enabled ? Theme.accentSoft : Theme.panelAlt
        border.color: Theme.border
    }
    Label {
        id: label
        anchors.centerIn: parent
        color: Theme.text
        font.pixelSize: small ? 10 : 13
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
