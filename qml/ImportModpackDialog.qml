// ImportModpackDialog.qml —— 整合包（.zip）导入向导
// 职责：选择整合包文件与解压目录，交由 importModpack（ModPackImporter）识别类型、
// 解析加载器 / 版本、拉取服务端核心并加入服务器列表。无边框模态窗口。
import QtQuick
import QtQuick.Controls
import MinecraftServerManager
import Qt5Compat.GraphicalEffects
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 540; height: 520
    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    property string closeMode: "close"
    // 整体淡入淡出（替代瞬时显隐）
    opacity: 0
    property bool _fadingOut: false
    NumberAnimation { id: fadeIn; target: window; property: "opacity"; from: 0; to: 1; duration: 240; easing.type: Easing.OutCubic }
    NumberAnimation { id: fadeOut; target: window; property: "opacity"; from: 1; to: 0; duration: 180; easing.type: Easing.InCubic; onStopped: window.close() }
    onClosing: (close) => { if (!_fadingOut) { close.accepted = false; _fadingOut = true; fadeOut.start() } }
    // 窗口真正可见后再触发淡入（show() 是异步的，紧跟 start 会因窗口尚不可见而不生效）
    onVisibleChanged: if (visible) { window.opacity = 0; fadeIn.start() }

    function resetAndOpen() {
        importModpack.reset()
        zipField.text = ""
        dirField.text = ""
        _fadingOut = false
        window.show()
        window.requestActivate()
    }

    FolderDialog {
        id: dirDialog
        currentFolder: "file:///" + importModpack.targetDir
        onAccepted: {
            importModpack.targetDir = selectedFolder.toString().replace(/^file:\/\/\/?/, "")
            dirField.text = importModpack.targetDir
        }
    }

    // 圆角遮罩源：供 frame 的 layer.effect 使用，把 frame 内所有子项按圆角裁剪
    Rectangle {
        id: frameMask
        anchors.fill: parent
        radius: Theme.radius
        color: "white"
        visible: false
    }

    // 背景图层（全窗口最底层，自带圆角裁剪）：所有窗口复用
    BackgroundLayer {
        radius: window.visibility === Window.Maximized ? 0 : Theme.radius
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radius
        color: "transparent"   // 透明：BackgroundLayer 背景图透出
        opacity: appController.uiTransparency   // 整体受界面控件透明度控制（含内嵌标题栏）
        border.width: 0
        clip: true
        // 用 layer.effect + OpacityMask 把整个 frame（含子项）按圆角矩形遮罩，
        // 这是裁掉四角方角、保证圆角的关键（clip:true 只裁矩形、layer.enabled 不裁子项）。
        layer.enabled: radius > 0
        layer.effect: OpacityMask {
            maskSource: frameMask
        }
        x: 0

        // 区域底色层：受区域底色透明度控制（0=全透，1=不透明），与主窗口主区域一致
        Rectangle {
            anchors.fill: parent
            radius: frame.radius
            color: Theme.bg
            opacity: appController.bgLayerOpacity
            z: -1
        }

        TitleBar {
            id: titleBar
            window: window
            title: I18n.t("导入整合包", I18n.lang)
            showDownloads: false
            anchors { top: parent.top; left: parent.left; right: parent.right }
            z: 3
        }

        ColumnLayout {
            anchors { top: titleBar.bottom; left: parent.left; right: parent.right; bottom: parent.bottom; margins: 18 }
            spacing: 14

            Label { text: I18n.t("选择整合包", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 14 }
            RowLayout { Layout.fillWidth: true; spacing: 8
                TextField { id: zipField; Layout.fillWidth: true; placeholderText: I18n.t("选择 .zip 整合包文件", I18n.lang); color: Theme.text
                    placeholderTextColor: Theme.textMuted; selectionColor: Theme.accent
                    background: Rectangle { color: Theme.panelAlt; border.color: Theme.border; radius: 6 } }
                Button {
                    text: I18n.t("浏览", I18n.lang)
                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                    onClicked: fileDialog.open()
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                }
            }

            Label { text: I18n.t("解压 / 安装目录", I18n.lang); color: Theme.text; font.bold: true; font.pixelSize: 14 }
            RowLayout { Layout.fillWidth: true; spacing: 8
                TextField { id: dirField; Layout.fillWidth: true; placeholderText: I18n.t("默认：下载目录/MSM/<整合包名>", I18n.lang); color: Theme.text
                    placeholderTextColor: Theme.textMuted; selectionColor: Theme.accent
                    background: Rectangle { color: Theme.panelAlt; border.color: Theme.border; radius: 6 } }
                Button {
                    text: I18n.t("浏览", I18n.lang)
                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                    onClicked: dirDialog.open()
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                }
            }

            Rectangle { height: 1; color: Theme.border; Layout.fillWidth: true }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 12
                rowSpacing: 8
                Label { text: I18n.t("整合包名称", I18n.lang); color: Theme.textMuted }
                Label { text: importModpack.modpackName || "—"; color: Theme.text }
                Label { text: I18n.t("识别类型", I18n.lang); color: Theme.textMuted }
                Label { text: importModpack.detectedType || "—"; color: Theme.text }
                Label { text: I18n.t("服务端加载器", I18n.lang); color: Theme.textMuted }
                Label { text: importModpack.loader || "—"; color: Theme.text }
                Label { text: I18n.t("游戏版本", I18n.lang); color: Theme.textMuted }
                Label { text: importModpack.gameVersion || "—"; color: Theme.text }
            }

            ProgressBar {
                Layout.fillWidth: true
                visible: importModpack.busy
                value: importModpack.progress / 100
                from: 0; to: 1
            }

            Label {
                visible: importModpack.statusText !== ""
                text: importModpack.statusText
                color: importModpack.done ? Theme.success : Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            Item { Layout.fillHeight: true; Layout.fillWidth: true }

            RowLayout { Layout.fillWidth: true; spacing: 10
                Button {
                    text: I18n.t("取消", I18n.lang)
                    Layout.fillWidth: true
                    palette.buttonText: Theme.text; palette.windowText: Theme.text
                    onClicked: window.close()
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                }
                Button {
                    text: importModpack.done ? I18n.t("完成", I18n.lang) : I18n.t("导入整合包", I18n.lang)
                    Layout.fillWidth: true
                    enabled: !importModpack.busy && !importModpack.done
                    palette.buttonText: "white"; palette.windowText: "white"
                    onClicked: {
                        if (importModpack.done) window.close()
                        else importModpack.import()
                    }
                    background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.accentHover : Theme.accent) : Theme.panelAlt; radius: 6 }
                }
            }
        }
    }

    FileDialog {
        id: fileDialog
        title: I18n.t("选择整合包文件", I18n.lang)
        nameFilters: ["Zip 整合包 (*.zip)"]
        onAccepted: {
            importModpack.zipPath = selectedFile.toLocalFile()
            zipField.text = importModpack.zipPath
        }
    }
}
