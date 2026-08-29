import QtQuick
import MinecraftServerManager

// 背景图层组件：放在任意窗口根最底层，自动显示用户自定义背景图（圆角裁剪）+ 可调压暗遮罩。
// 所有窗口复用，保证统一外观；圆角由自身 clip 处理，不依赖外层容器。
Item {
    id: root
    anchors.fill: parent
    z: 0   // 最底层

    property real radius: 0   // 由调用方传入窗口圆角（最大化时传 0）

    // 外层裁剪容器：统一圆角，Image 与遮罩都在其内，保证四角始终圆角
    Rectangle {
        id: clipRoot
        anchors.fill: parent
        color: "transparent"
        radius: root.radius
        clip: true

        Image {
            id: bgImage
            anchors.fill: parent
            source: appController.bgImageFullPath
            fillMode: Image.PreserveAspectCrop
            visible: status === Image.Ready
            onSourceChanged: console.log("[BGIMG] source changed:", source)
            onStatusChanged: console.log("[BGIMG] status:", status, "source:", source)
        }
        // 半透明压暗层：位于图之上、控件之下。透明度可调（bgScrimOpacity）
        Rectangle {
            anchors.fill: parent
            color: Theme.bg
            opacity: appController.bgImageVisible ? appController.bgScrimOpacity : 0
            visible: appController.bgImageVisible
        }
    }
}
