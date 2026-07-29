/*
 * botcontroller.cpp —— QQ 机器人控制（NapCat / NoneBot 独立于 WebUI）
 * -------------------------------------------------
 * - 用 QProcess 分别拉起/停止 NapCat 与 NoneBot 两个外部进程；
 * - 内置一个与 WebUI 完全解耦的本地 HTTP 控制通道（端口 25585），
 *   QQ 机器人插件经由该通道控制服务器，即使 WebUI 关闭也不受影响；
 * - 三者（WebUI / NapCat / NoneBot）可独立开关，也能通过该通道或
 *   WebUI 设置页互相启停；
 * - 推送策略：默认不主动推送；常态按间隔把设备占用(CPU/内存)更新到机器人 QQ 昵称；
 *   服务器异常退出时把日志私信管理员；其余只在指令执行后回传反馈。
 * - 安全：控制通道默认仅监听 127.0.0.1；WebUI 改为 HTTPS 并要求访问令牌。
 */
#include "botcontroller.h"
#include "servermanager.h"
#include "servercontroller.h"
#include "settingscontroller.h"
#include "downloadmanager.h"
#include "downloadlistmodel.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QDebug>
#include <QAbstractItemModel>
#include <QTcpSocket>
#include <QMetaObject>
#include <QProcess>
#include <QTcpServer>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QMap>
#include <QHostAddress>
#include <QCryptographicHash>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {
// 整机 CPU 占用率（%）。基于两次调用之间的 GetSystemTimes 差分，
// 首次调用返回 -1（无历史采样），之后返回区间平均值。
double deviceCpuPercent()
{
#ifdef Q_OS_WIN
    static quint64 prevIdle = 0, prevKernel = 0, prevUser = 0;
    FILETIME idleFt, kernelFt, userFt;
    if (!GetSystemTimes(&idleFt, &kernelFt, &userFt))
        return -1.0;
    auto toU64 = [](const FILETIME &ft) {
        return (quint64(ft.dwHighDateTime) << 32) | quint64(ft.dwLowDateTime);
    };
    const quint64 idle = toU64(idleFt);
    const quint64 kernel = toU64(kernelFt);   // 含 idle
    const quint64 user = toU64(userFt);
    double pct = -1.0;
    if (prevKernel || prevUser) {
        const quint64 total = (kernel - prevKernel) + (user - prevUser);
        const quint64 idled = idle - prevIdle;
        if (total > 0)
            pct = 100.0 * double(total - idled) / double(total);
    }
    prevIdle = idle;
    prevKernel = kernel;
    prevUser = user;
    return pct;
#else
    return -1.0;
#endif
}

// 整机物理内存：已用/总量（GB）。失败返回 false。
bool deviceMemoryGB(double &usedG, double &totalG)
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX ms;
    ms.dwLength = sizeof(ms);
    if (!GlobalMemoryStatusEx(&ms))
        return false;
    constexpr double kGiB = 1024.0 * 1024.0 * 1024.0;
    usedG = double(ms.ullTotalPhys - ms.ullAvailPhys) / kGiB;
    totalG = double(ms.ullTotalPhys) / kGiB;
    return true;
#else
    Q_UNUSED(usedG); Q_UNUSED(totalG);
    return false;
#endif
}

// NapCat 是 Node.js 程序，入口为 napcat.bat（= node.exe ./index.js）
QStringList napcatCandidates()
{
    QStringList c;
    // 常见绝对路径（VM/本机常见布局：C:\Robots\NapCat4.16.0\napcat\napcat.bat 等）
    c << QStringLiteral("C:/Robots/NapCat4.16.0/napcat.bat")
      << QStringLiteral("D:/Robots/NapCat4.16.0/napcat.bat")
      << QStringLiteral("C:/Robots/NapCat/napcat.bat")
      << QStringLiteral("D:/Robots/NapCat/napcat.bat")
      << QStringLiteral("C:/Program Files/NapCat/napcat.bat")
      << QStringLiteral("C:/NapCat/napcat.bat")
      << QStringLiteral("D:/NapCat/napcat.bat");

    // 以 exe 目录为基准的相对/上级路径
    const QString base = QCoreApplication::applicationDirPath();
    c << base + QStringLiteral("/NapCat4.16.0/napcat.bat")
      << base + QStringLiteral("/NapCat/napcat.bat")
      << base + QStringLiteral("/napcat.bat")
      << base + QStringLiteral("/../NapCat4.16.0/napcat.bat")
      << base + QStringLiteral("/../NapCat/napcat.bat")
      << base + QStringLiteral("/../Robots/NapCat4.16.0/napcat.bat")
      << base + QStringLiteral("/Robots/NapCat4.16.0/napcat.bat");

    // 扫描常见根目录的一级子目录，匹配 NapCat*/napcat.bat 或 */napcat/napcat.bat
    static const QStringList roots = { QStringLiteral("C:/"), QStringLiteral("D:/"),
                                        QStringLiteral("C:/Robots"), QStringLiteral("D:/Robots"),
                                        QStringLiteral("C:/Program Files"), QStringLiteral("C:/Users") };
    for (const QString &r : roots) {
        QDir dir(r);
        if (!dir.exists())
            continue;
        for (const QFileInfo &fi : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString name = fi.fileName().toLower();
            if (name.startsWith(QStringLiteral("napcat"))) {
                const QString p1 = fi.absoluteFilePath() + QStringLiteral("/napcat.bat");
                if (QFile::exists(p1)) c << QDir::toNativeSeparators(p1);
            }
            const QString p2 = fi.absoluteFilePath() + QStringLiteral("/napcat/napcat.bat");
            if (QFile::exists(p2)) c << QDir::toNativeSeparators(p2);
        }
    }
    return c;
}

// NoneBot 工程候选目录（nb create 生成的标准项目）
QStringList nonebotCandidates()
{
    QStringList c;
    // 常见绝对路径（VM 常见布局：C:\Robots\my_nonebot 等）
    c << QStringLiteral("C:/Robots/my_nonebot")
      << QStringLiteral("D:/Robots/my_nonebot")
      << QStringLiteral("C:/Robots/nonebot")
      << QStringLiteral("D:/Robots/nonebot")
      << QStringLiteral("C:/Program Files/nonebot")
      << QStringLiteral("C:/nonebot");

    // 以 exe 目录为基准的相对/上级路径
    const QString base = QCoreApplication::applicationDirPath();
    c << base + QStringLiteral("/my_nonebot")
      << base + QStringLiteral("/qqbot")
      << base + QStringLiteral("/bot")
      << base + QStringLiteral("/nonebot")
      << base + QStringLiteral("/../my_nonebot")
      << base + QStringLiteral("/../qqbot")
      << base + QStringLiteral("/../Robots/my_nonebot")
      << base + QStringLiteral("/Robots/my_nonebot");

    // 扫描常见根目录的一级子目录，匹配含 nonebot 的工程目录
    static const QStringList roots = { QStringLiteral("C:/"), QStringLiteral("D:/"),
                                        QStringLiteral("C:/Robots"), QStringLiteral("D:/Robots"),
                                        QStringLiteral("C:/Program Files"), QStringLiteral("C:/Users") };
    for (const QString &r : roots) {
        QDir dir(r);
        if (!dir.exists())
            continue;
        for (const QFileInfo &fi : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString name = fi.fileName().toLower();
            if (name.contains(QStringLiteral("nonebot"))) {
                const QString p = fi.absoluteFilePath();
                if (QFile::exists(p)) c << QDir::toNativeSeparators(p);
            }
        }
    }
    return c;
}
}

BotController::BotController(ServerManager *sm, ServerController *sc,
                             SettingsController *settings, DownloadManager *dm,
                             QObject *parent)
    : QObject(parent), m_sm(sm), m_sc(sc), m_settings(settings), m_dm(dm)
{
    m_net = new QNetworkAccessManager(this);
    m_usageTimer = new QTimer(this);
    m_usageTimer->setInterval(qMax(1, m_usageInterval) * 1000);
    connect(m_usageTimer, &QTimer::timeout, this, &BotController::pushUsage);
    m_usageTimer->start();
    (void)deviceCpuPercent();   // 先采样一次，下个周期即可得到区间平均 CPU

    // 控制通道连接保活检查：非联动模式下，若机器人长时间未连入则回到"等待连接"
    m_seenTimer = new QTimer(this);
    m_seenTimer->setInterval(5000);
    connect(m_seenTimer, &QTimer::timeout, this, &BotController::onSeenTick);
    m_seenTimer->start();

    // 访问令牌（与 WebUI 共用）重生成后，回写已安装的 NoneBot 插件配置，
    // 使其后续启动仍与最新令牌一致。本机链路本身免令牌，这里仅为一致性。
    if (m_settings)
        connect(m_settings, &SettingsController::webuiTokenChanged,
                this, &BotController::onWebuiTokenChanged);

    // 若设置中已启用，则启动时自动拉起（main 中随后会 setXxxEnabled）
}

