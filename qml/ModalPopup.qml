import QtQuick
import QtQuick.Controls
import MinecraftServerManager

// 公共弹窗外壳：统一 模态 / 焦点 / 居中 / 背景边框 / 内边距，
// 各业务弹窗（玩家、世界、模组、属性、删除确认、指令集）只需设置
// popupWidth / popupHeight 与 contentItem，避免重复样板，
// 并保证交互行为（点击外部不关闭、居中、主题边框）完全一致。
Popup {
    modal: true
    focus: true
    padding: 0
    x: Math.round((parent.width - width) / 2)
    y: Math.round((parent.height - height) / 2)
    background: Rectangle { color: Theme.bg; radius: Theme.radius; border.color: Theme.border }

    // 弹出/关闭过渡：缩放 + 淡入（轻微放大感，避免突兀）
    enter: Transition {
        NumberAnimation { target: contentItem; property: "scale"; from: 0.92; to: 1; duration: 160; easing.type: Easing.OutCubic }
        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 160; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { target: contentItem; property: "scale"; to: 0.92; duration: 130; easing.type: Easing.InCubic }
        NumberAnimation { property: "opacity"; to: 0; duration: 130; easing.type: Easing.InCubic }
    }

    property int popupWidth: 560
    property int popupHeight: 480
    width: Math.min(parent.width - 40, popupWidth)
    height: Math.min(parent.height - 40, popupHeight)
}
