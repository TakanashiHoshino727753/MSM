// CreateServerDialog.qml —— 创建 / 新建服务器向导
// 职责：选择创建方式（全新 / 从压缩包导入）、游戏版本、加载器（模组服支持多加载器）、
// 内存分配、Java 路径、名称与保存路径，并触发创建流程（createServerController）。
// 无边框模态窗口；EULA 勾选后才可创建；创建中以分阶段进度展示。
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MinecraftServerManager

ApplicationWindow {
    id: window
    width: 520
    height: 560
    title: I18n.t("创建服务器", I18n.lang)
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint
    modality: Qt.ApplicationModal

    property var chosenLoaders: []

    // 默认命名：类型+版本+加载器类型。用户手动改过名称后不再自动覆盖。
    property bool nameUserEdited: false
    property bool nameSyncing: false
    // 当前保存目录的父路径（供 FolderDialog 定位到文件夹上一级）
    property string parentDir: { var p = createServer.saveDir; var idx = Math.max(p.lastIndexOf("/"), p.lastIndexOf("\\")); return idx > 0 ? p.substring(0, idx) : p }

    // 压缩包导入模式：从已下载/已打包的服务器压缩包导入，而非全新构建
    property bool importMode: false
    property string importZipPath: ""

    // 目标保存目录校验：若已存在且非空（可能已有服务器），提示用户改名/换路径
    property string pathError: ""
    function validatePath() {
        if (createServer.dirOccupied(createServer.saveDir))
            pathError = I18n.t("该文件夹已存在且包含文件，可能已有服务器。请更改名称或选择其他路径。", I18n.lang)
        else
            pathError = ""
    }

    // 模组服：根据当前 MC 版本剔除不再兼容的已选（单选）
    function recomputeLoaders() {
        var kept = []
        for (var i = 0; i < chosenLoaders.length; i++) {
            if (createServer.loaderCompatible(chosenLoaders[i], createServer.currentVersion))
                kept.push(chosenLoaders[i])
        }
        chosenLoaders = kept
        createServer.selectedLoaders = kept
    }

    function resetAndOpen() {
        nameUserEdited = false
        createServer.reset()
        chosenLoaders = []
        importMode = false
        importZipPath = ""
        nameSyncing = true
        nameField.text = createServer.name
        pathField.text = createServer.saveDir
        nameSyncing = false
        validatePath()
        javaPathField.text = javaManager.manualJavaHome()
        typeBox.currentIndex = 0
        verBox.currentIndex = 0
        eulaBox.checked = createServer.eulaAccepted
        window.show()
        enterAnim.start()
        window.requestActivate()
    }

    FolderDialog {
        id: dirDialog
        // 定位到当前目录的父级（只让用户选位置，不选具体文件夹名）
        currentFolder: "file:///" + parentDir
        onAccepted: {
            var parentPath = selectedFolder.toString().replace(/^file:\/\/\/?/, "")
            // 保留当前文件夹名（用户在名称框里改的内容不影响实际文件夹位置）
            var sep = Math.max(createServer.saveDir.lastIndexOf("/"), createServer.saveDir.lastIndexOf("\\"))
            var folderName = sep > 0 ? createServer.saveDir.substring(sep + 1) : ""
            if (!folderName)
                folderName = createServer.name.replace(/[\\/:*?\"<>|]/g, "_")
            createServer.saveDir = parentPath + "/" + folderName
            pathField.text = createServer.saveDir
        }
    }

    FolderDialog {
        id: javaDirDialog
        currentFolder: "file:///" + (javaManager.manualJavaHome() || "C:/")
        onAccepted: {
            var p = selectedFolder.toString().replace(/^file:\/\/\/?/, "")
            javaManager.setManualJavaHome(p)
            javaPathField.text = p
        }
    }

    FileDialog {
        id: zipDialog
        title: I18n.t("选择服务器压缩包", I18n.lang)
        nameFilters: [I18n.t("压缩包 (*.zip)", I18n.lang)]
        fileMode: FileDialog.OpenFile
        onAccepted: {
            var p = selectedFile.toString().replace(/^file:\/\/\/?/, "")
            importZipPath = p
            zipPathField.text = p
        }
    }

    // 版本列表异步到达后刷新下拉框
    Connections {
        target: createServer
        function onVersionsChanged() {
            verBox.model = createServer.versions
            if (verBox.model.length > 0) {
                verBox.currentIndex = Math.max(0, verBox.currentIndex)
                createServer.currentVersion = verBox.currentText
            }
        }
        function onNameChanged() {
            nameSyncing = true
            nameField.text = createServer.name
            pathField.text = createServer.saveDir
            nameSyncing = false
        }
        function onSaveDirChanged() {
            pathField.text = createServer.saveDir
            validatePath()
        }
    }

    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.bg
        clip: true
        // 入场滑动动画：从右侧滑入 + 淡入（挂在内层 frame 上，Window 本身不支持 transform）
        x: 60
        opacity: 0
        ParallelAnimation {
            id: enterAnim
            NumberAnimation { target: frame; property: "x"; from: 60; to: 0; duration: 220; easing.type: Easing.OutCubic }
            NumberAnimation { target: frame; property: "opacity"; from: 0; to: 1; duration: 220; easing.type: Easing.OutCubic }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            TitleBar {
                title: I18n.t("创建服务器", I18n.lang)
            window: window
            showDownloads: false
            Layout.fillWidth: true
        }

        Flickable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.topMargin: 16
            contentHeight: formCol.implicitHeight
            clip: true
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: formCol
                width: parent.width
                spacing: 14

                Label { text: I18n.t("创建方式", I18n.lang); color: Theme.textMuted; font.pixelSize: 12 }
                RowLayout {
                    spacing: 16
                    RadioButton {
                        id: buildRadio
                        text: I18n.t("全新创建", I18n.lang)
                        checked: true
                        contentItem: Text {
                            text: buildRadio.text
                            color: Theme.text
                            font: buildRadio.font
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: buildRadio.indicator.width + buildRadio.spacing
                        }
                        onToggled: if (checked) importMode = false
                    }
                    RadioButton {
                        id: importRadio
                        text: I18n.t("从压缩包导入", I18n.lang)
                        contentItem: Text {
                            text: importRadio.text
                            color: Theme.text
                            font: importRadio.font
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: importRadio.indicator.width + importRadio.spacing
                        }
                        onToggled: if (checked) importMode = true
                    }
                }

                Label { text: I18n.t("服务器类型", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; visible: !importMode }
                ComboBoxEx {
                    id: typeBox
                    visible: !importMode
                    model: createServer.types
                    onActivated: {
                        createServer.currentType = currentText
                        if (currentText !== "模组服") {
                            chosenLoaders = []
                            createServer.selectedLoaders = []
                        }
                        createServer.loadVersions()
                    }
                }

                Label { text: I18n.t("游戏版本", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; visible: !importMode }
                ComboBoxEx {
                    id: verBox
                    visible: !importMode
                    model: createServer.versions
                    onActivated: {
                        createServer.currentVersion = currentText
                        recomputeLoaders()
                    }
                }

                // 模组服：选择要安装的加载器（单选，仅兼容当前版本的可选）
                Label {
                    visible: !importMode && createServer.currentType === "模组服"
                    text: I18n.t("选择加载器（请选择一个）", I18n.lang)
                    color: Theme.textMuted
                    font.pixelSize: 12
                }
                ColumnLayout {
                    visible: !importMode && createServer.currentType === "模组服"
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: createServer.modLoaders()
                        RadioButton {
                            Layout.fillWidth: true
                            text: createServer.loaderLabel(modelData)
                            enabled: createServer.loaderCompatible(modelData, createServer.currentVersion)
                            checked: chosenLoaders.length === 1 && chosenLoaders[0] === modelData
                            contentItem: Text {
                                text: parent.text
                                color: parent.enabled ? Theme.text : Theme.textMuted
                                font: parent.font
                                verticalAlignment: Text.AlignVCenter
                                leftPadding: parent.indicator.width + parent.spacing
                            }
                            onToggled: {
                                // 单选语义：选中的放入数组（仅一个），未选中则清空。
                                if (checked)
                                    chosenLoaders = [modelData]
                                else if (chosenLoaders.length === 1 && chosenLoaders[0] === modelData)
                                    chosenLoaders = []
                                createServer.selectedLoaders = chosenLoaders
                            }
                        }
                    }
                }
                Label {
                    id: enabledLoadersNote
                    visible: !importMode && createServer.currentType === "模组服"
                    text: {
                        var sel = createServer.selectedLoaders
                        if (sel.length === 0) return I18n.t("未选择任何加载器，请在下方选择一个。", I18n.lang)
                        return I18n.t("已选：", I18n.lang) + sel.map(function(k){ return createServer.loaderLabel(k) }).join("、")
                    }
                    color: Theme.textMuted
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                }

                Label { text: I18n.t("服务器名称", I18n.lang); color: Theme.textMuted; font.pixelSize: 12 }
                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    color: Theme.text
                    selectionColor: Theme.accent
                    background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                    onTextChanged: {
                        if (nameSyncing) return
                        nameUserEdited = true
                        createServer.name = text
                    }
                }

                Label { text: I18n.t("保存路径", I18n.lang); color: Theme.textMuted; font.pixelSize: 12 }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    TextField {
                        id: pathField
                        Layout.fillWidth: true
                        readOnly: true
                        color: Theme.text
                        placeholderTextColor: Theme.textMuted
                        selectionColor: Theme.accent
                        background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                    }
                    Button {
                        text: I18n.t("浏览", I18n.lang)
                        flat: true
                        palette.windowText: Theme.text
                        palette.buttonText: Theme.text
                        background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                        onClicked: dirDialog.open()
                    }
                }

                Label {
                    visible: pathError !== ""
                    text: pathError
                    color: Theme.danger
                    font.pixelSize: 11
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                // 压缩包导入模式：选择已下载/已打包的服务器压缩包
                Label {
                    visible: importMode
                    text: I18n.t("压缩包 (.zip)", I18n.lang)
                    color: Theme.textMuted
                    font.pixelSize: 12
                }
                RowLayout {
                    visible: importMode
                    Layout.fillWidth: true
                    spacing: 8
                    TextField {
                        id: zipPathField
                        Layout.fillWidth: true
                        readOnly: true
                        placeholderText: I18n.t("选择要导入的服务器压缩包", I18n.lang)
                        color: Theme.text
                        placeholderTextColor: Theme.textMuted
                        selectionColor: Theme.accent
                        background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                    }
                    Button {
                        text: I18n.t("选择", I18n.lang)
                        palette.buttonText: Theme.text; palette.windowText: Theme.text
                        onClicked: zipDialog.open()
                        background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    }
                }

                Label { text: I18n.t("内存分配 (MB)", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; visible: !importMode }
                RowLayout {
                    visible: !importMode
                    Layout.fillWidth: true
                    spacing: 8
                    Item {
                        id: memSpin
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        property int value: 2048
                        // 大整体：四角圆角
                        Rectangle {
                            anchors.fill: parent
                            radius: 6
                            color: Theme.panelAlt
                            border.color: Theme.border
                            clip: true
                            RowLayout {
                                anchors.fill: parent
                                spacing: 0
                                // 数字显示 / 输入
                                TextInput {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    Layout.preferredWidth: 14
                                    leftPadding: 10
                                    text: memSpin.value
                                    color: Theme.text
                                    verticalAlignment: Text.AlignVCenter
                                    horizontalAlignment: Qt.AlignLeft
                                    selectByMouse: true
                                    validator: IntValidator { bottom: 512; top: 32768 }
                                    onEditingFinished: {
                                        var v = parseInt(text, 10)
                                        if (!isNaN(v)) {
                                            v = Math.min(32768, Math.max(512, v))
                                            memSpin.value = v
                                        }
                                    }
                                }
                                // 数字区与按键组之间的分隔线
                                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: Theme.border }
                                // 按键组
                                ColumnLayout {
                                    spacing: 0
                                    Layout.fillHeight: true
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 1
                                    Layout.minimumWidth: 22
                                    Layout.maximumWidth: 28
                                    Button {
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        flat: true
                                        background: Rectangle { color: parent.hovered ? Theme.accentSoft : "transparent" }
                                        onClicked: stepPort(1)
                                        contentItem: Text { text: "+"; color: Theme.text; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.border }
                                    Button {
                                        Layout.fillWidth: true; Layout.fillHeight: true
                                        flat: true
                                        background: Rectangle { color: parent.hovered ? Theme.accentSoft : "transparent" }
                                        onClicked: stepPort(-1)
                                        contentItem: Text { text: "−"; color: Theme.text; font.bold: true; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    }
                                }
                            }
                        }
                        function stepPort(d) {
                            var v = Math.min(32768, Math.max(512, memSpin.value + d * 512))
                            memSpin.value = v
                        }
                    }
                    Label { text: memSpin.value + " MB"; color: Theme.textMuted }
                }

                Label { text: I18n.t("Java 路径（可选，留空则自动探测本机 JDK 或下载）", I18n.lang); color: Theme.textMuted; font.pixelSize: 12; visible: !importMode }
                RowLayout {
                    visible: !importMode
                    Layout.fillWidth: true
                    spacing: 8
                    TextField {
                        id: javaPathField
                        Layout.fillWidth: true
                        placeholderText: I18n.t("例如：C:/Program Files/Java/jdk-21 或 …/bin/java.exe", I18n.lang)
                        color: Theme.text
                        placeholderTextColor: Theme.textMuted
                        selectionColor: Theme.accent
                        background: Rectangle { color: Theme.panelAlt; radius: 6; border.color: Theme.border }
                        onEditingFinished: javaManager.setManualJavaHome(text)
                    }
                    Button {
                        text: I18n.t("选择", I18n.lang)
                        palette.buttonText: Theme.text; palette.windowText: Theme.text
                        onClicked: javaDirDialog.open()
                        background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    CheckBox {
                        id: eulaBox
                        text: I18n.t("我已阅读并同意 Minecraft EULA", I18n.lang)
                        Layout.fillWidth: true
                        contentItem: Text {
                            text: eulaBox.text
                            color: Theme.text
                            font: eulaBox.font
                            wrapMode: Text.Wrap
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: eulaBox.indicator.width + eulaBox.spacing
                        }
                        onToggled: createServer.eulaAccepted = checked
                    }
                    Button {
                        text: I18n.t("查看", I18n.lang)
                        palette.buttonText: Theme.text; palette.windowText: Theme.text
                        onClicked: Qt.openUrlExternally("https://www.minecraft.net/eula")
                        background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
                    }
                }

                // 下载进度
                ProgressBar {
                    Layout.fillWidth: true
                    visible: createServer.busy
                    value: createServer.progress / 100
                    from: 0
                    to: 1
                }

                Label {
                    visible: createServer.statusText !== ""
                    text: createServer.statusText
                    color: createServer.done ? Theme.success : Theme.textMuted
                    font.pixelSize: 12
                    wrapMode: Text.Wrap
                }
            }
        }

        // 底部操作栏
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 16
            Layout.topMargin: 8
            spacing: 10
            Button {
                text: I18n.t("取消", I18n.lang)
                Layout.fillWidth: true
                palette.buttonText: Theme.text; palette.windowText: Theme.text
                onClicked: window.close()
                background: Rectangle { color: parent.hovered ? Theme.panel : Theme.bg; radius: 6; border.color: Theme.border }
            }
            Button {
                text: createServer.done ? I18n.t("完成", I18n.lang) : (importMode ? I18n.t("导入压缩包", I18n.lang) : I18n.t("下载并安装", I18n.lang))
                Layout.fillWidth: true
                enabled: !createServer.busy && (!importMode || importZipPath !== "") && pathError === ""
                palette.buttonText: "white"; palette.windowText: "white"
                onClicked: {
                    if (createServer.done) window.close()
                    else if (importMode) createServer.importZip(importZipPath)
                    else createServer.create()
                }
                background: Rectangle { color: parent.enabled ? (parent.hovered ? Theme.accentHover : Theme.accent) : Theme.panelAlt; radius: 6 }
            }
        }
        }
    }
}
