#include <QApplication>
#include <functional>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QJSEngine>
#include <QSettings>
#include <QString>
#include <QQmlContext>
#include <QQmlComponent>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QQuickWindow>
#include <QEvent>
#include <QHash>
#include <QTimer>
#include <QPalette>
#include <QStyleFactory>
#include <QColor>
#include <QtQuickControls2/QQuickStyle>
#include <QLibraryInfo>
#ifdef Q_OS_LINUX
#include <sys/sysinfo.h>   // SystemMonitor 内存回退检测用
#endif
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QNetworkProxyFactory>

#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QVector>
#include <QPair>
#include <QFile>
#include <QStringList>

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QScreen>
#include <QMouseEvent>
#include <QPainter>



#include "downloadmanager.h"
#include "servermanager.h"
#include "downloadcatalog.h"
#include "createservercontroller.h"
#include "modpackimporter.h"
#include "servercontroller.h"
#include "javamanager.h"
#include "settingscontroller.h"
#include "webuiserver.h"
#include "botcontroller.h"
#include "proxycontroller.h"
#include "proxymanager.h"
#include "notifier.h"
#include "backupcontroller.h"
#include "schedulercontroller.h"
#include "updatecontroller.h"
#include "portmapper.h"

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <cstdio>
#include <cstdarg>
#include <QDirIterator>
#include <QDebug>
#include <QFile>
#include <QDateTime>
#include <QMessageLogContext>

// 文件日志：日志常态化开启，固定写入程序同目录下的 logs/msm.log
// （首次写入时自动创建 logs 子目录）。用于无控制台（schtasks/session 0）及日常排错。
// Debug 构建额外把日志回显到控制台；Release/MinSizeRel 构建仅写文件、不污染 cmd（见下方 QT_DEBUG 门控）。
static QString msmLogDir()
{
    QString dir = QCoreApplication::instance()
                      ? QCoreApplication::applicationDirPath()
                      : QDir::currentPath();
    return dir + QStringLiteral("/logs");
}
static QFile g_logFile;
// 确保日志文件已打开（首次调用时创建 logs 子目录并打开 msm.log）
static void msmEnsureLogFile()
{
    if (g_logFile.isOpen())
        return;
    QDir().mkpath(msmLogDir());
    g_logFile.setFileName(msmLogDir() + QStringLiteral("/msm.log"));
    (void)g_logFile.open(QIODevice::Append | QIODevice::Text);
}
// 把一行（不含换行）追加到日志文件
static void msmAppendLog(const QByteArray &line)
{
    msmEnsureLogFile();
    if (g_logFile.isOpen()) {
        g_logFile.write(line);
        g_logFile.write("\n");
        g_logFile.flush();
    }
}
static void msmMessageOutput(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const char *level = "?";
    switch (type) {
        case QtDebugMsg: level = "DBG"; break;
        case QtInfoMsg:  level = "INF"; break;
        case QtWarningMsg: level = "WRN"; break;
        case QtCriticalMsg: level = "CRT"; break;
        case QtFatalMsg: level = "FTL"; break;
    }
    QByteArray line = QDateTime::currentDateTime()
                          .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")).toLocal8Bit();
    line += ' '; line += level; line += ' ';
    if (ctx.category && ctx.category[0]) { line += ctx.category; line += ' '; }
    line += msg.toLocal8Bit();
    msmAppendLog(line);
    // 控制台回显：仅 Debug 构建输出到 cmd；Release 构建不污染控制台，日志只进 msm.log。
    // 使用 Qt 自带的 QT_DEBUG 宏（Release/MinSizeRel 下未定义 QT_DEBUG）做编译期限制。
#ifdef QT_DEBUG
    qt_message_output(type, ctx, msg);
#endif
}

// 全局语言切换：本地端 QML 已全面使用 qsTr()，此处用一个读取 JSON 词典的自定义
// 翻译器，在运行时按 settingsController.language 安装/卸载，实现"简体中文 / English"实时互译。
// 词典为 { "中文原文": "English" }，缺失项回退到中文原文，因此未翻译文案不会报错。
// 根据深浅色 + 主色调构建并应用全局调色板：原生控件（托盘菜单/Fusion 样式）
// 随之变色；同时写入 QSettings 持久化，供下次启动加载（见 main）。
static void applyTheme(bool dark, const QColor &accent)
{
    QSettings settings;
    settings.setValue(QStringLiteral("theme/dark"), dark);
    settings.setValue(QStringLiteral("theme/accent"), accent);

    QPalette pal = QApplication::palette();
    if (dark) {
        pal.setColor(QPalette::Window, QColor("#1b1c21"));
        pal.setColor(QPalette::WindowText, QColor("#e8e8ec"));
        pal.setColor(QPalette::Base, QColor("#2c2f39"));
        pal.setColor(QPalette::AlternateBase, QColor("#24262e"));
        pal.setColor(QPalette::Text, QColor("#e8e8ec"));
        pal.setColor(QPalette::Button, QColor("#24262e"));
        pal.setColor(QPalette::ButtonText, QColor("#e8e8ec"));
        pal.setColor(QPalette::ToolTipBase, QColor("#24262e"));
        pal.setColor(QPalette::ToolTipText, QColor("#e8e8ec"));
    } else {
        pal.setColor(QPalette::Window, QColor("#f2f3f5"));
        pal.setColor(QPalette::WindowText, QColor("#1b1c21"));
        pal.setColor(QPalette::Base, QColor("#ffffff"));
        pal.setColor(QPalette::AlternateBase, QColor("#e9eaee"));
        pal.setColor(QPalette::Text, QColor("#1b1c21"));
        pal.setColor(QPalette::Button, QColor("#ffffff"));
        pal.setColor(QPalette::ButtonText, QColor("#1b1c21"));
        pal.setColor(QPalette::ToolTipBase, QColor("#ffffff"));
        pal.setColor(QPalette::ToolTipText, QColor("#1b1c21"));
    }
    pal.setColor(QPalette::Highlight, accent);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    QApplication::setPalette(pal);
}

// 所有程序逻辑（托盘、窗口管理、退出）都在 C++ 里；QML 只负责画界面
class AppController : public QObject
{
    Q_OBJECT
public:
    explicit AppController(QQmlApplicationEngine *engine, QObject *parent = nullptr)
        : QObject(parent), m_engine(engine)
    {
        loadDict();  // 先加载翻译字典，供 rebuildTrayMenu 使用
        setupTray();
    }