void BotController::onWebuiTokenChanged()
{
    // 令牌重生成后，把最新令牌回写已安装插件配置：
    //  - NoneBot：pyproject.toml 的 msm_token
    //  - NapCat：插件目录 config.json 的 msm_token
    // 控制通道现已改为本机 loopback 也需令牌，故必须回写；同时重启正在运行的机器人，
    // 使其加载新令牌（否则运行中会持续 401 直至手动重启）。
    if (!m_nonebotDir.isEmpty())
        patchPyproject(m_nonebotDir);
    const QString bat = detectNapcatPath();
    if (!bat.isEmpty())
        patchNapcatConfig(bat);
    if (m_napcatProc) {
        m_restartNapcatPending = true;
        stopNapcat();
    }
    if (m_nonebotProc) {
        m_restartNonebotPending = true;
        stopNonebot();
    }
}

BotController::~BotController()
{
    stopControlServer();
    stopNonebot();
    stopNapcat();
    m_usageTimer->stop();
}

void BotController::setNapcatEnabled(bool v)
{
    if (m_napcat == v)
        return;
    m_napcat = v;
    emit napcatEnabledChanged();
    if (v) {
        ensureNapcatPlugin();       // 联动/非联动都先确保 napcat-plugin-msm 已装进 plugins/
        if (m_linkedStart)
            startNapcat();
        else
            setNapcatState(QStringLiteral("external"));   // 仅标记，由用户自行启动 NapCat
    } else {
        stopNapcat();
    }
}

void BotController::setNapcatPath(const QString &v)
{
    const QString p = v.startsWith(QStringLiteral("file:///")) ? v.mid(8) : v;
    if (m_napcatPath != p) {
        m_napcatPath = p;
        emit napcatPathChanged();
        if (m_napcat)
            ensureNapcatPlugin();   // 换了 NapCat 路径：往新目录补装插件
    }
}

void BotController::setNonebotEnabled(bool v)
{
    if (m_nonebot == v)
        return;
    m_nonebot = v;
    emit nonebotEnabledChanged();
    if (v) {
        if (m_linkedStart)
            startNonebot();
        else
            startNonebotConnectOnly();   // 只开放控制通道，等待机器人主动连入
    } else {
        stopNonebot();
    }
}

void BotController::setNonebotDir(const QString &v)
{
    const QString p = v.startsWith(QStringLiteral("file:///")) ? v.mid(8) : v;
    if (m_nonebotDir != p) {
        m_nonebotDir = p;
        emit nonebotDirChanged();
    }
}

void BotController::setUsageInterval(int v)
{
    if (m_usageInterval == v)
        return;
    m_usageInterval = qMax(0, v);
    if (m_usageTimer) {
        if (m_usageInterval > 0) {
            m_usageTimer->setInterval(m_usageInterval * 1000);
            if (!m_usageTimer->isActive())
                m_usageTimer->start();
        } else {
            m_usageTimer->stop();   // 0 = 关闭昵称状态更新
        }
    }
    emit usageIntervalChanged();
}

void BotController::setBotLinkedStart(bool v)
{
    if (m_linkedStart == v)
        return;
    m_linkedStart = v;
    emit botLinkedStartChanged();
    // 若当前已启用，则按新模式重建（拉起进程 / 仅开放控制通道）
    if (m_napcat) {
        stopNapcat();
        ensureNapcatPlugin();       // 模式切换重建前同样确保插件已就位
        if (v) startNapcat();
        else setNapcatState(QStringLiteral("external"));
    }
    if (m_nonebot) {
        stopNonebot();
        if (v) startNonebot();
        else startNonebotConnectOnly();
    }
}

void BotController::setBotEnabled(bool v)
{
    if (m_bot == v)
        return;
    m_bot = v;
    emit botEnabledChanged();
    if (v) {
        // 一起开：先拉起 NapCat（提供 OneBot 端口），再拉起 NoneBot
        if (!m_napcat)
            setNapcatEnabled(true);
        if (!m_nonebot)
            setNonebotEnabled(true);
    } else {
        // 一起关：先停 NoneBot（连同控制通道），再停 NapCat
        if (m_nonebot)
            setNonebotEnabled(false);
        if (m_napcat)
            setNapcatEnabled(false);
    }
}

void BotController::setNapcatState(const QString &s)
{
    if (m_napcatState != s) {
        m_napcatState = s;
        emit napcatStateChanged();
    }
}

void BotController::setNonebotState(const QString &s)
{
    if (m_nonebotState != s) {
        m_nonebotState = s;
        emit nonebotStateChanged();
    }
}

// ---------- 进程管理 ----------

void BotController::startNapcat()
{
    qDebug() << "[BOT] startNapcat() enter; m_napcatProc=" << (m_napcatProc ? "set" : "null")
             << "linkedStart=" << m_linkedStart;
    if (m_napcatProc)
        return;
    const QString bat = detectNapcatPath();
    qDebug() << "[BOT] detectNapcatPath ->" << bat;
    if (bat.isEmpty() || !QFile::exists(bat)) {
        setNapcatState(QStringLiteral("stopped"));
        qDebug() << "[BOT] NapCat 未找到：" << bat;
        return;
    }
    setNapcatState(QStringLiteral("starting"));
    ensureNapcatPlugin();   // 启动前确保 napcat-plugin-msm 已装入 plugins/（首次/重建时自动安装）
    const QString workDir = QFileInfo(bat).absolutePath();   // napcat.bat 内使用 node.exe ./index.js，需在该目录运行
    QProcess *p = new QProcess(this);
    p->setWorkingDirectory(workDir);
    p->setProcessChannelMode(QProcess::MergedChannels);
#ifdef Q_OS_WIN
    p->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a) {
        a->flags |= CREATE_NEW_CONSOLE;
        a->inheritHandles = false;
    });
#endif
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int, QProcess::ExitStatus) {
                p->deleteLater();
                if (m_napcatProc == p)
                    m_napcatProc = nullptr;
                if (m_restartNapcatPending) {
                    m_restartNapcatPending = false;
                    startNapcat();   // 加载注入的新令牌后重新拉起
                } else {
                    setNapcatState(QStringLiteral("stopped"));
                }
            });
    // NapCat 是 Node.js 程序：napcat.bat = `node.exe ./index.js`
#ifdef Q_OS_WIN
    p->start(QStringLiteral("cmd.exe"), QStringList() << QStringLiteral("/c") << bat);
#else
    p->start(QStringLiteral("bash"), QStringList() << QStringLiteral("-c") << bat);
#endif
    if (!p->waitForStarted(5000)) {
        qDebug() << "[BOT] NapCat waitForStarted 超时/失败，pid=" << p->processId()
                 << "err=" << p->errorString();
        p->deleteLater();
        setNapcatState(QStringLiteral("stopped"));
        return;
    }
    m_napcatProc = p;
    setNapcatState(QStringLiteral("running"));
}

void BotController::stopNapcat()
{
    if (m_napcatProc)
        m_napcatProc->kill();
    setNapcatState(QStringLiteral("stopped"));
}

void BotController::startNonebot()
{
    qDebug() << "[BOT] startNonebot() enter; linkedStart=" << m_linkedStart
             << "nonebotProc=" << (m_nonebotProc ? "set" : "null");
    if (m_nonebotProc)
        return;
    const QString dir = detectNonebotDir();
    if (dir.isEmpty() || !QFile::exists(dir)) {
        setNonebotState(QStringLiteral("stopped"));
        qDebug() << "[BOT] NoneBot 目录未找到：" << dir;
        return;
    }
    // 缺少 msm_control 控制插件：自动安装（拷贝插件 + 写入 pyproject 配置），无需手动改配置
    if (!isMsmPluginInstalled(dir)) {
        qDebug() << "[BOT] 未检测到 msm_control 插件，自动安装并写入 pyproject 配置";
        if (!installMsmPluginInto(dir)) {
            setMsmPluginState(QStringLiteral("missing"));
            emit pluginMissing();
            return;
        }
    }
    setMsmPluginState(QStringLiteral("ok"));
    setNonebotState(QStringLiteral("starting"));

    QString program;
    QStringList args;
    const QString nb = detectNb();
    if (!nb.isEmpty()) {                      // 优先用 nb run（Windows 下常见）
        program = nb;
        args << QStringLiteral("run");
#ifdef Q_OS_WIN
        // nb 可能是 .cmd/.bat，需经 cmd 运行
        if (nb.endsWith(QLatin1String(".cmd"), Qt::CaseInsensitive) ||
            nb.endsWith(QLatin1String(".bat"), Qt::CaseInsensitive)) {
            args.prepend(nb);
            args.prepend(QStringLiteral("/c"));
            program = QStringLiteral("cmd.exe");
        }
#endif
    } else {                                  // 回退：python -m nb_cli run
        program = detectPython(dir);
        args << QStringLiteral("-m") << QStringLiteral("nb_cli") << QStringLiteral("run");
    }

    m_nonebotStopping = false;   // 进入运行态，后续退出若为 finished 即视为“意外退出”
    QProcess *p = new QProcess(this);
    p->setWorkingDirectory(dir);              // nb run 读取当前目录的 pyproject
    p->setProcessChannelMode(QProcess::MergedChannels);
    // 捕获 nb 输出，便于排错（nb.exe 启动器可能在拉起真正的 venv python 后退出）
    connect(p, &QProcess::readyReadStandardError, this, [p]() {
        const QByteArray b = p->readAllStandardError();
        if (!b.isEmpty()) qDebug() << "[BOT][nonebot stderr]" << QString::fromLocal8Bit(b).trimmed();
    });
    connect(p, &QProcess::readyReadStandardOutput, this, [p]() {
        const QByteArray b = p->readAllStandardOutput();
        if (!b.isEmpty()) qDebug() << "[BOT][nonebot stdout]" << QString::fromLocal8Bit(b).trimmed();
    });
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, p](int code, QProcess::ExitStatus) {
                qDebug() << "[BOT] NoneBot 进程已退出 finished; code=" << code
                         << "restartPending=" << m_restartNonebotPending
                         << "deliberate=" << m_nonebotStopping;
                p->deleteLater();
                if (m_nonebotProc == p)
                    m_nonebotProc = nullptr;
                if (m_restartNonebotPending) {
                    m_restartNonebotPending = false;
                    startNonebot();   // 内部会 startControlServer() 并加载注入的新令牌
                    return;
                }
                if (m_nonebotStopping) {
                    // 用户显式停用：关闭控制通道（stopNonebot 已先调用过，这里幂等）
                    m_nonebotStopping = false;
                    setNonebotState(QStringLiteral("stopped"));
                    stopControlServer();
                    return;
                }
                // 意外退出：控制通道生命周期与 nb 启动器进程解耦——保持 25585 监听，
                // 让真正运行中的 venv python（及其 msm_control 插件）继续连入；
                // 不在此自动重启，避免 nb 启动器 re-exec 时产生重复机器人。
                setNonebotState(QStringLiteral("stopped"));
                qDebug() << "[BOT] NoneBot 意外退出：控制通道保持监听（25585），等待机器人重连";
            });
    qDebug() << "[BOT] startNonebot launching:" << program << args;
    p->start(program, args);
    if (!p->waitForStarted(15000)) {          // nb run 启动较慢，给足时间
        qDebug() << "[BOT] NoneBot waitForStarted 超时/失败，pid=" << p->processId();
        p->deleteLater();
        setNonebotState(QStringLiteral("stopped"));
        return;
    }
    m_nonebotProc = p;
    setNonebotState(QStringLiteral("running"));
    qDebug() << "[BOT] NoneBot 已启动 pid=" << p->processId() << "，准备启动控制服务器";
    startControlServer();
}

