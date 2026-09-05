import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager

// 统一数字输入：背景/文字/上下箭头跟随主题（深浅色自适应）。
SpinBox {
    id: control
    Layout.preferredWidth: 96
    background: Rectangle {
        color: Theme.panelAlt
        border.color: Theme.border
        radius: 6
    }
    contentItem: TextInput {
        text: control.value
        color: Theme.text
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter
        readOnly: !control.editable
        font: control.font
        validator: control.validator
    }
    up.indicator: Rectangle {
        x: parent.width - width; width: 22; height: parent.height / 2
        color: "transparent"
        Text { text: "▲"; color: Theme.textMuted; anchors.centerIn: parent; font.pixelSize: 9 }
    }
    down.indicator: Rectangle {
        x: parent.width - width; y: parent.height / 2; width: 22; height: parent.height / 2
        color: "transparent"
        Text { text: "▼"; color: Theme.textMuted; anchors.centerIn: parent; font.pixelSize: 9 }
    }
}