    private:
    void setupTray()
    {
        // 所有平台统一使用系统原生托盘（QSystemTrayIcon）：图标落到通知区。
        // 仅当通知区可用时程序常驻（关闭窗口=收起托盘而非退出）；
        // 通知区不可用时（如无桌面的服务器环境）保持默认行为——关闭窗口即退出，避免程序空跑。
        // 托盘右键菜单（原生托盘使用），按需重建以反映各窗口可见状态
        m_trayMenu = new QMenu;
        connect(m_trayMenu, &QMenu::aboutToShow, this, &AppController::rebuildTrayMenu);
        // 菜单真正隐藏时（其模态嵌套事件循环已退出、控制权回到主循环）再执行退出，
        // 规避“直接 quit 只退出了菜单循环、主程序不退出”的问题。
        connect(m_trayMenu, &QMenu::aboutToHide, this, [this]() {
            if (m_quitRequested) {
                m_quitRequested = false;
                QApplication::quit();
            }
        });

        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            QApplication::setQuitOnLastWindowClosed(false);
            m_tray = new QSystemTrayIcon(this);
            m_tray->setIcon(QIcon(QStringLiteral(":/icon/ApplicationIcon")));
            m_tray->setToolTip(QStringLiteral("Minecraft Server Manager"));
            m_tray->setContextMenu(m_trayMenu);

            // 单击托盘图标：切换主窗口显隐（QQ 式行为）
            connect(m_tray, &QSystemTrayIcon::activated, this,
                    [this](QSystemTrayIcon::ActivationReason reason) {
                        if (reason == QSystemTrayIcon::Trigger)
                            toggleWindow(QStringLiteral("MainWindow"));
                    });

            rebuildTrayMenu();
            m_tray->show();
            m_hasTray = true;
        } else {
            qWarning("系统托盘（通知区）不可用：窗口关闭将直接退出程序"
                     "（如需常驻请安装带通知区的桌面环境，或在桌面环境中启用通知区）");
        }
    }

    QQuickWindow *createWindow(const QString &type)
    {
        QQmlComponent comp(m_engine);
        comp.loadFromModule(QStringLiteral("MinecraftServerManager"), type);
        if (comp.isError()) {
            qWarning() << "加载 QML 类型失败:" << type << comp.errors();
            return nullptr;
        }
        QObject *obj = comp.create(m_engine->rootContext());
        auto *win = qobject_cast<QQuickWindow *>(obj);
        if (!win) {
            qWarning() << type << "不是 Window 类型";
            delete obj;
            return nullptr;
        }

        win->installEventFilter(this);
        m_windows.append(win);
#ifdef Q_OS_WIN
        // 让无边框窗口支持"单击任务栏图标最小化"（标准 Windows 行为）。
        // Qt::FramelessWindowHint 创建的是 WS_POPUP 窗口，Windows 不会自动
        // 处理任务栏点击。切到 WS_OVERLAPPED、补 WS_SYSMENU+WS_MINIMIZEBOX
        // 和 WS_EX_APPWINDOW，Windows 才能正确响应任务栏单击（显隐切换）。
        // 不加 WS_CAPTION / WS_THICKFRAME，因此仍是无边框外观。
        HWND hwnd = reinterpret_cast<HWND>(win->winId());
        LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
        style = (style & ~static_cast<LONG_PTR>(WS_POPUP)) | WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX;
        SetWindowLongPtr(hwnd, GWL_STYLE, style);
        LONG_PTR exStyle = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
        exStyle |= WS_EX_APPWINDOW;
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, exStyle);
        // 样式变更后必须 SetWindowPos + SWP_FRAMECHANGED，Windows 才会重算非客户区
        SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
#endif
        return win;
    }

    QQuickWindow *showUnique(const QString &type)
    {
        QQuickWindow *w = m_unique.value(type, nullptr);
        if (!w) {
            w = createWindow(type);
            if (w) {
                m_unique.insert(type, w);
                QObject::connect(w, &QObject::destroyed, this,
                                 [this, type]() { m_unique.remove(type); });
            }
        }
        if (w) {
            w->show();
            w->raise();
            w->requestActivate();
        }
        return w;
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Close) {
            auto *win = qobject_cast<QQuickWindow *>(watched);
            if (win) {
                const QString mode = win->property("closeMode").toString();
                if (mode == QStringLiteral("close")) {
                    // 显式"关闭"模式：真正退出该窗口
                    m_windows.removeAll(win);
                } else if (m_tray) {
                    // 原生托盘：收起托盘（隐藏窗口，无任务栏条目；由托盘恢复）
                    event->ignore();
                    win->hide();
                    return true;
                } else {
                    m_windows.removeAll(win);
                }
            }
        }
        return QObject::eventFilter(watched, event);
    }

signals:
    void themeApplied(bool dark, const QColor &accent);

