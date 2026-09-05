import QtQuick
import QtQuick.Controls
import MinecraftServerManager

// 统一开关：开启态用主题色，关闭态用次级面板色；手柄跟随文字色以保证深浅色对比。
Switch {
    id: control
    implicitWidth: 40
    implicitHeight: 22
    leftPadding: 0; rightPadding: 0; topPadding: 0; bottomPadding: 0
    spacing: 0
    background: Item {}
    indicator: Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: control.checked ? Theme.accent : Theme.panelAlt
        border.color: Theme.border
        Rectangle {
            x: control.checked ? parent.width - parent.height + 2 : 2
            y: 2
            width: parent.height - 4
            height: parent.height - 4
            radius: width / 2
            color: Theme.text
            Behavior on x { NumberAnimation { duration: 120 } }
        }
    }
}
