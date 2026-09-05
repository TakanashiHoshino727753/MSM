// Main.qml —— MSM_Remote 远控端主界面
// 与本地端一致的暗色风格；顶部自定义标题栏（可拖动/最小化/关闭），下方为连接页或服务器列表页。
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1
import QtQuick.Dialogs 1.3
import Theme 1.0

ApplicationWindow {
    id: window
    visible: true
    width: 980
    height: 660
    minimumWidth: 760
    minimumHeight: 520
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    title: "MSM 远控端"

    // —— 自定义标题栏（可拖动）——
    property point _dragPos
    Rectangle {
        id: titleBar
        height: 38
        anchors { left: parent.left; right: parent.right; top: parent.top }
        color: Theme.surface
        z: 100
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            spacing: 8
            Label { text: "MSM 远控端"; color: Theme.text; font.bold: true }
            Item { Layout.fillWidth: true }
            MouseArea {
                Layout.preferredWidth: 40; Layout.fillHeight: true
                cursorShape: Qt.PointingHandCursor
                onClicked: window.showMinimized()
                Text { anchors.centerIn: parent; text: "—"; color: Theme.text; font.pixelSize: 16 }
            }
            MouseArea {
                Layout.preferredWidth: 40; Layout.fillHeight: true
                cursorShape: Qt.PointingHandCursor
                onClicked: window.close()
                Text { anchors.centerIn: parent; text: "✕"; color: Theme.text; font.pixelSize: 14 }
            }
        }
        MouseArea {
            anchors.fill: parent
            anchors.rightMargin: 80
            property point _p
            onPressed: _p = Qt.point(mouse.x, mouse.y)
            onPositionChanged: if (pressed) {
                window.x += mouse.x - _p.x
                window.y += mouse.y - _p.y
            }
        }
    }

    // —— 主体 ——
    Rectangle {
        id: body
        anchors { left: parent.left; right: parent.right; top: titleBar.bottom; bottom: parent.bottom }
        color: "transparent"
        clip: true

        // 用户自定义背景图（设置里从电脑选文件/文件夹，复制到 backgroundpic/ 目录）
        BackgroundLayer { anchors.fill: parent }

        // 主区域底色：有壁纸时透明（透出壁纸），无壁纸时用 Theme.bg
        Rectangle {
            anchors.fill: parent
            z: 1
            color: appearance.bgImageVisible ? "transparent" : Theme.bg
        }
        // 区域底色染色（bgLayerOpacity 控制其不透明度，调小则更多透出壁纸）
        Rectangle {
            anchors.fill: parent
            z: 1
            color: Theme.accent
            opacity: appearance.bgLayerOpacity * 0.12
            visible: appearance.bgImageVisible
        }

        // 内容层（uiTransparency 控制全局控件透明度）
        Item {
            id: contentLayer
            anchors.fill: parent
            z: 2
            opacity: appearance.uiTransparency

            // 未连接：连接页
            ColumnLayout {
                visible: !remote.connected
                anchors.centerIn: parent
                width: 360
                spacing: 14
                Label { text: "连接到本地端控制器"; color: Theme.text; font.bold: true; font.pixelSize: 18 }
                Label {
                    text: "粘贴本地端「移动端配对」弹窗中的 msm:// 链接，或手动填写主机/端口/令牌"
                    color: Theme.textMuted; wrapMode: Text.Wrap; Layout.fillWidth: true
                    font.pixelSize: 12
                }
                TextField {
                    id: uriField
                    Layout.fillWidth: true
                    placeholderText: "msm://token@host:25575?t=token"
                    color: Theme.text
                    background: Rectangle { color: Theme.surface; radius: Theme.radiusSm; border.color: Theme.border }
                }
                Button {
                    text: "从链接连接"
                    Layout.fillWidth: true
                    onClicked: remote.connectFromUri(uriField.text)
                }
                Label { text: "或手动填写"; color: Theme.textMuted; font.pixelSize: 12 }
                GridLayout {
                    columns: 2; Layout.fillWidth: true; rowSpacing: 8; columnSpacing: 8
                    Label { text: "主机"; color: Theme.text }
                    TextField {
                        id: hostField; Layout.fillWidth: true; text: "127.0.0.1"
                        color: Theme.text; background: Rectangle { color: Theme.surface; radius: Theme.radiusSm; border.color: Theme.border }
                    }
                    Label { text: "端口"; color: Theme.text }
                    TextField {
                        id: portField; Layout.fillWidth: true; text: "25575"
                        color: Theme.text; background: Rectangle { color: Theme.surface; radius: Theme.radiusSm; border.color: Theme.border }
                        validator: IntValidator { bottom: 1; top: 65535 }
                    }
                    Label { text: "令牌"; color: Theme.text }
                    TextField {
                        id: tokenField; Layout.fillWidth: true
                        color: Theme.text; background: Rectangle { color: Theme.surface; radius: Theme.radiusSm; border.color: Theme.border }
                    }
                }
                CheckBox { id: httpsChk; text: "HTTPS（默认开启，自签证书）"; checked: true; contentItem: Label { text: "HTTPS（默认开启，自签证书）"; color: Theme.text } }
                Button {
                    text: "连接"; Layout.fillWidth: true
                    enabled: hostField.text && tokenField.text
                    onClicked: remote.connectTo(hostField.text, portField.text ? portField.text : 25575, tokenField.text, httpsChk.checked)
                }
                Label {
                    visible: remote.lastError
                    text: "错误：" + remote.lastError
                    color: Theme.danger; wrapMode: Text.Wrap; Layout.fillWidth: true
                }
            }

            // 已连接：服务器列表 + 操作
            ColumnLayout {
                visible: remote.connected
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "服务器列表（" + remote.host + ":" + remote.port + "）"; color: Theme.text; font.bold: true; font.pixelSize: 16 }
                    Item { Layout.fillWidth: true }
                    Button { text: "外观"; onClicked: appearanceDialog.open() }
                    Button { text: "断开"; onClicked: remote.disconnectFrom() }
                    Button { text: "刷新"; onClicked: remote.refreshServers() }
                }

                ScrollView {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    clip: true
                    ColumnLayout {
                        width: parent.width
                        spacing: 10
                        Repeater {
                            model: remote.servers
                            delegate: ServerCard { srv: modelData; rc: remote }
                        }
                        Label {
                            visible: remote.servers.length === 0
                            text: "暂无服务器"
                            color: Theme.textMuted
                        }
                    }
                }

                // 一键安装优化模组面板（底部抽屉式）
                Rectangle {
                    visible: optPanel.visible
                    Layout.fillWidth: true
                    color: Theme.surface
                    radius: Theme.radius
                    implicitHeight: optPanel.implicitHeight + 20
                    ColumnLayout {
                        id: optPanel
                        anchors { left: parent.left; right: parent.right; top: parent.top; margins: 10 }
                        spacing: 8
                        visible: false
                        property string _name: ""
                        Label { text: "优化模组（一键安装）"; color: Theme.text; font.bold: true }
                        RowLayout {
                            Layout.fillWidth: true
                            Label { text: "MC 版本"; color: Theme.text }
                            TextField { id: mcVer; Layout.preferredWidth: 90; text: "1.20.1"; color: Theme.text; background: Rectangle { color: Theme.bg; radius: Theme.radiusSm; border.color: Theme.border } }
                            Label { text: "加载器"; color: Theme.text }
                            ComboBox { id: loaderCb; model: ["fabric", "forge", "neoforge"]; width: 100 }
                            Button { text: "检索"; onClicked: remote.fetchOptimods(optPanel._name, mcVer.text, loaderCb.currentText) }
                            Label { visible: remote.optLoading; text: "检索中…"; color: Theme.textMuted }
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: remote.optMods
                                delegate: Chip {
                                    text: modelData.project + " " + modelData.version
                                    onClicked: remote.installOptimod(optPanel._name, mcVer.text, loaderCb.currentText)
                                }
                            }
                        }
                        property string _name: optPanel._name
                    }
                }
            }
        }
    }

    // Toast 提示
    Popup {
        id: toastPopup
        anchors { bottom: parent.bottom; horizontalCenter: parent.horizontalCenter; bottomMargin: 24 }
        background: Rectangle { color: Theme.surfaceAlt; radius: Theme.radius; border.color: Theme.border }
        contentItem: Label { id: toastLabel; color: Theme.text; padding: 10 }
        timeout: 2200
        Timer { id: toastTimer; interval: 2200; onTriggered: toastPopup.close() }
    }
    Connections {
        target: remote
        function onToast(title, text) {
            toastLabel.text = title + "：" + text
            toastPopup.open()
            toastTimer.restart()
        }
    }

    // —— 外观个性化弹窗（背景图/壁纸/背景音乐，用户从电脑选文件/文件夹复制到皮肤目录）——
    Dialog {
        id: appearanceDialog
        title: "外观个性化"
        modal: true
        standardButtons: Dialog.Close
        width: 460; height: 480
        contentItem: ScrollView {
            clip: true
            ColumnLayout {
                spacing: 12
                width: parent.width
                Label { text: "背景图（单张）"; color: Theme.text; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: appearance.bgImagePath ? "已设置" : "未设置"; color: Theme.textMuted; Layout.fillWidth: true }
                    Button { text: "选择图片"; onClicked: bgImageDialog.open() }
                    Button { text: "清除"; enabled: appearance.bgImagePath; onClicked: appearance.clearSkinImage() }
                    Switch { checked: appearance.bgEnabled; enabled: appearance.bgImagePath; onToggled: appearance.setBgEnabled(checked) }
                }

                // 壁纸：文件夹多图 + 顺序/随机轮换 + 程序内调序（与本地端一致）
                Label { text: "壁纸（文件夹轮播）"; color: Theme.text; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "选择文件夹"; onClicked: bgFolderDialog.open() }
                    Button {
                        text: "清空列表"; enabled: appearance.bgImageList.length > 0
                        onClicked: appearance.clearBgImages()
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: appearance.bgImageFolder ? ("文件夹：" + appearance.bgImageFolder) : "未选择文件夹（也可在下方单独添加图片）"
                    color: Theme.textMuted; wrapMode: Text.Wrap; font.pixelSize: 12
                }
                RowLayout {
                    Layout.fillWidth: true
                    Button { text: "添加图片"; onClicked: bgImagesDialog.open() }
                    Label { text: "轮换方式"; color: Theme.text }
                    ComboBox {
                        id: bgModeCombo
                        Layout.preferredWidth: 110
                        model: ["顺序", "随机"]
                        currentIndex: appearance.bgImageMode === "random" ? 1 : 0
                        onActivated: appearance.setBgImageMode(currentIndex === 1 ? "random" : "sequential")
                    }
                    Label { text: "间隔"; color: Theme.text }
                    SpinBox {
                        id: bgInterval
                        from: 1; to: 1000000; value: appearance.bgImageInterval
                        onValueChanged: appearance.setBgImageInterval(value)
                    }
                    ComboBox {
                        id: bgUnitCombo
                        Layout.preferredWidth: 80
                        property var units: ["sec", "min", "hour", "day"]
                        model: ["秒", "分", "时", "天"]
                        currentIndex: Math.max(0, units.indexOf(appearance.bgImageIntervalUnit))
                        onActivated: appearance.setBgImageIntervalUnit(units[currentIndex])
                    }
                }
                Label { text: "列表至少 2 张且已开启壁纸时才会自动轮换"; color: Theme.textMuted; font.pixelSize: 12 }
                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 120
                    clip: true
                    model: appearance.bgImageList
                    delegate: RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Label {
                            Layout.fillWidth: true
                            text: (index === appearance.bgImageIndex ? "▶ " : "") + modelData.split("/").pop()
                            color: index === appearance.bgImageIndex ? Theme.accent : Theme.text
                            elide: Text.ElideLeft
                        }
                        Button { text: "↑"; enabled: index > 0; onClicked: appearance.moveBgImage(index, index - 1) }
                        Button { text: "↓"; enabled: index < appearance.bgImageList.length - 1; onClicked: appearance.moveBgImage(index, index + 1) }
                        Button { text: "✕"; onClicked: appearance.removeBgImage(index) }
                    }
                }

                // 压暗 / 透明度
                Label { text: "显示效果"; color: Theme.text; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "压暗强度"; color: Theme.text }
                    Slider { Layout.fillWidth: true; from: 0; to: 1; stepSize: 0.05; value: appearance.bgScrimOpacity; onMoved: appearance.setBgScrimOpacity(value) }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "控件透明度"; color: Theme.text }
                    Slider { Layout.fillWidth: true; from: 0; to: 1; stepSize: 0.05; value: appearance.uiTransparency; onMoved: appearance.setUiTransparency(value) }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: "底色透明度"; color: Theme.text }
                    Slider { Layout.fillWidth: true; from: 0; to: 1; stepSize: 0.05; value: appearance.bgLayerOpacity; onMoved: appearance.setBgLayerOpacity(value) }
                }

                Label { text: "背景音乐"; color: Theme.text; font.bold: true }
                RowLayout {
                    Layout.fillWidth: true
                    Label { text: appearance.bgmPath ? "已设置" : "未设置"; color: Theme.textMuted; Layout.fillWidth: true }
                    Button { text: "选择音乐"; onClicked: bgMusicDialog.open() }
                    Button { text: "清除"; enabled: appearance.bgmPath; onClicked: appearance.clearSkinMusic() }
                    Switch { checked: appearance.bgmEnabled; onToggled: appearance.setBgmEnabled(checked) }
                }
                Label { text: "音量"; color: Theme.text }
                Slider { Layout.fillWidth: true; from: 0; to: 1; stepSize: 0.05; value: appearance.bgmVolume; onMoved: appearance.setBgmVolume(value) }
            }
        }
    }
    FileDialog {
        id: bgImageDialog
        title: "选择背景图片"
        nameFilters: ["图片 (*.png *.jpg *.jpeg *.bmp *.webp)"]
        onAccepted: appearance.importSkinImage(bgImageDialog.fileUrl)
    }
    FolderDialog {
        id: bgFolderDialog
        title: "选择壁纸文件夹"
        onAccepted: appearance.setBgImageFolder(bgFolderDialog.folder)
    }
    FileDialog {
        id: bgImagesDialog
        title: "添加壁纸图片（可多选）"
        nameFilters: ["图片 (*.png *.jpg *.jpeg *.bmp *.webp)"]
        selectMultiple: true
        onAccepted: {
            var list = []
            for (var i = 0; i < bgImagesDialog.fileUrls.length; ++i)
                list.push(bgImagesDialog.fileUrls[i])
            appearance.addBgImages(list)
        }
    }
    FileDialog {
        id: bgMusicDialog
        title: "选择背景音乐"
        nameFilters: ["音频 (*.mp3 *.ogg *.wav *.flac *.m4a)"]
        onAccepted: appearance.importSkinMusic(bgMusicDialog.fileUrl)
    }
}