void BotController::stopNonebot()
{
    m_nonebotStopping = true;   // 标记为显式停止，finished 时不再自动重启/保持服务器
    stopControlServer();
    if (m_nonebotProc)
        m_nonebotProc->kill();
    setNonebotState(QStringLiteral("stopped"));
}

void BotController::startNonebotConnectOnly()
{
    const QString dir = detectNonebotDir();
    if (dir.isEmpty() || !QFile::exists(dir)) {
        setNonebotState(QStringLiteral("stopped"));
        qDebug() << "[BOT] NoneBot 目录未找到：" << dir;
        return;
    }
    if (!isMsmPluginInstalled(dir)) {
        setMsmPluginState(QStringLiteral("missing"));
        emit pluginMissing();
        return;
    }
    setMsmPluginState(QStringLiteral("ok"));
    setNonebotState(QStringLiteral("waiting"));   // 已开放控制通道，等待机器人主动连入
    startControlServer();
}

QString BotController::detectNapcatPath() const
{
    if (!m_napcatPath.isEmpty()) {
        QFileInfo fi(m_napcatPath);
        if (fi.isFile() && QFile::exists(m_napcatPath))
            return m_napcatPath;            // 已直接指向 napcat.bat
        if (fi.isDir()) {                   // 配置的是目录：解析其中的 napcat.bat
            const QString bat = QDir(m_napcatPath).absoluteFilePath(QStringLiteral("napcat.bat"));
            if (QFile::exists(bat))
                return bat;
        }
    }
    for (const QString &c : napcatCandidates())
        if (QFile::exists(c))
            return c;
    return QString();
}

QStringList BotController::detectNapcatPaths() const
{
    QStringList out;
    for (const QString &c : napcatCandidates())
        if (QFile::exists(c))
            out << c;
    return out;
}

QString BotController::detectNonebotDir() const
{
    if (!m_nonebotDir.isEmpty() && QFile::exists(m_nonebotDir))
        return m_nonebotDir;
    for (const QString &c : nonebotCandidates())
        if (QFile::exists(c))
            return QDir(c).absolutePath();
    return m_nonebotDir;
}

QStringList BotController::detectNonebotDirs() const
{
    QStringList out;
    for (const QString &c : nonebotCandidates())
        if (QFile::exists(c))
            out << QDir(c).absolutePath();
    return out;
}

QString BotController::detectPython(const QString &dir) const
{
    const QStringList cands = {
        dir + QStringLiteral("/.venv/Scripts/python.exe"),
        dir + QStringLiteral("/venv/Scripts/python.exe"),
        dir + QStringLiteral("/.venv/Scripts/python"),
        dir + QStringLiteral("/venv/Scripts/python")};
    for (const QString &c : cands)
        if (QFile::exists(c))
            return c;
    return QStringLiteral("python");
}

QString BotController::detectNb() const
{
    // 优先在 PATH 中查找 nb 可执行（Windows 下可能为 nb.exe / nb.cmd / nb.bat / nb）
    const QString nb = QStandardPaths::findExecutable(QStringLiteral("nb"));
    if (!nb.isEmpty())
        return nb;
    return QString();
}

QString BotController::sourcePluginPath() const
{
    return QDir::toNativeSeparators(QCoreApplication::applicationDirPath()
                                    + QStringLiteral("/qqbot/plugins/msm_control.py"));
}

bool BotController::isMsmPluginInstalled(const QString &dir) const
{
    if (dir.isEmpty() || !QFile::exists(dir))
        return false;
    // 方式1：plugins/msm_control.py 存在
    const QString pluginFile = QDir(dir).filePath(QStringLiteral("plugins/msm_control.py"));
    if (QFile::exists(pluginFile))
        return true;
    // 方式2：pyproject 的 plugin_dirs 指向的目录里含 msm_control
    const QString toml = QDir(dir).filePath(QStringLiteral("pyproject.toml"));
    if (QFile::exists(toml)) {
        QFile f(toml);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString text = QString::fromUtf8(f.readAll());
            f.close();
            QRegularExpression reDirs(QStringLiteral(R"(plugin_dirs\s*=\s*\[([^\]]*)\])"));
            QRegularExpressionMatch m = reDirs.match(text);
            if (m.hasMatch()) {
                const QStringList dirs = m.captured(1).split(QLatin1Char(','), Qt::SkipEmptyParts);
                for (QString d : dirs) {
                    d = d.trimmed().remove(QLatin1Char('"')).remove(QLatin1Char('\''));
                    if (d.isEmpty())
                        continue;
                    if (QFile::exists(QDir(dir).filePath(d + QStringLiteral("/msm_control.py"))))
                        return true;
                }
            }
            // 方式3：@local 直接引用 msm_control
            if (text.contains(QStringLiteral("\"msm_control\"")) || text.contains(QStringLiteral("'msm_control'")))
                return true;
        }
    }
    return false;
}