public slots:
    void showMainWindow()            { showUnique(QStringLiteral("MainWindow")); }
    // 是否有原生托盘（通知区）可用
    bool hasTray() const { return m_hasTray; }
    // 切换指定窗口：已可见则收起（真实托盘隐藏 / 模拟托盘最小化到任务栏），
    // 已最小化则恢复，否则显示并置前（供托盘单击/菜单分别控制各窗口）
    void toggleWindow(const QString &type)
    {
        QQuickWindow *w = m_unique.value(type, nullptr);
        if (!w) {
            showUnique(type);
            return;
        }
        if (w->windowState() & Qt::WindowMinimized) {
            // 已最小化（模拟托盘场景）：恢复并置前
            w->showNormal();
            w->raise();
            w->requestActivate();
        } else if (w->isVisible()) {
            // 有托盘则收起隐藏；无托盘直接隐藏（窗口关闭即退出）
            w->hide();
        } else {
            showUnique(type);
        }
    }
    void openDownloadCenter()        { showUnique(QStringLiteral("DownloadCenter")); }
    void openCreateServer() {
        QQuickWindow *w = showUnique(QStringLiteral("CreateServerDialog"));
        if (w) QMetaObject::invokeMethod(w, "resetAndOpen");
    }
    void openImportModpack() {
        QQuickWindow *w = showUnique(QStringLiteral("ImportModpackDialog"));
        if (w) QMetaObject::invokeMethod(w, "resetAndOpen");
    }
    void openControllerSettings()    { showUnique(QStringLiteral("ControllerSettings")); }
    void setTheme(bool dark, const QColor &accent);   // 持久化并应用主题（QML 调用）

    void closeAllWindows()
    {
        QList<QQuickWindow *> list = m_windows;
        for (QQuickWindow *win : list) {
            if (!win)
                continue;
            const QString mode = win->property("closeMode").toString();
            if (mode == QStringLiteral("close"))
                win->close();          // 真正关闭
            else if (m_tray)
                win->hide();           // 原生托盘：收起托盘
            else
                win->close();          // 无托盘：关闭窗口（默认 quitOnLastWindowClosed 退出程序）
        }
    }

    void quitApp()
    {
        // 托盘菜单是模态的嵌套事件循环（exec/popup）。若直接在此调 QApplication::quit()，
        // 只会终止“菜单”这一层嵌套循环，最外层 app.exec() 主循环仍在运行——表现就是托盘
        // 图标消失但主程序进程不退出。正确做法：标记退出请求，关闭菜单；菜单 aboutToHide
        // 在其嵌套循环已退出、控制权回到主循环后才发出，那时再 QApplication::quit() 即退出主循环。
        // 另加一个较长延时（>菜单关闭耗时）的兜底 quit，确保即便 aboutToHide 未触发也能退出主循环。
        m_quitRequested = true;
        if (m_trayMenu && m_trayMenu->isVisible())
            m_trayMenu->close();
        else
            doQuit();
        if (m_tray)
            m_tray->hide();
        QTimer::singleShot(500, qApp, [this]() { doQuit(); });
    }

    // 注册退出前的清理回调（释放运行中的子进程等），由 main() 注入；
    // 仅在真正执行退出时调用一次，避免重复触发。
    void setQuitCleanup(std::function<void()> f) { m_cleanup = std::move(f); }

    void doQuit()
    {
        if (m_quitting)
            return;
        m_quitting = true;
        if (m_cleanup)
            m_cleanup();   // 释放所有运行中的子进程（服务器/bot/代理…），避免退出后残留
        QApplication::quit();
    }

private:
    QQmlApplicationEngine *m_engine = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QMenu *m_trayMenu = nullptr;
    bool m_quitRequested = false;     // 托盘菜单“退出”已点击，待菜单关闭后真正退出主循环
    bool m_quitting = false;          // 真正退出流程已触发，防止清理回调/quit 重入
    bool m_hasTray = false;           // 是否已有原生托盘（通知区）可用
    std::function<void()> m_cleanup;  // 退出前清理回调（释放子进程）
    QHash<QString, QQuickWindow *> m_unique;
    QList<QQuickWindow *> m_windows;
    QHash<QString, QString> m_dict;
    SettingsController *m_sc = nullptr;

public:
    void setSettingsController(SettingsController *sc) { m_sc = sc; }

private:
    void loadDict()
    {
        // tray 菜单只用这几条，硬编码避免资源路径问题
        m_dict = {
            {QStringLiteral("隐藏主窗口"), QStringLiteral("Hide Main Window")},
            {QStringLiteral("显示主窗口"), QStringLiteral("Show Main Window")},
            {QStringLiteral("下载中心"),     QStringLiteral("Downloads")},
            {QStringLiteral("创建服务器"),   QStringLiteral("Create Server")},
            {QStringLiteral("导入整合包"),   QStringLiteral("Import Modpack")},
            {QStringLiteral("控制器设置"),   QStringLiteral("Controller Settings")},
            {QStringLiteral("隐藏"),        QStringLiteral("Hide ")},
            {QStringLiteral("关闭所有窗口"), QStringLiteral("Close All Windows")},
            {QStringLiteral("退出"),        QStringLiteral("Exit")},
        };
    }

    bool isWindowVisible(const QString &type) const
    {
        QQuickWindow *w = m_unique.value(type, nullptr);
        return w && w->isVisible();
    }

public:
    // 销毁并重建全部 QML 窗口：让 QQmlComponent 在新的 uiLanguage 下重新创建窗口，
    // 新窗口中的 qsTr() 才会用新安装的翻译器重新求值。
    void recreateAllWindows()
    {
        const QList<QQuickWindow *> list = m_windows;
        m_unique.clear();
        m_windows.clear();
        for (QQuickWindow *w : list) {
            w->close();
            w->deleteLater();
        }
        // 清除 QML 内部组件缓存，否则 QQmlComponent::loadFromModule 仍使用之前缓存的编译结果，
        // qsTr 仍取不到新安装的翻译器。清完后再创建新窗口才会在新语言下重新编译 QML。
        m_engine->clearComponentCache();
        showMainWindow();
    }

    // 重建托盘右键菜单：主窗口单点切换项 + 各子窗口"打开/隐藏"切换 + 关闭全部/退出
    void rebuildTrayMenu()
    {
        QMenu *m = m_trayMenu;
        if (!m)
            return;
        m->clear();
        // 直接查表，不走 QCoreApplication::translate()（避免 Qt 版本差异导致不调用）
        auto T = [this](const QString &zh) -> QString {
            if (m_sc && m_sc->language() == QStringLiteral("English")) {
                auto it = m_dict.constFind(zh);
                return it != m_dict.constEnd() ? it.value() : zh;
            }
            return zh;  // 中文模式直接返回原文
        };
        QAction *a = m->addAction(isWindowVisible(QStringLiteral("MainWindow"))
                                  ? T(QStringLiteral("隐藏主窗口"))
                                  : T(QStringLiteral("显示主窗口")));
        connect(a, &QAction::triggered, this,
                [this]() { toggleWindow(QStringLiteral("MainWindow")); });
        m->addSeparator();

        // 多窗口分别控制：已打开则"隐藏"，未打开则"打开"
        const QVector<QPair<QString, QString>> entries = {
            { QStringLiteral("DownloadCenter"),      QStringLiteral("下载中心") },
            { QStringLiteral("CreateServerDialog"),  QStringLiteral("创建服务器") },
            { QStringLiteral("ImportModpackDialog"), QStringLiteral("导入整合包") },
            { QStringLiteral("ControllerSettings"),   QStringLiteral("控制器设置") },
        };
        for (const auto &e : entries) {
            const bool vis = isWindowVisible(e.first);
            const QString label = T(e.second);
            QAction *a2 = m->addAction(vis ? (T(QStringLiteral("隐藏")) + label) : label);
            connect(a2, &QAction::triggered, this,
                    [this, type = e.first]() { toggleWindow(type); });
        }
        m->addSeparator();
        QAction *hideAll = m->addAction(T(QStringLiteral("关闭所有窗口")));
        connect(hideAll, &QAction::triggered, this, &AppController::closeAllWindows);
        QAction *quit = m->addAction(T(QStringLiteral("退出")));
        connect(quit, &QAction::triggered, this, &AppController::quitApp);
    }
};

