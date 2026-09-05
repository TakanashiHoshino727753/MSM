import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager

// 统一滑块：轨道/已填充段/手柄均跟随主题色（深浅色自适应）。
Slider {
    id: control
    Layout.fillWidth: true
    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4
        radius: 2
        color: Theme.accentSoft
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: Theme.accent
        }
    }
    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 14; height: 14; radius: 7
        color: Theme.accent
        border.color: Theme.text
        border.width: 1
    }
}
