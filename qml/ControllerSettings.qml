// ControllerSettings.qml —— 应用设置窗口
// 职责：外观（深浅色 / 主题色）、语言与开机自启、WebUI 开关及端口、
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

    // 设置分类导航：锚点机制。每个分类对应卡片在 flick 内容坐标中的真实 y（锚点）。
    // 当前分类 = 最后一个其锚点已越过视口顶（contentY）的分类；有锚点进出视口顶时更新。
    property string currentSection: "appearance"
    property bool _navLock: false   // 程序滚动动画期间锁定，避免高亮乱跳
    property real _navTarget: 0     // 程序滚动的目标位置，到达后解锁
    function navList() { return [cardA, cardB, cardD, cardC, cardQ, cardBack, cardO] }
    function navKeys() { return ["appearance", "language", "dir", "webui", "bot", "backup", "ops"] }
    // 分类卡片在 flick.contentItem 坐标系中的真实 y（锚点），与 flick.contentY 同一坐标系
    function anchorY(item) { return item.mapToItem(flick.contentItem, 0, 0).y }
    function sectionOf(item) {
        var list = navList(), keys = navKeys()
        for (var i = 0; i < list.length; ++i)
            if (list[i] === item) return keys[i]
        return ""
    }
    // 点击：立即锁定高亮，并滚动到该分类锚点（顶部对齐视口顶）。动画期间不重算防跳。
    function scrollTo(item) {
        if (!item) return
        currentSection = sectionOf(item)
        _navLock = true
        _navTarget = Math.max(0, Math.min(flick.contentHeight - flick.height, anchorY(item)))
        flick.contentY = _navTarget
    }
    // 手动滚动 / 动画结束：根据锚点穿过视口顶判定当前分类
    function updateCurrent() {
        if (_navLock) return                // 程序滚动动画期间不重算，防乱跳
        var list = navList(), keys = navKeys()
        var y = flick.contentY
        var cur = keys[0]
        // 视口内最上面的卡片（第一个底部仍在视口顶之下的卡片）即当前分类：
        // 某卡片顶部一进入主区域（出现）就高亮它，而非等其顶穿透视口顶才切
        for (var i = 0; i < list.length; ++i) {
            if (anchorY(list[i]) + list[i].height > y + 1) { cur = keys[i]; break }
        }
        currentSection = cur
    }

    Component.onCompleted: {
        for (let i = 0; i < langBox.model.length; ++i)
            if (langBox.model[i] === settingsController.language) { langBox.currentIndex = i; break }
        updateCurrent()   // 进入页面时按视口顶锚点初始化当前分类高亮
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
                        anchors.margins: 12
                        spacing: 4
                        Label { text: I18n.t("设置分类", I18n.lang); color: Theme.textMuted; font.bold: true; font.pixelSize: 11 }

                        Button {
                            Layout.fillWidth: true; height: 32
                            leftPadding: 12; rightPadding: 12
                            text: I18n.t("外观", I18n.lang)
                            onClicked: scrollTo(cardA)
                            background: Rectangle {
                                radius: 6
                                color: currentSection === "appearance" ? Theme.accentSoft : (parent.hovered ? Theme.panelAlt : "transparent")
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            contentItem: Label { text: parent.text; color: currentSection === "appearance" ? Theme.accent : Theme.text; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter }
                        }
                        Button {
                            Layout.fillWidth: true; height: 32
                            leftPadding: 12; rightPadding: 12
                            text: I18n.t("语言与自启", I18n.lang)
                            onClicked: scrollTo(cardB)
                            background: Rectangle {
                                radius: 6
                                color: currentSection === "language" ? Theme.accentSoft : (parent.hovered ? Theme.panelAlt : "transparent")
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            contentItem: Label { text: parent.text; color: currentSection === "language" ? Theme.accent : Theme.text; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter }
                        }
                        Button {
                            Layout.fillWidth: true; height: 32
                            leftPadding: 12; rightPadding: 12
                            text: I18n.t("默认服务器目录", I18n.lang)
                            onClicked: scrollTo(cardD)
                            background: Rectangle {
                                radius: 6
                                color: currentSection === "dir" ? Theme.accentSoft : (parent.hovered ? Theme.panelAlt : "transparent")
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            contentItem: Label { text: parent.text; color: currentSection === "dir" ? Theme.accent : Theme.text; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter }
                        }
                        Button {
                            Layout.fillWidth: true; height: 32
                            leftPadding: 12; rightPadding: 12
                            text: I18n.t("WebUI", I18n.lang)
                            onClicked: scrollTo(cardC)
                            background: Rectangle {
                                radius: 6
                                color: currentSection === "webui" ? Theme.accentSoft : (parent.hovered ? Theme.panelAlt : "transparent")
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            contentItem: Label { text: parent.text; color: currentSection === "webui" ? Theme.accent : Theme.text; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter }
                        }
                        Button {
                            Layout.fillWidth: true; height: 32
                            leftPadding: 12; rightPadding: 12
                            text: I18n.t("QQ 机器人", I18n.lang)
                            onClicked: scrollTo(cardQ)
                            background: Rectangle {
                                radius: 6
                                color: currentSection === "bot" ? Theme.accentSoft : (parent.hovered ? Theme.panelAlt : "transparent")
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            contentItem: Label { text: parent.text; color: currentSection === "bot" ? Theme.accent : Theme.text; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter }
                        }
                        Button {
                            Layout.fillWidth: true; height: 32
                            leftPadding: 12; rightPadding: 12
                            text: I18n.t("备份与定时", I18n.lang)
                            onClicked: scrollTo(cardBack)
                            background: Rectangle {
                                radius: 6
                                color: currentSection === "backup" ? Theme.accentSoft : (parent.hovered ? Theme.panelAlt : "transparent")
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            contentItem: Label { text: parent.text; color: currentSection === "backup" ? Theme.accent : Theme.text; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter }
                        }
                        Button {
                            Layout.fillWidth: true; height: 32
                            leftPadding: 12; rightPadding: 12
                            text: I18n.t("运维与通知", I18n.lang)
                            onClicked: scrollTo(cardO)
                            background: Rectangle {
                                radius: 6
                                color: currentSection === "ops" ? Theme.accentSoft : (parent.hovered ? Theme.panelAlt : "transparent")
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            contentItem: Label { text: parent.text; color: currentSection === "ops" ? Theme.accent : Theme.text; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter }
                        }

                        Item { Layout.fillHeight: true }
                        Label { text: "Minecraft Server Manager"; color: Theme.textMuted; font.pixelSize: 11 }
                    }
            }

            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: Theme.border }

            Flickable {
                id: flick
                Layout.fillWidth: true
                Layout.fillHeight: true
                contentWidth: width
                contentHeight: col.height + 32
                clip: true
                ScrollBar.vertical: ScrollBar {}
                onContentYChanged: {
                    if (_navLock) {
                        // 程序滚动动画期间保持锁定；到达目标位置（误差<2px）后才解锁
                        if (Math.abs(flick.contentY - _navTarget) < 2) _navLock = false
                        return
                    }
                    updateCurrent()
                }
                // 仅用户真实拖拽才解锁（Behavior 动画驱动的 moving 不触发 dragging）
                onDraggingChanged: if (flick.dragging) _navLock = false
                Behavior on contentY { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
                ColumnLayout {
                    id: col
                    x: 16
                    y: 16
                    width: parent.width - 32
                    spacing: 14
                    // 内容高度变化（语言切换换行等）后重算当前分类，避免锚点错位
                    onHeightChanged: if (!_navLock) updateCurrent()

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
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("启动时显示窗口", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                Switch {
                                    checked: settingsController.showOnStartup
                                    onToggled: { settingsController.showOnStartup = checked; settingsController.apply() }
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

                    // WebUI
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
                            Label { text: I18n.t("WebUI", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 15 }
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
                                          ? I18n.t("运行中 · %2://localhost:%1", I18n.lang).arg(webuiServer.port).arg(webuiServer.https ? "https" : "http")
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
                                    onClicked: Qt.openUrlExternally((webuiServer.https ? "https" : "http") + "://localhost:" + webuiServer.port + "/?token=" + encodeURIComponent(settingsController.webuiToken))
                                    background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.panelAlt : Theme.bg) : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                            }

                            // 访问令牌（所有接口必须携带，防止局域网内任意设备控制服务器）
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("访问令牌", I18n.lang); color: Theme.text }
                                TextField {
                                    id: tokenField
                                    Layout.fillWidth: true
                                    readOnly: true
                                    text: settingsController.webuiToken
                                    color: Theme.text
                                    font.family: "Consolas, Menlo, monospace"
                                    selectByMouse: true
                                    background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                                }
                                Button {
                                    text: I18n.t("复制", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: { tokenField.selectAll(); tokenField.copy(); toast(I18n.t("已复制访问令牌", I18n.lang)) }
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                                Button {
                                    text: I18n.t("重新生成", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: settingsController.regenerateWebuiToken()
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                            }

                            // 暴露到局域网（默认仅本机；开启后需令牌，且有被扫描风险）
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: I18n.t("暴露到局域网", I18n.lang)
                                    color: Theme.text
                                    wrapMode: Text.Wrap
                                }
                                Switch {
                                    checked: settingsController.webuiExposeLan
                                    onToggled: {
                                        settingsController.webuiExposeLan = checked
                                        settingsController.apply()
                                        if (webuiServer.running) webuiServer.rebind()
                                    }
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: I18n.t("默认仅本机(127.0.0.1)可访问，最为安全。开启“暴露到局域网”后将监听 0.0.0.0，局域网/手机可访问，但必须凭访问令牌，且存在被扫描的风险。", I18n.lang)
                                color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap
                            }

                            // 自定义证书（可选，留空则用自动生成的自签证书）
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("证书文件(可选)", I18n.lang); color: Theme.text }
                                TextField {
                                    id: certField
                                    Layout.fillWidth: true
                                    text: settingsController.webuiCertPath
                                    color: Theme.text
                                    placeholderText: I18n.t("留空=自动自签证书", I18n.lang)
                                    placeholderTextColor: Theme.textMuted
                                    selectByMouse: true
                                    selectionColor: Theme.accent
                                    background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                                    onEditingFinished: { settingsController.webuiCertPath = text; settingsController.apply() }
                                }
                                Button {
                                    text: I18n.t("浏览", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: certDialog.open()
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("私钥文件(可选)", I18n.lang); color: Theme.text }
                                TextField {
                                    id: keyField
                                    Layout.fillWidth: true
                                    text: settingsController.webuiKeyPath
                                    color: Theme.text
                                    placeholderText: I18n.t("留空=自动自签证书", I18n.lang)
                                    placeholderTextColor: Theme.textMuted
                                    selectByMouse: true
                                    selectionColor: Theme.accent
                                    background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                                    onEditingFinished: { settingsController.webuiKeyPath = text; settingsController.apply() }
                                }
                                Button {
                                    text: I18n.t("浏览", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: keyDialog.open()
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                            }

                            Label { text: I18n.t("启用后 WebUI 会以 HTTPS 启动管理面板（自签证书，浏览器会提示“不安全”，点继续即可），所有接口需携带访问令牌。令牌可在手机/其他设备首次打开时输入或随链接自动带入。", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }

                        }
                    }

                    // QQ 机器人（NapCat + NoneBot，独立于 WebUI）
                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radius
                        color: Theme.panel
                        implicitHeight: cardQ.height + 32
                        ColumnLayout {
                            id: cardQ
                            x: 16; y: 16
                            width: parent.width - 32
                            spacing: 12
                            Label { text: I18n.t("QQ 机器人", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 15 }
                            Label { text: I18n.t("NapCat 是 QQ 协议端（OneBot 客户端），NoneBot 是机器人框架（加载 msm_control 插件）。两者已绑定，一起开、一起关，与 WebUI 互不影响。", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("联动启动", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                Switch {
                                    checked: settingsController.botLinkedStart
                                    onToggled: { settingsController.botLinkedStart = checked; settingsController.apply() }
                                }
                            }
                            Label { text: I18n.t("开启时由 MSM 一并拉起 NapCat 与 NoneBot；关闭则 MSM 只开放控制通道，由你自行启动机器人并连入（默认关闭）。", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("启用 QQ 机器人", I18n.lang); Layout.fillWidth: true; color: Theme.text; font.bold: true }
                                Switch {
                                    checked: settingsController.botEnabled
                                    onToggled: { settingsController.botEnabled = checked; settingsController.apply() }
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("NapCat 路径", I18n.lang); color: Theme.text }
                                ComboBoxEditable {
                                    id: napcatCombo
                                    model: botController.detectNapcatPaths()
                                    onEditTextChanged: { if (editText !== settingsController.napcatPath) { settingsController.napcatPath = editText; settingsController.apply() } }
                                }
                                Button {
                                    text: I18n.t("自动检测", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: {
                                        var paths = botController.detectNapcatPaths()
                                        var p = botController.detectNapcatPath()
                                        napcatHint.text = ""
                                        if (p) {
                                            // 刷新候选列表，并直写编辑框文本（不依赖 currentIndex/绑定，保证一定填充）
                                            if (paths.indexOf(p) < 0) paths.push(p)
                                            napcatCombo.model = paths
                                            napcatCombo.setText(p)
                                            if (settingsController.napcatPath !== p) {
                                                settingsController.napcatPath = p
                                                settingsController.apply()
                                            }
                                        } else {
                                            napcatHint.text = I18n.t("未检测到 NapCat，请手动输入路径或浏览选择", I18n.lang)
                                        }
                                    }
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                                Button {
                                    text: I18n.t("浏览", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: napcatDialog.open()
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                                Component.onCompleted: {
                                    napcatCombo.setText(settingsController.napcatPath)
                                    nonebotCombo.setText(settingsController.nonebotDir)
                                }
                            }
                            Label {
                                id: napcatHint
                                text: ""
                                color: Theme.accent
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                                visible: text !== ""
                            }
                            Label { text: I18n.t("NapCat 需要扫码登录，启动后会弹出独立控制台窗口，扫码后 QQ 消息才能互通。", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label { text: I18n.t("运行状态", I18n.lang); color: Theme.textMuted }
                                Label {
                                    Layout.fillWidth: true
                                    text: {
                                        if (!settingsController.botEnabled) return I18n.t("未启用", I18n.lang)
                                        var s = botController.napcatState
                                        if (s === "running") return I18n.t("运行中", I18n.lang)
                                        if (s === "starting") return I18n.t("启动中", I18n.lang)
                                        if (s === "external") return I18n.t("外部运行（由你自行启动）", I18n.lang)
                                        if (s === "stopped") return I18n.t("已停止", I18n.lang)
                                        return s
                                    }
                                    color: botController.napcatState === "running" ? Theme.accent : Theme.textMuted
                                    elide: Text.ElideRight
                                }
                            }

                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("NoneBot 目录", I18n.lang); color: Theme.text }
                                ComboBoxEditable {
                                    id: nonebotCombo
                                    model: botController.detectNonebotDirs()
                                    onEditTextChanged: { if (editText !== settingsController.nonebotDir) { settingsController.nonebotDir = editText; settingsController.apply() } }
                                }
                                Button {
                                    text: I18n.t("自动检测", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: {
                                        var dirs = botController.detectNonebotDirs()
                                        var d = botController.detectNonebotDir()
                                        nonebotHint.text = ""
                                        if (d) {
                                            // 刷新候选列表，并直写编辑框文本（不依赖 currentIndex/绑定，保证一定填充）
                                            if (dirs.indexOf(d) < 0) dirs.push(d)
                                            nonebotCombo.model = dirs
                                            nonebotCombo.setText(d)
                                            if (settingsController.nonebotDir !== d) {
                                                settingsController.nonebotDir = d
                                                settingsController.apply()
                                            }
                                        } else {
                                            nonebotHint.text = I18n.t("未检测到 NoneBot，请手动输入目录或浏览选择", I18n.lang)
                                        }
                                    }
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                                Button {
                                    text: I18n.t("浏览", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: nonebotDialog.open()
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                            }
                            Label {
                                id: nonebotHint
                                text: ""
                                color: Theme.accent
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                                visible: text !== ""
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("msm 控制插件", I18n.lang); color: Theme.text }
                                Label {
                                    Layout.fillWidth: true
                                    text: {
                                        var st = botController.msmPluginState
                                        if (st === "ok") return I18n.t("已安装", I18n.lang)
                                        if (st === "missing") return I18n.t("未安装（机器人将无法控制服务器）", I18n.lang)
                                        if (st === "installing") return I18n.t("安装中…", I18n.lang)
                                        if (st === "error") return I18n.t("安装失败", I18n.lang)
                                        return I18n.t("未知", I18n.lang)
                                    }
                                    color: botController.msmPluginState === "ok" ? Theme.accent : (botController.msmPluginState === "error" ? "#e06c5a" : Theme.textMuted)
                                    elide: Text.ElideRight
                                }
                                Button {
                                    text: I18n.t("安装 / 重装", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: openMsmInstall()
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                            }
                            Label { text: I18n.t("MSM 默认不主动推送：常态按间隔把设备占用(CPU/内存)更新到机器人 QQ 昵称，服务器异常退出时把日志私信管理员，其余只在执行指令后回传反馈。两者绑定运行，一起开/一起关。", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("昵称状态更新间隔(秒)", I18n.lang); color: Theme.text }
                                TextField {
                                    text: settingsController.botUsageInterval
                                    Layout.preferredWidth: 90
                                    validator: IntValidator { bottom: 0; top: 86400 }
                                    color: Theme.text
                                    background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                                    onEditingFinished: { settingsController.botUsageInterval = Number(text); settingsController.apply() }
                                }
                                Label { text: I18n.t("0=关闭", I18n.lang); color: Theme.textMuted; font.pixelSize: 12 }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label { text: I18n.t("运行状态", I18n.lang); color: Theme.textMuted }
                                Label {
                                    Layout.fillWidth: true
                                    text: {
                                        if (!settingsController.botEnabled) return I18n.t("未启用", I18n.lang)
                                        var s = botController.nonebotState
                                        if (s === "running") return I18n.t("运行中", I18n.lang)
                                        if (s === "starting") return I18n.t("启动中", I18n.lang)
                                        if (s === "waiting") return I18n.t("等待连接", I18n.lang)
                                        if (s === "stopped") return I18n.t("已停止", I18n.lang)
                                        return s
                                    }
                                    color: botController.nonebotState === "running" ? Theme.accent : Theme.textMuted
                                    elide: Text.ElideRight
                                }
                                Button {
                                    text: I18n.t("测试推送", I18n.lang)
                                    enabled: botController.botEnabled
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: botController.notify(I18n.t("这是一条来自 MSM 的测试推送", I18n.lang))
                                    background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.panelAlt : Theme.bg) : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                            }

                            // ===== 控制通道（机器人链路）保护：锁定本机 + 与 WebUI 共用令牌 =====
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
                            Label { text: I18n.t("控制通道（机器人链路）", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 13 }
                            Label {
                                text: I18n.t("已锁定本机 127.0.0.1:%1，仅本机可连、远程不可达；本机其他进程也须凭访问令牌，MSM 已自动注入 NapCat / NoneBot 插件配置。", I18n.lang).arg(botController.controlPort)
                                color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label { text: I18n.t("控制通道状态", I18n.lang); color: Theme.textMuted }
                                Label { Layout.fillWidth: true; text: I18n.t("已锁定本机 + 令牌校验（最安全）", I18n.lang); color: Theme.accent; elide: Text.ElideRight }
                                Button {
                                    text: I18n.t("复制令牌", I18n.lang)
                                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                                    onClicked: { Clipboard.text = settingsController.webuiToken; toast(I18n.t("已复制访问令牌", I18n.lang)) }
                                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                                }
                            }
                        }
                    }

                    // 备份与定时
                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radius
                        color: Theme.panel
                        implicitHeight: cardBack.height + 32
                        ColumnLayout {
                            id: cardBack
                            x: 16; y: 16
                            width: parent.width - 32
                            spacing: 12
                            Label { text: I18n.t("备份与定时", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 15 }
                            Label { text: I18n.t("定时把服务端目录打包备份，按保留份数滚动删除最旧备份（备份文件位于 AppData 下的 MSM/backups）。", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("启用定时备份", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                Switch { checked: backupController.enabled; onToggled: backupController.enabled = checked }
                            }
                            RowLayout { spacing: 6
                                Label { text: I18n.t("备份间隔(小时)", I18n.lang); color: Theme.text }
                                SpinBox { from: 1; to: 168; value: backupController.intervalHours; onValueChanged: if (value !== backupController.intervalHours) backupController.intervalHours = value }
                                Label { text: I18n.t("保留份数", I18n.lang); color: Theme.text }
                                SpinBox { from: 1; to: 50; value: backupController.retain; onValueChanged: if (value !== backupController.retain) backupController.retain = value }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("启动时也备份一次", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                Switch { checked: backupController.onStart; onToggled: backupController.onStart = checked }
                            }
                            Label {
                                text: backupController.lastBackup > 0
                                      ? I18n.t("上次备份：%1", I18n.lang).arg(new Date(backupController.lastBackup).toLocaleString(Qt.locale(), Locale.ShortFormat))
                                      : I18n.t("尚无备份记录", I18n.lang)
                                color: Theme.textMuted; font.pixelSize: 12
                            }
                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
                            Label { text: I18n.t("定时启停 / 定时备份：在每台服务器的详情页点「定时任务」里按 启动/停止/备份 + 时间 配置（全局统一调度）。", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }
                        }
                    }

                    // 运维与通知
                    Rectangle {
                        Layout.fillWidth: true
                        radius: Theme.radius
                        color: Theme.panel
                        implicitHeight: cardO.height + 32
                        ColumnLayout {
                            id: cardO
                            x: 16; y: 16
                            width: parent.width - 32
                            spacing: 12
                            Label { text: I18n.t("运维与通知", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 15 }
                            Label { text: I18n.t("Webhook 通知：崩溃、启停、玩家进服时向群/频道推送。", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true }

                            RowLayout {
                                Layout.fillWidth: true
                                Label { text: I18n.t("启用 Webhook", I18n.lang); Layout.fillWidth: true; color: Theme.text }
                                Switch { checked: settingsController.webhookEnabled; onToggled: { settingsController.webhookEnabled = checked; settingsController.apply() } }
                                Label { text: I18n.t("类型", I18n.lang); color: Theme.text }
                                ComboBox {
                                    model: [I18n.t("Discord", I18n.lang), I18n.t("企业微信", I18n.lang), I18n.t("通用 JSON", I18n.lang)]
                                    Component.onCompleted: {
                                        var m = ["discord", "wecom", "generic"]
                                        currentIndex = Math.max(0, m.indexOf(settingsController.webhookType))
                                    }
                                    onCurrentIndexChanged: {
                                        var m = ["discord", "wecom", "generic"]
                                        var v = m[currentIndex]
                                        if (v !== settingsController.webhookType) { settingsController.webhookType = v; settingsController.apply() }
                                    }
                                }
                            }
                            TextField {
                                Layout.fillWidth: true
                                placeholderText: I18n.t("Webhook 地址（https://...）", I18n.lang)
                                text: settingsController.webhookUrl
                                color: Theme.text
                                background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                                onEditingFinished: { settingsController.webhookUrl = text; settingsController.apply() }
                            }
                            Label { text: I18n.t("通知事件", I18n.lang); color: Theme.text; font.pixelSize: 12 }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 12
                                RowLayout { spacing: 6
                                    Switch { checked: settingsController.webhookCrash; onToggled: { settingsController.webhookCrash = checked; settingsController.apply() } }
                                    Label { text: I18n.t("崩溃", I18n.lang); color: Theme.text }
                                }
                                RowLayout { spacing: 6
                                    Switch { checked: settingsController.webhookState; onToggled: { settingsController.webhookState = checked; settingsController.apply() } }
                                    Label { text: I18n.t("启停", I18n.lang); color: Theme.text }
                                }
                                RowLayout { spacing: 6
                                    Switch { checked: settingsController.webhookPlayer; onToggled: { settingsController.webhookPlayer = checked; settingsController.apply() } }
                                    Label { text: I18n.t("玩家进服", I18n.lang); color: Theme.text }
                                }
                            }

                            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }

                            Label { text: I18n.t("后端崩溃自动重启", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 13 }
                            RowLayout { Layout.fillWidth: true
                                Switch { checked: serverController.autoRestart; onToggled: { serverController.autoRestart = checked } }
                                Label { text: I18n.t("崩溃后自动重拉起后端（指数退避）", I18n.lang); color: Theme.text; wrapMode: Text.Wrap; Layout.fillWidth: true }
                            }
                            RowLayout { spacing: 6
                                Label { text: I18n.t("最大重试", I18n.lang); color: Theme.text }
                                SpinBox { from: 0; to: 50; value: serverController.maxRetries; onValueChanged: if (value !== serverController.maxRetries) serverController.maxRetries = value }
                                Label { text: I18n.t("退避基数(秒)", I18n.lang); color: Theme.text }
                                SpinBox { from: 1; to: 300; value: serverController.backoffSec; onValueChanged: if (value !== serverController.backoffSec) serverController.backoffSec = value }
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

    FileDialog {
        id: certDialog
        title: I18n.t("选择证书文件", I18n.lang)
        nameFilters: ["PEM/CRT (*.pem *.crt *.cer)", "All files (*)"]
        onAccepted: {
            settingsController.webuiCertPath = certDialog.selectedFile.toString()
            settingsController.apply()
            certField.text = settingsController.webuiCertPath
        }
    }
    FileDialog {
        id: keyDialog
        title: I18n.t("选择私钥文件", I18n.lang)
        nameFilters: ["Key (*.pem *.key)", "All files (*)"]
        onAccepted: {
            settingsController.webuiKeyPath = keyDialog.selectedFile.toString()
            settingsController.apply()
            keyField.text = settingsController.webuiKeyPath
        }
    }
    FileDialog {
        id: napcatDialog
        title: I18n.t("选择 NapCat 入口（napcat.bat）", I18n.lang)
        nameFilters: ["NapCat (napcat.bat)", "*.bat", "*.exe", "All files (*)"]
                                onAccepted: {
                                    settingsController.napcatPath = napcatDialog.selectedFile.toString()
                                    settingsController.apply()
                                    napcatCombo.setText(settingsController.napcatPath)
                                }
    }
    FolderDialog {
        id: nonebotDialog
        onAccepted: {
            settingsController.nonebotDir = nonebotDialog.selectedFolder.toString()
            settingsController.apply()
            nonebotCombo.setText(settingsController.nonebotDir)
        }
    }

    function openMsmInstall() { msmPluginDialog.open() }

    Connections {
        target: botController
        function onPluginMissing() { msmPluginDialog.open() }
    }

    Dialog {
        id: msmPluginDialog
        title: I18n.t("msm 控制插件", I18n.lang)
        modal: true
        standardButtons: Dialog.NoButton
        width: 540
        contentItem: Rectangle {
            color: Theme.bg
            implicitWidth: 540
            implicitHeight: dlgCol.implicitHeight + 28
            ColumnLayout {
                id: dlgCol
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10
                Label {
                    text: I18n.t("当前 NoneBot 目录未接入 msm_control 控制插件，机器人将无法通过 QQ 控制 Minecraft 服务器。", I18n.lang)
                    color: Theme.text; wrapMode: Text.Wrap; Layout.fillWidth: true
                }
                Label {
                    text: I18n.t("方式一（推荐）：自动安装 —— MSM 会把自带的 msm_control 插件复制到该目录并写入 pyproject 配置。", I18n.lang)
                    color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
                }
                Label {
                    text: I18n.t("方式二：手动安装 —— 自行用 nb plugin install 安装 msm_control，或把 MSM 自带 qqbot/plugins/msm_control.py 放入该目录的 plugins/ 并配置 pyproject。", I18n.lang)
                    color: Theme.textMuted; font.pixelSize: 12; wrapMode: Text.Wrap; Layout.fillWidth: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignRight
                    spacing: 8
                    Button {
                        text: I18n.t("自动安装", I18n.lang)
                        palette.buttonText: "white"; palette.windowText: "white"
                        background: Rectangle { color: parent.hovered ? Theme.accentHover : Theme.accent; radius: 6 }
                        onClicked: {
                            if (botController.installMsmPlugin())
                                msmPluginDialog.close()
                            else
                                msmResult.text = I18n.t("安装失败：请查看程序日志，或改用手动安装。", I18n.lang)
                        }
                    }
                    Button {
                        text: I18n.t("我已手动装好，重试", I18n.lang)
                        palette.buttonText: Theme.text; palette.windowText: Theme.text
                        background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                        onClicked: { msmPluginDialog.close(); botController.retryStartNonebot() }
                    }
                    Button {
                        text: I18n.t("取消", I18n.lang)
                        palette.buttonText: Theme.text; palette.windowText: Theme.text
                        background: Rectangle { color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: 6; border.color: Theme.border }
                        onClicked: msmPluginDialog.close()
                    }
                }
                Label { id: msmResult; color: "#e06c5a"; wrapMode: Text.Wrap; Layout.fillWidth: true; text: "" }
            }
        }
    }
}
