import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager

// 统一下拉框：颜色全部跟随主题（深浅色均自适应），默认横向撑满。
ComboBox {
    id: control
    Layout.fillWidth: true
    Layout.preferredHeight: 30
    background: Rectangle {
        color: Theme.panelAlt
        border.color: Theme.border
        radius: 6
    }
    contentItem: Text {
        text: control.displayText
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        leftPadding: 10
        rightPadding: control.indicator ? control.indicator.width + 10 : 10
        elide: Text.ElideRight
    }
    // 只保留向下的展开箭头
    indicator: Text {
        x: control.width - width - 10
        y: control.topPadding + (control.availableHeight - height) / 2
        text: "▾"
        color: Theme.textMuted
        font.pixelSize: 12
    }
}
