import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager

// 服务器卡片：名称、版本、管理按键（启停、详情、属性）
Rectangle {
    id: root
    width: 250; height: 130
    radius: Theme.radius
    color: Theme.panelAlt
    border.color: hovered ? Theme.accent : Theme.border
    antialiasing: true
    property string name: I18n.t("未命名服务器", I18n.lang)
    property string version: "1.21.1"
    property bool running: false
    property bool hovered: false
    signal startClicked()
    signal stopClicked()
    signal forceStopClicked()
    signal detailsClicked()
    signal propertiesClicked()

    transform: Scale {
        id: cardScale
        origin.x: root.width / 2
        origin.y: root.height / 2
        xScale: 1; yScale: 1
    }
    Behavior on border.color { ColorAnimation { duration: 150 } }

    // 悬停微缩放
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: { root.hovered = true; cardScale.xScale = 1.02; cardScale.yScale = 1.02 }
        onExited:  { root.hovered = false; cardScale.xScale = 1; cardScale.yScale = 1 }
        onClicked: root.detailsClicked()
    }
    Behavior on scale { NumberAnimation { duration: 120 } }

    // 运行时状态联动（按名称匹配）：收到 ServerController 的实时 onStateChanged 信号即更新。
    Connections {
        target: serverController
        function onStateChanged(n, r) { if (n === root.name) root.running = r }
    }
    // 主动兜底校正：仅依赖信号在“窗口后打开 / 隐藏期间状态已变化 / 信号遗漏”时会失效。
    // 因此初始化时立即取一次真实运行态，并每 2 秒再校正一次，确保任何生命周期下都显示正确状态。
    Component.onCompleted: root.running = serverController.isRunning(root.name)
    Timer {
        interval: 2000; running: true; repeat: true
        onTriggered: root.running = serverController.isRunning(root.name)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8
        RowLayout {
            spacing: 6
            Rectangle {
                width: 9; height: 9; radius: 5
                color: root.running ? Theme.success : Theme.textMuted
                Behavior on color { ColorAnimation { duration: 200 } }
            }
            Label { text: root.name; color: Theme.text; font.pixelSize: 14; font.bold: true; Layout.fillWidth: true; elide: Text.ElideRight }
        }
        Label { text: I18n.t("版本：", I18n.lang) + root.version; color: Theme.textMuted; font.pixelSize: 12 }
        Item { Layout.fillHeight: true }
        RowLayout {
            spacing: 6
            AccentButton {
                text: I18n.t("启动", I18n.lang)
                accentColor: Theme.success
                visible: !root.running
                onClicked: root.startClicked()
            }
            AccentButton {
                text: I18n.t("停止", I18n.lang)
                accentColor: Qt.lighter(Theme.danger, 1.3)
                visible: root.running
                onClicked: root.stopClicked()
            }
            AccentButton {
                text: I18n.t("强制停止", I18n.lang)
                accentColor: Qt.darker(Theme.danger, 1.3)
                visible: root.running
                onClicked: root.forceStopClicked()
            }
            AccentButton {
                text: I18n.t("详情", I18n.lang)
                accentColor: Theme.bg
                onClicked: root.detailsClicked()
            }
            AccentButton {
                text: I18n.t("属性", I18n.lang)
                accentColor: Theme.bg
                onClicked: root.propertiesClicked()
            }
        }
    }
}