void AppController::setTheme(bool dark, const QColor &accent)
{
    applyTheme(dark, accent);
    // 持久化
    QSettings s;
    s.setValue(QStringLiteral("theme/dark"), dark);
    s.setValue(QStringLiteral("theme/accent"), accent);

    // 关键修复：Theme 是 pragma 单例，其 dark/accent 只在首次实例化时读取一次上下文属性，
    // 之后 setContextProperty 不会让它的绑定重新求值，导致切换深浅色/主题色界面无反应。
    // 因此直接拿到单例实例并写入属性，派生颜色（bg/panel/…）的绑定会随之重算。
    if (QObject *theme = m_engine->singletonInstance<QObject *>("MinecraftServerManager", "Theme")) {
        theme->setProperty("dark", dark);
        theme->setProperty("accent", QVariant::fromValue(accent));
    }
    emit themeApplied(dark, accent);
    // 同时同步上下文属性，避免其它直接引用 themeDark/themeAccent 的地方不一致
    m_engine->rootContext()->setContextProperty(QStringLiteral("themeDark"), dark);
    m_engine->rootContext()->setContextProperty(QStringLiteral("themeAccent"), accent);
}

class SystemMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY usageChanged)
    Q_PROPERTY(double memoryUsage READ memoryUsage NOTIFY usageChanged)
public:
    explicit SystemMonitor(QObject *parent = nullptr) : QObject(parent) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &SystemMonitor::update);
        m_timer->start(1500);
        update();
    }
    double cpuUsage() const { return m_cpu; }
    double memoryUsage() const { return m_mem; }
signals:
    void usageChanged();
private:
    void update() {
        double cpu = 0, mem = 0;
#ifdef Q_OS_WIN
        FILETIME idle, kernel, user;
        if (GetSystemTimes(&idle, &kernel, &user)) {
            auto toULL = [](FILETIME f) -> qint64 {
                ULARGE_INTEGER u; u.LowPart = f.dwLowDateTime; u.HighPart = f.dwHighDateTime;
                return qint64(u.QuadPart);
            };
            const qint64 idleT = toULL(idle);
            const qint64 sysT = toULL(kernel) + toULL(user);
            if (m_hasPrev) {
                const qint64 idleD = idleT - m_prevIdle;
                const qint64 sysD = sysT - m_prevSys;
                if (sysD > 0)
                    cpu = (1.0 - double(idleD) / double(sysD)) * 100.0;
            }
            m_prevIdle = idleT; m_prevSys = sysT; m_hasPrev = true;
        }
        MEMORYSTATUSEX ms;
        ms.dwLength = sizeof(ms);
        if (GlobalMemoryStatusEx(&ms))
            mem = double(ms.dwMemoryLoad);
#else
        // Linux：读取 /proc/stat 与 /proc/meminfo，无需任何外部命令
        // CPU 利用率：两次采样之间 (总刻度差 - 空闲刻度差) / 总刻度差
        QFile fstat(QStringLiteral("/proc/stat"));
        if (fstat.open(QIODevice::ReadOnly)) {
            const QString line = QString::fromLatin1(fstat.readLine()).trimmed();
            fstat.close();
            const QStringList parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                qint64 sum = 0;
                for (int i = 1; i < parts.size(); ++i) sum += parts.at(i).toLongLong();
                const qint64 idle = parts.at(4).toLongLong()
                                    + (parts.size() > 5 ? parts.at(5).toLongLong() : 0);
                if (m_hasPrev) {
                    const qint64 dTotal = sum - m_prevSys;
                    const qint64 dIdle  = idle - m_prevIdle;
                    if (dTotal > 0)
                        cpu = qBound(0.0, 100.0 * double(dTotal - dIdle) / double(dTotal), 100.0);
                }
                m_prevSys  = sum;
                m_prevIdle = idle;
                m_hasPrev  = true;
            }
        }
        // 内存利用率：优先读 /proc/meminfo 的 MemTotal - MemAvailable；
        // 老内核无 MemAvailable 时退化为 MemFree；仍不可用则回退 sysinfo()，保证有值。
        {
            QFile fmem(QStringLiteral("/proc/meminfo"));
            qint64 total = 0, avail = 0, freeMem = 0;
            bool got = false;
            if (fmem.open(QIODevice::ReadOnly)) {
                while (!fmem.atEnd()) {
                    const QString l = QString::fromLatin1(fmem.readLine()).trimmed();
                    const QStringList kv = l.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                    if (kv.size() >= 2) {
                        if (kv.at(0) == QStringLiteral("MemTotal:")) total = kv.at(1).toLongLong();
                        else if (kv.at(0) == QStringLiteral("MemAvailable:")) avail = kv.at(1).toLongLong();
                        else if (kv.at(0) == QStringLiteral("MemFree:")) freeMem = kv.at(1).toLongLong();
                    }
                }
                fmem.close();
                if (total > 0) {
                    const qint64 used = total - (avail > 0 ? avail : freeMem);
                    mem = qBound(0.0, 100.0 * double(used) / double(total), 100.0);
                    got = true;
                }
            }
            if (!got) {
                // 回退：sysinfo()（freeram 含页缓存，数值偏低但保证有值）
                struct sysinfo si;
                if (sysinfo(&si) == 0 && si.totalram > 0) {
                    const double used = double(si.totalram - si.freeram);
                    mem = qBound(0.0, 100.0 * used / double(si.totalram), 100.0);
                }
            }
        }
#endif
        if (qAbs(cpu - m_cpu) > 0.05 || qAbs(mem - m_mem) > 0.05) {
            m_cpu = cpu; m_mem = mem;
            emit usageChanged();
        }
    }
    double m_cpu = 0, m_mem = 0;
    bool m_hasPrev = false;
    qint64 m_prevIdle = 0, m_prevSys = 0;
    QTimer *m_timer = nullptr;
};

