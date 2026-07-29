// ProxyPage.qml —— Velocity 反向代理聚合页
// 多台同时运行的服务器通过一个统一入口端口对外服务：
// 玩家连接代理端口后，游戏内用 /server <名称> 在各后端之间切换。
// 由 C++ proxyController 驱动（安装 / 同步配置 / 启动 / 停止 / 控制台）。
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager

Item {
    id: page

    // 页签切到本页时刷新后端映射表
    property var backends: []
    function refreshBackends() { backends = proxyController.backendSummary() }
    Component.onCompleted: refreshBackends()
    onVisibleChanged: if (visible) refreshBackends()

    ScrollView {
        anchors.fill: parent
        contentWidth: width

        Column {
            anchors.left: parent.left; anchors.right: parent.right
            anchors.leftMargin: 16; anchors.rightMargin: 16
            spacing: 14; topPadding: 16; bottomPadding: 16

            // ---- 标题 + 状态 ----
            Row {
                spacing: 10; width: parent.width
                Label {
                    text: I18n.t("代理聚合（Velocity）", I18n.lang)
                    font.pixelSize: 18; font.bold: true; color: Theme.text
                }
                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    radius: 9; height: 20
                    width: statusText.implicitWidth + 16
                    color: proxyController.running ? "#2e7d32" : Theme.panel
                    border.color: Theme.border
                    Label {
                        id: statusText
                        anchors.centerIn: parent
                        font.pixelSize: 11
                        color: proxyController.running ? "white" : Theme.textMuted
                        text: proxyController.status
                    }
                }
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap
                color: Theme.textMuted; font.pixelSize: 12
                text: I18n.t("将多台同时运行的服务器聚合到一个入口端口：玩家统一连接代理端口，游戏内用 /server 名称 切换服务器。启动代理前会自动生成配置并修补各后端（online-mode=false）。", I18n.lang)
            }

            // ---- 安装 / 参数 ----
            Rectangle {
                width: parent.width
                color: Theme.panel; radius: Theme.radius; border.color: Theme.border
                height: setupCol.implicitHeight + 28
                Column {
                    id: setupCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                    spacing: 10
                    Row {
                        spacing: 10
                        Button {
                            text: proxyController.installed ? I18n.t("更新 Velocity", I18n.lang)
                                                            : I18n.t("安装 Velocity", I18n.lang)
                            enabled: !proxyController.busy && !proxyController.running
                            palette.windowText: "white"; palette.buttonText: "white"
                            background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.accentHover : Theme.accent) : Theme.panelAlt; radius: 6 }
                            onClicked: proxyController.install()
                        }
                        ProgressBar {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 160
                            from: 0; to: 100
                            value: proxyController.installProgress
                            visible: proxyController.busy
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.textMuted; font.pixelSize: 12
                            text: proxyController.installed ? I18n.t("已安装", I18n.lang) : I18n.t("未安装", I18n.lang)
                        }
                    }
                    Row {
                        spacing: 10
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: I18n.t("入口端口", I18n.lang); color: Theme.text
                        }
                        TextField {
                            id: portField
                            width: 90
                            enabled: !proxyController.running
                            text: String(proxyController.proxyPort)
                            validator: IntValidator { bottom: 1; top: 65535 }
                            color: Theme.text
                            background: Rectangle { color: Theme.bg; radius: 6; border.color: portField.activeFocus ? Theme.accent : Theme.border }
                            onEditingFinished: proxyController.proxyPort = parseInt(text)
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MOTD"; color: Theme.text
                        }
                        TextField {
                            id: motdField
                            width: 260
                            enabled: !proxyController.running
                            text: proxyController.motd
                            color: Theme.text
                            background: Rectangle { color: Theme.bg; radius: 6; border.color: motdField.activeFocus ? Theme.accent : Theme.border }
                            onEditingFinished: proxyController.motd = text
                        }
                    }

                    // 离线 / 非正版玩家开关
                    Row {
                        spacing: 10
                        Switch {
                            id: offlineSwitch
                            checked: proxyController.offlineMode
                            enabled: !proxyController.running
                            onCheckedChanged: if (checked !== proxyController.offlineMode) proxyController.offlineMode = checked
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: I18n.t("允许离线/非正版玩家（无正版校验）", I18n.lang)
                            color: Theme.text
                        }
                    }
                    Label {
                        width: parent.width; wrapMode: Text.Wrap
                        color: Theme.textMuted; font.pixelSize: 11
                        text: I18n.t("代理将关闭正版验证，任何用户名均可进入；后端已设为离线模式，可正常切换服务器。", I18n.lang)
                    }

                    // 停止代理时一并停止后端开关
                    Row {
                        spacing: 10
                        Switch {
                            id: stopBackendsSwitch
                            checked: proxyController.stopBackendsWithProxy
                            onCheckedChanged: if (checked !== proxyController.stopBackendsWithProxy) proxyController.stopBackendsWithProxy = checked
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: I18n.t("停止代理时一并停止后端", I18n.lang)
                            color: Theme.text
                        }
                    }
                    Label {
                        width: parent.width; wrapMode: Text.Wrap
                        color: Theme.textMuted; font.pixelSize: 11
                        text: I18n.t("关闭此选项后，停止代理时后端服务器将继续运行（仅停止由本代理自动拉起的后端）。", I18n.lang)
                    }

                    // 代理崩溃自动重拉起开关
                    Row {
                        spacing: 10
                        Switch {
                            id: autoRestartSwitch
                            checked: proxyController.autoRestart
                            onCheckedChanged: if (checked !== proxyController.autoRestart) proxyController.autoRestart = checked
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: I18n.t("代理崩溃后自动重拉起", I18n.lang)
                            color: Theme.text
                        }
                    }
                    Label {
                        width: parent.width; wrapMode: Text.Wrap
                        color: Theme.textMuted; font.pixelSize: 11
                        text: I18n.t("代理异常退出时按指数退避（最多 5 次）自动重启；手动停止不触发。", I18n.lang)
                    }
                }
            }

            // ---- 后端映射表 ----
            Label {
                text: I18n.t("后端服务器", I18n.lang)
                font.pixelSize: 15; font.bold: true; color: Theme.text
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap
                visible: page.backends.length === 0
                color: Theme.textMuted
                text: I18n.t("暂无服务器，请先在“服务器总览”创建。", I18n.lang)
            }
            Rectangle {
                width: parent.width
                visible: page.backends.length > 0
                color: Theme.panel; radius: Theme.radius; border.color: Theme.border
                height: backendCol.implicitHeight + 20
                Column {
                    id: backendCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                    spacing: 2
                    Row {
                        spacing: 0; height: 26
                        Label { width: 220; text: I18n.t("名称", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Label { width: 160; text: I18n.t("地址", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Label { width: 100; text: I18n.t("类型", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Label { width: 120; text: I18n.t("转发模式", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                    }
                    Repeater {
                        model: page.backends
                        Row {
                            spacing: 0; height: 26
                            Label { width: 220; elide: Text.ElideRight; text: modelData.name; color: Theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            Label { width: 160; text: modelData.host + ":" + modelData.port; color: Theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            Label { width: 100; text: modelData.type; color: Theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                            Label { width: 120; text: modelData.forwarding; color: Theme.textMuted; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
                        }
                    }
                }
            }

            // ---- 操作 ----
            Row {
                spacing: 10
                Button {
                    text: I18n.t("同步配置", I18n.lang)
                    enabled: !proxyController.running
                    palette.windowText: Theme.text; palette.buttonText: Theme.text
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: { proxyController.syncConfig(); page.refreshBackends() }
                }
                Button {
                    text: I18n.t("启动代理", I18n.lang)
                    enabled: proxyController.installed && !proxyController.running && !proxyController.busy
                    palette.windowText: "white"; palette.buttonText: "white"
                    background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.accentHover : Theme.accent) : Theme.panelAlt; radius: 6 }
                    onClicked: { proxyController.start(); page.refreshBackends() }
                }
                Button {
                    text: I18n.t("停止代理", I18n.lang)
                    enabled: proxyController.running
                    palette.windowText: Theme.text; palette.buttonText: Theme.text
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: proxyController.stop()
                }
                Button {
                    text: I18n.t("打开代理目录", I18n.lang)
                    palette.windowText: Theme.text; palette.buttonText: Theme.text
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: Qt.openUrlExternally("file:///" + proxyController.proxyDir)
                }
            }

            // ---- 控制台 ----
            Label {
                text: I18n.t("代理控制台", I18n.lang)
                font.pixelSize: 15; font.bold: true; color: Theme.text
            }
            Rectangle {
                width: parent.width; height: 240
                color: Theme.bg; radius: Theme.radius; border.color: Theme.border
                ScrollView {
                    id: consoleScroll
                    anchors.fill: parent; anchors.margins: 8
                    TextArea {
                        id: consoleArea
                        readOnly: true
                        wrapMode: TextArea.Wrap
                        color: Theme.text
                        font.family: "Consolas"; font.pixelSize: 12
                        background: null
                        text: proxyController.getConsole()
                    }
                }
                Connections {
                    target: proxyController
                    function onConsoleAppended(line) {
                        consoleArea.append(line)
                        // 滚动到底部
                        consoleArea.cursorPosition = consoleArea.length
                    }
                }
            }
        }
    }
}
