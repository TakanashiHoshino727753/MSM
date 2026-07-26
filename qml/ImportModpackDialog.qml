// ImportModpackDialog.qml —— 整合包（.zip）导入向导
// 职责：选择整合包文件与解压目录，交由 importModpack（ModPackImporter）识别类型、
// 解析加载器 / 版本、拉取服务端核心并加入服务器列表。无边框模态窗口。
import QtQuick
import QtQuick.Controls
import MinecraftServerManager
import QtQuick.Layouts
import QtQuick.Dialogs

ApplicationWindow {
    id: window
    width: 540; height: 520
    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    color: "transparent"
    property string closeMode: "close"

    function resetAndOpen() {
        importModpack.reset()
        zipField.text = ""
        dirField.text = ""
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

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.bg
        border.width: 0
        clip: true

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