bool BotController::patchPyproject(const QString &dir) const
{
    const QString toml = QDir(dir).filePath(QStringLiteral("pyproject.toml"));
    if (!QFile::exists(toml))
        return false;
    QFile f(toml);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
    f.close();

    const QMap<QString, QString> msmCfg = {
        {QStringLiteral("msm_control_url"), QStringLiteral("http://127.0.0.1:25585")},
        {QStringLiteral("msm_target_server"), QStringLiteral("")},
        {QStringLiteral("msm_allowed_commands"),
         QStringLiteral("list,say,op,deop,gamemode,whitelist,kick,ban,tp,tell,me,gamerule,time,weather,difficulty,save,reload,execute")},
        {QStringLiteral("msm_admins"), QStringLiteral("")},
        {QStringLiteral("msm_admin"), QStringLiteral("")},
        {QStringLiteral("msm_notify_targets"), QStringLiteral("")},
        {QStringLiteral("msm_token"), m_settings ? m_settings->webuiToken() : QString()},
    };

    int mainTable = -1;
    for (int i = 0; i < lines.size(); ++i) {
        if (lines.at(i).trimmed() == QStringLiteral("[tool.nonebot]")) {
            mainTable = i;
            break;
        }
    }

    if (mainTable < 0) {
        if (!lines.isEmpty() && !lines.last().isEmpty())
            lines.append(QString());
        lines.append(QStringLiteral("[tool.nonebot]"));
        lines.append(QStringLiteral("plugin_dirs = [\"plugins\"]"));
        for (auto it = msmCfg.begin(); it != msmCfg.end(); ++it)
            lines.append(QString::fromLatin1("%1 = \"%2\"").arg(it.key(), it.value()));
    } else {
        int tableEnd = lines.size();
        for (int i = mainTable + 1; i < lines.size(); ++i) {
            if (lines.at(i).trimmed().startsWith(QLatin1Char('['))) {
                tableEnd = i;
                break;
            }
        }
        // 处理 plugin_dirs：存在但缺 "plugins" 则追加（不覆盖用户原有目录）
        for (int i = mainTable + 1; i < tableEnd; ++i) {
            QString t = lines.at(i).trimmed();
            if (t.startsWith(QLatin1String("plugin_dirs"))) {
                if (!t.contains(QLatin1String("\"plugins\"")) && !t.contains(QLatin1String("'plugins'"))) {
                    QRegularExpression reDirs(QStringLiteral(R"(plugin_dirs\s*=\s*\[([^\]]*)\])"));
                    QRegularExpressionMatch m = reDirs.match(t);
                    if (m.hasMatch()) {
                        const QString inner = m.captured(1).trimmed();
                        const QString newInner = inner.isEmpty()
                            ? QStringLiteral("\"plugins\"")
                            : inner + QStringLiteral(", \"plugins\"");
                        lines[i] = QStringLiteral("plugin_dirs = [") + newInner + QStringLiteral("]");
                    }
                }
            }
        }
        bool hasPluginDirs = false;
        for (int i = mainTable + 1; i < tableEnd; ++i)
            if (lines.at(i).trimmed().startsWith(QLatin1String("plugin_dirs")))
                hasPluginDirs = true;
        // 始终刷新 msm_ 配置（尤其是访问令牌与控制地址）：已存在的键就地更新其值，
        // 缺失的键追加到表尾。避免历史遗留的旧令牌导致机器人连控制通道被 401 拒绝。
        QMap<QString, QString> remaining = msmCfg;
        for (int i = mainTable + 1; i < tableEnd; ++i) {
            QString t = lines.at(i).trimmed();
            for (auto it = remaining.begin(); it != remaining.end(); ) {
                const QString key = it.key();
                if (t == key || t.startsWith(key + QLatin1String(" "))
                    || t.startsWith(key + QLatin1String("="))) {
                    lines[i] = QString::fromLatin1("%1 = \"%2\"").arg(key, it.value());
                    it = remaining.erase(it);
                } else {
                    ++it;
                }
            }
        }
        QStringList toInsert;
        if (!hasPluginDirs)
            toInsert << QStringLiteral("plugin_dirs = [\"plugins\"]");
        for (auto it = remaining.begin(); it != remaining.end(); ++it)
            toInsert << QString::fromLatin1("%1 = \"%2\"").arg(it.key(), it.value());
        for (int k = toInsert.size() - 1; k >= 0; --k)
            lines.insert(tableEnd, toInsert.at(k));
    }

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qDebug() << "[BOT] 无法写入 pyproject.toml：" << toml;
        return false;
    }
    f.write(lines.join(QLatin1Char('\n')).toUtf8());
    f.close();
    return true;
}

bool BotController::patchNapcatConfig(const QString &batPath) const
{
    const QString destDir = napcatPluginDestDir(batPath);
    if (destDir.isEmpty())
        return false;
    const QString cfgFile = QDir(destDir).filePath(QStringLiteral("config.json"));
    QJsonObject obj;
    if (QFile::exists(cfgFile)) {
        QFile f(cfgFile);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            if (doc.isObject())
                obj = doc.object();
            f.close();
        }
    }
    // 仅更新访问令牌与控制地址，保留插件已有的其它配置（命令白名单、通知目标等）。
    // 控制通道已改为本机也需令牌，故必须在此注入，避免用户手动填写。
    obj[QStringLiteral("msm_token")] = m_settings ? m_settings->webuiToken() : QString();
    obj[QStringLiteral("msm_control_url")] = QStringLiteral("http://127.0.0.1:")
        + QString::number(m_controlPort);
    QFile f(cfgFile);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qDebug() << "[BOT] 无法写入 NapCat 插件配置：" << cfgFile;
        return false;
    }
    f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    f.close();
    qDebug() << "[BOT] 已把访问令牌注入 NapCat 插件配置：" << cfgFile;
    return true;
}

bool BotController::installMsmPluginInto(const QString &dir)
{
    const QString src = sourcePluginPath();
    if (src.isEmpty() || !QFile::exists(src)) {
        qDebug() << "[BOT] 自带的 msm_control.py 源不存在：" << src;
        return false;
    }
    QDir d(dir);
    if (!d.mkpath(QStringLiteral("plugins"))) {
        qDebug() << "[BOT] 无法创建 plugins 目录：" << dir;
        return false;
    }
    const QString dest = d.filePath(QStringLiteral("plugins/msm_control.py"));
    if (QFile::exists(dest))
        QFile::remove(dest);
    if (!QFile::copy(src, dest)) {
        qDebug() << "[BOT] 复制 msm_control.py 失败：" << src << "->" << dest;
        return false;
    }
    const QString initFile = d.filePath(QStringLiteral("plugins/__init__.py"));
    if (!QFile::exists(initFile)) {
        QFile f(initFile);
        f.open(QIODevice::WriteOnly);
        f.close();
    }
    if (!patchPyproject(dir)) {
        qDebug() << "[BOT] 改写 pyproject.toml 失败";
        return false;
    }
    return true;
}

void BotController::setMsmPluginState(const QString &s)
{
    if (m_msmPluginState != s) {
        m_msmPluginState = s;
        emit msmPluginStateChanged();
    }
}

// ---------- NapCat 插件（napcat-plugin-msm）自动安装 ----------

void BotController::setNapcatPluginState(const QString &s)
{
    if (m_napcatPluginState != s) {
        m_napcatPluginState = s;
        emit napcatPluginStateChanged();
    }
}

QString BotController::napcatPluginSourceDir() const
{
    // 在 qqbot/ 下查找 napcat-plugin-msm 插件源。兼容多种布局：
    //   - qqbot/napcat/dist        （vite 构建产物，最常见）
    //   - qqbot/napcat             （扁平：index.mjs 直接在此，无 dist 子目录）
    //   - qqbot/napcat-plugin-msm  （独立插件文件夹，含或不合 dist）
    //   - 递归兜底：任意含 index.mjs 且 package.json 名为 napcat-plugin-* 的目录
    // 注意：NapCat 程序自身往往没有 dist 目录，这里找的是「MSM 自带的插件源」，
    // 不能假设它一定在 dist/ 里。
    const QString base = QCoreApplication::applicationDirPath();
    const QStringList roots = {
        base + QStringLiteral("/qqbot"),
        base + QStringLiteral("/../qqbot"),
        QStringLiteral("C:/Robots/MSM/qqbot"),
        QStringLiteral("D:/Robots/MSM/qqbot"),
    };
    for (const QString &root : roots) {
        QDir d(root);
        if (!d.exists())
            continue;
        const QStringList subs = {
            QStringLiteral("napcat/dist"),
            QStringLiteral("napcat"),
            QStringLiteral("napcat-plugin-msm"),
            QStringLiteral("napcat-plugin-msm/dist"),
        };
        for (const QString &s : subs) {
            const QString cand = QDir::cleanPath(d.filePath(s));
            if (QFile::exists(cand + QStringLiteral("/index.mjs")) ||
                QFile::exists(cand + QStringLiteral("/index.js")))
                return QDir(cand).absolutePath();
        }
        const QString found = findNapcatPluginSource(d);   // 递归兜底
        if (!found.isEmpty())
            return found;
    }
    return QString();
}

// 在 dir 下递归查找 napcat-plugin-msm 源目录（depth 层内），优先返回带正确 package.json 的
QString BotController::findNapcatPluginSource(const QDir &dir, int depth) const
{
    if (depth <= 0)
        return QString();
    QString fallback;
    for (const QFileInfo &fi :
         dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString path = fi.absoluteFilePath();
        const bool hasEntry = QFile::exists(path + QStringLiteral("/index.mjs")) ||
                              QFile::exists(path + QStringLiteral("/index.js"));
        if (hasEntry) {
            const QString pkg = path + QStringLiteral("/package.json");
            if (QFile::exists(pkg)) {
                QFile f(pkg);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    const QString txt = QString::fromUtf8(f.readAll());
                    f.close();
                    if (txt.contains(QStringLiteral("\"napcat-plugin-"))) {
                        return QDir(path).absolutePath();   // 命中：直接返回
                    }
                }
            }
            if (fallback.isEmpty())
                fallback = QDir(path).absolutePath();       // 记录首个候选，最后兜底
        }
        const QString deeper = findNapcatPluginSource(QDir(path), depth - 1);
        if (!deeper.isEmpty())
            return deeper;
    }
    return fallback;
}

QString BotController::napcatPluginDestDir(const QString &batPath) const
{
    if (batPath.isEmpty() || !QFile::exists(batPath))
        return QString();
    const QDir batDir(QFileInfo(batPath).absolutePath());

    // 1) 优先复用 NapCat 已有的 plugins 目录（不管它叫 napcat/plugins、plugins、
    //    napcat_data/plugins 还是别的），把插件装到同一个目录，确保 NapCat 能扫到。
    const QString anchor = findExistingPluginsDir(batDir);
    if (!anchor.isEmpty())
        return QDir::cleanPath(anchor + QStringLiteral("/napcat-plugin-msm"));

    // 2) 常见布局：napcat.bat 旁有 napcat/ 运行时 → napcat/plugins/
    const QString runtime = batDir.filePath(QStringLiteral("napcat"));
    if (QFileInfo(runtime).isDir())
        return QDir::cleanPath(runtime + QStringLiteral("/plugins/napcat-plugin-msm"));

    // 3) 扁平布局：napcat.bat 同级 plugins/
    return QDir::cleanPath(batDir.filePath(QStringLiteral("plugins/napcat-plugin-msm")));
}

