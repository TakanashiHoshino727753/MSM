// Chip.qml —— 可点击标签（优化模组项）
import QtQuick 2.15
import QtQuick.Controls 2.15
import Theme 1.0

Rectangle {
    id: root
    property string text: ""
    signal clicked()
    radius: Theme.radiusSm
    color: mouse.containsMouse ? Theme.accentHover : Theme.surfaceAlt
    border.color: Theme.border
    implicitWidth: label.implicitWidth + 20
    implicitHeight: label.implicitHeight + 10
    Label {
        id: label
        anchors.centerIn: parent
        text: root.text
        color: Theme.text
        font.pixelSize: 12
    }
    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: root.clicked()
    }
}
