import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager

// 可输入 + 可下拉选择的组合框：外观与 ComboBoxEx（创建服务器页的版本选择控件）保持一致。
// 用于"既能手动输入路径，又能从候选列表选择"的场景（如 NapCat 路径 / NoneBot 目录）。
ComboBox {
    id: control
    editable: true
    // 可靠地把文本填入编辑框：直接写内部 TextField。
    // （editText 的声明式绑定可能因用户手动输入被打破；currentIndex 赋相同值时不触发更新，
    //   所以“并入 model + 选中”并不保证刷新文本，唯一稳妥的是直写 contentItem.text）
    function setText(t) {
        editText = t
        if (contentItem)
            contentItem.text = t
    }
    Layout.fillWidth: true
    Layout.preferredHeight: 30
    // 给右侧箭头留边距，避免文字压到箭头、箭头贴边
    rightPadding: 36
    background: Rectangle {
        color: Theme.panelAlt
        border.color: Theme.border
        radius: 6
    }
    contentItem: TextField {
        text: control.editable ? control.editText : control.displayText
        color: Theme.text
        font: control.font
        verticalAlignment: Text.AlignVCenter
        leftPadding: 10
        rightPadding: 36
        topPadding: 0
        bottomPadding: 0
        selectByMouse: true
        selectionColor: Theme.accent
        enabled: control.editable
        autoScroll: control.editable
        readOnly: control.down
        inputMethodHints: control.inputMethodHints
        validator: control.validator
        background: null
    }
    // 只保留向下的展开箭头（与 ComboBoxEx 相同）；editable 模式下只有 indicator 区域能开弹窗，需可点击
    indicator: Item {
        width: 28
        height: control.height
        x: control.width - width - 6
        y: control.topPadding + (control.availableHeight - height) / 2
        Text {
            anchors.centerIn: parent
            text: "▾"
            color: Theme.textMuted
            font.pixelSize: 12
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                if (control.popup.visible) control.popup.close()
                else control.popup.open()
            }
        }
    }
    popup.background: Rectangle {
        color: Theme.panel
        radius: 6
        border.color: Theme.border
    }
    // delegate 是 ComboBox 自身属性（不能写在 popup 下）；文字色跟随深浅主题
    delegate: ItemDelegate {
        width: control.width
        text: modelData
        highlighted: control.highlightedIndex === index
        palette.text: Theme.text
        palette.highlightedText: "#ffffff"
        background: Rectangle {
            color: highlighted ? Theme.accent : (hovered ? Theme.panelAlt : "transparent")
            radius: 4
        }
    }
}