// 从 napcat.bat 目录出发，向上 2 级、向下 3 级查找一个已存在的 plugins 目录
// （里面通常已含官方 napcat-plugin-*，作为真实插件路径的锚点）。
QString BotController::findExistingPluginsDir(const QDir &batDir) const
{
    // 先以 napcat.bat 目录为根，向下找（深度 3）
    QString down = findPluginsDirRecursive(batDir, 3);
    if (!down.isEmpty())
        return down;
    // 再向上逐层找（最多 2 级父目录），每层从父目录向下找（深度 3）
    QDir cur = batDir;
    for (int up = 0; up < 2; ++up) {
        QDir parent = cur;
        if (!parent.cdUp())
            break;
        const QString upDir = findPluginsDirRecursive(parent, 3);
        if (!upDir.isEmpty())
            return upDir;
        cur = parent;
    }
    return QString();
}

QString BotController::findPluginsDirRecursive(const QDir &dir, int depth) const
{
    if (depth <= 0)
        return QString();
    // 当前层直接命中 plugins/ 目录
    const QString plugins = QDir::cleanPath(dir.filePath(QStringLiteral("plugins")));
    if (QFileInfo(plugins).isDir())
        return plugins;
    for (const QFileInfo &fi : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString r = findPluginsDirRecursive(QDir(fi.absoluteFilePath()), depth - 1);
        if (!r.isEmpty())
            return r;
    }
    return QString();
}

bool BotController::isNapcatPluginInstalled(const QString &batPath) const
{
    const QString dest = napcatPluginDestDir(batPath);
    return !dest.isEmpty() && QFile::exists(dest + QStringLiteral("/index.mjs"));
}

bool BotController::copyDirRecursively(const QString &srcDir, const QString &dstDir)
{
    QDir src(srcDir);
    if (!src.exists())
        return false;
    if (!QDir().mkpath(dstDir))
        return false;
    bool ok = true;
    const QFileInfoList entries =
        src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        const QString dst = QDir(dstDir).filePath(fi.fileName());
        if (fi.isDir()) {
            ok = copyDirRecursively(fi.absoluteFilePath(), dst) && ok;
        } else {
            if (QFile::exists(dst))
                QFile::remove(dst);            // 覆盖旧版本
            ok = QFile::copy(fi.absoluteFilePath(), dst) && ok;
        }
    }
    return ok;
}

bool BotController::installNapcatPluginInto(const QString &batPath)
{
    const QString srcDir = napcatPluginSourceDir();
    if (srcDir.isEmpty()) {
        qDebug() << "[BOT] napcat-plugin-msm 构建产物不存在（qqbot/napcat/dist），"
                    "请先 pnpm run build";
        setNapcatPluginState(QStringLiteral("missing-source"));
        return false;
    }
    const QString destDir = napcatPluginDestDir(batPath);
    if (destDir.isEmpty()) {
        setNapcatPluginState(QStringLiteral("error"));
        return false;
    }
    if (!copyDirRecursively(srcDir, destDir)) {
        qDebug() << "[BOT] 复制 napcat-plugin-msm 失败：" << srcDir << "->" << destDir;
        setNapcatPluginState(QStringLiteral("error"));
        return false;
    }
    // dist 里缺 package.json 时补一个最小版（NapCat 要求 name 以 napcat-plugin- 开头）
    const QString pkg = destDir + QStringLiteral("/package.json");
    if (!QFile::exists(pkg)) {
        QFile f(pkg);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QByteArrayLiteral(
                "{\n  \"name\": \"napcat-plugin-msm\",\n  \"version\": \"1.0.0\",\n"
                "  \"main\": \"index.mjs\",\n  \"type\": \"module\",\n"
                "  \"napcat\": {\n    \"type\": \"extension\",\n"
                "    \"name\": \"napcat-plugin-msm\",\n"
                "    \"tags\": [\"工具\", \"游戏\", \"Minecraft\"],\n"
                "    \"minVersion\": \"4.14.0\"\n  }\n}\n"));
            f.close();
        }
    }
    qDebug() << "[BOT] 已安装 napcat-plugin-msm 到" << destDir;
    // 安装即注入访问令牌（loopback 也需令牌），免用户手动填写 msm_token
    patchNapcatConfig(batPath);
    setNapcatPluginState(QStringLiteral("ok"));
    return true;
}

bool BotController::ensureNapcatWhitelist(const QString &batPath)
{
    if (batPath.isEmpty() || !QFile::exists(batPath))
        return false;

    // napcat.mjs 位于 napcat.bat 同级的 napcat/ 子目录
    const QDir batDir(QFileInfo(batPath).absolutePath());
    const QString runtime = batDir.filePath(QStringLiteral("napcat"));
    QString mjs;
    if (QFileInfo(runtime).isDir())
        mjs = runtime + QStringLiteral("/napcat.mjs");
    else
        mjs = batDir.filePath(QStringLiteral("napcat.mjs"));
    if (!QFile::exists(mjs)) {
        qDebug() << "[BOT] 未找到 napcat.mjs，跳过白名单补丁：" << mjs;
        return false;
    }

    QFile f(mjs);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "[BOT] 无法读取 napcat.mjs：" << mjs;
        return false;
    }
    QString text = QString::fromUtf8(f.readAll());
    f.close();

    // 已包含插件名则无需补丁（白名单 Set 与插件自身引用都算已放行）
    if (text.contains(QStringLiteral("\"napcat-plugin-msm\""))) {
        qDebug() << "[BOT] napcat-plugin-msm 已在白名单中，无需补丁";
        return true;
    }

    // 定位官方白名单 Set 的锚点（napcat-plugin-qce 是最后一个官方项）
    const QString anchor = QStringLiteral("\"napcat-plugin-qce\"");
    const int qce = text.indexOf(anchor);
    if (qce < 0) {
        qDebug() << "[BOT] 未找到 NapCat 白名单锚点，跳过白名单补丁";
        return false;
    }
    // 从锚点向后找 Set 的闭合 ]（白名单为纯字符串集合，内部无嵌套括号）
    const int close = text.indexOf(QLatin1Char(']'), qce);
    if (close < 0) {
        qDebug() << "[BOT] 未找到白名单 Set 闭合括号，跳过白名单补丁";
        return false;
    }
    const QString tail = text.mid(qce, close - qce);
    const QString insert = tail.trimmed().endsWith(QLatin1Char('"'))
                               ? QStringLiteral(",\n  \"napcat-plugin-msm\"")   // "...qce"] 紧闭，需补逗号
                               : QStringLiteral("\n  \"napcat-plugin-msm\"");
    text = text.left(close) + insert + text.mid(close);

    if (!f.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qDebug() << "[BOT] 无法写入 napcat.mjs：" << mjs;
        return false;
    }
    f.write(text.toUtf8());
    f.close();
    qDebug() << "[BOT] 已将 napcat-plugin-msm 加入 NapCat 插件白名单：" << mjs;
    return true;
}

void BotController::ensureNapcatPlugin()
{
    const QString bat = detectNapcatPath();
    if (bat.isEmpty() || !QFile::exists(bat)) {
        setNapcatPluginState(QStringLiteral("unknown"));   // NapCat 未找到，无处可装
        return;
    }
    // 自动检测并给 NapCat 4.18+ 的硬编码插件白名单打补丁（第三方插件会被拒）
    ensureNapcatWhitelist(bat);

    const QString srcDir = napcatPluginSourceDir();
    if (srcDir.isEmpty()) {
        // 没有构建产物：已装则保持 ok，否则标记缺源（不打断启动流程）
        setNapcatPluginState(isNapcatPluginInstalled(bat)
                                 ? QStringLiteral("ok")
                                 : QStringLiteral("missing-source"));
        return;
    }
    // 目标缺失或源文件更新（重新构建过）都触发（重新）安装
    if (isNapcatPluginInstalled(bat)) {
        const QDateTime srcTime =
            QFileInfo(srcDir + QStringLiteral("/index.mjs")).lastModified();
        const QDateTime dstTime =
            QFileInfo(napcatPluginDestDir(bat) + QStringLiteral("/index.mjs")).lastModified();
        if (dstTime.isValid() && srcTime.isValid() && srcTime <= dstTime) {
            setNapcatPluginState(QStringLiteral("ok"));
            return;
        }
    }
    installNapcatPluginInto(bat);
}

bool BotController::isNapcatPluginInstalled() const
{
    return isNapcatPluginInstalled(detectNapcatPath());
}

bool BotController::installNapcatPlugin()
{
    const QString bat = detectNapcatPath();
    if (bat.isEmpty() || !QFile::exists(bat)) {
        setNapcatPluginState(QStringLiteral("error"));
        qDebug() << "[BOT] NapCat 路径未设置或不存在，无法安装插件";
        return false;
    }
    ensureNapcatWhitelist(bat);   // 安装前先确保插件不会被白名单拒绝
    return installNapcatPluginInto(bat);
}