// 下载引擎定义见 downloadmanager.h（基于 QNetworkAccessManager 的真实 HTTP 文件下载）。

// 安装协调器：在应用层（可依赖所有模块）为"下载中心"的每个加载器
// 新建独立 CreateServerController 实例，复用已验证的单加载器安装流程，
// 每个加载器生成独立服务端目录并加入服务器列表。放在应用层可避免
// download 模块反向依赖 serverctl 模块（保持单向无环的分层约束）。
class InstallCoordinator : public QObject
{
    Q_OBJECT
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(qreal stageProgress READ stageProgress NOTIFY stageProgressChanged)
    Q_PROPERTY(QString stageText READ stageText NOTIFY stageTextChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
public:
    InstallCoordinator(DownloadManager *dm, ServerManager *sm, JavaManager *java, QObject *parent = nullptr)
        : QObject(parent), m_dm(dm), m_sm(sm), m_java(java) {}

    qreal progress() const { return m_progress; }
    qreal stageProgress() const { return m_stageProgress; }
    QString stageText() const { return m_stageText; }
    bool busy() const { return m_busy; }

    Q_INVOKABLE void install(const QString &loader,
                             const QString &version,
                             const QString &label)
    {
        if (!m_sm || !m_dm) {
            emit status(QStringLiteral("服务器管理器未初始化，无法安装到服务端"));
            return;
        }
        if (version.isEmpty()) {
            emit status(QStringLiteral("请先选择 Minecraft 版本"));
            return;
        }
        if (m_busy) {
            emit status(QStringLiteral("已有打包任务进行中，请稍候…"));
            return;
        }
        setBusy(true);
        setProgress(0);
        setStageProgress(0);
        setStageText(QString());

        // "下载中心 - 模组服"复用与创建服务器完全相同的安装逻辑（同样的临时目录构建 +
        // 与普通服务器一致的 Java 安装策略），只准备服务端目录、不加入服务器列表、
        // 不打包：产物直接落在下载目录/<名称> 下（含 jvm-{feature}/ 的 Java）。
        const QString name = label + QStringLiteral(" ") + version;
        const QString safe = name.toLower().replace(QLatin1Char(' '), QStringLiteral("_"));
        const QString dir = QDir::cleanPath(QDir::fromNativeSeparators(m_dm->defaultDownloadDir())
                                            + QStringLiteral("/") + safe);

        // 每次新建独立控制器，复用已验证的安装逻辑，互不污染、各自删除
        auto *ctrl = new CreateServerController(m_dm, m_sm, m_java, this);
        connect(ctrl, &CreateServerController::statusTextChanged, this, [this, ctrl]() {
            emit status(ctrl->statusText());
        });
        // 转发安装进度到协调器（供下载中心进度条显示）
        connect(ctrl, &CreateServerController::progressChanged, this, [this, ctrl]() {
            setProgress(ctrl->progress());
        });
        // 转发当前阶段子进度与阶段文案
        connect(ctrl, &CreateServerController::stageProgressChanged, this, [this, ctrl]() {
            setStageProgress(ctrl->stageProgress());
        });
        connect(ctrl, &CreateServerController::stageTextChanged, this, [this, ctrl]() {
            setStageText(ctrl->stageText());
        });
        // 安装中途失败：busy 变 false 且未 done（deleteLater 延后删除，不影响后续 done 信号）
        connect(ctrl, &CreateServerController::busyChanged, this, [this, ctrl]() {
            if (!ctrl->busy() && !ctrl->done()) {
                setBusy(false);
                ctrl->deleteLater();
            }
        });
        connect(ctrl, &CreateServerController::doneChanged, this, [this, ctrl, name]() {
            if (ctrl->done()) {
                setProgress(100);
                setStageProgress(100);
                setStageText(QString());
                setBusy(false);
                emit status(name + QStringLiteral(" 安装完成（未加入服务器列表，已生成到下载文件夹）"));
                ctrl->deleteLater();
            }
        });

        ctrl->setCurrentType(QStringLiteral("模组服"));   // 内部 key 解析为 "mod"
        ctrl->setCurrentVersion(version);
        ctrl->setName(name);
        ctrl->setSaveDir(dir);
        ctrl->setEulaAccepted(true);
        ctrl->setSkipAddList(true);   // 不加入服务器列表
        ctrl->setPackaged(true);      // 下载中心：模组服直接打包为压缩包
        ctrl->setSelectedLoaders(QStringList() << loader);
        ctrl->create();
    }

signals:
    void status(const QString &text);
    void progressChanged();
    void stageProgressChanged();
    void stageTextChanged();
    void busyChanged();

private:
    void setProgress(qreal p) {
        if (qAbs(p - m_progress) < 0.001) return;
        m_progress = p;
        emit progressChanged();
    }
    void setStageProgress(qreal p) {
        if (qAbs(p - m_stageProgress) < 0.001) return;
        m_stageProgress = p;
        emit stageProgressChanged();
    }
    void setStageText(const QString &t) {
        if (m_stageText != t) { m_stageText = t; emit stageTextChanged(); }
    }
    void setBusy(bool b) {
        if (m_busy == b) return;
        m_busy = b;
        emit busyChanged();
    }

    DownloadManager *m_dm = nullptr;
    ServerManager *m_sm = nullptr;
    JavaManager *m_java = nullptr;
    qreal m_progress = 0;
    qreal m_stageProgress = 0;
    QString m_stageText;
    bool m_busy = false;
};

// 诊断模式（Debug 专属）：在 cmd/控制台打印 qrc 资源路径、QML 导入路径等信息后退出（不启动 GUI）。
// 用法：MinecraftServerManager.exe --console   （简写 -c / -diag）。仅 Debug 构建编译/可用。
#ifdef QT_DEBUG
static void msmConsoleMsgHandler(QtMsgType, const QMessageLogContext &, const QString &msg)
{
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    fflush(stderr);
    msmAppendLog(msg.toLocal8Bit());   // 诊断模式的 qDebug/qWarning 同样进 msm.log
}
// 诊断模式专用：同时打印到控制台(stdout)并写入 logs/msm.log
static void msmDiagPrint(const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt);
    const QString s = QString::vasprintf(fmt, ap);
    va_end(ap);
    fprintf(stdout, "%s", qPrintable(s));
    fflush(stdout);
    msmAppendLog(s.toLocal8Bit());
}

