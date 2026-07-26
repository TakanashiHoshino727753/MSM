// ============================================================================
// DownloadCenter.qml —— 下载中心窗口
// ----------------------------------------------------------------------------
// 职责：作为“资源下载”的统一入口，提供 6 个主分类（Java / 服务端 / 模组 /
//       整合包 / 资源包 / 插件）的浏览与下载；其中“服务端”下再分 Paper /
//       Vanilla / 模组服三种子类型，模组服为特殊的多加载器面板（可对多个加载器
//       分别打包成独立服务端压缩包）。
// 数据来源：所有列表数据与下载动作均由 C++ 侧的 downloadCatalog（内容目录/模型）
//       与 installCoordinator（打包协调器）驱动，本文件只负责 UI 与交互。
// 关键约定：
//   - downloadCatalog 同时充当 ListView 的 model（见底部 ListView.model）。
//   - currentKey 决定当前主分类；serverType 决定服务端子类型。
//   - 文案统一走 I18n.t(原文, 当前语言)，语言变化时通过 Binding 实时回传 C++。
// ============================================================================
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import MinecraftServerManager

ApplicationWindow {
    id: window
    width: 920
    height: 640
    title: I18n.t("下载中心", I18n.lang)
    color: "transparent"                              // 透明底色，配合下方带圆角的 frame 实现圆角窗口
    flags: Qt.Window | Qt.FramelessWindowHint         // 无系统边框：标题栏由自定义 TitleBar 提供

    // 主分类定义：key 传给 C++（downloadCatalog.currentKey），label 为界面显示文案（随语言翻译）
    property var categories: [
        { key: "java",         label: I18n.t("Java", I18n.lang) },
        { key: "server",       label: I18n.t("服务端", I18n.lang) },
        { key: "mod",          label: I18n.t("模组", I18n.lang) },
        { key: "modpack",      label: I18n.t("整合包", I18n.lang) },
        { key: "resourcepack", label: I18n.t("资源包", I18n.lang) },
        { key: "plugin",       label: I18n.t("插件", I18n.lang) }
    ]
    // 服务端子类型：仅当主分类为“服务端”时才展示这一行选择
    property var serverTypes: [
        { key: "paper",   label: I18n.t("Paper", I18n.lang) },
        { key: "vanilla", label: I18n.t("Vanilla", I18n.lang) },
        { key: "mod",     label: I18n.t("模组服", I18n.lang) }
    ]
    // 缓存加载器列表：modLoaders() 是 C++ 侧不变的固定列表，缓存一次避免 Repeater 反复调用 invokable
    property var loaderList: []

    // ------------------------------------------------------------------------
    // CatTab：顶部“主分类行”和“服务端子类型行”共用的内联 tab 按钮组件。
    //   - 把选中态判断收敛为单个 active 属性绑定（原先每按钮在样式里要各求值 5 次），
    //     切换分类时的绑定求值量降到 1/5。
    //   - tabLabel：显示文案；active：是否为当前选中项；cellHeight：按钮高度（两行略有不同）。
    // ------------------------------------------------------------------------
    component CatTab: Button {
        id: catTab
        property string tabLabel: ""
        property bool active: false
        property int cellHeight: 36
        Layout.fillWidth: true                        // 平分整行宽度
        Layout.preferredHeight: cellHeight
        flat: true                                    // 去掉原生按钮底，完全用自绘背景
        // 背景：选中态用主色柔和底 + 主色描边，未选中透明
        background: Rectangle {
            color: catTab.active ? Theme.accentSoft : "transparent"
            radius: 6
            border.color: catTab.active ? Theme.accent : "transparent"
            border.width: catTab.active ? 1 : 0
        }
        // 文字：选中态主色加粗，未选中弱化色
        contentItem: Text {
            text: catTab.tabLabel
            color: catTab.active ? Theme.accent : Theme.textMuted
            font.bold: catTab.active
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    // 初始化：同步语言、置默认主分类/子类型、缓存加载器列表，最后拉取一次数据
    Component.onCompleted: {
        downloadCatalog.language = I18n.lang
        downloadCatalog.currentKey = categories[0].key   // 默认选中第一个主分类（Java）
        downloadCatalog.serverType = serverTypes[0].key  // 服务端默认子类型（Paper）
        loaderList = downloadCatalog.modLoaders()         // 固定列表，缓存一次即可
        downloadCatalog.refresh()                         // 首次加载对应分类的内容
    }

    // 语言变化时实时同步给 C++ 下载目录，使其状态/错误文案随界面翻译
    Binding { target: downloadCatalog; property: "language"; value: I18n.lang }

    // 搜索防抖：文本每次变化都 restart 该计时器，只有停止输入 400ms 后才真正 refresh，
    // 避免逐字符敲击时频繁发起网络请求。
    Timer { id: searchTimer; interval: 400; onTriggered: downloadCatalog.refresh() }

    // 保存目录选择对话框：把 file:/// URL 还原为本地路径写回 saveDir
    FolderDialog {
        id: saveDirDialog
        currentFolder: "file:///" + downloadCatalog.saveDir
        onAccepted: downloadCatalog.saveDir = selectedFolder.toString().replace(/^file:\/\/\/?/, "")
    }

    // frame：带圆角的窗口主体。窗口本身透明，靠此矩形绘制背景并 clip 出圆角外观。
    Rectangle {
        id: frame
        anchors.fill: parent
        radius: Theme.radius
        color: Theme.bg
        clip: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            TitleBar {
                title: I18n.t("下载中心", I18n.lang)
            window: window
            Layout.fillWidth: true
            onDownloadsClicked: downloadsPanel.open()
        }

        // 工具栏：主分类一行 + 服务端子类型一行 + 搜索/目录一行
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6

            // 第一行：主分类（Java / 服务端 / 模组 / 整合包 / 资源包 / 插件）独占一行
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.topMargin: 12
                spacing: 8
                Repeater {
                    model: categories
                    CatTab {
                        tabLabel: modelData.label
                        active: downloadCatalog.currentKey === modelData.key
                        onClicked: {
                            downloadCatalog.currentKey = modelData.key
                            downloadCatalog.refresh()
                        }
                    }
                }
            }

            // 第二行：服务端子类型（仅服务端页可见，单独成行）
            RowLayout {
                visible: downloadCatalog.currentKey === "server"
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                spacing: 8
                Label { text: I18n.t("类型", I18n.lang); color: Theme.textMuted; font.bold: true }
                Repeater {
                    model: serverTypes
                    CatTab {
                        tabLabel: modelData.label
                        cellHeight: 34
                        active: downloadCatalog.serverType === modelData.key
                        onClicked: {
                            downloadCatalog.serverType = modelData.key
                            downloadCatalog.refresh()
                        }
                    }
                }
            }

            // 第三行：搜索框 + 保存目录
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 4
                spacing: 10

                Item { Layout.fillWidth: true }

                // 搜索框（Java / 服务端页是固定列表，无需搜索，故隐藏）
                TextField {
                    visible: downloadCatalog.currentKey !== "java"
                             && downloadCatalog.currentKey !== "server"
                    Layout.preferredWidth: 200
                    placeholderText: I18n.t("搜索…", I18n.lang)
                    background: Rectangle {
                        color: Theme.panelAlt
                        radius: 6
                        border.color: Theme.border
                    }
                    color: Theme.text
                    placeholderTextColor: Theme.textMuted
                    selectionColor: Theme.accent
                    // 每次输入即写回关键字并重启防抖计时器（真正刷新延迟到停止输入后）
                    onTextChanged: {
                        downloadCatalog.searchText = text
                        searchTimer.restart()
                    }
                }

                // 保存目录
                RowLayout {
                    spacing: 6
                    Button {
                        text: I18n.t("保存目录", I18n.lang)
                        flat: true
                        onClicked: saveDirDialog.open()
                        contentItem: Label {
                            text: parent.text
                            color: Theme.text
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: parent.hovered ? Theme.panelAlt : "transparent"
                            radius: Theme.radius
                        }
                    }
                    Label {
                        text: downloadCatalog.saveDir
                        color: Theme.textMuted
                        elide: Text.ElideMiddle
                        Layout.preferredWidth: 180
                    }
                }

                // Java 区：临时静默准备的 Java 用完后可一键清理
                Button {
                    text: I18n.t("清理临时 Java", I18n.lang)
                    visible: downloadCatalog.currentKey === "java"
                    onClicked: downloadCatalog.cleanupTempJava()
                    contentItem: Label {
                        text: parent.text
                        color: Theme.text
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        color: parent.hovered ? Theme.panelAlt : "transparent"
                        radius: Theme.radius
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.leftMargin: 16; Layout.rightMargin: 16; Layout.preferredHeight: 1; color: Theme.border }

        // 模组服多加载器面板（仅服务端=模组服时显示）
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 8
            spacing: 10
            visible: downloadCatalog.currentKey === "server" && downloadCatalog.serverType === "mod"

            // MC 版本下拉：currentIndex 通过在 mcReleases 里查找当前 modVersion 定位，
            // 找不到时回退 0（Math.max 兜底避免 -1）
            Label { text: I18n.t("选择 Minecraft 版本", I18n.lang); color: Theme.text; font.bold: true }
            ComboBoxEx {
                model: downloadCatalog.mcReleases
                currentIndex: Math.max(0, downloadCatalog.mcReleases.indexOf(downloadCatalog.modVersion))
                onActivated: downloadCatalog.modVersion = model[index]
            }

            // 加载器选择区：Flow 自动换行排列，每个加载器是一个可选中的卡片按钮
            Label { text: I18n.t("选择加载器（每个加载器会生成独立服务端实例）", I18n.lang); color: Theme.text; font.bold: true }
            Flow {
                Layout.fillWidth: true
                spacing: 12
                Repeater {
                    model: loaderList                 // 使用缓存的加载器列表（见顶部 loaderList）
                    delegate: Button {
                        id: loaderBtn
                        text: downloadCatalog.loaderLabel(modelData)                         // 加载器显示名（Forge/Fabric…）
                        enabled: downloadCatalog.loaderCompatible(modelData, downloadCatalog.modVersion)  // 与所选 MC 版本不兼容则禁用
                        checkable: true
                        checked: downloadCatalog.modLoaderType === modelData                 // 是否为当前选中加载器
                        onClicked: downloadCatalog.modLoaderType = modelData

                        readonly property bool selected: checked   // 别名：供下方样式统一引用，避免重复写 checked
                        implicitHeight: 44
                        leftPadding: 20
                        rightPadding: 20
                        hoverEnabled: true

                        // 背景卡片：选中(主色描边+柔和底) / hover(变亮) / 普通 / 禁用(降透明度) 四态
                        background: Rectangle {
                            radius: Theme.radius
                            color: loaderBtn.selected ? Theme.accentSoft
                                   : (loaderBtn.hovered ? Theme.panelAlt : Theme.panel)
                            border.width: loaderBtn.selected ? 2 : 1
                            border.color: loaderBtn.selected ? Theme.accent
                                          : (loaderBtn.hovered ? Theme.accentHover : Theme.border)
                            opacity: loaderBtn.enabled ? 1 : 0.45
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Behavior on border.color { ColorAnimation { duration: 120 } }

                            // 选中态左侧强调竖条
                            Rectangle {
                                anchors.left: parent.left
                                anchors.leftMargin: 6
                                anchors.verticalCenter: parent.verticalCenter
                                width: 3
                                height: parent.height * 0.5
                                radius: 2
                                color: Theme.accent
                                visible: loaderBtn.selected
                            }
                        }

                        contentItem: Text {
                            text: loaderBtn.text
                            font.pixelSize: 14
                            font.bold: loaderBtn.selected
                            color: loaderBtn.enabled
                                   ? (loaderBtn.selected ? Theme.accent : Theme.text)
                                   : Theme.textMuted
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            leftPadding: loaderBtn.selected ? 6 : 0
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }
                    }
                }
            }

            // 已选加载器的操作区：仅当选中了某个加载器（modLoaderType 非空）时显示
            ColumnLayout {
                visible: downloadCatalog.modLoaderType !== ""
                Layout.fillWidth: true
                spacing: 8
                // 概要：已选加载器名 + 目标 MC 版本
                Label {
                    text: I18n.t("已选加载器：", I18n.lang) + downloadCatalog.loaderLabel(downloadCatalog.modLoaderType) + "  ·  Minecraft " + downloadCatalog.modVersion
                    color: Theme.text
                }
                RowLayout {
                    spacing: 10
                    // 主操作：下载安装器（主色实心按钮）
                    Button {
                        id: dlLoaderBtn
                        text: I18n.t("下载安装器", I18n.lang)
                        onClicked: downloadCatalog.downloadLoader(downloadCatalog.modLoaderType)
                        implicitHeight: 36
                        leftPadding: 18; rightPadding: 18
                        hoverEnabled: true
                        icon.source: "qrc:/icon/download"
                        icon.color: "#ffffff"
                        icon.width: 15; icon.height: 15
                        palette.buttonText: "white"; palette.windowText: "white"   // 确保文字在主色底上为白
                        background: Rectangle {
                            radius: Theme.radius
                            color: dlLoaderBtn.hovered ? Theme.accentHover : Theme.accent
                            Behavior on color { ColorAnimation { duration: 120 } }
                        }
                    }
                    // 次操作：打包到下载文件夹（描边次级按钮）
                    Button {
                        id: packLoaderBtn
                        text: I18n.t("打包到下载文件夹", I18n.lang)
                        onClicked: installCoordinator.install(downloadCatalog.modLoaderType, downloadCatalog.modVersion, downloadCatalog.loaderLabel(downloadCatalog.modLoaderType))
                        implicitHeight: 36
                        leftPadding: 18; rightPadding: 18
                        hoverEnabled: true
                        palette.buttonText: Theme.text; palette.windowText: Theme.text
                        background: Rectangle {
                            radius: Theme.radius
                            color: packLoaderBtn.hovered ? Theme.panelAlt : Theme.panel
                            border.width: 1
                            border.color: packLoaderBtn.hovered ? Theme.accentHover : Theme.border
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Behavior on border.color { ColorAnimation { duration: 120 } }
                        }
                    }
                }
                Label {
                    text: I18n.t("仅打包到下载文件夹，不会加入服务器列表。每个加载器会生成独立压缩包（<名称>-<版本>.zip）到下载目录；之后可在“创建服务器”中导入该压缩包。", I18n.lang)
                    wrapMode: Text.Wrap
                    color: Theme.textMuted
                    font.pointSize: 10
                    Layout.fillWidth: true
                }
                // 打包进度：总进度 + 当前阶段。进度按“准备 Java / 各加载器 / 打包”分阶段推进，
                // 不再直接跳到 100%。
                ColumnLayout {
                    visible: installCoordinator.busy || installCoordinator.progress > 0
                    Layout.fillWidth: true
                    spacing: 4
                    Label {
                        text: installCoordinator.stageText !== ""
                              ? installCoordinator.stageText
                              : (installCoordinator.busy ? I18n.t("打包中…", I18n.lang) : I18n.t("已完成", I18n.lang))
                        color: Theme.text
                        font.pointSize: 10
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    ProgressBar {
                        Layout.fillWidth: true
                        from: 0
                        to: 100
                        value: installCoordinator.progress
                    }
                    Label {
                        text: I18n.t("总进度", I18n.lang) + " " + Math.round(installCoordinator.progress) + "%"
                        color: Theme.textMuted
                        font.pointSize: 10
                    }
                    // 当前阶段子进度（阶段进行时显示）
                    RowLayout {
                        visible: installCoordinator.stageText !== ""
                        spacing: 6
                        ProgressBar {
                            Layout.fillWidth: true
                            from: 0
                            to: 100
                            value: installCoordinator.stageProgress
                        }
                        Label {
                            text: Math.round(installCoordinator.stageProgress) + "%"
                            color: Theme.textMuted
                            font.pointSize: 10
                        }
                    }
                }
                Label {
                    visible: downloadCatalog.status !== ""
                    text: downloadCatalog.status
                    wrapMode: Text.Wrap
                    color: Theme.accent
                    Layout.fillWidth: true
                }
            }
            Item { Layout.fillHeight: true }
        }

        // 通用资源列表：除“模组服”外的所有分类都用它展示可下载条目。
        // model 直接绑定 downloadCatalog（C++ 侧同时实现了列表模型角色：title/subtitle/errorText）。
        ListView {
            visible: !(downloadCatalog.currentKey === "server" && downloadCatalog.serverType === "mod")  // 模组服走上面的专用面板
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 8
            spacing: 8
            clip: true
            model: downloadCatalog
            // 单个条目：左侧标题/副标题/错误信息，右侧圆形下载按钮
            delegate: Rectangle {
                width: ListView.view.width
                height: 64
                radius: Theme.radius
                color: Theme.panel
                border.color: Theme.border

                ColumnLayout {
                    anchors.left: parent.left
                    anchors.leftMargin: 12
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 12
                    anchors.right: dlBtn.left
                    anchors.rightMargin: 12
                    spacing: 4
                    Label {
                        text: model.title
                        color: Theme.text
                        font.bold: true
                        font.pixelSize: 14
                    }
                    Label {
                        text: I18n.t(model.subtitle, I18n.lang)
                        color: Theme.textMuted
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                    // 状态行：仅非 Java 行显示“下载中…/已准备/错误”。
                    // Java 行（url 以 java:// 开头）不显示任何状态文字——进度统一交给“下载任务”面板指示。
                    Label {
                        visible: !model.url.startsWith("java://") && (model.downloading || model.done || model.errorText !== "")
                        text: model.downloading ? I18n.t("下载中…", I18n.lang)
                                                : model.done ? I18n.t("已准备", I18n.lang)
                                                             : model.errorText
                        color: model.errorText !== "" ? Theme.danger
                             : model.done ? Theme.ok : Theme.textMuted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                // 右侧下载按钮：点击对该行索引发起下载
                Button {
                    id: dlBtn
                    anchors.right: parent.right
                    anchors.rightMargin: 18
                    anchors.verticalCenter: parent.verticalCenter
                    width: 40; height: 40
                    enabled: !model.downloading
                    icon.source: model.done ? "qrc:/icon/redownload" : "qrc:/icon/download"
                    icon.color: "#ffffff"
                    icon.width: 20; icon.height: 20
                    ToolTip.text: model.url.startsWith("java://")
                                 ? (model.done ? I18n.t("重新下载", I18n.lang)
                                               : I18n.t("下载", I18n.lang))
                                 : (model.downloading ? I18n.t("下载中…", I18n.lang)
                                                      : (model.done ? I18n.t("重新下载", I18n.lang)
                                                                    : I18n.t("下载", I18n.lang)))
                    ToolTip.visible: hovered
                    background: Rectangle {
                        radius: Theme.radius
                        color: parent.hovered ? Theme.accentHover : Theme.accent
                    }
                    onClicked: downloadCatalog.download(index)   // index 为当前 delegate 的行号
                }
            }
        }
        }
    }

    // 全局下载任务面板（右侧抽屉）：由标题栏“下载”按钮 onDownloadsClicked 打开
    DownloadsPanel { id: downloadsPanel }
}