bool BotController::isMsmPluginInstalled() const
{
    return isMsmPluginInstalled(detectNonebotDir());
}

bool BotController::installMsmPlugin()
{
    const QString dir = detectNonebotDir();
    if (dir.isEmpty() || !QFile::exists(dir)) {
        setMsmPluginState(QStringLiteral("error"));
        qDebug() << "[BOT] NoneBot 目录未设置或不存在，无法安装插件";
        return false;
    }
    setMsmPluginState(QStringLiteral("installing"));
    const bool ok = installMsmPluginInto(dir);
    setMsmPluginState(ok ? QStringLiteral("ok") : QStringLiteral("error"));
    if (ok) {
        qDebug() << "[BOT] 已安装 msm_control 插件到" << dir;
        // 安装完成后按联动开关决定：拉起进程 or 仅开放控制通道
        if (m_linkedStart)
            startNonebot();
        else
            startNonebotConnectOnly();
    }
    return ok;
}

void BotController::retryStartNonebot()
{
    if (m_nonebot) {
        if (m_linkedStart)
            startNonebot();
        else
            startNonebotConnectOnly();
    } else {
        setMsmPluginState(isMsmPluginInstalled() ? QStringLiteral("ok") : QStringLiteral("missing"));
    }
}

// ---------- 独立控制通道（与 WebUI 解耦） ----------

namespace {
QString resolveName(ServerManager *sm, ServerController *sc, const QString &name)
{
    if (!name.isEmpty())
        return name;
    const QVariantList list = sm->serverSummary();
    for (const QVariant &v : list) {
        const QString n = v.toMap().value(QStringLiteral("name")).toString();
        if (sc->isRunning(n))
            return n;
    }
    return list.isEmpty() ? QString() : list.first().toMap().value(QStringLiteral("name")).toString();
}
QJsonObject jsonErr(const QString &msg)
{
    QJsonObject o;
    o[QStringLiteral("success")] = false;
    o[QStringLiteral("message")] = msg;
    return o;
}
}

void BotController::startControlServer()
{
    qDebug() << "[BOT] startControlServer() enter; m_tcp=" << (m_tcp ? "exists" : "null")
             << "port=" << m_controlPort;
    if (m_tcp)
        return;
    m_tcp = new QTcpServer(this);
    connect(m_tcp, &QTcpServer::newConnection, this, &BotController::onApiNewConnection);
    if (!m_tcp->listen(QHostAddress::LocalHost, m_controlPort)) {
        qDebug() << "[BOT] 控制端口" << m_controlPort << "监听失败：" << m_tcp->errorString();
        delete m_tcp;
        m_tcp = nullptr;
        return;
    }
    qDebug() << "[BOT] 控制通道已启动：http://127.0.0.1:" << m_tcp->serverPort();
}

void BotController::stopControlServer()
{
    qDebug() << "[BOT] stopControlServer() enter; m_tcp=" << (m_tcp ? "exists" : "null");
    // nextPendingConnection() 返回的 socket 默认以 m_tcp 为父对象，直接 delete m_tcp 会
    // 一并销毁所有子 socket。若再对 ws 客户端调 deleteLater 会造成双重释放 / 堆损坏
    //（表现为关闭 NoneBot 开关时闪退）。因此先把 ws 客户端解绑到本对象并各自延期删除，
    // 最后才删除监听 socket。
    const auto clients = m_wsClients;
    m_wsClients.clear();
    m_buffers.clear();
    for (QTcpSocket *c : clients) {
        c->disconnect(this);   // 断开到本对象的信号，避免析构期回调
        c->setParent(this);    // 脱离 m_tcp，防止被 delete m_tcp 二次销毁
        c->deleteLater();
    }
    if (m_tcp) {
        m_tcp->close();
        m_tcp->deleteLater();
        m_tcp = nullptr;
    }
}

void BotController::onApiNewConnection()
{
    m_botSeen.restart();                       // 记录机器人最近一次连入
    if (m_nonebotState == QStringLiteral("waiting"))
        setNonebotState(QStringLiteral("running"));
    while (m_tcp && m_tcp->hasPendingConnections()) {
        QTcpSocket *s = m_tcp->nextPendingConnection();
        connect(s, &QTcpSocket::readyRead, this, &BotController::onApiReadyRead);
        connect(s, &QTcpSocket::disconnected, this, &BotController::onApiDisconnected);
    }
}

void BotController::onSeenTick()
{
    // 非联动模式下，若机器人超过 30s 未连入或已断开，则回到“等待连接”
    if (!m_nonebot || m_linkedStart)
        return;
    if (!m_wsClients.isEmpty()) {
        // 存在 WebSocket 长连接即视为在线，保持“运行中”
        m_botSeen.restart();
        if (m_nonebotState != QStringLiteral("running"))
            setNonebotState(QStringLiteral("running"));
        return;
    }
    if (m_nonebotState == QStringLiteral("running") && m_botSeen.isValid() && m_botSeen.hasExpired(30000))
        setNonebotState(QStringLiteral("waiting"));
}

void BotController::onApiDisconnected()
{
    QTcpSocket *s = qobject_cast<QTcpSocket *>(sender());
    if (!s)
        return;
    m_wsClients.remove(s);
    m_buffers.remove(s);
    s->deleteLater();
}