static int runConsoleDiagnostics(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    // 优先附加到父控制台（在已有 cmd 中运行）；失败则说明是双击启动，新建一个控制台窗口
    if (!AttachConsole(ATTACH_PARENT_PROCESS))
        AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
#endif
    qInstallMessageHandler(msmConsoleMsgHandler);
    QCoreApplication app(argc, argv);

    msmDiagPrint("=== Minecraft Server Manager 诊断模式 ===\n");
    msmDiagPrint("Qt 版本 : %s\n", qVersion());
    msmDiagPrint("程序目录 : %s\n", qPrintable(QCoreApplication::applicationDirPath()));
    msmDiagPrint("\n--- QML 导入路径 ---\n");
    msmDiagPrint("%s\n", qPrintable(QLibraryInfo::path(QLibraryInfo::Qml2ImportsPath)));
    msmDiagPrint("D:/Developer/Qt/6.11.1/mingw_64/qml\n");

    msmDiagPrint("\n--- 已注册 qrc 资源路径 ---\n");
    QStringList paths;
    QDirIterator it(QStringLiteral(":/"), QDir::AllEntries | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); paths.append(it.filePath()); }
    paths.sort();
    for (const QString &p : paths)
        msmDiagPrint("%s\n", qPrintable(p));
    msmDiagPrint("\n共 %d 个 qrc 资源。\n", (int)paths.size());

    msmDiagPrint("\n按 Enter 退出...");
    getchar();
    return 0;
}
#endif // QT_DEBUG

