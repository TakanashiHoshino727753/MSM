import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager

// 远程管理状态面板：每种远程管理方式一个指示灯（圆点 + 名称 + 状态文字），
// 按统一配色规则反映各自状态。控件本身为单一条目排入侧边栏（自带 implicit 尺寸）。
//
// 配色规则：
//   需配对的方式（移动端）：未启用灰 / 未配对红 / 配对未连接黄 / 配对且连接绿
//   免配对的方式（Web 控制台 / 远控端 / QQ 机器人）：未启用灰 / 未连接橙 / 已连接蓝
Rectangle {
    id: root

    radius: Theme.radius
    color: Theme.bg
    border.color: Theme.border

    implicitWidth: body.implicitWidth + 16
    implicitHeight: body.implicitHeight + 16

    // 状态枚举 → 指示灯颜色
    function stateColor(s) {
        if (s === "unpaired")   return "#e74c3c"   // 红：需配对但未配对
        if (s === "pairedIdle") return "#f1c40f"   // 黄：已配对未连接
        if (s === "pairedOn")   return "#2ecc71"   // 绿：已配对且连接
        if (s === "off")        return "#e67e22"   // 橙：免配对未连接
        if (s === "on")         return "#3498db"   // 蓝：免配对已连接
        return "#8a8a8a"                              // 灰：未启用/默认
    }
    // 状态枚举 → 状态文字
    function stateText(s) {
        if (s === "unpaired")   return I18n.t("未配对", I18n.lang)
        if (s === "pairedIdle") return I18n.t("待连接", I18n.lang)
        if (s === "pairedOn")   return I18n.t("已连接", I18n.lang)
        if (s === "off")        return I18n.t("未连接", I18n.lang)
        if (s === "on")         return I18n.t("已连接", I18n.lang)
        return I18n.t("未启用", I18n.lang)
    }

    // ---- 各方式实时状态（只读聚合，供下方灯与文字绑定）----
    readonly property bool webuiEnabled: webuiServer ? webuiServer.enabled : false
    readonly property bool webuiRunning: webuiServer ? webuiServer.running : false
    readonly property bool webuiPaired:  webuiServer ? webuiServer.paired : false
    readonly property bool webuiConn:    webuiServer ? webuiServer.mobileConnected : false
    readonly property bool botEnabled:   botController ? botController.botEnabled : false
    readonly property string napcatState:  botController ? botController.napcatState : "stopped"
    readonly property string nonebotState: botController ? botController.nonebotState : "stopped"

    // Web 控制台 / 远控端 共用 WebUI 传输：未启用灰；监听中蓝；否则橙
    readonly property string webConsoleState: !webuiEnabled ? "disabled" : (webuiRunning ? "on" : "off")
    readonly property string remoteState:     !webuiEnabled ? "disabled" : (webuiRunning ? "on" : "off")
    // 移动端：需配对。优先判定未配对（即便底层仍有活动也按未配对显示，杜绝“未配对但已连接”）
    readonly property string mobileState: !webuiEnabled ? "disabled"
        : (!webuiPaired ? "unpaired" : (webuiConn ? "pairedOn" : "pairedIdle"))
    // QQ 机器人：免配对。NapCat 与 NoneBot 均 running 才算已连接（蓝），启用但未跑为橙
    readonly property string botState: !botEnabled ? "disabled"
        : (napcatState === "running" && nonebotState === "running" ? "on" : "off")

    ColumnLayout {
        id: body
        anchors.fill: parent
        anchors.margins: 8
        spacing: 7

        // 每个远程管理方式一个指示灯：圆点(左) + 名称 + 状态文字(右)
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Rectangle { width: 9; height: 9; radius: 4.5; color: root.stateColor(root.webConsoleState) }
            Label { text: I18n.t("Web 控制台", I18n.lang); color: Theme.text; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
            Label { text: root.stateText(root.webConsoleState); color: Theme.textMuted; font.pixelSize: 11 }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Rectangle { width: 9; height: 9; radius: 4.5; color: root.stateColor(root.mobileState) }
            Label { text: I18n.t("移动端", I18n.lang); color: Theme.text; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
            Label { text: root.stateText(root.mobileState); color: Theme.textMuted; font.pixelSize: 11 }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Rectangle { width: 9; height: 9; radius: 4.5; color: root.stateColor(root.remoteState) }
            Label { text: I18n.t("远控端", I18n.lang); color: Theme.text; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
            Label { text: root.stateText(root.remoteState); color: Theme.textMuted; font.pixelSize: 11 }
        }
        RowLayout {
            Layout.fillWidth: true; spacing: 6
            Rectangle { width: 9; height: 9; radius: 4.5; color: root.stateColor(root.botState) }
            Label { text: I18n.t("QQ 机器人", I18n.lang); color: Theme.text; font.pixelSize: 12; Layout.fillWidth: true; elide: Text.ElideRight }
            Label { text: root.stateText(root.botState); color: Theme.textMuted; font.pixelSize: 11 }
        }

        // 快速链接按键：打开移动端配对二维码（配对入口）
        AccentButton {
            Layout.fillWidth: true
            text: I18n.t("快速链接", I18n.lang)
            onClicked: pairPopup.open()
        }
    }

    // 配对二维码弹窗：手机 App 扫码连接本机控制台
    Popup {
        id: pairPopup
        anchors.centerIn: Overlay.overlay
        modal: true
        focus: true
        opacity: appController.uiTransparency   // 配对弹窗整体受界面控件透明度控制
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle { color: Theme.panel; radius: Theme.radius; border.color: Theme.border; opacity: appController.bgLayerOpacity }
        contentItem: ColumnLayout {
            spacing: 14
            Label {
                text: I18n.t("移动端配对", I18n.lang)
                color: Theme.text
                font.pixelSize: 15; font.bold: true
            }
            Rectangle {
                Layout.alignment: Qt.AlignHCenter
                width: 220; height: 220
                color: "#ffffff"; radius: 6
                Image {
                    id: pairQr
                    anchors.fill: parent
                    anchors.margins: 10
                    fillMode: Image.PreserveAspectFit
                    cache: false
                }
            }
            Label {
                Layout.preferredWidth: 300
                text: I18n.t("用手机 App 的“扫码连接”扫描上方二维码。确保手机与本机在同一局域网，且 Web 控制台已启用。", I18n.lang)
                color: Theme.textMuted
                font.pixelSize: 12
                wrapMode: Text.Wrap
            }
            Label {
                Layout.preferredWidth: 300
                text: webuiServer ? webuiServer.pairUri() : ""
                color: Theme.textMuted
                font.pixelSize: 10
                elide: Text.ElideMiddle
                wrapMode: Text.Wrap
            }
            Button {
                Layout.alignment: Qt.AlignHCenter
                text: I18n.t("关闭", I18n.lang)
                onClicked: pairPopup.close()
            }
        }
        onOpened: {
            // 每次打开都重新取配对 URI 生成二维码（token/端口可能变化）
            if (webuiServer)
                pairQr.source = "image://qr/" + encodeURIComponent(webuiServer.pairUri())
        }
    }
}
