// MainWindow.qml —— 应用主窗口（无边框）
// 职责：承载标题栏、设备占用侧边栏、总览/下载中心两个主视图，以及下载抽屉面板。
// 采用 FramelessWindowHint + 透明底色 + 圆角 frame 实现现代无边框外观；
// 最大化时圆角归零（由系统接管窗口）。由 C++ 上下文属性
// （serverManager / serverController / downloadsPanel / systemMonitor 等）驱动。
import QtQuick
import QtQuick.Controls
import MinecraftServerManager
import QtQuick.Window
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 1000; height: 680
    minimumWidth: 820; minimumHeight: 540
    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    property string closeMode: "hide"   // 点 X 仅收起托盘，由 C++ 控制

    // 当前主区域显示的页面 key（overview / proxy / server:<name>），用于同步 TabBar 高亮
    property string currentKey: "overview"

    // 根据 currentKey 同步 TabBar 高亮（不再用 TabBar.currentIndex 驱动页面）
    function syncTabHighlight() {
        if (currentKey === "overview") tabBar.currentIndex = 0
        else if (currentKey === "proxy") tabBar.currentIndex = (serverManager ? serverManager.count : 0) + 1
        else tabBar.currentIndex = -1   // 服务器详情页：高亮对应服务器页签
        // 服务器详情：遍历 serverTabs 找到同名页签并高亮
        if (currentKey.startsWith("server:")) {
            var name = currentKey.substring(7)
            for (var i = 0; i < serverTabs.count; i++) {
                if (serverTabs.itemAt(i).text === name) { tabBar.currentIndex = i + 1; break }
            }
        }
    }

    // 总览 / 代理 切换
    function selectPage(key) {
        if (key === currentKey) return
        currentKey = key
        if (key === "overview") stackView.replace(overviewComponent)
        else if (key === "proxy") stackView.replace(proxyComponent)
        syncTabHighlight()
    }

    // 切换到指定服务器详情页（动态按 name 注入属性）
    function selectServer(name) {
        if (!serverManager) return
        var list = serverManager.servers
        for (var i = 0; i < list.length; i++) {
            if (list[i].name === name) {
                currentKey = "server:" + name
                stackView.replace(serverPageComponent,
                    { serverName: list[i].name, serverVersion: list[i].version,
                      serverType: list[i].type, serverPath: list[i].path, serverIndex: i })
                syncTabHighlight()
                return
            }
        }
    }

    // 返回当前 StackView 中活动的 ServerPage 实例（供打开属性弹窗用）
    function currentServerPage() {
        var it = stackView.currentItem
        return (it && it.objectName === "ServerPageRoot") ? it : null
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: window.visibility === Window.Maximized ? 0 : Theme.radius
        color: Theme.bg
        border.width: 0
        clip: true

        TitleBar {
            id: titleBar
            window: window
            title: I18n.t("Minecraft Server Manager", I18n.lang)
            // 主界面和下载中心需要下载按钮；服务器详情页隐藏
            showDownloads: tabBar.currentIndex === 0
            // 切换页签时重新计算
            Connections {
                target: tabBar
                function onCurrentIndexChanged() { titleBar.showDownloads = tabBar.currentIndex === 0 }
            }
        anchors { top: parent.top; left: parent.left; right: parent.right }
        z: 3
        onDownloadsClicked: downloadsPanel.open()
    }

    // 边缘拖拽改变窗口大小（无边框窗口需要）
    MouseArea {
        height: 4; cursorShape: Qt.SizeVerCursor
        anchors { top: parent.top; left: parent.left; right: parent.right }
        onPressed: window.startSystemResize(Qt.TopEdge)
    }
    MouseArea {
        height: 4; cursorShape: Qt.SizeVerCursor; z: 4
        anchors { bottom: parent.bottom; left: parent.left; right: parent.right }
        onPressed: window.startSystemResize(Qt.BottomEdge)
    }
    MouseArea {
        width: 4; cursorShape: Qt.SizeHorCursor; z: 4
        anchors { top: parent.top; bottom: parent.bottom; left: parent.left }
        onPressed: window.startSystemResize(Qt.LeftEdge)
    }
    MouseArea {
        width: 4; cursorShape: Qt.SizeHorCursor; z: 4
        anchors { top: parent.top; bottom: parent.bottom; right: parent.right }
        onPressed: window.startSystemResize(Qt.RightEdge)
    }
    MouseArea {
        width: 8; height: 8; cursorShape: Qt.SizeFDiagCursor; z: 4
        anchors { top: parent.top; left: parent.left }
        onPressed: window.startSystemResize(Qt.TopEdge | Qt.LeftEdge)
    }
    MouseArea {
        width: 8; height: 8; cursorShape: Qt.SizeBDiagCursor; z: 4
        anchors { top: parent.top; right: parent.right }
        onPressed: window.startSystemResize(Qt.TopEdge | Qt.RightEdge)
    }
    MouseArea {
        width: 8; height: 8; cursorShape: Qt.SizeBDiagCursor; z: 4
        anchors { bottom: parent.bottom; left: parent.left }
        onPressed: window.startSystemResize(Qt.BottomEdge | Qt.LeftEdge)
    }
    MouseArea {
        width: 8; height: 8; cursorShape: Qt.SizeFDiagCursor; z: 4
        anchors { bottom: parent.bottom; right: parent.right }
        onPressed: window.startSystemResize(Qt.BottomEdge | Qt.RightEdge)
    }

    // 组合式布局：标题栏 + 侧边栏 + 主区域。侧边栏位于标题栏之下、独立成块（无边框）
    SideBar {
        id: sideBar
        anchors { top: titleBar.bottom; bottom: frame.bottom; left: frame.left }
        z: 0
        onDownloadClicked: appController.openDownloadCenter()
        onSettingsClicked: appController.openControllerSettings()
    }

    // 主区域：锚到侧边栏右缘，仅左侧有一条分隔线，其余无边框
    Item {
        anchors { top: titleBar.bottom; left: sideBar.right; right: parent.right; bottom: parent.bottom }
        // 内容区整体底色（页签条 + 下方区域）明显跟随主色调，与标题栏/侧边栏同色系
        Rectangle {
            anchors.fill: parent; color: Theme.accentSoft
            // 右下角跟随窗口圆角（frame 的 clip 不会裁剪子项的圆角，需在此单独设置）
            bottomRightRadius: window.visibility === Window.Maximized ? 0 : Theme.radius
        }
        Rectangle {
            width: 1; anchors.left: parent.left; color: Theme.border
            anchors { top: parent.top; bottom: parent.bottom }
        }

        ColumnLayout {
            // 内部整体留出左右内边距，避开左侧分隔线（否则页签与内容贴着左边线很难看）
            anchors { top: parent.top; bottom: parent.bottom; left: parent.left; right: parent.right; leftMargin: 14; rightMargin: 14 }
            TabBar {
                id: tabBar
                spacing: 0                              // 去掉按键间距
                // 整条页签条用淡主色染色（圆角与按键外圆角对齐）
                background: Rectangle { color: Theme.accentSoft; radius: Theme.radius }
                // 服务器总览那一组：选中态底色用主色调，文字跟随深浅色（Theme.text）
                TabButton {
                    id: bOverview
                    text: I18n.t("服务器总览", I18n.lang)
                    palette.windowText: Theme.text; palette.buttonText: Theme.text; palette.brightText: Theme.text
                    background: Rectangle {
                        color: bOverview.checked ? Theme.accent : Theme.accentSoft
                        // 左侧永远圆角（右侧由最后的“代理聚合”页签收尾）
                        topLeftRadius: Theme.radius; bottomLeftRadius: Theme.radius
                    }
                    onClicked: window.selectPage("overview")
                }
                Repeater {
                    id: serverTabs
                    model: serverManager ? serverManager.servers : null
                    TabButton {
                        id: bServer
                        text: modelData.name
                        palette.windowText: Theme.text; palette.buttonText: Theme.text; palette.brightText: Theme.text
                        background: Rectangle {
                            color: bServer.checked ? Theme.accent : Theme.accentSoft
                        }
                        onClicked: window.selectServer(modelData.name)
                    }
                }
                // 代理聚合页签：永远位于最后，右两圆角收尾
                TabButton {
                    id: bProxy
                    text: I18n.t("代理聚合", I18n.lang)
                    palette.windowText: Theme.text; palette.buttonText: Theme.text; palette.brightText: Theme.text
                    background: Rectangle {
                        color: bProxy.checked ? Theme.accent : Theme.accentSoft
                        topRightRadius: Theme.radius; bottomRightRadius: Theme.radius
                    }
                    onClicked: window.selectPage("proxy")
                }
            }
            StackView {
                id: stackView
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                // 自定义左右滑动 + 淡入过渡（替代 StackLayout 瞬时切换）
                popEnter: Transition { XAnimator { from: (stackView.width); to: 0; duration: 220; easing.type: Easing.OutCubic } }
                popExit:  Transition { XAnimator { from: 0; to: (-stackView.width); duration: 220; easing.type: Easing.OutCubic } }
                pushEnter: Transition { XAnimator { from: (-stackView.width); to: 0; duration: 220; easing.type: Easing.OutCubic } }
                pushExit:  Transition { XAnimator { from: 0; to: (stackView.width); duration: 220; easing.type: Easing.OutCubic } }
                // 兼容 replace：用透明淡入保证切换有过渡
                replaceEnter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 } }
                replaceExit:  Transition { NumberAnimation { property: "opacity"; from: 1; to: 0; duration: 200 } }

                initialItem: overviewComponent

                // ---- 服务器总览页（Component，供 StackView 实例化）----
                Component {
                    id: overviewComponent
                    ScrollView {
                        contentWidth: width
                        Column {
                            anchors.left: parent.left; anchors.right: parent.right
                            anchors.leftMargin: 16; anchors.rightMargin: 16
                            spacing: 14; topPadding: 16; bottomPadding: 16
                            Row {
                                spacing: 8; width: parent.width
                                Label { text: I18n.t("服务器总览", I18n.lang); font.pixelSize: 18; color: Theme.text; font.bold: true; Layout.fillWidth: true }
                                Button { text: I18n.t("创建新的服务器", I18n.lang); palette.windowText: "white"; palette.buttonText: "white"; onClicked: appController.openCreateServer(); background: Rectangle { color: parent.hovered ? Theme.accentHover : Theme.accent; radius: 6 } }
                                Button { text: I18n.t("导入整合包", I18n.lang); palette.windowText: Theme.text; palette.buttonText: Theme.text; onClicked: appController.openImportModpack(); background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border } }
                                Button { text: I18n.t("刷新服务器", I18n.lang); icon.source: "qrc:/icon/redownload"; icon.color: Theme.text; icon.width: 14; icon.height: 14; palette.windowText: Theme.text; palette.buttonText: Theme.text; onClicked: serverManager.scanServers(); background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border } }
                            }
                            Label {
                                text: I18n.t("暂无服务器，点击右上角“创建新的服务器”或“导入整合包”。", I18n.lang)
                                color: Theme.textMuted; wrapMode: Text.Wrap; width: parent.width
                                visible: (serverManager ? serverManager.count : 0) === 0
                            }
                            Flow {
                                width: parent.width; spacing: 12
                                Repeater {
                                    model: serverManager ? serverManager.servers : null
                                    ServerCard {
                                        name: modelData.name; version: modelData.version; path: modelData.path
                                        onStartClicked: serverController.start(modelData.name, modelData.path)
                                        onStopClicked: serverController.stop(modelData.path)
                                        onForceStopClicked: serverController.forceStop(modelData.path)
                                        onDetailsClicked: function() { window.selectServer(modelData.name) }
                                        // 属性：先切到对应服务器页（StackView 滑动），再打开其属性弹窗
                                        onPropertiesClicked: function() {
                                            window.selectServer(modelData.name)
                                            // 滑动动画启动后，下一帧再取页面实例打开属性弹窗
                                            Qt.callLater(function() {
                                                var sp = window.currentServerPage()
                                                if (sp && sp.openProps) sp.openProps()
                                            })
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // ---- 服务器详情页模板（按 name 动态 replace，属性由 selectServer 注入）----
                Component {
                    id: serverPageComponent
                    ServerPage { objectName: "ServerPageRoot" }
                }
                // ---- 代理聚合页 ----
                Component {
                    id: proxyComponent
                    ProxyPage { }
                }
            }
        }
        }
    }

    DownloadsPanel { id: downloadsPanel }

    // ---- 多开端口冲突：启动被取消时弹窗，允许一键自动分配空闲端口后重新启动 ----
    ModalPopup {
        id: portConflictPopup
        popupWidth: 480
        popupHeight: 200
        property string srvName: ""
        property string srvPath: ""
        property int    port: 0
        property string holder: ""
        contentItem: Column {
            spacing: 14
            padding: 20
            Label {
                text: I18n.t("端口冲突", I18n.lang)
                font.pixelSize: 17; font.bold: true; color: Theme.text
            }
            Label {
                width: parent.width - 40
                wrapMode: Text.Wrap
                color: Theme.text
                text: (portConflictPopup.holder !== ""
                       ? I18n.t("端口 %1 已被服务器“%2”占用，无法同时启动。", I18n.lang)
                             .replace("%1", portConflictPopup.port).replace("%2", portConflictPopup.holder)
                       : I18n.t("端口 %1 已被其他程序占用，无法启动。", I18n.lang)
                             .replace("%1", portConflictPopup.port))
                      + " " + I18n.t("可自动分配一个空闲端口并立即启动。", I18n.lang)
            }
            Row {
                spacing: 10
                anchors.right: parent.right; anchors.rightMargin: 20
                Button {
                    text: I18n.t("自动分配端口并启动", I18n.lang)
                    palette.windowText: "white"; palette.buttonText: "white"
                    background: Rectangle { color: parent.hovered ? Theme.accentHover : Theme.accent; radius: 6 }
                    onClicked: {
                        var np = serverController.assignFreePort(portConflictPopup.srvPath)
                        portConflictPopup.close()
                        if (np > 0)
                            serverController.start(portConflictPopup.srvName, portConflictPopup.srvPath)
                    }
                }
                Button {
                    text: I18n.t("取消", I18n.lang)
                    palette.windowText: Theme.text; palette.buttonText: Theme.text
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: portConflictPopup.close()
                }
            }
        }
    }
    Connections {
        target: serverController
        function onPortConflict(name, path, port, holder) {
            portConflictPopup.srvName = name
            portConflictPopup.srvPath = path
            portConflictPopup.port = port
            portConflictPopup.holder = holder
            portConflictPopup.open()
        }
    }
    // 服务器列表变化时重新对齐 TabBar 高亮（TabBar 重建会把 currentIndex 重置为 0）
    Connections {
        target: serverManager
        function onServersChanged() { syncTabHighlight() }
    }
}