int main(int argc, char *argv[])
{
    qInstallMessageHandler(msmMessageOutput);
    // 使用 Windows 原生 SChannel 作为 TLS 后端（无需额外 OpenSSL DLL），
    // 修复发布/运行目录下因找不到 OpenSSL 而"所有 HTTPS 请求失败（网络异常）"的问题。
    qputenv("QT_TLS_BACKEND", "schannel");

    // 锁定 Qt RHI 渲染后端为 OpenGL：避免依赖 Vulkan 头文件/SDK（构建时
    // "Could NOT find WrapVulkanHeaders"、运行时无需 Vulkan），改用系统 OpenGL。
    qputenv("QSG_RHI_BACKEND", "opengl");

    // 诊断模式（Debug 专属）：传入 --console / -c / -diag 时打印 qrc 资源路径等信息后退出。
    // 仅 Debug 构建可用；Release 构建忽略这些参数，正常启动 GUI。
#ifdef QT_DEBUG
    for (int i = 1; i < argc; ++i) {
        const QString a = QString::fromLocal8Bit(argv[i]);
        if (a == QStringLiteral("--console") || a == QStringLiteral("-c") || a == QStringLiteral("-diag")) {
            return runConsoleDiagnostics(argc, argv);
        }
    }
#endif

    QApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
        qDebug() << "[APP] aboutToQuit";
    });

    // 设置应用图标：让任务栏/运行窗口使用 MSM.ico（exe 资源图标只影响资源管理器文件图标，
    // 运行时窗口与任务栏按钮需显式设置才会显示，否则为默认空白图标）。
    app.setWindowIcon(QIcon(QStringLiteral(":/icon/ApplicationIcon")));

    // 关键修复：Qt 的 QNetworkAccessManager 默认不读取系统代理。若用户处于需要代理才能访问外网
    // 的环境（公司/校园网），所有出站 HTTPS 直连会被防火墙重置，表现为"Connection Closed"/"网络异常"，
    // 同时导致 WebUI 下载中心各分类（Java/服务端/整合包…）列表永远加载不出来。
    // 开启系统代理配置后，所有 QNAM（下载目录、Java、WebUI 目录）都会自动走系统（WinHTTP/IE）代理。
    QNetworkProxyFactory::setUseSystemConfiguration(true);
    // 固定组织/应用名，使 QSettings 落在稳定位置（主题持久化）
    app.setOrganizationName(QStringLiteral("MSM"));
    app.setApplicationName(QStringLiteral("MinecraftServerManager"));

    // 加载持久化主题（默认深色 + 蓝主色），应用到全局调色板并写回 QSettings
    QSettings settings;
    const bool darkTheme = settings.value(QStringLiteral("theme/dark"), true).toBool();
    const QColor accentColor = settings.value(QStringLiteral("theme/accent"),
                                              QColor("#4f8cff")).value<QColor>();
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));   // 原生控件（托盘菜单）跟随调色板
    applyTheme(darkTheme, accentColor);                                          // 应用持久化调色板（须在样式之后）
    QQuickStyle::setStyle(QStringLiteral("Basic"));     // QML 控件样式（独立于原生控件）

    // ===== 后端控制器（栈对象，按依赖顺序构造）=====
    ServerManager serverManager;
    SystemMonitor systemMonitor;
    DownloadManager downloadManager;
    // Java 运行环境管理：按 MC 版本选择并自动下载安装合适的 JDK。
    // 显式注入统一下载管理器，Java 下载才能进入下载面板（并行/进度/暂停/取消/超时）。
    JavaManager javaManager(&downloadManager);
    // 下载中心 / 创建服务器的逻辑层（C++）；QML 只负责绑定与渲染
    DownloadCatalog downloadCatalog(&downloadManager);
    downloadCatalog.setJavaManager(&javaManager);
    // 下载中心"模组服"安装到服务端：为每个加载器新建独立服务端实例，复用已验证安装流程
    InstallCoordinator installCoordinator(&downloadManager, &serverManager, &javaManager);
    CreateServerController createServer(&downloadManager, &serverManager, &javaManager);
    ModpackImporter importModpack(&downloadManager, &serverManager);
    ServerController serverController;
    SettingsController settingsController;
    // 让服务器扫描只针对“默认服务器目录”（文档/MSM 或用户自定义），不递归整个文档目录。
    serverManager.setDefaultServerDir(settingsController.defaultServerDir());
    QObject::connect(&settingsController, &SettingsController::defaultServerDirChanged,
                     &serverManager, [&]() { serverManager.setDefaultServerDir(settingsController.defaultServerDir()); });
    // WebUI 本地 HTTP 服务：随设置开关/端口变化启动或停止
    WebUIServer webuiServer(&serverManager, &serverController, &downloadManager, &createServer,
                            &importModpack, &settingsController, &systemMonitor, &javaManager);

    // QQ 机器人控制：独立管理 NapCat / NoneBot 外部进程，内置与 WebUI 解耦的控制通道
    BotController botController(&serverManager, &serverController, &settingsController, &downloadManager);
    webuiServer.setBotController(&botController);
    botController.setNapcatPath(settingsController.napcatPath());
    botController.setNonebotDir(settingsController.nonebotDir());
    botController.setUsageInterval(settingsController.botUsageInterval());
    botController.setBotLinkedStart(settingsController.botLinkedStart());   // 联动启动开关（默认关闭）
    qDebug() << "[APP] before setBotEnabled; linkedStart=" << settingsController.botLinkedStart()
             << "botEnabled=" << settingsController.botEnabled();
    botController.setBotEnabled(settingsController.botEnabled());
    qDebug() << "[APP] after setBotEnabled";
    QObject::connect(&settingsController, &SettingsController::botEnabledChanged,
                     &botController, [&]() { botController.setBotEnabled(settingsController.botEnabled()); });
    QObject::connect(&settingsController, &SettingsController::napcatPathChanged,
                     &botController, [&]() { botController.setNapcatPath(settingsController.napcatPath()); });
    QObject::connect(&settingsController, &SettingsController::nonebotDirChanged,
                     &botController, [&]() { botController.setNonebotDir(settingsController.nonebotDir()); });
    QObject::connect(&settingsController, &SettingsController::botUsageIntervalChanged,
                     &botController, [&]() { botController.setUsageInterval(settingsController.botUsageInterval()); });
    QObject::connect(&settingsController, &SettingsController::botLinkedStartChanged,
                     &botController, [&]() { botController.setBotLinkedStart(settingsController.botLinkedStart()); });
    // MSM -> QQ：仅在服务器异常退出时把日志私信管理员（不再主动推送启停事件）
    QObject::connect(&serverController, &ServerController::serverError,
                     &botController, [&](const QString &name, const QString &tail) {
                         botController.pushError(name, tail);
                     });

    // Velocity 反向代理聚合（P2 多实例）：索引 0 为默认实例（兼容旧版单代理），
    // 其余实例各自独立端口/后端筛选，共享同一个 velocity.jar。
    ProxyManager proxyManager(&serverManager, &serverController, &javaManager);
    ProxyController &proxyController = *proxyManager.defaultProxy();

    // A3 公网暴露：UPnP IGD 端口映射（实验性，需路由器开启 UPnP）
    PortMapper portMapper;

    // 运维自动化（B3 定时备份 / B4 定时启停 / B2 一键更新 jar）
    BackupController backupController;
    SchedulerController schedulerController(&serverManager, &serverController, &backupController);
    UpdateController updateController;

    // 同步受管服务器列表给备份控制器，供自动备份遍历
    auto syncBackupServers = [&]() {
        backupController.setServerList(serverManager.serverSummary());
    };
    syncBackupServers();
    QObject::connect(&serverManager, &ServerManager::serversChanged, &backupController, syncBackupServers);
    // 启动即备份（用户开启时）：遍历所有受管服务器各打一份初始备份
    if (backupController.onStart()) {
        for (const QVariant &v : serverManager.serverSummary()) {
            const QVariantMap m = v.toMap();
            backupController.backupNow(m.value(QStringLiteral("name")).toString(),
                                       m.value(QStringLiteral("path")).toString());
        }
    }

    // Webhook 通知器：崩溃 / 启停 / 玩家进服推送
    Notifier notifier;
    auto syncNotifier = [&]() {
        notifier.setUrl(settingsController.webhookUrl());
        notifier.setType(settingsController.webhookType());
        notifier.setEnabled(settingsController.webhookEnabled());
        notifier.setNotifyCrash(settingsController.webhookCrash());
        notifier.setNotifyState(settingsController.webhookState());
        notifier.setNotifyPlayer(settingsController.webhookPlayer());
    };
    syncNotifier();
    QObject::connect(&settingsController, &SettingsController::webhookUrlChanged, &notifier, syncNotifier);
    QObject::connect(&settingsController, &SettingsController::webhookTypeChanged, &notifier, syncNotifier);
    QObject::connect(&settingsController, &SettingsController::webhookEnabledChanged, &notifier, syncNotifier);
    QObject::connect(&settingsController, &SettingsController::webhookCrashChanged, &notifier, syncNotifier);
    QObject::connect(&settingsController, &SettingsController::webhookStateChanged, &notifier, syncNotifier);
    QObject::connect(&settingsController, &SettingsController::webhookPlayerChanged, &notifier, syncNotifier);

    // 后端事件 → Webhook
    QObject::connect(&serverController, &ServerController::serverError, &notifier,
                     [&](const QString &name, const QString &) {
                         if (notifier.notifyCrash())
                             notifier.send(QStringLiteral("服务器异常退出"),
                                           name + QStringLiteral(" 异常退出，已尝试自动重启"));
                     });
    QObject::connect(&serverController, &ServerController::stateChanged, &notifier,
                     [&](const QString &name, bool running) {
                         if (notifier.notifyState())
                             notifier.send(running ? QStringLiteral("服务器已启动") : QStringLiteral("服务器已停止"), name);
                     });
    QObject::connect(&serverController, &ServerController::playerJoined, &notifier,
                     [&](const QString &name, const QString &who) {
                         if (notifier.notifyPlayer())
                             notifier.send(QStringLiteral("玩家进服"), who + QStringLiteral(" 进入了 ") + name);
                     });
    // 代理事件 → Webhook
    QObject::connect(&proxyController, &ProxyController::crashed, &notifier, [&]() {
        if (notifier.notifyCrash())
            notifier.send(QStringLiteral("代理异常退出"), QStringLiteral("Velocity 代理异常退出，已尝试自动重启"));
    });
    QObject::connect(&proxyController, &ProxyController::runningChanged, &notifier, [&]() {
        if (notifier.notifyState())
            notifier.send(proxyController.running() ? QStringLiteral("代理已启动") : QStringLiteral("代理已停止"),
                          QStringLiteral("Velocity 代理"));
    });
    // 一键更新 jar → Webhook
    QObject::connect(&updateController, &UpdateController::updateFinished, &notifier,
                     [&](const QString &name, bool ok, const QString &msg, const QString &) {
                         if (notifier.enabled())
                             notifier.send(ok ? QStringLiteral("服务端更新完成") : QStringLiteral("服务端更新失败"),
                                           name + QStringLiteral("：") + msg);
                     });
    // 状态推送（Q2）：服务器启停 / 代理启停 同步到 QQ 机器人群消息（崩溃日志推送已在上方单独接线）
    QObject::connect(&serverController, &ServerController::stateChanged, &botController,
                     [&](const QString &name, bool running) {
                         botController.notify(QStringLiteral("[MSM] 服务器 %1 %2")
                                               .arg(name, running ? QStringLiteral("已启动") : QStringLiteral("已停止")),
                                               QStringLiteral("group"));
                     });
    QObject::connect(&proxyController, &ProxyController::runningChanged, &botController,
                     [&]() {
                         botController.notify(QStringLiteral("[MSM] 代理 %1")
                                               .arg(proxyController.running() ? QStringLiteral("已启动") : QStringLiteral("已停止")),
                                               QStringLiteral("group"));
                     });
    // QML 引擎放在所有后端控制器之后创建：退出时引擎最先析构，先销毁 QML 对象树；
    // 此时所有上下文属性对象（控制器）尚未析构，绑定求值访问到的仍是有效对象，
    // 避免退出时大量 "Cannot read property 'xxx' of null" 报错。
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    // AppController 作为引擎子对象：引擎析构晚期（QObject 基类阶段）才销毁，
    // 退出时 QML 仍可安全访问 appController。
    AppController *appController = new AppController(&engine);
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), appController);

    // 退出前释放所有运行中的子进程，避免调试器/detach 或正常退出后服务器/bot/代理进程残留。
    appController->setQuitCleanup([&]() {
        serverController.stopAll();           // 强制终止所有运行中的 Minecraft 服务器进程
        if (proxyController.running())
            proxyController.stop();           // 停止代理后端
        botController.stopAll();              // 关闭控制服务 / NapCat / NoneBot
    });

    // 注册各后端控制器为上下文属性（供 QML 绑定）
    engine.rootContext()->setContextProperty(QStringLiteral("serverManager"), &serverManager);
    engine.rootContext()->setContextProperty(QStringLiteral("systemMonitor"), &systemMonitor);
    engine.rootContext()->setContextProperty(QStringLiteral("downloadManager"), &downloadManager);
    engine.rootContext()->setContextProperty(QStringLiteral("javaManager"), &javaManager);
    engine.rootContext()->setContextProperty(QStringLiteral("downloadCatalog"), &downloadCatalog);
    engine.rootContext()->setContextProperty(QStringLiteral("installCoordinator"), &installCoordinator);
    engine.rootContext()->setContextProperty(QStringLiteral("createServer"), &createServer);
    engine.rootContext()->setContextProperty(QStringLiteral("importModpack"), &importModpack);
    engine.rootContext()->setContextProperty(QStringLiteral("serverController"), &serverController);
    engine.rootContext()->setContextProperty(QStringLiteral("settingsController"), &settingsController);
    engine.rootContext()->setContextProperty(QStringLiteral("webuiServer"), &webuiServer);
    engine.rootContext()->setContextProperty(QStringLiteral("botController"), &botController);
    engine.rootContext()->setContextProperty(QStringLiteral("proxyController"), &proxyController);
    engine.rootContext()->setContextProperty(QStringLiteral("proxyManager"), &proxyManager);
    engine.rootContext()->setContextProperty(QStringLiteral("portMapper"), &portMapper);
    engine.rootContext()->setContextProperty(QStringLiteral("backupController"), &backupController);
    engine.rootContext()->setContextProperty(QStringLiteral("schedulerController"), &schedulerController);
    engine.rootContext()->setContextProperty(QStringLiteral("updateController"), &updateController);

    QObject::connect(&installCoordinator, &InstallCoordinator::status,
                     &downloadCatalog, &DownloadCatalog::setStatus);

    appController->setSettingsController(&settingsController);

    // 语言切换：QML 侧通过 I18n.t(I18n.lang) 绑定自动重算；
    // C++ 侧 tray 菜单通过 AppController::m_dict 查表 + m_sc->language() 决定中/英。
    appController->rebuildTrayMenu();

    QObject::connect(&settingsController, &SettingsController::languageChanged,
                     appController, [&]() { appController->rebuildTrayMenu(); });

    webuiServer.setThemeState(darkTheme, accentColor);
    webuiServer.setPort(settingsController.webuiPort());
    webuiServer.setEnabled(settingsController.webuiEnabled());
    QObject::connect(&settingsController, &SettingsController::webuiEnabledChanged,
                     [&webuiServer, &settingsController]() { webuiServer.setEnabled(settingsController.webuiEnabled()); });
    QObject::connect(&settingsController, &SettingsController::webuiPortChanged,
                     [&webuiServer, &settingsController]() { webuiServer.setPort(settingsController.webuiPort()); });
    // WebUI 内修改主题 → 应用回本地端
    QObject::connect(&webuiServer, &WebUIServer::themeChangeRequested,
                     appController, &AppController::setTheme);
    // 本地端修改主题 → 同步给 WebUI
    QObject::connect(appController, &AppController::themeApplied,
                     &webuiServer, &WebUIServer::setThemeState);

    // 持久化主题下发给 QML 单例 Theme（加载即生效：文字/背景颜色随之切换）
    engine.rootContext()->setContextProperty(QStringLiteral("themeDark"), darkTheme);
    engine.rootContext()->setContextProperty(QStringLiteral("themeAccent"), accentColor);

    // 只用 Qt 自身安装位置解析出的 QML 导入目录；去掉原先硬编码的
    // D:/Developer/Qt/6.11.1/mingw_64/qml —— 该绝对路径与机器不符时会导致
    // QtQuick/QtQuick.Controls 等模块加载失败，进而 MainWindow 创建不出来（启动后无操作页面）。
    engine.addImportPath(QLibraryInfo::path(QLibraryInfo::Qml2ImportsPath));

    // 启动窗口策略：
    // - 开启"启动时显示窗口"或有托盘：创建主窗口并（按需）显示；
    //   有原生托盘时关闭窗口只是收起托盘，程序常驻。
    // - 无托盘（无通知区的环境）：显示主窗口，关闭即退出程序。
    if (settingsController.showOnStartup() || !appController->hasTray()) {
        appController->showMainWindow();
    }
    // 有托盘且未要求启动时显示窗口：仅驻留托盘，不显示主窗口。

    return app.exec();
}

#include "main.moc"