void BotController::onApiReadyRead()
{
    QTcpSocket *s = qobject_cast<QTcpSocket *>(sender());
    if (!s)
        return;
    if (m_wsClients.contains(s)) {      // 已是 WebSocket 长连接，按帧解析
        parseWsFrames(s);
        return;
    }
    m_buffers[s].append(s->readAll());
    const int headerEnd = m_buffers[s].indexOf("\r\n\r\n");
    if (headerEnd < 0)
        return;
    const QByteArray head = m_buffers[s].left(headerEnd);
    const QList<QByteArray> lines = head.split('\n');
    if (lines.isEmpty())
        return;
    const QByteArray first = lines.at(0).trimmed();
    const int sp1 = first.indexOf(' ');
    const int sp2 = first.lastIndexOf(' ');
    if (sp1 < 0 || sp2 < 0 || sp2 <= sp1)
        return;
    const QString method = QString::fromLatin1(first.left(sp1));
    const QString fullPath = QString::fromLatin1(first.mid(sp1 + 1, sp2 - sp1 - 1));

    // 解析全部 header（含 WebSocket 升级所需字段）
    QMap<QString, QString> hdr;
    for (int i = 1; i < lines.size(); ++i) {
        const QString t = QString::fromUtf8(lines.at(i)).trimmed();
        const int cpos = t.indexOf(QLatin1Char(':'));
        if (cpos > 0)
            hdr[t.left(cpos).trimmed().toLower()] = t.mid(cpos + 1).trimmed();
    }

    // WebSocket 升级：GET /ws 且带 Upgrade: websocket + Sec-WebSocket-Key
    {
        QString wsPath = fullPath;
        const int wq = wsPath.indexOf(QLatin1Char('?'));
        if (wq >= 0)
            wsPath = wsPath.left(wq);
        if (method == QStringLiteral("GET") && wsPath == QStringLiteral("/ws")
            && hdr.value(QStringLiteral("upgrade")).contains(QStringLiteral("websocket"), Qt::CaseInsensitive)
            && hdr.contains(QStringLiteral("sec-websocket-key"))) {
            const QString wsQuery = wq >= 0 ? fullPath.mid(wq + 1) : QString();
            if (!ctrlAuthorized(s, hdr, wsQuery)) {
                s->write("HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                s->disconnectFromHost();
                m_buffers.remove(s);
                return;
            }
            upgradeToWs(s, hdr);
            m_buffers[s].clear();
            return;
        }
    }

    int cl = 0;
    if (hdr.contains(QStringLiteral("content-length")))
        cl = hdr.value(QStringLiteral("content-length")).toInt();
    const int bodyStart = headerEnd + 4;
    if (m_buffers[s].size() < bodyStart + cl)
        return;
    const QByteArray body = m_buffers[s].mid(bodyStart, cl);
    m_buffers[s].clear();
    QString path = fullPath, query;
    const int qidx = fullPath.indexOf(QLatin1Char('?'));
    if (qidx >= 0) {
        path = fullPath.left(qidx);
        query = fullPath.mid(qidx + 1);
    }
    dispatchApi(method, path, query, body, hdr, s);
}

QString BotController::urlDecode(const QString &s)
{
    return QUrl::fromPercentEncoding(s.toUtf8());
}

QMap<QString, QString> BotController::parseQuery(const QString &q)
{
    QMap<QString, QString> out;
    if (q.isEmpty())
        return out;
    for (const QString &p : q.split(QLatin1Char('&'), Qt::SkipEmptyParts)) {
        const int eq = p.indexOf(QLatin1Char('='));
        if (eq < 0)
            out[urlDecode(p)] = QString();
        else
            out[urlDecode(p.left(eq))] = urlDecode(p.mid(eq + 1));
    }
    return out;
}

// 控制通道鉴权：本机(loopback)客户端直接放行；非本机必须携带正确令牌。
// 该通道默认仅监听 127.0.0.1，故正常 bot 流量不受限；一旦被改到 0.0.0.0 暴露，
// 令牌即成为必要屏障（与 WebUI 共用 webuiToken）。
bool BotController::ctrlAuthorized(QTcpSocket *s, const QMap<QString, QString> &hdr,
                                    const QString &query) const
{
    Q_UNUSED(s);
    // 本机(loopback)不再豁免：连本机的其它进程/恶意程序也必须凭令牌，仅受信子进程
    // （NapCat/NoneBot，其 msm_token 由 MSM 自动注入）才能通过。
    const QString tok = m_settings ? m_settings->webuiToken() : QString();
    if (tok.isEmpty())
        return true;   // 未配置令牌（降级模式），与历史行为一致
    QString provided = hdr.value(QStringLiteral("authorization")).trimmed();
    if (provided.startsWith(QLatin1String("Bearer "), Qt::CaseInsensitive))
        provided = provided.mid(7).trimmed();
    if (provided.isEmpty())
        provided = parseQuery(query).value(QStringLiteral("token")).trimmed();
    if (provided.isEmpty())
        return false;
    return QCryptographicHash::hash(provided.toUtf8(), QCryptographicHash::Sha256)
        == QCryptographicHash::hash(tok.toUtf8(), QCryptographicHash::Sha256);
}

void BotController::sendJson(QTcpSocket *sock, const QJsonObject &obj, int code)
{
    const QByteArray data = QJsonDocument(obj).toJson(QJsonDocument::Compact);
    QByteArray resp = "HTTP/1.1 " + QByteArray::number(code) + " OK\r\n"
                      "Content-Type: application/json; charset=utf-8\r\n"
                      "Content-Length: " + QByteArray::number(data.size()) + "\r\n"
                      "Connection: close\r\n\r\n" + data;
    sock->write(resp);
    sock->disconnectFromHost();
}

void BotController::sendText(QTcpSocket *sock, const QString &text, const QString &ct)
{
    const QByteArray data = text.toUtf8();
    QByteArray resp = "HTTP/1.1 200 OK\r\nContent-Type: " + ct.toUtf8() +
                      "\r\nContent-Length: " + QByteArray::number(data.size()) +
                      "\r\nConnection: close\r\n\r\n" + data;
    sock->write(resp);
    sock->disconnectFromHost();
}

void BotController::sendStatus(QTcpSocket *sock, int code, const QString &msg)
{
    // 所有 API 错误统一返回 JSON（{success:false, message}），便于插件解析；
    // 否则插件对 text/plain 做 resp.json() 会抛 "Unexpected token" 异常。
    QJsonObject obj;
    obj[QStringLiteral("success")] = false;
    obj[QStringLiteral("message")] = msg;
    sendJson(sock, obj, code);
}

void BotController::dispatchApi(const QString &method, const QString &path,
                                const QString &query, const QByteArray &body,
                                const QMap<QString, QString> &hdr, QTcpSocket *sock)
{
    // 令牌校验：所有客户端（含本机 loopback）均需正确令牌；系统/OS 级操作不经由本通道，不受影响。
    if (!ctrlAuthorized(sock, hdr, query)) {
        sendStatus(sock, 401, QStringLiteral("Unauthorized"));
        return;
    }
    const QMap<QString, QString> q = parseQuery(query);
    QJsonObject bodyJson;
    if (!body.isEmpty()) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(body, &err);
        if (doc.isObject())
            bodyJson = doc.object();
    }

    if (path == QStringLiteral("/api/ping")) {
        sendJson(sock, QJsonObject{{QStringLiteral("success"), true}});
        return;
    }
    if (path == QStringLiteral("/api/status")) {
        QJsonObject remotes;
        QJsonObject w; w[QStringLiteral("enabled")] = m_settings->webuiEnabled(); remotes[QStringLiteral("webui")] = w;
        QJsonObject n; n[QStringLiteral("enabled")] = m_napcat; n[QStringLiteral("state")] = m_napcatState; remotes[QStringLiteral("napcat")] = n;
        QJsonObject nb; nb[QStringLiteral("enabled")] = m_nonebot; nb[QStringLiteral("state")] = m_nonebotState; remotes[QStringLiteral("nonebot")] = nb;
        QJsonArray names;
        for (const QVariant &v : m_sm->serverSummary())
            names << v.toMap().value(QStringLiteral("name")).toString();
        QJsonObject o;
        o[QStringLiteral("remotes")] = remotes;
        o[QStringLiteral("running")] = m_sc->runningCount();
        o[QStringLiteral("servers")] = names;
        o[QStringLiteral("controlPort")] = m_controlPort;
        sendJson(sock, o);
        return;
    }
    if (path == QStringLiteral("/api/servers")) {
        QJsonArray arr;
        for (const QVariant &v : m_sm->serverSummary()) {
            const QVariantMap m = v.toMap();
            const QString name = m.value(QStringLiteral("name")).toString();
            QJsonObject s;
            s[QStringLiteral("name")] = name;
            s[QStringLiteral("version")] = m.value(QStringLiteral("version")).toString();
            s[QStringLiteral("type")] = m.value(QStringLiteral("type")).toString();
            s[QStringLiteral("path")] = m.value(QStringLiteral("path")).toString();
            s[QStringLiteral("running")] = m_sc->isRunning(name);
            QJsonArray pl;
            for (const QString &p : m_sc->players(name)) pl << p;
            s[QStringLiteral("players")] = pl;
            arr << s;
        }
        sendJson(sock, QJsonObject{{QStringLiteral("servers"), arr}});
        return;
    }
    if (path.startsWith(QStringLiteral("/api/players"))) {
        const QString name = resolveName(m_sm, m_sc, q.value(QStringLiteral("name")));
        if (name.isEmpty()) { sendStatus(sock, 404, QStringLiteral("未找到服务器")); return; }
        sendJson(sock, QJsonObject{{QStringLiteral("name"), name},
                                   {QStringLiteral("players"), QJsonArray::fromStringList(m_sc->players(name))}});
        return;
    }
    if (path.startsWith(QStringLiteral("/api/console"))) {
        const QString name = resolveName(m_sm, m_sc, q.value(QStringLiteral("name")));
        if (name.isEmpty()) { sendStatus(sock, 404, QStringLiteral("未找到服务器")); return; }
        const int lines = q.value(QStringLiteral("lines")).toInt();
        const int n = lines > 0 ? lines : 20;
        const QStringList all = m_sc->getConsole(name).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        const QStringList tail = all.size() > n ? all.mid(all.size() - n) : all;
        sendJson(sock, QJsonObject{{QStringLiteral("name"), name},
                                   {QStringLiteral("console"), tail.join(QLatin1Char('\n'))}});
        return;
    }
    if (path.startsWith(QStringLiteral("/api/props"))) {
        const QString name = resolveName(m_sm, m_sc, q.value(QStringLiteral("name")));
        Server *s = m_sm->serverByName(name);
        const QString p = s ? s->path() : QString();
        if (p.isEmpty()) { sendStatus(sock, 404, QStringLiteral("未找到服务器")); return; }
        QJsonObject props;
        const QVariantMap pm = m_sc->readProperties(p);
        for (auto it = pm.begin(); it != pm.end(); ++it)
            props[it.key()] = it.value().toString();
        sendJson(sock, QJsonObject{{QStringLiteral("name"), name}, {QStringLiteral("properties"), props}});
        return;
    }
    if (path.startsWith(QStringLiteral("/api/mods"))) {
        const QString name = resolveName(m_sm, m_sc, q.value(QStringLiteral("name")));
        Server *s = m_sm->serverByName(name);
        const QString p = s ? s->path() : QString();
        if (p.isEmpty()) { sendStatus(sock, 404, QStringLiteral("未找到服务器")); return; }
        sendJson(sock, QJsonObject{{QStringLiteral("name"), name},
                                   {QStringLiteral("mods"), QJsonArray::fromStringList(m_sc->listMods(p))}});
        return;
    }
    if (path == QStringLiteral("/api/usage")) {
        QJsonObject o;
        o[QStringLiteral("servers")] = QJsonDocument::fromVariant(m_sc->runningServerUsages()).array();
        sendJson(sock, o);
        return;
    }
    if (path == QStringLiteral("/api/downloads")) {
        QJsonArray arr;
        QAbstractItemModel *model = m_dm ? m_dm->downloadList() : nullptr;
        if (model) {
            const QHash<int, QByteArray> roles = model->roleNames();
            for (int r = 0; r < model->rowCount(); ++r) {
                QJsonObject row;
                for (auto it = roles.begin(); it != roles.end(); ++it)
                    row[QString::fromUtf8(it.value())] =
                        QJsonValue::fromVariant(model->data(model->index(r, 0), it.key()));
                arr << row;
            }
        }
        sendJson(sock, QJsonObject{{QStringLiteral("downloads"), arr}});
        return;
    }
    if (method == QStringLiteral("POST")) {
        const QString name = bodyJson.value(QStringLiteral("name")).toString();
        if (path == QStringLiteral("/api/start")) {
            Server *s = m_sm->serverByName(name);
            if (!s) { sendStatus(sock, 404, QStringLiteral("未找到服务器")); return; }
            m_sc->start(name, s->path());
            sendJson(sock, QJsonObject{{QStringLiteral("success"), true}, {QStringLiteral("name"), name}});
            return;
        }
        if (path == QStringLiteral("/api/stop")) {
            if (name.isEmpty()) { sendStatus(sock, 404, QStringLiteral("未找到服务器")); return; }
            m_sc->stop(name);
            sendJson(sock, QJsonObject{{QStringLiteral("success"), true}, {QStringLiteral("name"), name}});
            return;
        }
        if (path == QStringLiteral("/api/forcestop")) {
            if (name.isEmpty()) { sendStatus(sock, 404, QStringLiteral("未找到服务器")); return; }
            m_sc->forceStop(name);
            sendJson(sock, QJsonObject{{QStringLiteral("success"), true}, {QStringLiteral("name"), name}});
            return;
        }
        if (path == QStringLiteral("/api/command")) {
            const QString cmd = bodyJson.value(QStringLiteral("command")).toString();
            if (name.isEmpty()) { sendStatus(sock, 404, QStringLiteral("未找到服务器")); return; }
            if (cmd.isEmpty()) { sendStatus(sock, 400, QStringLiteral("指令为空")); return; }
            m_sc->send(name, cmd);
            sendJson(sock, QJsonObject{{QStringLiteral("success"), true}, {QStringLiteral("name"), name}, {QStringLiteral("command"), cmd}});
            return;
        }
        if (path == QStringLiteral("/api/delete")) {
            if (name.isEmpty()) { sendStatus(sock, 400, QStringLiteral("未提供服务器名")); return; }
            const int idx = m_sm->indexOfServer(name);
            if (idx < 0) { sendStatus(sock, 404, QStringLiteral("未找到服务器")); return; }
            m_sm->removeServer(idx);
            sendJson(sock, QJsonObject{{QStringLiteral("success"), true}, {QStringLiteral("name"), name}});
            return;
        }
        if (path == QStringLiteral("/api/control")) {
            const QString target = bodyJson.value(QStringLiteral("target")).toString();
            const bool en = bodyJson.value(QStringLiteral("enabled")).toBool();
            if (target == QStringLiteral("webui")) {
                QTimer::singleShot(0, this, [this, en]() { m_settings->setWebuiEnabled(en); m_settings->apply(); });
            } else if (target == QStringLiteral("napcat")) {
                QTimer::singleShot(0, this, [this, en]() { setNapcatEnabled(en); });
            } else if (target == QStringLiteral("nonebot")) {
                QTimer::singleShot(0, this, [this, en]() { setNonebotEnabled(en); });
            } else {
                sendStatus(sock, 400, QStringLiteral("未知目标")); return;
            }
            sendJson(sock, QJsonObject{{QStringLiteral("success"), true}, {QStringLiteral("target"), target}, {QStringLiteral("enabled"), en}});
            return;
        }
    }
    sendStatus(sock, 404, QStringLiteral("Not Found"));
}

