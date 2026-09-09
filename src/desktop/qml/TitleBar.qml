import QtQuick
import QtQuick.Controls
import MinecraftServerManager
import QtQuick.Window

// 通用自定义标题栏：标题图片 + 标题文字 + 最小化/最大化(还原)/关闭
Rectangle {
    id: root
    height: Theme.titleBarHeight
    color: Theme.panel
    // 标题栏保持不透明（按需求），不透明底色
    // 顶部两角跟随窗口圆角（最大化时窗口由系统接管，圆角归零）
    topLeftRadius: Window.window && Window.window.visibility === Window.Maximized ? 0 : Theme.radius
    topRightRadius: Window.window && Window.window.visibility === Window.Maximized ? 0 : Theme.radius
    property var window
    property string titleText: ""            // 自定义标题文字（空=默认）
    property string titleImageSource: ""     // 自定义标题图片 URL（file:// 或 qrc:），仅 image/mixed 模式使用
    property string titleLayout: "text"      // "text" | "image" | "mixed"
    readonly property string defaultTitleText: "Minecraft Server Manager"
    property bool showMin: true
    property bool showMax: true
    property bool closable: true
    property bool showDownloads: true   // 默认显示，调用方按需隐藏
    signal downloadsClicked()

    // 拖拽移动
    MouseArea {
        anchors.fill: parent
        onPressed: if (root.window) root.window.startSystemMove()
    }

    Row {
        spacing: 8
        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        // 自定义标题图片（image / mixed 模式且提供了图片）
        Image {
            visible: (root.titleLayout === "image" || root.titleLayout === "mixed") && root.titleImageSource
            source: root.titleImageSource
            width: 20; height: 20
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
        }
        // 默认程序图标（text 模式，或 image/mixed 但未提供图片）
        Image {
            visible: root.titleLayout === "text" || !root.titleImageSource
            source: "qrc:/icon/ApplicationIcon"
            width: 20; height: 20
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
        }
        Label {
            visible: root.titleLayout !== "image"
            text: root.titleText !== "" ? root.titleText : root.defaultTitleText
            color: Theme.text
            font.pixelSize: 13
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            visible: root.titleLayout !== "image"
            text: appController.appVersion
            color: Theme.textMuted
            font.pixelSize: 11
            anchors.verticalCenter: parent.verticalCenter
        }
    }

    Row {
        spacing: 0
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter

        Button {
            visible: root.showDownloads
            width: 42; height: Theme.titleBarHeight
            flat: true
            icon.source: "qrc:/icon/download"
            icon.color: Theme.text
            icon.width: 16; icon.height: 16
            onClicked: root.downloadsClicked()
            background: Rectangle {
                color: parent.hovered ? Theme.panelAlt : "transparent"
                radius: 0
                Behavior on color { ColorAnimation { duration: 100 } }
            }
        }
        Button {
            visible: root.showMin
            width: 42; height: Theme.titleBarHeight
            flat: true
            icon.source: "qrc:/icon/minimize"
            icon.color: Theme.text
            icon.width: 16; icon.height: 16
            onClicked: if (root.window) root.window.showMinimized()
            background: Rectangle {
                color: parent.hovered ? Theme.panelAlt : "transparent"
                radius: 0
                Behavior on color { ColorAnimation { duration: 100 } }
            }
        }
        Button {
            visible: root.showMax
            width: 42; height: Theme.titleBarHeight
            flat: true
            icon.source: (root.window && root.window.visibility === Window.Maximized) ? "qrc:/icon/restore" : "qrc:/icon/maximize"
            icon.color: Theme.text
            icon.width: 16; icon.height: 16
            onClicked: if (root.window) {
                if (root.window.visibility === Window.Maximized) root.window.showNormal()
                else root.window.showMaximized()
            }
            background: Rectangle {
                color: parent.hovered ? Theme.panelAlt : "transparent"
                radius: 0
                Behavior on color { ColorAnimation { duration: 100 } }
            }
        }
        Button {
            visible: root.closable
            width: 42; height: Theme.titleBarHeight
            flat: true
            icon.source: "qrc:/icon/close"
            icon.color: parent.hovered ? "#ffffff" : Theme.text
            icon.width: 16; icon.height: 16
            onClicked: if (root.window) root.window.close()
            background: Rectangle {
                color: parent.hovered ? Theme.danger : "transparent"
                radius: 0
                // 关闭按键位于最右，悬浮红块右上角跟随窗口圆角（最大化时归零）
                topRightRadius: (Window.window && Window.window.visibility === Window.Maximized) ? 0 : Theme.radius
                Behavior on color { ColorAnimation { duration: 100 } }
            }
            Behavior on icon.color { ColorAnimation { duration: 100 } }
        }
    }
}
