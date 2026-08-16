import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Qt.labs.qmlmodels

// 异常纠错中心：集中展示所有“活动异常”（崩溃 / IO 卡死 / 心跳无响应 / EULA 未同意），
// 跟踪自动重拉起进度，并提供手动干预（现在重试 / 停止重试 / 标记已解决 / 查看日志）。
Page {
    id: root
    property var records: serverController.errorRecords
    function refresh() { root.records = serverController.errorRecords; }
    Component.onCompleted: {
        serverController.errorRecordsChanged.connect(root.refresh);
    }
    Component.onDestruction: {
        serverController.errorRecordsChanged.disconnect(root.refresh);
    }

    background: Rectangle { color: Theme.bg }

    // 类型 → 配色
    function typeColor(t) {
        if (t === "eula") return "#f0a020";
        if (t === "io") return "#e0533d";
        if (t === "heartbeat") return "#b060d0";
        return "#e0533d"; // crash
    }

    header: Rectangle {
        height: 56
        color: Theme.panel
        border.color: Theme.border
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 18; anchors.rightMargin: 18
            Text {
                text: I18n.t("异常纠错中心", I18n.lang)
                color: Theme.text; font.bold: true; font.pixelSize: 17
            }
            Rectangle {
                visible: root.records.length > 0
                width: 26; height: 20; radius: 10
                color: Theme.danger
                Text { anchors.centerIn: parent; text: root.records.length; color: "#fff"; font.pixelSize: 12; font.bold: true }
            }
            Item { Layout.fillWidth: true }
            Button {
                text: I18n.t("刷新", I18n.lang)
                background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                onClicked: root.refresh()
            }
        }
    }

    // 空状态（覆盖式居中）
    Item {
        anchors.fill: parent
        visible: root.records.length === 0
        Text {
            anchors.centerIn: parent
            text: I18n.t("当前无活动异常，所有服务器运行正常 ✓", I18n.lang)
            color: Theme.success; font.pixelSize: 15
        }
    }

    ScrollView {
        anchors.fill: parent
        anchors.margins: 16
        visible: root.records.length > 0
        clip: true
        Column {
            spacing: 14
            width: parent.width
            Repeater {
                model: root.records
                delegate: Rectangle {
                    width: parent.width
                    height: cardCol.implicitHeight + 28
                    radius: 10
                    color: Theme.panel
                    border.color: Theme.border
                    ColumnLayout {
                        id: cardCol
                        x: 14; y: 14; width: parent.width - 28
                        spacing: 10
                        // 第一行：类型 + 名称 + 时间
                        RowLayout {
                            spacing: 10
                            Rectangle { width: 12; height: 12; radius: 6; color: typeColor(modelData.type) }
                            Text { text: modelData.typeLabel; color: typeColor(modelData.type); font.bold: true; font.pixelSize: 14 }
                            Text { text: "· " + modelData.name; color: Theme.text; font.pixelSize: 14 }
                            Item { Layout.fillWidth: true }
                            Text {
                                text: modelData.time
                                color: Theme.textMuted; font.pixelSize: 12
                            }
                        }
                        // 第二行：重试进度
                        RowLayout {
                            spacing: 10
                            Text {
                                text: modelData.fatal
                                    ? I18n.t("致命错误：无法自动恢复", I18n.lang)
                                    : (modelData.retrying
                                        ? I18n.t("自动重启中：第 %1/%2 次，%3 秒后重试").arg(modelData.retryCount + 1).arg(modelData.maxRetries).arg(modelData.nextRetryInSec)
                                        : (modelData.retryCount >= modelData.maxRetries
                                            ? I18n.t("已达最大重试次数（%1），已停止自动重启").arg(modelData.maxRetries)
                                            : I18n.t("自动重启已关闭，等待手动处理")))
                                color: modelData.fatal ? "#f0a020" : (modelData.retrying ? Theme.text : Theme.textMuted)
                                font.pixelSize: 12
                            }
                        }
                        // 第三行：日志尾部（折叠）
                        ColumnLayout {
                            spacing: 4
                            Button {
                                flat: true; padding: 0
                                text: I18n.t("查看尾部日志 ▾", I18n.lang)
                                background: Item {}
                                contentItem: Text { text: parent.text; color: Theme.accent; font.pixelSize: 12 }
                                onClicked: logMore.visible = !logMore.visible
                            }
                            TextArea {
                                id: logMore
                                visible: false
                                readOnly: true
                                wrapMode: Text.WrapAnywhere
                                text: modelData.logTail || I18n.t("（无可用日志）", I18n.lang)
                                font.family: "Consolas, monospace"; font.pixelSize: 11
                                color: Theme.textMuted
                                background: Rectangle { color: Theme.bg; radius: 6; border.color: Theme.border }
                                Layout.fillWidth: true
                                Layout.maximumHeight: 160
                            }
                        }
                        // 第四行：操作按钮
                        RowLayout {
                            spacing: 8
                            Button {
                                text: I18n.t("现在重试", I18n.lang)
                                enabled: !modelData.fatal
                                background: Rectangle { color: parent.hovered && !modelData.fatal ? Theme.accentHover : Theme.accent; radius: 6 }
                                contentItem: Text { text: parent.text; color: "#fff"; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                                onClicked: { serverController.retryNow(modelData.path); root.refresh(); }
                            }
                            Button {
                                text: I18n.t("停止重试", I18n.lang)
                                enabled: modelData.retrying
                                background: Rectangle { color: parent.hovered && modelData.retrying ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                contentItem: Text { text: parent.text; color: Theme.text; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                                onClicked: { serverController.stopRetries(modelData.path); root.refresh(); }
                            }
                            Item { Layout.fillWidth: true }
                            Button {
                                text: I18n.t("标记已解决", I18n.lang)
                                background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                contentItem: Text { text: parent.text; color: Theme.success; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter }
                                onClicked: { serverController.clearError(modelData.path); root.refresh(); }
                            }
                        }
                        // EULA 提示
                        Text {
                            visible: modelData.type === "eula"
                            text: I18n.t("处理建议：在服务器目录的 eula.txt 中将 eula=false 改为 eula=true 后，点击“现在重试”。", I18n.lang)
                            color: "#f0a020"; font.pixelSize: 12; wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }
    }
}
