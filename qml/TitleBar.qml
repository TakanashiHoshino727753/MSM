import QtQuick
import QtQuick.Controls
import MinecraftServerManager
import QtQuick.Window

// 通用自定义标题栏：标题图片 + 标题文字 + 最小化/最大化(还原)/关闭
Rectangle {
    id: root
    height: Theme.titleBarHeight
    color: Theme.panel
    // 顶部两角跟随窗口圆角（最大化时窗口由系统接管，圆角归零）
    topLeftRadius: Window.window && Window.window.visibility === Window.Maximized ? 0 : Theme.radius
    topRightRadius: Window.window && Window.window.visibility === Window.Maximized ? 0 : Theme.radius
    property var window
    property string title: ""
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
        Image {
            source: "qrc:/icon/ApplicationIcon"
            width: 20; height: 20
            anchors.verticalCenter: parent.verticalCenter
            fillMode: Image.PreserveAspectFit
        }
        Label {
            text: root.title
            color: Theme.text
            font.pixelSize: 13
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
