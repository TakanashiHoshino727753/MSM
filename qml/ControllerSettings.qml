// ControllerSettings.qml —— 应用设置窗口
// 职责：外观（深浅色 / 主题色）、语言与开机自启、WebUI 与机器人开关及端口、
// 默认服务器目录。修改即时经 settingsController / appController 持久化。
// 无边框窗口，圆角跟随 Theme；最大化时圆角归零。
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MinecraftServerManager

ApplicationWindow {
    id: window
    width: 900; height: 620
    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"

    function applyTheme(dark, accent) { appController.setTheme(dark, accent) }

    Component.onCompleted: {
        for (let i = 0; i < langBox.model.length; ++i)
            if (langBox.model[i] === settingsController.language) { langBox.currentIndex = i; break }
    }

    // 背景：纯色 + 轻微主色染色，避免依赖外部图片资源
    // 注意：clip 只按矩形裁剪，铺满的子 Rectangle 必须自带圆角，否则会盖掉窗口圆角
    Rectangle {
        id: frame
        anchors.fill: parent
        radius: window.visibility === Window.Maximized ? 0 : Theme.radius
        color: Theme.bg
        clip: true

        Rectangle {
            anchors.fill: parent
            radius: frame.radius
            color: Theme.accent
            opacity: 0.05
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            TitleBar {
                id: titleBar
                window: window
                title: I18n.t("控制器设置", I18n.lang)
                showDownloads: false
                Layout.fillWidth: true
            }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // 左侧设置导航（与侧边栏同色系，弱化分类说明）
            Rectangle {
                Layout.preferredWidth: 200
                Layout.fillHeight: true
                color: Theme.panel
                // 左下角跟随窗口圆角（frame 的 clip 不会裁剪子项的圆角，需在此单独设置）
                bottomLeftRadius: window.visibility === Window.Maximized ? 0 : Theme.radius
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 6
                    Label { text: I18n.t("外观", I18n.lang); color: Theme.textMuted; font.bold: true }
                    Label { text: I18n.t("语言与自启", I18n.lang); color: Theme.textMuted; font.bold: true }
                    Label { text: I18n.t("WebUI 与机器人", I18n.lang); color: Theme.textMuted; font.bold: true }
                    Item { Layout.fillHeight: true }
                    Label { text: "Minecraft Server Manager"; color: Theme.textMuted; font.pixelSize: 11 }
                }
            }

            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.border }

            Flickable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: col.height + 32
                clip: true
                ScrollBar.vertical: ScrollBar {}
                ColumnLayout {
                    id: col
                    x: 16
                    y: 16
                    width: parent.width - 32
                    spacing: 14

                    // 外观
                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radius
                        color: Theme.panel
                        implicitHeight: cardA.height + 32
                        ColumnLayout {
                            id: cardA
                            x: 16; y: 16
                            width: parent.width - 32
                            spacing: 12
                            Label { text: I18n.t("外观", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 15 }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("深色模式", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                Switch {
                                    id: darkToggle
                                    checked: Theme.dark
                                    onToggled: window.applyTheme(darkToggle.checked, Theme.accent)
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("主题色", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                RowLayout { spacing: 8
                                    Repeater {
                                        model: [
                                            "#4f8cff", "#8a5cf6", "#ec4899", "#22c55e",
                                            "#f59e0b", "#ef4444", "#06b6d4", "#14b8a6",
                                            "#a855f7", "#eab308", "#3b82f6", "#64748b"
                                        ]
                                        Rectangle {
                                            width: 26; height: 26; radius: 13
                                            color: modelData
                                            border.width: Theme.accent === modelData ? 3 : 1
                                            border.color: Theme.accent === modelData ? Theme.text : "#00000022"
                                            MouseArea {
                                                anchors.fill: parent
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: window.applyTheme(Theme.dark, modelData)
                                            }
                                        }
                                    }
                                    // 自定义任意颜色
                                    Rectangle {
                                        width: 26; height: 26; radius: 13
                                        color: "transparent"
                                        border.width: 1
                                        border.color: Theme.border
                                        Text {
                                            anchors.centerIn: parent
                                            text: "＋"
                                            color: Theme.textMuted
                                            font.pixelSize: 16
                                        }
                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: colorDialog.open()
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // 语言与自启
                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radius
                        color: Theme.panel
                        implicitHeight: cardB.height + 32
                        ColumnLayout {
                            id: cardB
                            x: 16; y: 16
                            width: parent.width - 32
                            spacing: 12
                            Label { text: I18n.t("语言与自启", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 15 }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("界面语言", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                ComboBoxEx {
                                    id: langBox
                                    Layout.fillWidth: false
                                    model: ["简体中文", "English"]
                                    onActivated: { settingsController.language = currentText; settingsController.apply() }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("开机自启", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                Switch {
                                    checked: settingsController.autoStart
                                    onToggled: { settingsController.autoStart = checked; settingsController.apply() }
                                }
                            }
                        }
                    }

                    // 默认服务器目录
                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radius
                        color: Theme.panel
                        implicitHeight: cardD.height + 32
                        ColumnLayout {
                            id: cardD
                            x: 16; y: 16
                            width: parent.width - 32
                            spacing: 12
                            Label { text: I18n.t("默认服务器目录", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 15 }
                            RowLayout {
                                Layout.fillWidth: true
                                TextField {
                                    id: dirField
                                    text: settingsController.defaultServerDir
                                    Layout.fillWidth: true
                                    readOnly: true
                                    color: Theme.text
                                    background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                                }
                                Button {
                                    text: I18n.t("浏览", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: dirDialog.open()
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                FolderDialog {
                    id: dirDialog
                    currentFolder: "file:///" + settingsController.defaultServerDir
                    onAccepted: {
                        // selectedFolder 是 file:/// URL，交给 C++ setter 统一去前缀
                        settingsController.defaultServerDir = selectedFolder.toString()
                        dirField.text = settingsController.defaultServerDir
                    }
                }
                            }
                        }
                    }

                    // WebUI 与机器人
                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radius
                        color: Theme.panel
                        implicitHeight: cardC.height + 32
                        ColumnLayout {
                            id: cardC
                            x: 16; y: 16
                            width: parent.width - 32
                            spacing: 12
                            Label { text: I18n.t("WebUI 与机器人", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 15 }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("启用 WebUI", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                Switch {
                                    checked: settingsController.webuiEnabled
                                    onToggled: { settingsController.webuiEnabled = checked; settingsController.apply() }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("WebUI 端口", I18n.lang); color: Theme.text }
                            Item {
                                id: portSpin
                                Layout.preferredHeight: 30
                                implicitWidth: 96
                                // 大整体：四角圆角
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 6
                                    color: Theme.panelAlt
                                    border.color: Theme.border
                                    clip: true
                                    RowLayout {
                                        anchors.fill: parent
                                        spacing: 0
                                        // 数字显示 / 输入
                                        TextInput {
                                            Layout.fillWidth: true
                                            Layout.fillHeight: true
                                            Layout.preferredWidth: 14
                                            leftPadding: 10
                                            text: settingsController.webuiPort
                                            color: Theme.text
                                            verticalAlignment: Text.AlignVCenter
                                            horizontalAlignment: Qt.AlignLeft
                                            selectByMouse: true
                                            validator: IntValidator { bottom: 1; top: 65535 }
                                            onEditingFinished: {
                                                var v = parseInt(text, 10)
                                                if (!isNaN(v)) {
                                                    v = Math.min(65535, Math.max(1, v))
                                                    settingsController.webuiPort = v
                                                    settingsController.apply()
                                                }
                                            }
                                        }
                                        // 数字区与按键组之间的分隔线
                                        Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Theme.border }
                                        // 按键组
                                        ColumnLayout {
                                            spacing: 0
                                            Layout.fillHeight: true
                                            Layout.fillWidth: true
                                            Layout.preferredWidth: 1
                                            Layout.minimumWidth: 22
                                            Layout.maximumWidth: 28
                                            Button {
                                                Layout.fillWidth: true; Layout.fillHeight: true
                                                flat: true
                                                background: Rectangle { color: parent.hovered ? Theme.accentSoft : "transparent" }
                                                onClicked: stepPort(1)
                                                contentItem: Text { text: "+"; color: Theme.text; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            }
                                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
                                            Button {
                                                Layout.fillWidth: true; Layout.fillHeight: true
                                                flat: true
                                                background: Rectangle { color: parent.hovered ? Theme.accentSoft : "transparent" }
                                                onClicked: stepPort(-1)
                                                contentItem: Text { text: "−"; color: Theme.text; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            }
                                        }
                                    }
                                }
                                function stepPort(d) {
                                    var v = Math.min(65535, Math.max(1, settingsController.webuiPort + d))
                                    settingsController.webuiPort = v
                                    settingsController.apply()
                                }
                            }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label { text: I18n.t("运行状态", I18n.lang); color: Theme.textMuted }
                                Label {
                                    Layout.fillWidth: true
                                    text: webuiServer.running
                                          ? I18n.t("运行中 · http://localhost:%1", I18n.lang).arg(webuiServer.port)
                                          : (settingsController.webuiEnabled
                                             ? I18n.t("启动失败：%1", I18n.lang).arg(webuiServer.error)
                                             : I18n.t("未启用", I18n.lang))
                                    color: webuiServer.running ? Theme.accent : Theme.textMuted
                                    elide: Text.ElideRight
                                }
                                Button {
                                    text: I18n.t("打开", I18n.lang)
                                    enabled: webuiServer.running
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: Qt.openUrlExternally("http://localhost:" + webuiServer.port)
                                    background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.panelAlt : Theme.bg) : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                            }
                            Label { text: I18n.t("启用后 WebUI 会在本地端口启动一个管理面板，可通过浏览器访问并启停服务器。", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("启用机器人插件", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                Switch {
                                    checked: settingsController.botEnabled
                                    onToggled: { settingsController.botEnabled = checked; settingsController.apply() }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            text: I18n.t("打开下载中心", I18n.lang)
                            palette.buttonText: "white"; palette.windowText: "white"
                            onClicked: appController.openDownloadCenter()
                            background: Rectangle { color: parent.hovered ? Theme.accentHover : Theme.accent; radius: 6 }
                        }
                        Button {
                            text: I18n.t("导入整合包", I18n.lang)
                            palette.buttonText: Theme.text; palette.windowText: Theme.text
                            onClicked: appController.openImportModpack()
                            background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                        }
                        Item { Layout.fillWidth: true }
                    }
                }
            }
        }
        }
    }

    ColorDialog {
        id: colorDialog
        title: I18n.t("选择主题色", I18n.lang)
        onAccepted: window.applyTheme(Theme.dark, colorDialog.selectedColor)
    }
}
