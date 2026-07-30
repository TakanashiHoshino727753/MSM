// ProxyPage.qml —— Velocity 反向代理聚合页（P2 多实例 + A3 UPnP 公网暴露）
// 多台同时运行的服务器通过一个统一入口端口对外服务：
// 玩家连接代理端口后，游戏内用 /server <名称> 在各后端之间切换。
// 顶部实例页签切换多个代理实例（各自独立端口/后端筛选，共享 velocity.jar）；
// 页面底部提供 UPnP 端口映射（实验性）把代理端口暴露到公网。
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager

Item {
    id: page

    // 当前选中的代理实例（索引 0 恒为默认实例）
    property int currentIndex: 0
    property var proxy: proxyManager.proxyAt(currentIndex)

    // 页签切到本页/切换实例时刷新后端映射表
    property var backends: []
    function refreshBackends() { backends = page.proxy ? page.proxy.backendSummary() : [] }
    Component.onCompleted: refreshBackends()
    onVisibleChanged: if (visible) refreshBackends()
    onProxyChanged: {
        refreshBackends()
        consoleArea.text = page.proxy ? page.proxy.getConsole() : ""
    }

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
                    color: page.proxy && page.proxy.running ? "#2e7d32" : Theme.panel
                    border.color: Theme.border
                    Label {
                        id: statusText
                        anchors.centerIn: parent
                        font.pixelSize: 11
                        color: page.proxy && page.proxy.running ? "white" : Theme.textMuted
                        text: page.proxy ? page.proxy.status : ""
                    }
                }
                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: page.proxy && page.proxy.running
                    color: Theme.textMuted; font.pixelSize: 12
                    text: I18n.t("在线人数", I18n.lang) + ": " + (page.proxy ? page.proxy.playerCount : 0)
                }
            }
            Label {
                width: parent.width; wrapMode: Text.Wrap
                color: Theme.textMuted; font.pixelSize: 12
                text: I18n.t("将多台同时运行的服务器聚合到一个入口端口：玩家统一连接代理端口，游戏内用 /server 名称 切换服务器。启动代理前会自动生成配置并修补各后端（online-mode=false）。", I18n.lang)
            }

            // ---- P2 实例页签 ----
            Flow {
                width: parent.width; spacing: 8
                Repeater {
                    model: proxyManager.proxies
                    Rectangle {
                        radius: 6; height: 30
                        width: tabRow.implicitWidth + 20
                        color: index === page.currentIndex ? Theme.accent : Theme.panel
                        border.color: index === page.currentIndex ? Theme.accent : Theme.border
                        Row {
                            id: tabRow
                            anchors.centerIn: parent; spacing: 6
                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 8; height: 8; radius: 4
                                color: modelData.running ? "#43d17a" : Theme.border
                            }
                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                text: modelData.name + " :" + modelData.port
                                      + (modelData.running ? " · " + modelData.players : "")
                                font.pixelSize: 12
                                color: index === page.currentIndex ? "white" : Theme.text
                            }
                        }
                        MouseArea { anchors.fill: parent; onClicked: page.currentIndex = index }
                    }
                }
                Button {
                    height: 30
                    text: "+ " + I18n.t("新建代理", I18n.lang)
                    palette.windowText: Theme.text; palette.buttonText: Theme.text
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: page.currentIndex = proxyManager.addProxy("")
                }
                Button {
                    height: 30
                    visible: page.currentIndex > 0
                    enabled: page.proxy && !page.proxy.running
                    text: I18n.t("删除实例", I18n.lang)
                    palette.windowText: "#e05f5f"; palette.buttonText: "#e05f5f"
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: {
                        var idx = page.currentIndex
                        page.currentIndex = 0
                        proxyManager.removeProxy(idx)
                    }
                }
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
                            text: page.proxy && page.proxy.installed ? I18n.t("更新 Velocity", I18n.lang)
                                                                     : I18n.t("安装 Velocity", I18n.lang)
                            enabled: page.proxy && !page.proxy.busy && !page.proxy.running
                            palette.windowText: "white"; palette.buttonText: "white"
                            background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.accentHover : Theme.accent) : Theme.panelAlt; radius: 6 }
                            onClicked: page.proxy.install()
                        }
                        ProgressBar {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 160
                            from: 0; to: 100
                            value: page.proxy ? page.proxy.installProgress : 0
                            visible: page.proxy && page.proxy.busy
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.textMuted; font.pixelSize: 12
                            text: page.proxy && page.proxy.installed ? I18n.t("已安装", I18n.lang) : I18n.t("未安装", I18n.lang)
                        }
                    }
                    Row {
                        spacing: 10
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: I18n.t("实例名称", I18n.lang); color: Theme.text
                        }
                        TextField {
                            id: nameField
                            width: 140
                            text: page.proxy ? page.proxy.name : ""
                            color: Theme.text
                            background: Rectangle { color: Theme.bg; radius: 6; border.color: nameField.activeFocus ? Theme.accent : Theme.border }
                            onEditingFinished: if (page.proxy) page.proxy.name = text
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: I18n.t("入口端口", I18n.lang); color: Theme.text
                        }
                        TextField {
                            id: portField
                            width: 90
                            enabled: page.proxy && !page.proxy.running
                            text: page.proxy ? String(page.proxy.proxyPort) : ""
                            validator: IntValidator { bottom: 1; top: 65535 }
                            color: Theme.text
                            background: Rectangle { color: Theme.bg; radius: 6; border.color: portField.activeFocus ? Theme.accent : Theme.border }
                            onEditingFinished: if (page.proxy) page.proxy.proxyPort = parseInt(text)
                        }
                        Label {
                            anchors.verticalCenter: parent.verticalCenter
                            text: "MOTD"; color: Theme.text
                        }
                        TextField {
                            id: motdField
                            width: 220
                            enabled: page.proxy && !page.proxy.running
                            text: page.proxy ? page.proxy.motd : ""
                            color: Theme.text
                            background: Rectangle { color: Theme.bg; radius: 6; border.color: motdField.activeFocus ? Theme.accent : Theme.border }
                            onEditingFinished: if (page.proxy) page.proxy.motd = text
                        }
                    }

                    // 离线 / 非正版玩家开关
                    Row {
                        spacing: 10
                        Switch {
                            id: offlineSwitch
                            checked: page.proxy ? page.proxy.offlineMode : true
                            enabled: page.proxy && !page.proxy.running
                            onCheckedChanged: if (page.proxy && checked !== page.proxy.offlineMode) page.proxy.offlineMode = checked
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
                            checked: page.proxy ? page.proxy.stopBackendsWithProxy : true
                            onCheckedChanged: if (page.proxy && checked !== page.proxy.stopBackendsWithProxy) page.proxy.stopBackendsWithProxy = checked
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
                            checked: page.proxy ? page.proxy.autoRestart : true
                            onCheckedChanged: if (page.proxy && checked !== page.proxy.autoRestart) page.proxy.autoRestart = checked
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

            // ---- 后端映射表（P2：勾选纳入本实例的后端）----
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
                        Label { width: 60;  text: I18n.t("聚合", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Label { width: 200; text: I18n.t("名称", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Label { width: 160; text: I18n.t("地址", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Label { width: 100; text: I18n.t("类型", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                        Label { width: 120; text: I18n.t("转发模式", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; font.bold: true }
                    }
                    Repeater {
                        model: page.backends
                        Row {
                            spacing: 0; height: 28
                            CheckBox {
                                width: 60; height: 26
                                anchors.verticalCenter: parent.verticalCenter
                                checked: modelData.enabled
                                enabled: page.proxy && !page.proxy.running
                                onToggled: {
                                    page.proxy.setServerEnabled(modelData.name, checked)
                                    page.refreshBackends()
                                }
                            }
                            Label { width: 200; elide: Text.ElideRight; text: modelData.name; color: Theme.text; font.pixelSize: 12; anchors.verticalCenter: parent.verticalCenter }
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
                    enabled: page.proxy && !page.proxy.running
                    palette.windowText: Theme.text; palette.buttonText: Theme.text
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: { page.proxy.syncConfig(); page.refreshBackends() }
                }
                Button {
                    text: I18n.t("启动代理", I18n.lang)
                    enabled: page.proxy && page.proxy.installed && !page.proxy.running && !page.proxy.busy
                    palette.windowText: "white"; palette.buttonText: "white"
                    background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.accentHover : Theme.accent) : Theme.panelAlt; radius: 6 }
                    onClicked: { page.proxy.start(); page.refreshBackends() }
                }
                Button {
                    text: I18n.t("停止代理", I18n.lang)
                    enabled: page.proxy && page.proxy.running
                    palette.windowText: Theme.text; palette.buttonText: Theme.text
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: page.proxy.stop()
                }
                Button {
                    text: I18n.t("打开代理目录", I18n.lang)
                    palette.windowText: Theme.text; palette.buttonText: Theme.text
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: Qt.openUrlExternally("file:///" + (page.proxy ? page.proxy.proxyDir : ""))
                }
            }

            // ---- A3 公网暴露（UPnP，实验性）----
            Label {
                text: I18n.t("公网暴露（UPnP · 实验性）", I18n.lang)
                font.pixelSize: 15; font.bold: true; color: Theme.text
            }
            Rectangle {
                width: parent.width
                color: Theme.panel; radius: Theme.radius; border.color: Theme.border
                height: upnpCol.implicitHeight + 28
                Column {
                    id: upnpCol
                    anchors { left: parent.left; right: parent.right; top: parent.top; margins: 14 }
                    spacing: 10
                    Label {
                        width: parent.width; wrapMode: Text.Wrap
                        color: Theme.textMuted; font.pixelSize: 11
                        text: I18n.t("通过路由器 UPnP 把代理端口映射到公网（需路由器开启 UPnP）。若运营商为你分配的是私网地址（CG-NAT），映射成功也可能无法从公网直连，此时建议使用内网穿透工具（frp/ngrok 等）。", I18n.lang)
                    }
                    Row {
                        spacing: 10
                        Button {
                            text: I18n.t("发现网关", I18n.lang)
                            enabled: !portMapper.busy
                            palette.windowText: "white"; palette.buttonText: "white"
                            background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.accentHover : Theme.accent) : Theme.panelAlt; radius: 6 }
                            onClicked: portMapper.discover()
                        }
                        Button {
                            text: I18n.t("映射代理端口", I18n.lang)
                            enabled: portMapper.available && !portMapper.busy && page.proxy
                            palette.windowText: Theme.text; palette.buttonText: Theme.text
                            background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                            onClicked: portMapper.addMapping(page.proxy.proxyPort, page.proxy.proxyPort, "TCP",
                                                             "MSM-" + page.proxy.name)
                        }
                        BusyIndicator {
                            anchors.verticalCenter: parent.verticalCenter
                            width: 22; height: 22
                            running: portMapper.busy
                            visible: portMapper.busy
                        }
                    }
                    Label {
                        width: parent.width; wrapMode: Text.Wrap
                        color: Theme.textMuted; font.pixelSize: 12
                        text: portMapper.status
                    }
                    Row {
                        spacing: 20
                        visible: portMapper.available
                        Label {
                            color: Theme.text; font.pixelSize: 12
                            text: I18n.t("网关", I18n.lang) + ": " + portMapper.gatewayName
                        }
                        Label {
                            color: Theme.text; font.pixelSize: 12
                            text: I18n.t("本机 IP", I18n.lang) + ": " + portMapper.lanIp
                        }
                        Label {
                            color: Theme.text; font.pixelSize: 12
                            visible: portMapper.externalIp.length > 0
                            text: I18n.t("外部 IP", I18n.lang) + ": " + portMapper.externalIp
                        }
                    }
                    // 已添加的映射
                    Repeater {
                        model: portMapper.mappings
                        Row {
                            spacing: 10; height: 28
                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                color: Theme.text; font.pixelSize: 12
                                text: (portMapper.externalIp.length > 0 ? portMapper.externalIp : I18n.t("公网", I18n.lang))
                                      + ":" + modelData.externalPort + " → " + portMapper.lanIp + ":" + modelData.internalPort
                                      + " (" + modelData.protocol + ")"
                            }
                            Button {
                                height: 26
                                text: I18n.t("删除映射", I18n.lang)
                                palette.windowText: "#e05f5f"; palette.buttonText: "#e05f5f"
                                background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                                onClicked: portMapper.removeMapping(modelData.externalPort, modelData.protocol)
                            }
                        }
                    }
                    Label {
                        id: upnpResult
                        width: parent.width; wrapMode: Text.Wrap
                        visible: text.length > 0
                        font.pixelSize: 12
                        Connections {
                            target: portMapper
                            function onMappingResult(ok, message) {
                                upnpResult.color = ok ? "#43d17a" : "#e05f5f"
                                upnpResult.text = message
                            }
                        }
                    }
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
                        Component.onCompleted: text = page.proxy ? page.proxy.getConsole() : ""
                    }
                }
                Connections {
                    target: page.proxy
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