// ---------- 推送 ----------

void BotController::pushToClients(const QJsonObject &obj)
{
    if (m_wsClients.isEmpty())
        return;
    const QByteArray frame = wsEncodeFrame(
        QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Compact)));
    for (QTcpSocket *c : m_wsClients) {
        if (c->state() == QAbstractSocket::ConnectedState)
            c->write(frame);
    }
}

void BotController::sendWsText(QTcpSocket *s, const QString &text)
{
    if (s->state() == QAbstractSocket::ConnectedState)
        s->write(wsEncodeFrame(text));
}

QByteArray BotController::wsEncodeFrame(const QString &text)
{
    const QByteArray payload = text.toUtf8();
    const quint64 n = static_cast<quint64>(payload.size());
    QByteArray frame;
    frame.append(static_cast<char>(0x81));   // FIN + text
    if (n <= 125) {
        frame.append(static_cast<char>(n));
    } else if (n <= 0xFFFF) {
        frame.append(static_cast<char>(126));
        frame.append(static_cast<char>((n >> 8) & 0xFF));
        frame.append(static_cast<char>(n & 0xFF));
    } else {
        frame.append(static_cast<char>(127));
        for (int i = 7; i >= 0; --i)
            frame.append(static_cast<char>((n >> (8 * i)) & 0xFF));
    }
    frame.append(payload);
    return frame;
}

void BotController::upgradeToWs(QTcpSocket *s, const QMap<QString, QString> &hdr)
{
    const QString key = hdr.value(QStringLiteral("sec-websocket-key"));
    const QByteArray accept = QCryptographicHash::hash(
        (key + QStringLiteral("258EAFA5-E914-47DA-95CA-C5AB0DC85B11")).toUtf8(),
        QCryptographicHash::Sha1).toBase64();
    const QByteArray resp = "HTTP/1.1 101 Switching Protocols\r\n"
                            "Upgrade: websocket\r\n"
                            "Connection: Upgrade\r\n"
                            "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    s->write(resp);
    m_wsClients.insert(s);
    m_botSeen.restart();
    if (m_nonebotState == QStringLiteral("waiting"))
        setNonebotState(QStringLiteral("running"));
    qDebug() << "[BOT] 控制通道 WebSocket 已连入";
}

void BotController::parseWsFrames(QTcpSocket *s)
{
    QByteArray &buf = m_buffers[s];
    buf.append(s->readAll());
    for (;;) {
        if (buf.size() < 2)
            return;
        const quint8 b0 = static_cast<quint8>(buf.at(0));
        const quint8 b1 = static_cast<quint8>(buf.at(1));
        const bool masked = (b1 & 0x80);
        quint64 len = b1 & 0x7F;
        quint64 idx = 2;
        if (len == 126) {
            if (buf.size() < 4) return;
            len = (static_cast<quint8>(buf.at(2)) << 8) | static_cast<quint8>(buf.at(3));
            idx = 4;
        } else if (len == 127) {
            if (buf.size() < 10) return;
            len = 0;
            for (int i = 0; i < 8; ++i)
                len = (len << 8) | static_cast<quint8>(buf.at(2 + i));
            idx = 10;
        }
        QByteArray maskKey;
        if (masked) {
            if (buf.size() < static_cast<int>(idx + 4)) return;
            maskKey = buf.mid(static_cast<int>(idx), 4);
            idx += 4;
        }
        if (buf.size() < static_cast<int>(idx + len)) return;
        QByteArray payload = buf.mid(static_cast<int>(idx), static_cast<int>(len));
        if (masked) {
            for (quint64 i = 0; i < len; ++i)
                payload[static_cast<int>(i)] = static_cast<char>(
                    payload.at(static_cast<int>(i)) ^ maskKey.at(static_cast<int>(i & 3)));
        }
        buf.remove(0, static_cast<int>(idx + len));

        const quint8 opcode = b0 & 0x0F;
        if (opcode == 0x8) {                  // close
            if (s->state() == QAbstractSocket::ConnectedState)
                s->write(QByteArrayLiteral("\x88\x00"));
            s->disconnectFromHost();
            return;
        } else if (opcode == 0x9) {           // ping -> pong
            if (s->state() == QAbstractSocket::ConnectedState)
                s->write(QByteArrayLiteral("\x8A\x00"));
        }
        // 0x1/0x2(text/binary) 与 0xA(pong)：插件仅接收推送，不发指令，忽略
    }
}

void BotController::notify(const QString &message, const QString &scope)
{
    if (message.isEmpty())
        return;
    QJsonObject body;
    body[QStringLiteral("message")] = message;
    body[QStringLiteral("scope")] = scope;
    pushToClients(body);
}

void BotController::pushError(const QString &name, const QString &logTail)
{
    QString tail = logTail;
    if (tail.size() > 1500)
        tail = tail.right(1500);
    const QString msg = QStringLiteral("[MSM] 服务器 %1 异常退出！\n最近日志：\n%2").arg(name, tail);
    notify(msg, QStringLiteral("admin"));
}

void BotController::pushUsage()
{
    // 状态推送：不再往群里刷占用消息，改为让机器人把设备占用写进 QQ 昵称。
    // 通过 WS 推送 {type:"nick", nick:"..."}，NapCat 插件 / NoneBot 插件收到后
    // 调用 OneBot set_qq_profile 更新昵称。无 message 字段，旧版客户端会忽略。
    if (m_wsClients.isEmpty())
        return;

    const double cpuPct = deviceCpuPercent();
    double memUsedG = 0.0, memTotalG = 0.0;
    const bool hasMem = deviceMemoryGB(memUsedG, memTotalG);
    const int running = m_sc->runningServerUsages().size();

    // QQ 昵称上限约 36 字符，保持精简，如：MSM丨CPU23%丨内存6.2/16G丨1服
    QStringList parts;
    parts << QStringLiteral("MSM");
    if (cpuPct >= 0.0)
        parts << QStringLiteral("CPU%1%").arg(cpuPct, 0, 'f', 0);
    if (hasMem)
        parts << QStringLiteral("内存%1/%2G")
                     .arg(memUsedG, 0, 'f', 1)
                     .arg(memTotalG, 0, 'f', 0);
    parts << QStringLiteral("%1服").arg(running);

    QJsonObject body;
    body[QStringLiteral("type")] = QStringLiteral("nick");
    body[QStringLiteral("nick")] = parts.join(QStringLiteral("丨"));
    pushToClients(body);
}
