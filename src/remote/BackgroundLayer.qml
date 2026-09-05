import QtQuick 2.15
import Theme 1.0

// 背景图层组件：放在窗口根最底层，显示用户自定义背景图（PreserveAspectCrop）+ 可调压暗遮罩。
// 与本地端 BackgroundLayer 行为一致；远控端未链接 Qt5Compat.GraphicalEffects，故用普通 Rectangle 压暗，
// 不做圆角遮罩（远控端窗口为无边框矩形，无需裁圆角）。
Item {
    id: root
    anchors.fill: parent
    z: 0   // 最底层

    Image {
        id: bgImage
        anchors.fill: parent
        source: appearance.bgImageFullPath
        fillMode: Image.PreserveAspectCrop
        visible: status === Image.Ready
    }
    // 半透明压暗层：位于图之上、控件之下。透明度可调（appearance.bgScrimOpacity）
    Rectangle {
        anchors.fill: parent
        color: Theme.bg
        opacity: appearance.bgImageVisible ? appearance.bgScrimOpacity : 0
        visible: appearance.bgImageVisible
    }
}
