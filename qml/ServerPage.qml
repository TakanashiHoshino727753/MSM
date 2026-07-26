// ServerPage.qml —— 单个服务器的详情/管理页
// 职责：展示实时状态、控制台输出、在线玩家、已装模组、server.properties 编辑，
// 并提供指令集、快捷操作、世界管理、OP/封禁/白名单等常用管理动作。
// 由 ServerCard 的“详情”按键打开；通过 serverController 订阅实时信号刷新。
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import MinecraftServerManager
import "PropsNames.js" as PropsNames

Item {
    id: root
    property string serverName
    property string serverVersion
    property string serverType: ""
    property string serverPath
    property int serverIndex: -1

    property bool running: false
    property string statusText: running ? I18n.t("运行中", I18n.lang) : I18n.t("已停止", I18n.lang)
    property date runningSince: new Date(0)
    property string uptimeText: "—"

    ListModel { id: consoleModel }
    ListModel { id: modsModel }
    ListModel { id: propsModel }
    ListModel { id: playersModel }
    ListModel { id: worldStateModel }

    property bool worldQueryActive: false

    // ================= 指令集 / 指令提示 / 快捷操作 数据 =================
    readonly property var cmdCatalog: [
        { cat: I18n.t("玩家", I18n.lang), items: [
            { c: "gamemode <mode> <player>", d: I18n.t("设置玩家游戏模式", I18n.lang) },
            { c: "tp <player> <target>", d: I18n.t("传送玩家到目标", I18n.lang) },
            { c: "give <player> <item> [count]", d: I18n.t("给予玩家物品", I18n.lang) },
            { c: "kill <player>", d: I18n.t("杀死实体/玩家", I18n.lang) },
            { c: "op <player>", d: I18n.t("给予管理员权限", I18n.lang) },
            { c: "deop <player>", d: I18n.t("撤销管理员权限", I18n.lang) },
            { c: "kick <player> [reason]", d: I18n.t("踢出玩家", I18n.lang) },
            { c: "ban <player> [reason]", d: I18n.t("封禁玩家", I18n.lang) },
            { c: "pardon <player>", d: I18n.t("解封玩家", I18n.lang) },
            { c: "whitelist add <player>", d: I18n.t("加入白名单", I18n.lang) },
            { c: "whitelist remove <player>", d: I18n.t("移除白名单", I18n.lang) }
        ]},
        { cat: I18n.t("世界 / 环境", I18n.lang), items: [
            { c: "time set day", d: I18n.t("设为白天", I18n.lang) },
            { c: "time set night", d: I18n.t("设为夜晚", I18n.lang) },
            { c: "weather clear", d: I18n.t("放晴", I18n.lang) },
            { c: "weather rain", d: I18n.t("下雨", I18n.lang) },
            { c: "weather thunder", d: I18n.t("雷暴", I18n.lang) },
            { c: "difficulty <difficulty>", d: I18n.t("设置游戏难度", I18n.lang) },
            { c: "gamerule keepInventory true", d: I18n.t("死亡保留物品", I18n.lang) },
            { c: "gamerule doDaylightCycle false", d: I18n.t("停止昼夜循环", I18n.lang) },
            { c: "worldborder set <size>", d: I18n.t("设置世界边界", I18n.lang) }
        ]},
        { cat: I18n.t("管理", I18n.lang), items: [
            { c: "save-all", d: I18n.t("保存所有存档", I18n.lang) },
            { c: "save-on", d: I18n.t("开启自动保存", I18n.lang) },
            { c: "save-off", d: I18n.t("关闭自动保存", I18n.lang) },
            { c: "stop", d: I18n.t("关闭服务器", I18n.lang) },
            { c: "reload", d: I18n.t("重新加载配置", I18n.lang) },
            { c: "whitelist on", d: I18n.t("开启白名单", I18n.lang) },
            { c: "whitelist off", d: I18n.t("关闭白名单", I18n.lang) },
            { c: "banlist", d: I18n.t("查看封禁列表", I18n.lang) },
            { c: "list", d: I18n.t("列出在线玩家", I18n.lang) }
        ]}
    ]
    readonly property var quickActions: [
        { label: I18n.t("保存", I18n.lang), cmd: "save-all" },
        { label: I18n.t("白天", I18n.lang), cmd: "time set day" },
        { label: I18n.t("夜晚", I18n.lang), cmd: "time set night" },
        { label: I18n.t("放晴", I18n.lang), cmd: "weather clear" },
        { label: I18n.t("难度", I18n.lang), cmd: "difficulty <difficulty>" },
        { label: I18n.t("OP", I18n.lang), cmd: "op <player>" },
        { label: I18n.t("传送", I18n.lang), cmd: "tp <player> <target>" },
        { label: I18n.t("给予", I18n.lang), cmd: "give <player> <item> [count]" },
        { label: I18n.t("踢出", I18n.lang), cmd: "kick <player>" },
        { label: I18n.t("重载", I18n.lang), cmd: "reload" },
        { label: I18n.t("列出", I18n.lang), cmd: "list" },
        { label: I18n.t("关机", I18n.lang), cmd: "stop" }
    ]
    ListModel { id: hintModel }

    function updateHints() {
        const t = cmdField.text.trim().toLowerCase()
        hintModel.clear()
        if (!t) return
        for (let ci = 0; ci < cmdCatalog.length; ++ci) {
            const items = cmdCatalog[ci].items
            for (let i = 0; i < items.length; ++i) {
                const c = items[i].c.toLowerCase()
                if (c.indexOf(t) >= 0)
                    hintModel.append({ cmd: items[i].c, desc: items[i].d })
            }
        }
    }
    function applyCommand(cmd) {
        cmdField.text = cmd
        cmdField.forceActiveFocus()
        cmdField.cursorPosition = cmd.length
    }
    function runQuick(cmd) {
        if (cmd.indexOf("<") >= 0 || cmd.indexOf("[") >= 0)
            applyCommand(cmd)
        else
            serverController.send(root.serverName, cmd)
    }
    function updateUptime() {
        if (!root.running) { uptimeText = "—"; return }
        const s = Math.floor((new Date() - root.runningSince) / 1000)
        const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), sec = s % 60
        uptimeText = (h > 0 ? h + "h " : "") + (m > 0 || h > 0 ? m + "m " : "") + sec + "s"
    }
    // 主动校正：不依赖 onStateChanged 信号。窗口在服务器已启动后才打开（信号早于连接发出）、
    // 或隐藏期间状态变化、或信号因线程时序遗漏时，仍按 ServerController 当前真实运行态修正本页，
    // 避免状态显示卡在“已停止”。
    function syncState() {
        const r = serverController.isRunning(root.serverName)
        if (r === root.running) return
        root.running = r
        if (r) {
            if (!root.runningSince || root.runningSince.getTime() === 0) root.runningSince = new Date()
            root.uptimeText = "0s"
            reloadConsole(); reloadMods(); reloadPlayers(); queryWorld()
        } else {
            root.uptimeText = "—"
            reloadConsole(); reloadMods(); reloadPlayers()
        }
    }

    function consoleColor(line) {
        const c = Theme.dark
            ? { red:"#ff6b6b", orange:"#ffa94d", yellow:"#ffe066", green:"#69db7c", blue:"#74c0fc", white:"#cfe3ff" }
            : { red:"#c92a2a", orange:"#e8590c", yellow:"#b08900", green:"#2f9e44", blue:"#1971c2", white:Theme.text }
        if (/\[(ERROR|SEVERE|CRITICAL|FATAL)\]/i.test(line) || /\b(ERROR|SEVERE|CRITICAL|FATAL|EXCEPTION|THROWABLE|NULLPOINTER|FAILED|STACKOVERFLOW)\b/i.test(line)) return c.red
        if (/\b(WARN|WARNING|DEPRECAT)/i.test(line)) return c.orange
        if (/\b(DEBUG|TRACE|FINE|VERBOSE)\b/i.test(line)) return c.blue
        if (/\b(STARTING|PREPARING|LOADING|STOPPING|SAVING|ENABLED|VERSION)\b/i.test(line)) return c.yellow
        if (/\b(DONE|SUCCESS|SAVED|STARTED|JOINED)\b/i.test(line)) return c.green
        return c.white
    }

    // ================= 数据加载 / 实时读取 =================
    function reloadConsole() {
        const text = serverController.getConsole(root.serverName)
        consoleModel.clear()
        const lines = text.split("\n")
        for (let i = 0; i < lines.length; ++i)
            if (lines[i].length) consoleModel.append({ line: lines[i] })
    }
    function reloadMods() {
        modsModel.clear()
        const list = serverController.listMods(root.serverPath)
        for (let i = 0; i < list.length; ++i) modsModel.append({ name: list[i], version: "" })
    }
    function reloadPlayersFromList(players) {
        playersModel.clear()
        for (let i = 0; i < players.length; ++i) playersModel.append({ name: players[i] })
    }
    function reloadPlayers() {
        reloadPlayersFromList(serverController.players(root.serverName))
    }
    function queryWorld() {
        if (!root.running) return
        worldStateModel.clear()
        worldQueryActive = true
        worldQueryTimer.restart()
        serverController.send(root.serverName, "difficulty")
        serverController.send(root.serverName, "time query daytime")
        serverController.send(root.serverName, "weather")
        serverController.send(root.serverName, "seed")
    }
    function captureWorldLine(line) {
        let m
        if ((m = /difficulty:\s*(\w+)/i.exec(line))) setWorldState(I18n.t("难度", I18n.lang), m[1])
        else if ((m = /the time is (\d+)/i.exec(line))) setWorldState(I18n.t("时间(daytime)", I18n.lang), m[1])
        else if ((m = /weather state:\s*(\w+)/i.exec(line))) setWorldState(I18n.t("天气", I18n.lang), m[1])
        else if ((m = /seed:\s*\[?(-?\d+)\]?/i.exec(line))) setWorldState(I18n.t("种子", I18n.lang), m[1])
    }
    function setWorldState(k, v) {
        for (let i = 0; i < worldStateModel.count; ++i)
            if (worldStateModel.get(i).k === k) { worldStateModel.setProperty(i, "v", v); return }
        worldStateModel.append({ k: k, v: v })
    }
    function sendWorld(cmd) {
        serverController.send(root.serverName, cmd)
        worldRetryTimer.restart()
    }

    function loadProps() {
        try {
            PropsNames.setLanguage(settingsController.language)
            const map = serverController.readProperties(root.serverPath)
            const keys = Object.keys(map || {})
            keys.sort()
            propsModel.clear()
            if (keys.length === 0) {
                propsModel.append({ key: I18n.t("（读取失败或文件为空）", I18n.lang), value: "", isBool: false })
                return
            }
            for (const k of keys) {
                const v = map[k]
                propsModel.append({ key: k, value: v, isBool: (v === "true" || v === "false") })
            }
        } catch (e) {
            propsModel.append({ key: I18n.t("读取失败：", I18n.lang) + e, value: "", isBool: false })
        }
    }
    // 供卡片“属性”按键直接打开属性弹窗（无需先进入详情页）
    function openProps() {
        loadProps()
        propsPopup.open()
    }
    function saveProps() {
        const map = {}
        for (let i = 0; i < propsModel.count; ++i) {
            const d = propsModel.get(i)
            map[d.key] = d.value
        }
        serverController.writeProperties(root.serverPath, map)
        if (root.running) { serverController.send(root.serverName, "reload"); queryWorld() }
    }

    Connections {
        target: serverController
        function onConsoleAppended(name, line) {
            if (name !== root.serverName) return
            consoleModel.append({ line: line })
            if (worldQueryActive) captureWorldLine(line)
        }
        function onStateChanged(name, running) {
            if (name !== root.serverName) return
            root.running = running
            if (running) { root.runningSince = new Date(); root.uptimeText = "0s" }
            else { root.uptimeText = "—" }
            reloadConsole(); reloadMods(); reloadPlayers()
            if (running) queryWorld()
        }
        function onPlayersChanged(name, players) {
            if (name !== root.serverName) return
            reloadPlayersFromList(players)
        }
    }
    Connections {
        target: settingsController
        function onLanguageChanged() {
            PropsNames.setLanguage(settingsController.language)
        }
    }

    Timer { id: uptimeTimer; interval: 1000; running: root.running; repeat: true; onTriggered: updateUptime() }
    Timer { id: worldQueryTimer; interval: 2500; onTriggered: worldQueryActive = false }
    Timer { id: worldRetryTimer; interval: 700; onTriggered: queryWorld() }
    // 周期兜底校正运行态（每 2 秒）：与 syncState 同理，确保任何生命周期下状态显示都不会
    // 卡在旧值（例如本地端窗口关闭期间由 WebUI 启动服务器，再打开本地窗口时仍显示正确）。
    Timer { id: stateSyncTimer; interval: 2000; running: true; repeat: true; onTriggered: {
        const r = serverController.isRunning(root.serverName)
        if (r !== root.running) {
            root.running = r
            if (r) { if (!root.runningSince || root.runningSince.getTime() === 0) root.runningSince = new Date(); queryWorld() }
        }
    } }

    Component.onCompleted: { PropsNames.setLanguage(settingsController.language); reloadConsole(); reloadMods(); reloadPlayers(); syncState() }

    // ================= 主布局 =================
    ColumnLayout { anchors.fill: parent; spacing: 8

        // ===== 上半部分：服务器信息 =====
        ColumnLayout { spacing: 10; Layout.fillWidth: true
            Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.topMargin: 14; Layout.bottomMargin: 10

            RowLayout { spacing: 10
                Label { text: root.serverName; font.pixelSize: 18; font.bold: true; color: Theme.text }
                RowLayout { spacing: 5
                    Rectangle { width: 9; height: 9; radius: 4.5; color: root.running ? "#3ec46d" : "#9aa0a6" }
                    Label { text: statusText; color: Theme.textMuted }
                }
                Item { Layout.fillWidth: true }
                // 启停：停止时绿底「启动」，运行时浅红「停止」+ 深红「强制停止」
                AccentButton {
                    text: I18n.t("启动", I18n.lang)
                    accentColor: Theme.success
                    visible: !root.running
                    onClicked: serverController.start(root.serverName, root.serverPath)
                }
                RowLayout {
                    visible: root.running
                    spacing: 4
                    AccentButton {
                        text: I18n.t("停止", I18n.lang)
                        accentColor: Qt.lighter(Theme.danger, 1.3)
                        onClicked: serverController.stop(root.serverName)
                    }
                    AccentButton {
                        text: I18n.t("强制停止", I18n.lang)
                        accentColor: Qt.darker(Theme.danger, 1.3)
                        onClicked: serverController.forceStop(root.serverName)
                    }
                }
        AccentButton {
            text: I18n.t("属性", I18n.lang)
            onClicked: { loadProps(); propsPopup.open() }
        }
        AccentButton {
            text: I18n.t("删除服务器", I18n.lang)
            accentColor: Theme.danger
            onClicked: delDlg.open()
        }
    }

            RowLayout { spacing: 18
                Label { text: I18n.t("类型: ", I18n.lang) + (root.serverType ? root.serverType : "—"); color: Theme.textMuted }
                Label { text: I18n.t("版本: ", I18n.lang) + root.serverVersion; color: Theme.textMuted }
                Label { text: I18n.t("运行时间: ", I18n.lang) + uptimeText; color: Theme.textMuted }
                Item { Layout.fillWidth: true }
            }
        }
        Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

        // ===== 操作行：玩家 / 世界管理 / 模组（同一行，贴右） =====
        RowLayout { spacing: 6; Layout.fillWidth: true
            Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.topMargin: 6
            Item { Layout.fillWidth: true }
            AccentButton {
                text: I18n.t("玩家", I18n.lang)
                onClicked: { reloadPlayers(); playersPopup.open() }
            }
            AccentButton {
                text: I18n.t("世界管理", I18n.lang)
                onClicked: { loadProps(); queryWorld(); worldPopup.open() }
            }
            AccentButton {
                text: I18n.t("模组", I18n.lang)
                onClicked: { reloadMods(); modsPopup.open() }
            }
        }

        // ===== 底部框：指令集 + 快捷操作 + 日志 + 指令输入 =====
        Rectangle { Layout.fillWidth: true; Layout.fillHeight: true
            Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.bottomMargin: 14
            color: Theme.panelAlt
            radius: 8
            border.color: Theme.border
            ColumnLayout { anchors.fill: parent; spacing: 0
                Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.topMargin: 14; Layout.bottomMargin: 14

                // 指令集 + 快捷操作：内嵌"frame"容器（类似 .ui 里的 QFrame 套 widget）
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: quickFrameContent.implicitHeight + 16
                    color: Theme.panelAlt
                    radius: 6
                    border.color: Theme.border
                    ColumnLayout {
                        id: quickFrameContent
                        anchors.fill: parent
                        anchors.margins: 8
                        spacing: 8
                        RowLayout { spacing: 8; Layout.fillWidth: true
                            Label {
                                text: I18n.t("快捷操作", I18n.lang)
                                color: Theme.textMuted; font.pixelSize: 12
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Flickable {
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignVCenter
                                Layout.minimumWidth: 80
                                height: quickRow.height
                                contentWidth: quickRow.implicitWidth
                                clip: true
                                boundsBehavior: Flickable.StopAtBounds
                                Row {
                                    id: quickRow
                                    spacing: 4
                                    Repeater {
                                        model: quickActions
                                        SubtleButton { small: true; text: modelData.label; onClicked: runQuick(modelData.cmd) }
                                    }
                                }
                                ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
                            }
                            AccentButton {
                                text: I18n.t("指令集", I18n.lang)
                                onClicked: cmdSetPopup.open()
                                Layout.alignment: Qt.AlignVCenter
                            }
                            Item { Layout.fillWidth: true }
                        }
                    }
                }

                // 日志输出区（框内）
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    color: Theme.panel
                    radius: 6
                    border.color: Theme.border
                    ListView {
                        id: consoleView
                        anchors.fill: parent
                        anchors.margins: 4
                        model: consoleModel
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        delegate: Rectangle {
                            width: parent.width
                            height: consoleLine.implicitHeight + 6
                            color: index % 2 === 0 ? "transparent" : Qt.rgba(0,0,0,0.04)
                Text {
                    id: consoleLine
                    x: 6; y: 3
                    width: parent.width - 12
                    text: line
                    color: root.consoleColor(line)
                                font.pointSize: 11
                                font.family: "Consolas, monospace"
                                wrapMode: Text.Wrap
                            }
                        }
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            width: 8
                        }
                        onCountChanged: consoleView.positionViewAtEnd()
                    }
                    Label {
                        anchors.centerIn: parent
                        text: consoleModel.count === 0
                              ? (running ? I18n.t("等待服务器输出…", I18n.lang) : I18n.t("暂无日志，启动服务器后将显示输出", I18n.lang))
                              : ""
                        color: Theme.textMuted
                        font.pixelSize: 13
                        visible: consoleModel.count === 0
                    }
                }

                // 指令输入（最下面，框内）
                ColumnLayout { spacing: 4; Layout.fillWidth: true
                    Item {
                        id: hintView
                        Layout.fillWidth: true
                        Layout.preferredHeight: hintView.visible ? Math.min(hintModel.count * 26 + 8, 170) : 0
                        visible: cmdField.text.trim().length > 0 && hintModel.count > 0 && cmdField.activeFocus
                        Rectangle {
                            anchors.fill: parent
                            color: Theme.panel; radius: 6; border.color: Theme.border
                            visible: hintView.visible
                        }
                        ListView {
                            visible: hintView.visible
                            anchors.fill: parent
                            anchors.margins: 4
                            clip: true
                            model: hintModel
                            delegate: Item {
                                width: ListView.view.width
                                height: 26
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8; anchors.rightMargin: 8
                                    spacing: 10
                                    Label { text: modelData.cmd; color: Theme.text; font.family: "Consolas, monospace"; Layout.fillWidth: true }
                                    Label { text: modelData.desc; color: Theme.textMuted; font.pixelSize: 11 }
                                }
                                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: applyCommand(modelData.cmd) }
                            }
                        }
                    }
                    RowLayout { spacing: 0; Layout.fillWidth: true
                        TextField {
                            id: cmdField
                            Layout.fillWidth: true
                            Layout.minimumWidth: 180
                            placeholderText: I18n.t("输入服务器指令，如 gamemode creative @a（输入时显示指令提示）", I18n.lang)
                            palette.text: Theme.text
                            palette.placeholderText: Theme.textMuted
                            background: Rectangle { color: Theme.panel; radius: 6; border.color: Theme.border }
                            onTextChanged: updateHints()
                            onAccepted: { serverController.send(root.serverName, text); text = ""; hintModel.clear() }
                        }
                        SubtleButton {
                            text: I18n.t("发送", I18n.lang)
                            Layout.fillWidth: true
                            Layout.minimumWidth: 70
                            enabled: root.running
                            onClicked: { serverController.send(root.serverName, cmdField.text); cmdField.text = ""; hintModel.clear() }
                        }
                    }
                }
            }
        }
    }

    // ================= 弹窗：玩家（实时在线列表） =================
    ModalPopup {
        id: playersPopup
        popupWidth: 520; popupHeight: 480
        contentItem: ColumnLayout { spacing: 0
            RowLayout { spacing: 8; Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.topMargin: 14
                Label { text: I18n.t("在线玩家", I18n.lang) + "（" + playersModel.count + "）"; font.pixelSize: 15; font.bold: true; color: Theme.text }
                Item { Layout.fillWidth: true }
                Button { text: I18n.t("刷新", I18n.lang); flat: true; implicitHeight: 28; background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border } onClicked: reloadPlayers() }
                Button { text: I18n.t("关闭", I18n.lang); flat: true; implicitHeight: 28; background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border } onClicked: playersPopup.close() }
            }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                contentWidth: width
                ColumnLayout {
                    width: parent.width
                    spacing: 4; Layout.leftMargin: 14; Layout.rightMargin: 14
                    Repeater {
                        model: playersModel
                        RowLayout { spacing: 6
                            Label { text: modelData.name; color: Theme.text; font.family: "Consolas, monospace"; Layout.fillWidth: true }
                            Button { text: I18n.t("OP", I18n.lang); flat: true; implicitHeight: 24; implicitWidth: 46; background: Rectangle { color: parent.hovered ? Theme.accentSoft : Theme.panelAlt; radius: 5; border.color: Theme.border } onClicked: serverController.send(root.serverName, "op " + modelData.name) }
                            Button { text: I18n.t("取消OP", I18n.lang); flat: true; implicitHeight: 24; implicitWidth: 56; background: Rectangle { color: parent.hovered ? Theme.accentSoft : Theme.panelAlt; radius: 5; border.color: Theme.border } onClicked: serverController.send(root.serverName, "deop " + modelData.name) }
                            Button { text: I18n.t("踢出", I18n.lang); flat: true; implicitHeight: 24; implicitWidth: 46; background: Rectangle { color: parent.hovered ? Theme.accentSoft : Theme.panelAlt; radius: 5; border.color: Theme.border } onClicked: serverController.send(root.serverName, "kick " + modelData.name) }
                            Button { text: I18n.t("封禁", I18n.lang); flat: true; implicitHeight: 24; implicitWidth: 46; background: Rectangle { color: parent.hovered ? Theme.accentSoft : Theme.panelAlt; radius: 5; border.color: Theme.border } onClicked: serverController.send(root.serverName, "ban " + modelData.name) }
                        }
                    }
                    Label { text: I18n.t("（当前无在线玩家，或服务器未运行）", I18n.lang); color: Theme.textMuted; visible: playersModel.count === 0 }
                }
            }
        }
    }

    // ================= 弹窗：世界管理（实时读取） =================
    ModalPopup {
        id: worldPopup
        popupWidth: 560; popupHeight: 540
        contentItem: ColumnLayout { spacing: 0
            RowLayout { spacing: 8; Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.topMargin: 14
                Label { text: I18n.t("世界管理", I18n.lang); font.pixelSize: 15; font.bold: true; color: Theme.text }
                Item { Layout.fillWidth: true }
                Button { text: I18n.t("刷新状态", I18n.lang); flat: true; implicitHeight: 28; background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border } onClicked: queryWorld() }
                Button { text: I18n.t("关闭", I18n.lang); flat: true; implicitHeight: 28; background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border } onClicked: worldPopup.close() }
            }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                contentWidth: width
                ColumnLayout {
                    width: parent.width
                    spacing: 10; Layout.leftMargin: 14; Layout.rightMargin: 14

                    Label { text: I18n.t("实时状态", I18n.lang); color: Theme.accent; font.bold: true; font.pixelSize: 12 }
                    Repeater {
                        model: worldStateModel
                        RowLayout { spacing: 10
                            Label { text: modelData.k; color: Theme.textMuted; Layout.preferredWidth: 120 }
                            Label { text: modelData.v; color: Theme.text; font.family: "Consolas, monospace" }
                        }
                    }
                    Label { text: I18n.t("（正在读取，或服务器未运行 / 非英文输出）", I18n.lang); color: Theme.textMuted; visible: worldStateModel.count === 0 }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                    Label { text: I18n.t("快捷命令", I18n.lang); color: Theme.accent; font.bold: true; font.pixelSize: 12 }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: [
                                { t: I18n.t("白天", I18n.lang), c: "time set day" },
                                { t: I18n.t("夜晚", I18n.lang), c: "time set night" },
                                { t: I18n.t("放晴", I18n.lang), c: "weather clear" },
                                { t: I18n.t("下雨", I18n.lang), c: "weather rain" },
                                { t: I18n.t("雷暴", I18n.lang), c: "weather thunder" },
                                { t: I18n.t("和平", I18n.lang), c: "difficulty peaceful" },
                                { t: I18n.t("简单", I18n.lang), c: "difficulty easy" },
                                { t: I18n.t("普通", I18n.lang), c: "difficulty normal" },
                                { t: I18n.t("困难", I18n.lang), c: "difficulty hard" },
                                { t: I18n.t("保存", I18n.lang), c: "save-all" },
                                { t: I18n.t("重载", I18n.lang), c: "reload" }
                            ]
                            Button { text: modelData.t; flat: true; implicitHeight: 28; background: Rectangle { color: hovered ? Theme.accentSoft : Theme.panelAlt; radius: 6; border.color: Theme.border } onClicked: sendWorld(modelData.c) }
                        }
                    }

                    Rectangle { Layout.fillWidth: true; height: 1; color: Theme.border }

                    Label { text: I18n.t("世界配置（来自 server.properties）", I18n.lang); color: Theme.accent; font.bold: true; font.pixelSize: 12 }
                    Repeater {
                        model: propsModel
                        RowLayout { spacing: 10
                            Label { text: PropsNames.label(modelData.key); color: Theme.textMuted; Layout.fillWidth: true; elide: Text.ElideRight }
                            Label { text: modelData.value; color: Theme.text; font.family: "Consolas, monospace" }
                        }
                    }
                }
            }
        }
    }

    // ================= 弹窗：模组 =================
    ModalPopup {
        id: modsPopup
        popupWidth: 560; popupHeight: 460
        contentItem: ColumnLayout { spacing: 0
            Label { text: I18n.t("已安装模组", I18n.lang) + "（" + modsModel.count + "）"; font.pixelSize: 15; font.bold: true; color: Theme.text; padding: 14 }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                contentWidth: width
                ColumnLayout {
                    width: parent.width
                    spacing: 4; Layout.leftMargin: 14; Layout.rightMargin: 14
                    Repeater {
                        model: modsModel
                        RowLayout { spacing: 10
                            Label { text: modelData.name; color: Theme.text; Layout.fillWidth: true }
                            Label { text: modelData.version || ""; color: Theme.textMuted }
                        }
                    }
                    Label { text: I18n.t("（服务器未运行或无模组）", I18n.lang); color: Theme.textMuted; visible: modsModel.count === 0 }
                }
            }
            RowLayout { spacing: 8; Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.bottomMargin: 14; Layout.topMargin: 8
                Item { Layout.fillWidth: true }
                Button { text: I18n.t("关闭", I18n.lang); background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border } onClicked: modsPopup.close() }
            }
        }
    }

    // ================= 弹窗：服务器属性（编辑） =================
    ModalPopup {
        id: propsPopup
        popupWidth: 560; popupHeight: 520
        contentItem: ColumnLayout { spacing: 0
            Label { text: I18n.t("属性", I18n.lang); font.pixelSize: 15; font.bold: true; color: Theme.text; padding: 14 }
            ListView {
                Layout.fillWidth: true; Layout.fillHeight: true
                clip: true
                model: propsModel
                spacing: 8
                leftMargin: 14; rightMargin: 14
                topMargin: 8; bottomMargin: 8
                ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                delegate: RowLayout {
                    width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                    spacing: 10
                    Label {
                        text: PropsNames.label(key) + " (" + key + ")"
                        color: Theme.text
                        Layout.fillWidth: true
                        Layout.minimumWidth: 120
                        elide: Text.ElideRight
                    }
                    Switch {
                        visible: isBool
                        checked: value === 'true'
                        onToggled: value = checked ? 'true' : 'false'
                    }
                    TextField {
                        visible: !isBool
                        text: value
                        color: Theme.text
                        Layout.preferredWidth: 160
                        background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                        onTextChanged: value = text
                    }
                }
            }
            RowLayout { spacing: 8; Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.bottomMargin: 14; Layout.topMargin: 8
                Item { Layout.fillWidth: true }
                Button { text: I18n.t("取消", I18n.lang); background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border } onClicked: propsPopup.close() }
                Button {
                    text: I18n.t("保存", I18n.lang)
                    contentItem: Label { text: parent.text; color: Theme.text; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter; font: parent.font }
                    background: Rectangle { color: parent.hovered ? Qt.darker(Theme.accent, 1.15) : Theme.accent; radius: 6 }
                    onClicked: { saveProps(); propsPopup.close() }
                }
            }
        }
    }

    // ================= 弹窗：删除服务器确认（自定义美化，与属性弹窗风格一致） =================
    ModalPopup {
        id: delDlg
        popupWidth: 420; popupHeight: 220
        contentItem: ColumnLayout { spacing: 0
            Label { text: I18n.t("删除服务器", I18n.lang); font.pixelSize: 15; font.bold: true; color: Theme.danger; padding: 14 }
            Rectangle { height: 1; color: Theme.border }
            Label {
                text: I18n.t("确定删除服务器 “%1” 吗？此操作不可撤销。").arg(root.serverName)
                color: Theme.text; wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.leftMargin: 14; Layout.rightMargin: 14
                Layout.topMargin: 14; Layout.bottomMargin: 14
            }
            Rectangle { height: 1; color: Theme.border }
            RowLayout {
                Layout.fillWidth: true; Layout.rightMargin: 14
                Layout.topMargin: 10; Layout.bottomMargin: 10
                Layout.alignment: Qt.AlignRight
                SubtleButton { text: I18n.t("取消", I18n.lang); onClicked: delDlg.close() }
                AccentButton {
                    text: I18n.t("删除", I18n.lang)
                    accentColor: Theme.danger
                    onClicked: { serverManager.removeServer(root.serverIndex); delDlg.close(); }
                }
            }
        }
    }

    // ================= 弹窗：指令集 =================
    ModalPopup {
        id: cmdSetPopup
        popupWidth: 640; popupHeight: 560
        contentItem: ColumnLayout { spacing: 0
            Label { text: I18n.t("指令集", I18n.lang); font.pixelSize: 15; font.bold: true; color: Theme.text; padding: 14 }
            ScrollView {
                Layout.fillWidth: true; Layout.fillHeight: true
                contentWidth: width
                ColumnLayout {
                    width: parent.width
                    spacing: 10; Layout.leftMargin: 14; Layout.rightMargin: 14
                    Repeater {
                        model: cmdCatalog
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 4
                            Label { text: modelData.cat; color: Theme.accent; font.bold: true; font.pixelSize: 12 }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Repeater {
                                    model: modelData.items
                                    Button {
                                        flat: true
                                        implicitWidth: Math.max(tm.width + 24, 170)
                                        implicitHeight: 48
                                        background: Rectangle { color: hovered ? Theme.accentSoft : Theme.panel; radius: 6; border.color: Theme.border }
                                        contentItem: ColumnLayout {
                                            anchors.fill: parent
                                            anchors.margins: 6
                                            spacing: 3
                                            Text {
                                                text: modelData.c
                                                color: Theme.text
                                                font.family: "Consolas, monospace"
                                                font.pixelSize: 12
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                            Text {
                                                text: modelData.d
                                                color: Theme.textMuted
                                                font.pixelSize: 11
                                                elide: Text.ElideRight
                                                Layout.fillWidth: true
                                            }
                                        }
                                        TextMetrics { id: tm; text: modelData.c; font.family: "Consolas, monospace"; font.pixelSize: 12 }
                                        onClicked: applyCommand(modelData.c)
                                    }
                                }
                            }
                        }
                    }
                }
            }
            RowLayout { spacing: 8; Layout.leftMargin: 14; Layout.rightMargin: 14; Layout.bottomMargin: 14; Layout.topMargin: 8
                Label { text: I18n.t("点击指令以填入输入框", I18n.lang); color: Theme.textMuted; font.pixelSize: 11 }
                Item { Layout.fillWidth: true }
                Button {
                    text: I18n.t("关闭", I18n.lang)
                    background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    onClicked: cmdSetPopup.close()
                }
            }
        }
    }
}
