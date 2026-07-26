import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager

// Edge 式下载列表弹层：聚合所有 DownloadManager 下载任务，支持进度展示、取消、移除、清空已完成。
Popup {
    id: root
    width: 380
    height: 480
    padding: 0
    margins: 0
    modal: false
    focus: true

    x: (parent ? parent.width - width - 12 : 0)
    y: 52

    background: Rectangle {
        color: Theme.panel
        radius: Theme.radius
        border.color: Theme.border
        border.width: 1
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // 头部：固定 47px（标题栏 42 + 5）。
        // 用 Rectangle 锁死高度——Rectangle 的高度不受子项影响，比直接约束 RowLayout 可靠，
        // 之前 RowLayout 的高度会被内部按钮隐式高度顶高，怎么设都压不下去。
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 47
            Layout.minimumHeight: 47
            Layout.maximumHeight: 47
            height: 47
            color: "transparent"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14; anchors.rightMargin: 10
                Label { text: I18n.t("下载", I18n.lang); font.bold: true; font.pixelSize: 15; color: Theme.text }
                Label {
                    text: "(%1)".arg(downloadManager.downloadList.count)
                    color: Theme.textMuted; font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                Button {
                    text: I18n.t("清空已完成", I18n.lang)
                    flat: true
                    height: 30
                    enabled: downloadManager.downloadList.count > 0
                    onClicked: downloadManager.downloadList.clearFinished()
                    palette.windowText: Theme.textMuted; palette.buttonText: Theme.textMuted
                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : "transparent"; radius: 6 }
                }
                Button {
                    flat: true; width: 30; height: 30
                    icon.source: "qrc:/icon/close"
                    icon.color: Theme.text
                    icon.width: 16; icon.height: 16
                    onClicked: root.close()
                    background: Rectangle { color: parent.hovered ? Theme.panelAlt : "transparent"; radius: 6 }
                }
            }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border
            Layout.leftMargin: 10; Layout.rightMargin: 10
        }

        // 内容区：始终 fillHeight，吸收弹窗全部剩余高度，
        // 这样无论列表为空还是有任务，头部 Rectangle 都不会被 ColumnLayout 拉伸。
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 列表（用 anchors 填满容器，不再用布局高度，避免退出布局时拉伸头部）
            ListView {
                anchors.fill: parent
                anchors.leftMargin: 8; anchors.rightMargin: 8; anchors.topMargin: 6
                spacing: 8; clip: true
                model: downloadManager.downloadList
                visible: downloadManager.downloadList.count > 0
                delegate: Rectangle {
                    width: ListView.view.width; height: 74; radius: Theme.radius
                    color: Theme.panelAlt; border.color: Theme.border; border.width: 1
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 10; spacing: 10
                        ColumnLayout {
                            Layout.fillWidth: true; spacing: 4
                            Label {
                                text: model.title
                                color: Theme.text; font.bold: true; font.pixelSize: 13
                                elide: Text.ElideRight; Layout.fillWidth: true
                            }
                        Label {
                            text: {
                                if (model.state === "downloading") return "%1%".arg(Math.round(model.percent));
                                if (model.state === "paused")    return I18n.t("已暂停 %1%").arg(Math.round(model.percent));
                                if (model.state === "done")      return I18n.t("已完成");
                                if (model.state === "error")     return I18n.t("失败：%1").arg(model.errorText);
                                return I18n.t("已取消");
                            }
                            color: model.state === "error" ? Theme.danger : Theme.textMuted
                            font.pixelSize: 11; elide: Text.ElideRight; Layout.fillWidth: true
                        }
                        // 进度条（下载中 / 已暂停时显示）
                        ProgressBar {
                            visible: model.state === "downloading" || model.state === "paused"
                            value: model.percent / 100
                            from: 0; to: 1
                            Layout.fillWidth: true
                            Layout.preferredHeight: 6
                            background: Rectangle { radius: 3; color: Theme.border }
                            contentItem: Rectangle {
                                radius: 3
                                color: Theme.accent
                                width: parent.visualPosition * parent.width
                                height: parent.height
                            }
                        }
                    }
                    // 操作区：按状态显示不同图标按钮
                    RowLayout {
                        spacing: 6
                        // 暂停（下载中）
                        Button {
                            visible: model.state === "downloading"
                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                            icon.source: "qrc:/icon/pause"; icon.color: Theme.text
                            icon.width: 16; icon.height: 16
                            ToolTip.text: I18n.t("暂停", I18n.lang); ToolTip.visible: hovered
                            background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border; border.width: 1 }
                            onClicked: downloadManager.downloadList.pauseAt(index)
                        }
                        // 继续（已暂停）
                        Button {
                            visible: model.state === "paused"
                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                            icon.source: "qrc:/icon/play"; icon.color: Theme.text
                            icon.width: 16; icon.height: 16
                            ToolTip.text: I18n.t("继续", I18n.lang); ToolTip.visible: hovered
                            background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border; border.width: 1 }
                            onClicked: downloadManager.downloadList.resumeAt(index)
                        }
                        // 取消（下载中或已暂停）
                        Button {
                            visible: model.state === "downloading" || model.state === "paused"
                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                            icon.source: "qrc:/icon/close"
                            icon.color: model.state === "downloading" ? "#ffffff" : Theme.text
                            icon.width: 16; icon.height: 16
                            ToolTip.text: I18n.t("取消", I18n.lang); ToolTip.visible: hovered
                            background: Rectangle {
                                color: model.state === "downloading"
                                       ? (parent.hovered ? Theme.dangerHover : Theme.danger)
                                       : (parent.hovered ? Theme.panel : Theme.bg)
                                radius: 6
                                border.color: model.state === "downloading" ? "transparent" : Theme.border
                                border.width: 1
                            }
                            onClicked: downloadManager.downloadList.cancelAt(index)
                        }
                        // 重启（已取消或出错）
                        Button {
                            visible: model.state === "canceled" || model.state === "error"
                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                            icon.source: "qrc:/icon/redownload"; icon.color: Theme.text
                            icon.width: 16; icon.height: 16
                            ToolTip.text: I18n.t("重新下载", I18n.lang); ToolTip.visible: hovered
                            background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border; border.width: 1 }
                            onClicked: downloadManager.downloadList.restartAt(index)
                        }
                        // 打开文件（已完成且文件存在）
                        Text {
                            visible: model.state === "done" && model.path !== ""
                            text: I18n.t("打开文件", I18n.lang)
                            color: Theme.accent; font.pixelSize: 12
                            font.underline: openMouse.containsMouse
                            Layout.alignment: Qt.AlignVCenter
                            MouseArea {
                                id: openMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: Qt.openUrlExternally("file:///" + model.path.replace(/\\/g, "/"))
                            }
                        }
                        // 移除（完成/已取消/出错）
                        Button {
                            visible: model.state === "done" || model.state === "canceled" || model.state === "error"
                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                            icon.source: "qrc:/icon/remove"; icon.color: Theme.text
                            icon.width: 16; icon.height: 16
                            ToolTip.text: I18n.t("移除", I18n.lang); ToolTip.visible: hovered
                            background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border; border.width: 1 }
                            onClicked: downloadManager.downloadList.removeAt(index)
                        }
                    }
                    }
                }
            }

            Label {
                visible: downloadManager.downloadList.count === 0
                text: I18n.t("暂无下载任务", I18n.lang)
                color: Theme.textMuted; font.pixelSize: 12
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top; anchors.topMargin: 40
            }
        }
    }
}
