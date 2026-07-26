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
                        // 左侧永远圆角；当没有服务器页签时（仅一个页面）右侧也圆角
                        topLeftRadius: Theme.radius; bottomLeftRadius: Theme.radius
                        topRightRadius: (serverManager ? serverManager.count : 0) === 0 ? Theme.radius : 0
                        bottomRightRadius: (serverManager ? serverManager.count : 0) === 0 ? Theme.radius : 0
                    }
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
                            // 最后一个服务器页签：右两圆角
                            topRightRadius: index === (serverManager ? serverManager.count - 1 : -1) ? Theme.radius : 0
                            bottomRightRadius: index === (serverManager ? serverManager.count - 1 : -1) ? Theme.radius : 0
                        }
                    }
                }
            }
            StackLayout {
                currentIndex: tabBar.currentIndex
                Layout.fillWidth: true
                Layout.fillHeight: true
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
                                    name: modelData.name; version: modelData.version
                                    onStartClicked: serverController.start(modelData.name, modelData.path)
                                    onStopClicked: serverController.stop(modelData.name)
                                    onForceStopClicked: serverController.forceStop(modelData.name)
                                    onDetailsClicked: function() { tabBar.currentIndex = index + 1 }
                                    // 属性：先切到对应服务器页签（使 ServerPage 可见），再直接打开其属性弹窗，
                                    // 避免仅跳转到详情页而看不到属性；openProps 同时加载属性并弹出 propsPopup。
                                    onPropertiesClicked: function() { tabBar.currentIndex = index + 1; const sp = serverPageRepeater.itemAt(index); if (sp && sp.openProps) sp.openProps() }
                                }
                            }
                        }
                    }
                }
                Repeater {
                    id: serverPageRepeater
                    model: serverManager ? serverManager.servers : null
                ServerPage { serverName: modelData.name; serverVersion: modelData.version; serverType: modelData.type; serverPath: modelData.path; serverIndex: index }
            }
        }
        }
    }
    }

    DownloadsPanel { id: downloadsPanel }
}

