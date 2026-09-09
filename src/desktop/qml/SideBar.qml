import QtQuick
import QtQuick.Controls
import MinecraftServerManager
import QtQuick.Layouts

// 侧边栏：功能按键（下载中心、应用设置，均为弹窗）+ 设备占用监视（CPU/内存，仿任务管理器）
Rectangle {
    id: root
    width: 127
    color: "transparent"   // 根透明，避免整体半透明；仅背景层半透明
    // 组合式布局：侧边栏位于标题栏之下、独立成块，无边框；仅左下角跟随窗口圆角（最大化时归零）
    topLeftRadius: 0
    bottomLeftRadius: Window.window && Window.window.visibility === Window.Maximized ? 0 : Theme.radius
    signal downloadClicked()
    signal settingsClicked()

    // 侧边栏背景层：受区域底色透明度控制，为主区域底色的约 1.1 倍（0.2 起步，上限 1）
    Rectangle {
        anchors.fill: parent
        color: Theme.panel
        opacity: Math.min(1.0, 0.2 + appController.bgLayerOpacity * 1.1)
        topLeftRadius: 0
        bottomLeftRadius: Window.window && Window.window.visibility === Window.Maximized ? 0 : Theme.radius
        z: -1
    }

    // 折线图绘制：newest 在最右，随新采样点自右向左滑动（固定容量，不挤压）
    function drawLineChart(ctx, w, h, history, capacity) {
        ctx.clearRect(0, 0, w, h)
        var len = history.length
        if (len === 0) return
        var step = w / (capacity - 1)
        var last = history[len - 1] / 100
        var lineColor = last > 0.85 ? "#e74c3c" : Theme.accent
        // x：最新点固定在最右缘，越旧越靠左
        function px(i) { return w - (len - 1 - i) * step }
        function py(i) { return h - (history[i] / 100) * h }
        // 填充区域
        ctx.beginPath()
        ctx.moveTo(px(0), h)
        for (var i = 0; i < len; i++) ctx.lineTo(px(i), py(i))
        ctx.lineTo(px(len - 1), h)
        ctx.closePath()
        ctx.fillStyle = last > 0.85 ? "rgba(231,76,60,0.18)" : Theme.accentSoft
        ctx.fill()
        // 折线
        ctx.beginPath()
        ctx.moveTo(px(0), py(0))
        for (var j = 1; j < len; j++) ctx.lineTo(px(j), py(j))
        ctx.lineWidth = 1.5
        ctx.strokeStyle = lineColor
        ctx.stroke()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        anchors.topMargin: 14
        spacing: 8
        // 侧边栏内所有控件（按钮/文字/图表）受界面控件透明度控制（0=全透，1=不透明）
        opacity: appController.uiTransparency

        Label { text: I18n.t("功能", I18n.lang); color: Theme.textMuted; font.pixelSize: 11 }

        Button {
            height: Theme.controlHeight
            Layout.alignment: Qt.AlignHCenter
            leftPadding: 6; rightPadding: 6; topPadding: 6; bottomPadding: 6
            text: I18n.t("下载中心", I18n.lang)
            onClicked: root.downloadClicked()
            background: Rectangle {
                color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: Theme.radius
                Behavior on color { ColorAnimation { duration: 100 } }
            }
            contentItem: Label { text: parent.text; color: Theme.text; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter }
        }
        Button {
            height: Theme.controlHeight
            Layout.alignment: Qt.AlignHCenter
            leftPadding: 6; rightPadding: 6; topPadding: 6; bottomPadding: 6
            text: I18n.t("应用设置", I18n.lang)
            onClicked: root.settingsClicked()
            background: Rectangle {
                color: parent.hovered ? Theme.panelAlt : Theme.bg; radius: Theme.radius
                Behavior on color { ColorAnimation { duration: 100 } }
            }
            contentItem: Label { text: parent.text; color: Theme.text; horizontalAlignment: Text.AlignLeft; verticalAlignment: Text.AlignVCenter }
        }
        // 移动端配对：左半边状态，右半边连接按键
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            width: parent.width - 24
            radius: Theme.radius
            color: Theme.bg
            RowLayout {
                spacing: 8
                anchors.fill: parent
                anchors.margins: 8
                ColumnLayout {
                    spacing: 5
                    Label { text: I18n.t("移动端配对", I18n.lang); color: Theme.text; font.pixelSize: 12; font.bold: true }
                    RowLayout { spacing: 5
                        Rectangle { width: 8; height: 8; radius: 4; color: (webuiServer && webuiServer.paired) ? Theme.success : "#888" }
                        Label { text: (webuiServer && webuiServer.paired) ? I18n.t("已配对", I18n.lang) : I18n.t("未配对", I18n.lang); color: Theme.textMuted; font.pixelSize: 11 }
                    }
                    RowLayout { spacing: 5
                        Rectangle { width: 8; height: 8; radius: 4; color: (webuiServer && webuiServer.mobileConnected) ? Theme.accent : "#888" }
                        Label { text: (webuiServer && webuiServer.mobileConnected) ? I18n.t("已连接", I18n.lang) : I18n.t("未连接", I18n.lang); color: Theme.textMuted; font.pixelSize: 11 }
                    }
                }
                Item { Layout.fillWidth: true }
                AccentButton {
                    Layout.alignment: Qt.AlignVCenter
                    text: I18n.t("快速链接", I18n.lang)
                    onClicked: pairPopup.open()
                }
            }
        }

        Item { Layout.fillHeight: false; Layout.preferredHeight: 14 }

        Label { text: I18n.t("设备占用", I18n.lang); color: Theme.textMuted; font.pixelSize: 11 }

        // CPU：左曲线图 + 右标签
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Rectangle {
                Layout.preferredWidth: 70; Layout.preferredHeight: 36
                color: Theme.bg; radius: 4
                border.color: Theme.border
                Canvas {
                    id: cpuChart
                    anchors.fill: parent; anchors.margins: 2
                    property var history: []
                    property int capacity: 60
                    onPaint: drawLineChart(getContext("2d"), width, height, history, capacity)
                }
            }
            ColumnLayout {
                spacing: 0; Layout.fillWidth: true
                Label { text: I18n.t("CPU", I18n.lang); color: Theme.text; font.pixelSize: 12 }
                Label { text: Math.round(systemMonitor ? systemMonitor.cpuUsage : 0) + "%"; color: Theme.textMuted; font.pixelSize: 11 }
            }
        }

        // 内存：左曲线图 + 右标签
        RowLayout {
            Layout.fillWidth: true; spacing: 8
            Rectangle {
                Layout.preferredWidth: 70; Layout.preferredHeight: 36
                color: Theme.bg; radius: 4
                border.color: Theme.border
                Canvas {
                    id: memChart
                    anchors.fill: parent; anchors.margins: 2
                    property var history: []
                    property int capacity: 60
                    onPaint: drawLineChart(getContext("2d"), width, height, history, capacity)
                }
            }
            ColumnLayout {
                spacing: 0; Layout.fillWidth: true
                Label { text: I18n.t("内存", I18n.lang); color: Theme.text; font.pixelSize: 12 }
                Label {
                    text: Math.round(systemMonitor ? systemMonitor.memoryUsage : 0) + "%"
                    color: systemMonitor && systemMonitor.memoryUsage > 85 ? Theme.danger : Theme.textMuted
                    font.pixelSize: 11
                }
            }
        }
        // 统一监听 usageChanged，两个曲线图一起刷新
        Connections {
            target: systemMonitor
            function onUsageChanged() {
                cpuChart.history.push(systemMonitor ? systemMonitor.cpuUsage : 0)
                if (cpuChart.history.length > cpuChart.capacity) cpuChart.history.shift()
                cpuChart.requestPaint()
                memChart.history.push(systemMonitor ? systemMonitor.memoryUsage : 0)
                if (memChart.history.length > memChart.capacity) memChart.history.shift()
                memChart.requestPaint()
            }
        }

        Item { Layout.fillHeight: true }
    }

    // 移动端配对弹窗：展示 WebUI 配对二维码，手机 App 扫码即可连接本机控制台
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
