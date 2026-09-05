// ServerCard.qml —— 单个服务器卡片（启停 + 命令 + 优化模组入口）
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Theme 1.0

Rectangle {
    property var srv
    property var rc
    Layout.fillWidth: true
    color: Theme.surface
    radius: Theme.radius
    implicitHeight: col.implicitHeight + 20

    ColumnLayout {
        id: col
        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
        spacing: 8
        RowLayout {
            Layout.fillWidth: true
            Label { text: srv.name; color: Theme.text; font.bold: true; font.pixelSize: 15 }
            Label {
                text: srv.running ? "运行中" : "已停止"
                color: srv.running ? Theme.ok : Theme.textMuted
                font.pixelSize: 12
            }
            Item { Layout.fillWidth: true }
            Button {
                text: srv.running ? "停止" : "启动"
                onClicked: srv.running ? rc.stopServer(srv.name) : rc.startServer(srv.name)
            }
            Button { text: "强停"; onClicked: rc.forceStopServer(srv.name) }
            Button {
                text: "优化模组"
                onClicked: {
                    optPanel._name = srv.name
                    optPanel.visible = true
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            TextField {
                id: cmdField
                Layout.fillWidth: true
                placeholderText: "输入命令发给服务器，如 /op Steve"
                color: Theme.text
                background: Rectangle { color: Theme.bg; radius: Theme.radiusSm; border.color: Theme.border }
            }
            Button {
                text: "发送"
                onClicked: { rc.sendCommand(srv.name, cmdField.text); cmdField.text = "" }
            }
        }
    }
}
