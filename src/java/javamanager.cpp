/*
 * javamanager.cpp —— JDK 运行环境管理
 * -------------------------------------------------
 * 职责：
 *   1. 根据 Minecraft 版本号推导所需的 Java 特性版本（requiredFeature）。
 *   2. 探测本机已安装 / 已托管的 JDK（detectJava / scanCommonJava / probeJava）。
 *   3. 下载并安装指定版本的 JDK 到托管目录（managedDir / installBase）。
 *   4. 为创建/模组服流程提供临时 Java 环境（prepareTempJava / cleanupTempJava）。
 * 关键约定：
 *   * 探测与下载全部异步（QProcess 信号 + QTimer 超时保护），禁止阻塞 UI 线程。
 *   * 下载通过构造注入的 DownloadListModel 接入统一下载列表（进度/取消/暂停）。
 *   * requiredFeature 同时兼容 "1.21.9" 与裸版本号 "21" 两种输入。
 */
#include "javamanager.h"
#include "downloadmanager.h"     // 完整定义：Java 下载统一经 DownloadManager 进入下载面板
#include "downloadlistmodel.h"   // 由 DownloadManager::downloadList() 取得

#include <functional>
#include <memory>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QTimer>
#include <QPointer>
#include <QElapsedTimer>
#include <QDebug>
#include <QDesktopServices>
#include <QTextStream>
#include <QWinEventNotifier>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <string>
#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shellapi.h>
#  pragma comment(lib, "shell32.lib")
#endif

// 判断当前进程是否已提权（管理员令牌）。非 Windows 平台恒为 true。
static bool isCurrentProcessElevated()
{
#ifdef Q_OS_WIN
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return false;
    TOKEN_ELEVATION te;
    DWORD size = 0;
    const bool ok = GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &size);
    CloseHandle(hToken);
    return ok && te.TokenIsElevated;
#else
    return true;
#endif
}
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

JavaManager::JavaManager(DownloadManager *dm, QObject *parent)
    : QObject(parent)
    , m_dm(dm)
    , m_list(dm ? dm->downloadList() : nullptr)
{
    // 依赖必须显式注入：缺省管理器会导致 Java 下载无法进入统一列表，属于硬性错误而非静默降级。
    Q_ASSERT_X(m_dm, "JavaManager", "必须传入 DownloadManager*，否则 Java 下载无法进入统一下载面板");
    m_nam = new QNetworkAccessManager(this);
    // 通过注入的管理器把 Java 下载接入统一下载面板（并行 + 进度展示 + 取消 + 90s 超时）。
    connect(m_list, &DownloadListModel::cancelRequested, this,
            [this](const QString &id) {
                auto it = m_javaReplies.find(id);
                if (it != m_javaReplies.end() && it.value()) {
                    m_javaAborted[id] = true; // 标记主动中断，finished 不再重试/报错
                    it.value()->abort();      // 触发 finished -> 走"已取消"分支
                } else if (m_javaPaused.contains(id)) {
                    // 已暂停（无活动 reply）：直接清理部分文件与断点，置为已取消
                    QFile::remove(managedDir() + QStringLiteral("/jdk-%1.partial").arg(id));
                    m_javaOffset.remove(id);
                    m_javaTotal.remove(id);
                    m_javaPaused.remove(id);
                    m_list->setState(id, DownloadListModel::StCanceled, QStringLiteral("已取消"));
                }
                // 下载中心临时 Java（ensureTemp）走 DownloadManager 任务，取消由 DM 自身处理
                if (m_pending.contains(id)) {
                    m_pending.remove(id);
                    setStatus(QStringLiteral("Java 下载已取消"));
                }
            });
    connect(m_list, &DownloadListModel::pauseRequested, this, &JavaManager::pause);
    connect(m_list, &DownloadListModel::resumeRequested, this, &JavaManager::resume);
    connect(m_list, &DownloadListModel::restartRequested, this, &JavaManager::restart);
    // ensureTemp 经 DownloadManager 下载 Java 压缩包：任务完成/出错时续跑提取与校验
    if (m_dm) {
        connect(m_dm, &DownloadManager::finished, this, &JavaManager::onDmFinished);
        connect(m_dm, &DownloadManager::error, this, &JavaManager::onDmError);
    }
}

void JavaManager::onDmFinished(const QString &id, const QString &path)
{
    Q_UNUSED(path);
    auto it = m_pending.find(id);
    if (it == m_pending.end())
        return; // 非 ensureTemp 任务（如服务端核心），由 DownloadManager 自行处理
    std::function<void(bool, QString)> cb = it.value();
    m_pending.erase(it);
    cb(true, QString());
}

void JavaManager::onDmError(const QString &id, const QString &msg)
{
    auto it = m_pending.find(id);
    if (it == m_pending.end())
        return; // 非 ensureTemp 任务，由 DownloadManager 自行处理
    std::function<void(bool, QString)> cb = it.value();
    m_pending.erase(it);
    cb(false, msg);
}

int JavaManager::requiredFeature(const QString &mcVersion)
{
    const QStringList p = mcVersion.split(QLatin1Char('.'));
    // 处理裸版本号 "8" / "17" / "21" → 直接作为 Java 特性版本返回
    if (p.size() == 1 && !mcVersion.isEmpty()) {
        bool ok = false;
        const int v = p.at(0).toInt(&ok);
        if (ok && v >= 7 && v <= 30)
            return v;
    }
    if (p.size() >= 2) {
        const int major = p.at(0).toInt();
        // 如果首段 >= 2 且是合理的 Java 特性版本，说明可能缺失了 "1." 前缀
        // （如 "21.9" 实为 "1.21.9"），重新以完整格式解析
        if (major >= 2 && major <= 30)
            return requiredFeature(QStringLiteral("1.") + mcVersion);
        // 标准 MC 版本号 "1.x.y"
        const int b = p.at(1).toInt();
        const int patch = p.size() >= 3 ? p.at(2).toInt() : 0;
        // 1.26+（2026 年起启用“年.次”版本号，如 26.2）需要 Java 25（class file 69.0）
        if (b >= 26)        return 25;
        if (b >= 21)        return 21;          // 1.21 ~ 1.25 需 Java 21
        if (b == 20)        return (patch >= 5) ? 21 : 17; // 1.20.5+ 需 21，其余 17
        if (b >= 17)        return 17;          // 1.17 ~ 1.19
        if (b <= 16)        return 8;           // 1.16 及更早
    }
    return 17;
}

QString JavaManager::managedDir() const
{
    // 与下载目录同级：<downloadDir 的父目录>/jvm
    // 使用 QFileInfo::absolutePath() 取到父目录再用 cleanPath，避免路径里残留 ".."
    // 导致 mkpath / Expand-Archive / java.exe 查找在某些环境下失败。
    QSettings s;
    QString base = s.value(QStringLiteral("app/downloadDir"),
                           QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
                           + QStringLiteral("/MinecraftServerManager")).toString();
    QFileInfo fi(base);
    QDir parentDir(fi.absolutePath());
    parentDir.mkpath(QStringLiteral("jvm"));
    return QDir::cleanPath(parentDir.absoluteFilePath(QStringLiteral("jvm")));
}

QString JavaManager::installBase() const
{
    // 优先使用 setInstallBase() 指定的覆盖目录（让 Java 与服务端核心同目录），
    // 否则回退到全局管理目录 Downloads/.../jvm/。
    if (!m_installBase.isEmpty()) {
        const QString p = QDir::cleanPath(m_installBase);
        QDir().mkpath(p);
        return p;
    }
    return managedDir();
}

void JavaManager::setInstallBase(const QString &path)
{
    m_installBase = path;
}

QString JavaManager::installedJava(int feature) const
{
    // 当显式调用 setInstallBase 设置了覆盖路径时，installBase() 返回的已是完整目标目录
    // （如 {path}/jvm-21/），不再追加 /{feature} 层。
    // 未设置覆盖时回退到默认 managedDir + /{feature}（Downloads/jvm/{feature}）。
    const QString root = m_installBase.isEmpty()
        ? (installBase() + QStringLiteral("/") + QString::number(feature))
        : installBase();
    QDir dir(root);
    if (!dir.exists())
        return QString();
    // 递归查找 java.exe
    QFileInfoList stack;
    stack << QDir::drives(); // 占位，实际用 stack 方式
    QList<QDir> dirs; dirs << dir;
    while (!dirs.isEmpty()) {
        QDir d = dirs.takeFirst();
        const QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo &fi : entries) {
            if (fi.isDir()) {
                dirs.append(QDir(fi.absoluteFilePath()));
            } else if (fi.fileName().compare(QStringLiteral("java.exe"), Qt::CaseInsensitive) == 0
                       || fi.fileName().compare(QStringLiteral("java"), Qt::CaseInsensitive) == 0) {
                return fi.absoluteFilePath();
            }
        }
    }
    return QString();
}

QString JavaManager::manualJavaHome() const
{
    return QSettings().value(QStringLiteral("java/manualHome")).toString();
}

void JavaManager::setManualJavaHome(const QString &dir)
{
    QSettings s;
    if (dir.trimmed().isEmpty())
        s.remove(QStringLiteral("java/manualHome"));
    else
        s.setValue(QStringLiteral("java/manualHome"), QDir::fromNativeSeparators(dir.trimmed()));
}

void JavaManager::detectJava(int feature, std::function<void(const QString &)> cb)
{
    // 按优先级依次尝试：手动目录 -> PATH -> JAVA_HOME -> 常见安装目录/MC 运行时
    auto steps = std::make_shared<QList<std::function<void(std::function<void(const QString &)>)>>>();
    steps->append([this, feature](std::function<void(const QString &)> next) {
        const QString manual = manualJavaHome();
        if (manual.isEmpty()) { next(QString()); return; }
        const QFileInfo mi(manual);
        const QString direct = mi.isDir()
            ? mi.absoluteFilePath() + QStringLiteral("/bin/java.exe")
            : mi.absoluteFilePath();
        if (!QFile::exists(direct)) { next(QString()); return; }
        probeJava(direct, feature, next);
    });
    steps->append([this, feature](std::function<void(const QString &)> next) {
        const QString j = QStandardPaths::findExecutable(QStringLiteral("java"));
        if (j.isEmpty()) { next(QString()); return; }
        probeJava(j, feature, next);
    });
    steps->append([this, feature](std::function<void(const QString &)> next) {
        const QString home = qEnvironmentVariable("JAVA_HOME");
        if (home.isEmpty()) { next(QString()); return; }
        const QString he = QDir(home).absolutePath() + QStringLiteral("/bin/java.exe");
        if (!QFile::exists(he)) { next(QString()); return; }
        probeJava(he, feature, next);
    });
    steps->append([this, feature](std::function<void(const QString &)> next) {
        scanCommonJava(feature, next);
    });
    auto idx = std::make_shared<int>(0);
    // 必须用 shared_ptr 持有递归函数：若直接按值捕获 std::function，捕获到的是赋值前的空对象，
    // 递归调用（go()）会触发 std::bad_function_call 导致崩溃（机器无匹配 Java 时每次都会走到）。
    auto go = std::make_shared<std::function<void()>>();
    *go = [steps, idx, go, cb]() {
        if (*idx >= steps->size()) { cb(QString()); return; }
        const auto step = (*steps)[*idx];
        ++(*idx);
        step([cb, go](const QString &r) {
            if (!r.isEmpty()) cb(r);
            else (*go)();
        });
    };
    (*go)();
}

void JavaManager::probeJava(const QString &exe, int feature, std::function<void(const QString &)> cb)
{
    if (exe.isEmpty()) { cb(QString()); return; }
    auto *p = new QProcess(this);
    QPointer<QProcess> guard(p);
    auto finished = std::make_shared<bool>(false);
    auto finalize = [finished, guard, cb](bool ok, const QString &path) {
        if (*finished) return;
        *finished = true;
        if (guard) guard->deleteLater();
        cb(ok ? path : QString());
    };
    QObject::connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), p,
        [p, finalize, exe, feature](int code, QProcess::ExitStatus) {
            if (code != 0) { finalize(false, exe); return; }
            const QString out = QString::fromLocal8Bit(p->readAllStandardOutput())
                                + QString::fromLocal8Bit(p->readAllStandardError());
            auto majorOf = [](const QString &s) -> int {
                int idx = s.indexOf(QLatin1String("version \""));
                if (idx < 0) return -1;
                QString v = s.mid(idx + 9);
                int q = v.indexOf(QLatin1Char('"'));
                if (q >= 0) v = v.left(q);
                if (v.startsWith(QLatin1String("1.")))   // 1.8.x
                    return v.mid(2).section(QLatin1Char('.'), 0, 0).toInt();
                return v.section(QLatin1Char('.'), 0, 0).toInt();
            };
            finalize(majorOf(out) == feature, exe);
        });
    QObject::connect(p, &QProcess::errorOccurred, p, [finalize]() { finalize(false, QString()); });
    // 超时保护：最多等待 3s（与旧 waitForFinished(3000) 等价的安全上限），超时则终止进程
    auto *timer = new QTimer(p);
    timer->setSingleShot(true);
    QObject::connect(timer, &QTimer::timeout, p, [guard]() {
        if (guard && guard->state() != QProcess::NotRunning) guard->kill();
    });
    timer->start(3000);
    p->start(exe, QStringList() << QStringLiteral("-version"));
}

int JavaManager::majorFromName(const QString &name) const
{
    const QString n = name.toLower();
    if (n.contains(QStringLiteral("1.8"))) return 8;
    // jdk-21 / zulu21 / corretto-21 / openjdk21 / java21 …
    static const QRegularExpression re(QStringLiteral("(jdk|jre|zulu|corretto|java|openjdk)[-_ ]?(\\d+)"));
    QRegularExpressionMatch m = re.match(n);
    if (m.hasMatch()) {
        const int v = m.captured(2).toInt();
        if (v >= 7 && v <= 30) return v;
    }
    // 形如 21.0.3 / 17.0.x 的数字段
    static const QRegularExpression re2(QStringLiteral("(^|[^0-9])(\\d{1,2})\\.\\d"));
    QRegularExpressionMatch m2 = re2.match(n);
    if (m2.hasMatch()) {
        const int v = m2.captured(2).toInt();
        if (v >= 7 && v <= 30) return v;
    }
    return 0; // 无法从名称判断
}

QStringList JavaManager::commonJavaBases() const
{
    QStringList bases;
    const QString pf = QStringLiteral("C:/Program Files");
    const QString pfx86 = QStringLiteral("C:/Program Files (x86)");
    bases << pf + QStringLiteral("/Java")
          << pf + QStringLiteral("/Eclipse Adoptium")
          << pf + QStringLiteral("/AdoptOpenJDK")
          << pf + QStringLiteral("/Microsoft/jdk")
          << pf + QStringLiteral("/Amazon Corretto")
          << pf + QStringLiteral("/Zulu")
          << pf + QStringLiteral("/BellSoft")
          << pfx86 + QStringLiteral("/Java")
          << QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                 + QStringLiteral("/.minecraft/runtime");
    return bases;
}

void JavaManager::scanCommonJava(int feature, std::function<void(const QString &)> cb)
{
    QStringList candidates;
    for (const QString &base : commonJavaBases()) {
        QDir d(base);
        if (!d.exists()) continue;
        const QFileInfoList subs = d.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
        for (const QFileInfo &sub : subs) {
            const QString je = sub.absoluteFilePath() + QStringLiteral("/bin/java.exe");
            if (!QFile::exists(je)) continue;
            const int maj = majorFromName(sub.fileName());
            // 名称能确定主版本且不匹配则跳过（不浪费一次进程探测）；其余情况再实测
            if (maj != 0 && maj != feature) continue;
            candidates << je;
            if (candidates.size() >= 16) break;   // 候选上限，避免过慢
        }
        if (candidates.size() >= 16) break;
    }
    auto idx = std::make_shared<int>(0);
    // 同上：用 shared_ptr 持有递归函数，避免按值捕获到空 std::function 触发 std::bad_function_call
    auto step = std::make_shared<std::function<void()>>();
    *step = [this, feature, candidates, idx, step, cb]() {
        if (*idx >= candidates.size()) { cb(QString()); return; }
        const QString je = candidates[(*idx)++];
        probeJava(je, feature, [step, cb](const QString &r) {
            if (!r.isEmpty()) cb(r);
            else (*step)();
        });
    };
    (*step)();
}

QString JavaManager::javaPathFor(int feature)
{
    auto it = m_javaCache.constFind(feature);
    if (it != m_javaCache.constEnd() && !it.value().isEmpty())
        return it.value();
    const QString inst = installedJava(feature);
    if (!inst.isEmpty()) {
        m_javaCache[feature] = inst;
        return inst;
    }
    refreshJavaCache(feature);   // 后台异步探测，结果经 javaResolved 暴露，下次调用即可命中
    return QString();
}

QString JavaManager::javaPathFor(const QString &mcVersion)
{
    return javaPathFor(requiredFeature(mcVersion));
}

void JavaManager::javaPathForAsync(int feature, std::function<void(const QString &)> cb)
{
    auto it = m_javaCache.constFind(feature);
    if (it != m_javaCache.constEnd() && !it.value().isEmpty()) { cb(it.value()); return; }
    const QString inst = installedJava(feature);
    if (!inst.isEmpty()) { m_javaCache[feature] = inst; cb(inst); return; }
    detectJava(feature, [this, feature, cb](const QString &r) {
        if (!r.isEmpty()) { m_javaCache[feature] = r; emit javaResolved(feature, r); }
        cb(r);
    });
}

void JavaManager::refreshJavaCache(int feature)
{
    auto it = m_javaCache.constFind(feature);
    if (it != m_javaCache.constEnd() && !it.value().isEmpty()) return;
    const QString inst = installedJava(feature);
    if (!inst.isEmpty()) { m_javaCache[feature] = inst; return; }
    if (m_detecting.contains(feature)) return;   // 同一 feature 不重复后台探测
    m_detecting.insert(feature);
    detectJava(feature, [this, feature](const QString &r) {
        m_detecting.remove(feature);
        if (!r.isEmpty()) { m_javaCache[feature] = r; emit javaResolved(feature, r); }
    });
}

QVariantMap JavaManager::statusFor(const QString &mcVersion)
{
    return statusForFeature(requiredFeature(mcVersion));
}

QVariantMap JavaManager::statusForFeature(int feature)
{
    QVariantMap m;
    const QString path = javaPathFor(QString::number(feature));
    m[QStringLiteral("feature")] = feature;
    m[QStringLiteral("available")] = !path.isEmpty();
    m[QStringLiteral("path")] = path;
    const bool dl = m_downloadingFeatures.contains(feature);
    m[QStringLiteral("downloading")] = dl;
    m[QStringLiteral("progress")] = dl ? m_progressByFeature.value(feature, 0.0) : (path.isEmpty() ? 0 : 1);
    m[QStringLiteral("statusText")] = m_status;
    m[QStringLiteral("errorText")] = m_error;
    return m;
}

void JavaManager::ensure(const QString &mcVersion, std::function<void(bool, QString)> cb)
{
    // 转交给按特性版本的入口，便于 WebUI/下载中心直接指定 JDK 大版本
    ensureFeature(requiredFeature(mcVersion), cb);
}

void JavaManager::ensureFeature(int feature, std::function<void(bool, QString)> cb)
{
    // 已可用（已安装/已缓存）：直接回调（下一事件循环，保持异步语义）；否则异步权威探测后再决定是否下载
    javaPathForAsync(feature, [this, feature, cb](const QString &have) {
    if (!have.isEmpty()) {
        QTimer::singleShot(0, this, [cb, have]() { cb(true, have); });
        return;
    }

    // 并行下载：每个 ensureFeature 都启动自己独立的下载链（独立 QNetworkReply），
    // 不再用全局 m_busy / m_pending 串行排队。统一下载列表会分别展示每个版本进度。
    m_downloadingFeatures.insert(feature);
    setBusy(!m_downloadingFeatures.isEmpty());
    download(feature, [this, feature, cb](bool ok, QString path) {
        m_downloadingFeatures.remove(feature);
        setBusy(!m_downloadingFeatures.isEmpty());
        cb(ok, path);
        if (ok) emit ready(feature, path);
    });
    });
}

void JavaManager::download(int feature, std::function<void(bool, QString)> cb)
{
    // 注册到统一下载列表（并行下载，每个版本一行），listId 全程透传给 fetchInstaller 更新进度/状态
    const QString listId = QStringLiteral("java-%1-%2").arg(feature).arg(++m_javaSeq);
    const QString title = QStringLiteral("Java %1 安装包").arg(feature);
    const QString dlPath = managedDir() + QStringLiteral("/jdk-%1").arg(feature);
    m_list->add(listId, title, dlPath, QString());
    m_javaFeature[listId] = feature;
    startDownload(feature, listId, cb);
}

void JavaManager::startDownload(int feature, const QString &listId, std::function<void(bool, QString)> cb)
{
    m_javaFeature[listId] = feature;

    // 收尾：同步更新统一列表状态（成功/失败），并回调调用方
    auto finish = [this, feature, listId, cb](bool ok, QString p) {
        m_list->setState(listId,
            ok ? DownloadListModel::StDone : DownloadListModel::StError, m_error);
        cb(ok, p);
    };

    setStatus(QStringLiteral("正在获取 Java %1 安装包信息…").arg(feature));
    resolveInstaller(feature, [this, feature, finish, listId](bool ok, QString tuna, QString fallback) {
        // 源顺序：优先甲骨文官网直链（公开 archive 路径，无需 otn cookie，已规避 SIGSEGV）；
        // Adoptium（TUNA 镜像）仅在 Oracle 源全部失败时才作为兜底使用，尽量用 Oracle 源。
        Q_UNUSED(ok);
        QStringList sources;
        if (!fallback.isEmpty()) sources << fallback; // 甲骨文官网直链（优先）
        if (!tuna.isEmpty()) sources << tuna;
        sources << latestRedirectUrl(feature);         // Adoptium TUNA 镜像（兜底）

        // 用 shared_ptr 持有递归函数，避免异步回调里捕获到已析构的局部 std::function
        auto tryNext = std::make_shared<std::function<void(int)>>();
        *tryNext = [this, feature, finish, sources, tryNext, listId](int idx) {
            if (idx >= sources.size()) {
                setError(QStringLiteral("无法下载 Java %1，请检查网络或手动安装 JDK %1 后重试").arg(feature));
                setStatus(QStringLiteral("下载 Java %1 失败：所有源均不可用").arg(feature));
                finish(false, QString());
                return;
            }
            const QUrl url(sources.at(idx));
            const QString host = url.host();
            setStatus(QStringLiteral("正在从 %1 下载 Java %2 安装程序…").arg(host).arg(feature));
            fetchInstaller(feature, url, listId,
                [this, feature, finish, sources, idx, tryNext](bool ok2, QString path) {
                if (ok2) { finish(true, path); return; }
                if (!path.isEmpty()) {
                    setError(QStringLiteral("已下载 Java %1 安装包，但解压失败，文件位于：%2（可手动解压或运行）")
                                     .arg(feature).arg(path));
                    finish(false, QString());
                    return;
                }
                (*tryNext)(idx + 1); // 下载失败 -> 换下一个源
            });
        };
        (*tryNext)(0);
    });
}

QString JavaManager::latestRedirectUrl(int feature) const
{
    return QStringLiteral("https://api.adoptium.net/v3/binary/latest/%1/ga/windows/x64/jdk/hotspot/normal/eclipse")
            .arg(feature);
}

void JavaManager::resolveDownloadUrl(int feature, std::function<void(bool, QString, QString)> cb)
{
    // 优先使用用户指定的 Oracle 官网直链（优先于自动解析，确保稳定可用）
    switch (feature) {
    case 26:
        cb(true, QStringLiteral("https://download.oracle.com/java/26/latest/jdk-26_windows-x64_bin.exe"),
           QStringLiteral("jdk-26_windows-x64_bin.exe"));
        return;
    case 25:
        cb(true, QStringLiteral("https://download.oracle.com/java/25/latest/jdk-25_windows-x64_bin.exe"),
           QStringLiteral("jdk-25_windows-x64_bin.exe"));
        return;
    case 21:
        cb(true, QStringLiteral("https://download.oracle.com/java/21/latest/jdk-21_windows-x64_bin.exe"),
           QStringLiteral("jdk-21_windows-x64_bin.exe"));
        return;
    case 17:
        cb(true, QStringLiteral("https://download.oracle.com/java/17/archive/jdk-17.0.12_windows-x64_bin.exe"),
           QStringLiteral("jdk-17.0.12_windows-x64_bin.exe"));
        return;
    }
    // 8/11 走 Adoptium（TUNA 镜像）——Oracle 官网强制登录不可用
    if (feature == 8 || feature == 11) {
        resolveAdoptium(feature, [this, feature, cb](bool ok, QString tuna) {
            if (ok && !tuna.isEmpty()) {
                // 从 TUNA 直链末尾提取真实文件名（如 OpenJDK8U-jdk_x64_windows_hotspot_8u442b06.msi）
                const QString fname = tuna.mid(tuna.lastIndexOf(QLatin1Char('/')) + 1);
                cb(true, tuna, fname);
                return;
            }
            // 兜底：Adoptium latest 重定向，文件名用默认 .msi
            const QString fallback = QStringLiteral("OpenJDK%1-installer.msi").arg(feature);
            cb(true, latestRedirectUrl(feature), fallback);
        });
        return;
    }
    // 其他未知版本兜底
    cb(true, latestRedirectUrl(feature),
       QStringLiteral("OpenJDK%1-installer.msi").arg(feature));
}

void JavaManager::pause(const QString &id)
{
    auto it = m_javaReplies.find(id);
    if (it == m_javaReplies.end() || !it.value())
        return;
    m_javaAborted[id] = true;   // 标记主动中断，finished 不再重试/报错
    m_javaPaused[id] = true;    // 标记为暂停态（继续按钮依据）
    it.value()->abort();
    m_list->setState(id, DownloadListModel::StPaused);
}

void JavaManager::resume(const QString &id)
{
    if (!m_javaPaused.contains(id))
        return;
    const int feature = m_javaFeature.value(id, -1);
    const QUrl url = m_javaUrl.value(id);
    m_javaPaused.remove(id);
    if (feature < 0) {
        m_list->setState(id, DownloadListModel::StError, QStringLiteral("无法继续：缺少下载信息"));
        return;
    }
    if (url.isEmpty()) {
        // 未记录到源地址：退化为完整重新下载
        startDownload(feature, id,
            [this, id, feature](bool ok, QString path) {
                if (ok) { m_list->setState(id, DownloadListModel::StDone, QString()); finalizeFeature(feature); emit ready(feature, path); }
            });
        return;
    }
    m_list->setState(id, DownloadListModel::StDownloading);
    // 从已落盘的断点继续：先按已下载量刷新进度，fetchInstaller 会发送 Range 请求续传
    const qint64 off = m_javaOffset.value(id, 0);
    const qint64 tot = m_javaTotal.value(id, 0);
    m_list->setProgress(id, tot > 0 ? (100.0 * off / tot) : 0);
    setStatus(QStringLiteral("正在从断点继续下载 Java %1（已下载 %2 MB）…")
                      .arg(feature).arg(off / (1024 * 1024)));
    fetchInstaller(feature, url, id,
        [this, feature, id](bool ok, QString path) {
            if (ok) {
                m_list->setState(id, DownloadListModel::StDone, m_error);
                finalizeFeature(feature);
                emit ready(feature, path);
            } else if (path.isEmpty()) {
                // 该源失败：退化到完整重试（尝试其它源）
                startDownload(feature, id,
                    [this, feature, id](bool ok2, QString p2) {
                        if (!ok2) m_list->setState(id, DownloadListModel::StError, m_error);
                        else { m_list->setState(id, DownloadListModel::StDone, QString()); finalizeFeature(feature); emit ready(feature, p2); }
                    });
            } else {
                m_list->setState(id, DownloadListModel::StError, m_error);
            }
        });
}

void JavaManager::finalizeFeature(int feature)
{
    m_downloadingFeatures.remove(feature);
    setBusy(m_downloadingFeatures.isEmpty() ? false : m_busy);
}

void JavaManager::restart(const QString &id)
{
    const int feature = m_javaFeature.value(id, -1);
    if (feature < 0) {
        m_list->setState(id, DownloadListModel::StError, QStringLiteral("无法重试：缺少下载信息"));
        return;
    }
    m_javaPaused.remove(id);
    m_javaAborted.remove(id);
    // 清掉断点续传的状态与部分文件，从头重新开始
    QFile::remove(managedDir() + QStringLiteral("/jdk-%1.partial").arg(id));
    m_javaOffset.remove(id);
    m_javaTotal.remove(id);
    // 清掉可能残留的 reply
    auto it = m_javaReplies.find(id);
    if (it != m_javaReplies.end() && it.value()) {
        it.value()->abort();
        it.value()->deleteLater();
        m_javaReplies.erase(it);
    }
    m_list->setState(id, DownloadListModel::StDownloading);
    m_list->setProgress(id, 0);
    // 复用同一行，重新走完整流程（解析源 + 下载 + 安装）
    startDownload(feature, id,
        [this, feature, id](bool ok, QString path) {
            if (ok) { finalizeFeature(feature); emit ready(feature, path); }
        });
}

void JavaManager::resolveInstaller(int feature, std::function<void(bool, QString, QString)> cb)
{
    // 所有版本优先走 Adoptium (Temurin) 清华 TUNA 镜像：
    //   TUNA 国内直连、速度快，不依赖被反复屏蔽的 Oracle download.oracle.com。
    //   resolveAdoptium() 对任意 feature 都可用（18/21/25 等），返回 MSI 直链。
    //   MSI 经 msiexec /quiet 静默安装在用户目录下，无需管理员/UAC。
    // 若 TUNA 不可用（资产 API 超时等），由 fetchInstaller 走 Adoptium GitHub 回退；
    // 然后才尝试 Oracle 归档页（被墙/鉴权概率高，只做最后兜底）。
    resolveAdoptium(feature, [this, feature, cb](bool ok, QString tunaUrl) {
        if (ok && !tunaUrl.isEmpty()) {
            cb(true, QString(), tunaUrl);  // TUNA Adoptium 直连 zip -> 解压直接使用
            return;
        }

        // —— Adoptium 不可用，尝试 Oracle zip 兜底 ——
        // Oracle latest 直链（zip 版本，解压即用）
        const QString latest = QStringLiteral("https://download.oracle.com/java/%1/latest/jdk-%1_windows-x64_bin.zip")
                .arg(feature);
        QUrl latestUrl(latest);
        QNetworkRequest headReq(latestUrl);
        headReq.setTransferTimeout(15000);
        headReq.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        headReq.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        QNetworkReply *head = m_nam->head(headReq);
        connect(head, &QNetworkReply::finished, this, [this, feature, latest, head, cb]() {
            head->deleteLater();
            const int code = head->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (head->error() == QNetworkReply::NoError && code == 200) {
                cb(true, QString(), latest);
                return;
            }
            // latest 不可用 → 归档页提取 zip 直链
            const QString page = QStringLiteral(
                    "https://www.oracle.com/java/technologies/javase/jdk%1-archive-downloads.html")
                    .arg(feature);
            QUrl pageUrl(page);
            QNetworkRequest req(pageUrl);
            req.setTransferTimeout(15000);
            req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
            req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
            req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MSM/%1")
                          .arg(QCoreApplication::applicationVersion()));
            QNetworkReply *reply = m_nam->get(req);
            connect(reply, &QNetworkReply::finished, this, [this, reply, feature, cb]() {
                reply->deleteLater();
                const QString fallback = knownOracleLink(feature);
                if (reply->error() != QNetworkReply::NoError) {
                    if (!fallback.isEmpty()) { cb(true, QString(), fallback); return; }
                    setStatus(QStringLiteral("无法获取 JDK %1 下载链接，请手动安装后重试").arg(feature));
                    cb(false, QString(), QString());
                    return;
                }
                const QString html = QString::fromUtf8(reply->readAll());
                // 匹配 .zip 链接（解压即用）；也保留 .exe 匹配作为兼容
                const QRegularExpression reZip(
                    QStringLiteral(R"(https://download\.oracle\.com/[^\s"'>]+windows-x64_bin\.zip)"));
                QRegularExpressionMatch mz = reZip.match(html);
                QString winLink = mz.hasMatch() ? mz.captured(0) : QString();
                // linux 备用链接缓存
                const QRegularExpression reLinux(
                    QStringLiteral(R"(https://download\.oracle\.com/[^\s"'>]+linux-x64_bin\.tar\.gz)"));
                const QRegularExpressionMatch ml = reLinux.match(html);
                if (ml.hasMatch()) m_linuxLinks[feature] = ml.captured(0);

                if (winLink.isEmpty()) {
                    if (!fallback.isEmpty()) { cb(true, QString(), fallback); return; }
                    setStatus(QStringLiteral("无法获取 JDK %1 下载链接，请手动安装后重试").arg(feature));
                    cb(false, QString(), QString());
                    return;
                }
                cb(true, QString(), winLink);
            });
        });
    });
}

QString JavaManager::knownOracleLink(int feature) const
{
    // 抓取归档页失败时的兜底（依当前实测）。用公开的 download.oracle.com 归档直链，
    // 无需 otn cookie / 二次鉴权（otn 路径在 Qt6+MinGW+Schannel 下大文件下载易 SIGSEGV）。
    switch (feature) {
    case 17: return QStringLiteral("https://download.oracle.com/java/17/archive/jdk-17.0.12_windows-x64_bin.zip");
    case 11: return QStringLiteral("https://download.oracle.com/java/11/archive/jdk-11.0.30_windows-x64_bin.zip");
    default: return QString();
    }
}

void JavaManager::resolveAdoptium(int feature, std::function<void(bool, QString)> cb)
{
    // 查 Adoptium 资产 API（仅返回小 JSON，毫秒级，不受 GitHub 大文件下载慢的影响），
    // 拿到 Windows x64 JDK 的 ZIP 包文件名，再拼出清华 TUNA 的 Adoptium 直连镜像：
    //   https://mirrors.tuna.tsinghua.edu.cn/Adoptium/{feature}/jdk/x64/windows/{zip}
    // 取 package（ZIP）而非 installer（MSI），ZIP 解压即用、不需安装器/管理员/UAC。
    // TUNA 镜像直接托管二进制、不走 GitHub，国内下载快且稳。
    // 候选顺序：TUNA 镜像 API -> 腾讯云镜像 API -> 官方 API（国内优先，避免被墙时龟速）。
    const QStringList candidates = {
        QStringLiteral("https://mirrors.tuna.tsinghua.edu.cn/adoptium/api/v3/assets/feature_releases/%1/ga?architecture=x64&image_type=jdk&os=windows&vendor=eclipse&page_size=1").arg(feature),
        QStringLiteral("https://mirrors.cloud.tencent.com/adoptium/api/v3/assets/feature_releases/%1/ga?architecture=x64&image_type=jdk&os=windows&vendor=eclipse&page_size=1").arg(feature),
        QStringLiteral("https://api.adoptium.net/v3/assets/feature_releases/%1/ga?architecture=x64&image_type=jdk&os=windows&vendor=eclipse&page_size=1").arg(feature)
    };
    QPointer<JavaManager> self(this);
    // 递归尝试各候选 API（按值捕获的 std::function 需以 shared_ptr 持有，避免递归时捕获到空对象）
    auto step = std::make_shared<std::function<void(int)>>();
    *step = [self, feature, candidates, cb, step](int idx) {
        if (idx >= candidates.size()) { cb(false, QString()); return; }
        QNetworkRequest req{QUrl(candidates[idx])};
        req.setTransferTimeout(15000);
        req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                                     "(KHTML, like Gecko) Chrome/120.0 Safari/537.36"));
        QNetworkReply *reply = self->m_nam->get(req);
        connect(reply, &QNetworkReply::finished, self.data(), [self, reply, feature, idx, cb, step]() {
            reply->deleteLater();
            if (!self) return;
            QString zipName;
            if (reply->error() == QNetworkReply::NoError) {
                const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
                // 结构：[{ "binaries": [ { "package": { "name": "...zip", "link": "..." } } ] }]
                // 取 package 而非 installer：ZIP 解压即用、不需安装器/管理员权限
                if (doc.isArray()) {
                    const QJsonArray arr = doc.array();
                    if (!arr.isEmpty()) {
                        const QJsonObject obj = arr.first().toObject();
                        const QJsonArray binaries = obj.value(QStringLiteral("binaries")).toArray();
                        if (!binaries.isEmpty()) {
                            const QJsonObject bin = binaries.first().toObject();
                            const QJsonObject pkg = bin.value(QStringLiteral("package")).toObject();
                            zipName = pkg.value(QStringLiteral("name")).toString();
                        }
                    }
                }
            }
            if (!zipName.isEmpty()) {
                QUrl tuna; tuna.setScheme(QStringLiteral("https")); tuna.setHost(QStringLiteral("mirrors.tuna.tsinghua.edu.cn"));
                tuna.setPath(QStringLiteral("/Adoptium/%1/jdk/x64/windows/%2").arg(feature).arg(zipName));
                cb(true, tuna.toString());
                return;
            }
            (*step)(idx + 1); // 当前镜像 API 不可用 -> 尝试下一个
        });
    };
    (*step)(0);
}

void JavaManager::launchInstaller(int feature, const QString &msiPath, std::function<void(bool)> done)
{
    // 已经装过则直接成功
    if (!javaPathFor(QString::number(feature)).isEmpty()) { done(true); return; }

    const QString target = QDir::toNativeSeparators(managedDir() + QStringLiteral("/") + QString::number(feature));
    QDir().mkpath(target);
    const QString native = QDir::toNativeSeparators(msiPath);

    const QString ext = msiPath.mid(msiPath.lastIndexOf(QLatin1Char('.'))).toLower();
    const bool isMsi = ext == QStringLiteral(".msi");
    QString program;
    QStringList args;
    if (isMsi) {
        program = QStandardPaths::findExecutable(QStringLiteral("msiexec"),
                { QStringLiteral("C:/Windows/System32") });
        if (program.isEmpty()) program = QStringLiteral("msiexec.exe");
        args << QStringLiteral("/i") << native
             << QStringLiteral("/quiet") << QStringLiteral("/norestart")
             << (QStringLiteral("INSTALLDIR=\"") + target + QStringLiteral("\""));
    } else {
        // 甲骨文 .exe 安装包：用 QProcess 直接运行（类似命令行执行）。
        // 不跑 ShellExecuteEx+runas（UAC 弹窗在非管理员下不可靠），
        // 移除 /L 标志（JDK 21+ 安装器不支持）。exe 自身的 manifest 会在
        // 需要时自动弹 UAC，无需我们主动请求。
        // 安装到用户目录下（managedDir），通常无需提权。
        program = native;
        args << QStringLiteral("/s")
             << (QStringLiteral("/INSTALLDIR=") + target);
    }

    // 用共享指针管理安装器输出缓冲区，确保 read/readyReadStandardOutput 和 finished
    // 两个 lambda 操作的是同一个缓冲区（read 按引用改，finished 按值读）。
    // 直接按值拷贝会导致 finished 拿到的是创建时的空副本，永远看不到输出。
    auto logBuf = std::make_shared<QString>();

    auto finish = [this, feature, msiPath, target, isMsi, done](int code, const QString &extra) {
        const bool okCode = isMsi ? (code == 0 || code == 3010) : (code == 0);
        if (!okCode) {
            QString msg = QStringLiteral("安装 Java %1 失败（返回 %2）").arg(feature).arg(code);
            if (!extra.isEmpty())
                msg += QStringLiteral("\n安装器输出：") + extra;
            setStatus(msg);
            done(false);
            return;
        }
        QString java = javaPathFor(QString::number(feature));
        QFile::remove(msiPath);
        if (java.isEmpty()) {
            setStatus(QStringLiteral("Java %1 安装完成但未找到 java.exe，请检查目录：%2").arg(feature).arg(target));
            done(false);
            return;
        }
        QSettings s;
        s.setValue(QStringLiteral("java/installed/%1").arg(feature),
                   QDir::fromNativeSeparators(QFileInfo(java).absolutePath()));
        done(true);
    };

    qDebug() << "[JavaManager] launching installer:" << program << args;
    setStatus(QStringLiteral("正在安装 Java %1（静默安装，请稍候）…").arg(feature));
    auto *proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);
    proc->start(program, args);
    if (!proc->waitForStarted(10000)) {
        setError(QStringLiteral("无法启动 Java %1 安装程序：%2").arg(feature).arg(proc->errorString()));
        done(false);
        return;
    }
    // 累积安装器输出（失败时取末尾展示真实原因）
    connect(proc, &QProcess::readyReadStandardOutput, this, [proc, logBuf]() {
        *logBuf += QString::fromLocal8Bit(proc->readAllStandardOutput());
        if (logBuf->size() > 65536) logBuf->remove(0, logBuf->size() - 65536);
    });
    // 10 分钟超时保护
    QTimer *guard = new QTimer(this);
    guard->setSingleShot(true);
    guard->setInterval(10 * 60 * 1000);
    connect(guard, &QTimer::timeout, this, [this, proc, guard, feature]() {
        if (!proc || proc->state() == QProcess::NotRunning) return;
        qWarning() << "[JavaManager] Java" << feature << "installer timed out, killing";
        proc->kill();
    });
    guard->start();
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, feature, guard, proc, logBuf, finish](int exitCode, QProcess::ExitStatus) {
        guard->stop();
        QString tail = *logBuf;
        if (tail.size() > 2000) tail = tail.right(2000);
        const QStringList lines = tail.split(QLatin1Char('\n'));
        if (lines.size() > 25)
            tail = QStringList(lines.mid(lines.size() - 25)).join(QLatin1Char('\n'));
        qDebug() << "[JavaManager] installer exit=" << exitCode << "tail=" << tail.left(500);
        finish(exitCode, tail.trimmed());
    });
}

void JavaManager::fetchInstaller(int feature, const QUrl &url, const QString &listId, std::function<void(bool, QString)> cb)
{
    const QString root = installBase();
    // 断点续传：若已有偏移，则从该字节之后继续；否则从头开始。
    // 部分文件以 listId 命名（同一 feature 的并行下载互不干扰），下载完成后再改名为带类型的文件。
    const qint64 resumed = m_javaOffset.value(listId, 0);
    const bool resuming = resumed > 0;
    const QString partPath = root + QStringLiteral("/jdk-%1.partial").arg(listId);

    QNetworkRequest req(url);
    // 关闭 HTTP/2：Qt 6 在 Windows/MinGW + TLS(Schannel) 下进行大文件下载时，HTTP/2 易触发
    // 网络后端崩溃（SIGSEGV）；退回 HTTP/1.1 对单次安装包下载无性能影响且更稳定
    req.setAttribute(QNetworkRequest::Http2AllowedAttribute, false);
    // 较短的传输超时：连不上/卡死时快速失败并回退，而不是长时间停在 0%
    req.setTransferTimeout(20000);
    // 手动处理重定向：这样才能拦截 Adoptium 302 到被墙的 github.com，改写为国内可达的 TUNA 镜像
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    // 浏览器 UA，避免被下载服务器拒绝
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                                 "(KHTML, like Gecko) Chrome/120.0 Safari/537.36"));
    // java.com 的 AutoDL 直链需要带 java.com 的 Referer 才会返回安装包（否则只给页面）；
    // 其它甲骨文链接带 oracle 系 Referer 也无影响
    const QString referer = url.host().contains(QStringLiteral("javadl.oracle.com"))
            ? QStringLiteral("https://www.java.com/en/download/?locale=zh-cn")
            : QStringLiteral("https://www.oracle.com/");
    req.setRawHeader(QByteArrayLiteral("Referer"), referer.toUtf8());
    // 甲骨文 otn 链接（17/11 归档页）与 java.com 的 JRE 8 下载前需接受许可协议
    req.setHeader(QNetworkRequest::CookieHeader, QStringLiteral("oraclelicense=accept-license"));
    if (resuming) {
        // 请求从断点之后继续。若服务器不支持 Range，会回 200(整文件)；finished 里检测到后截断重来。
        req.setRawHeader(QByteArrayLiteral("Range"),
                         QStringLiteral("bytes=%1-").arg(resumed).toUtf8());
    }

    QNetworkReply *reply = m_nam->get(req);
    m_javaReplies[listId] = reply;   // 记录 reply，供统一列表"取消"时 abort
    m_javaUrl[listId] = url;         // 记录当前源，供"继续"复用同一源并从断点续传
    m_javaFeature[listId] = feature;
    const QString host = url.host();
    // 守卫：若 JavaManager 在本异步回调触发前已被销毁，安全跳过，避免悬空 this 解引用导致 SIGSEGV
    QPointer<JavaManager> self(this);
    QElapsedTimer progT;
    progT.start();

    // 部分文件：续传用 append（保留已下载内容），否则截断新建
    QFile *file = new QFile(partPath, this);
    const bool opened = file->open(resuming ? QIODevice::Append : (QIODevice::WriteOnly | QIODevice::Truncate));
    if (!opened) {
        delete file;
        setError(QStringLiteral("无法创建临时文件：%1").arg(partPath));
        cb(false, QString());
        return;
    }
    qint64 *written = new qint64(resumed);   // 已落盘字节数（堆上，供各信号 lambda 共享）

    // 记录总大小：优先取 206 的 Content-Range，其次 200 的 Content-Length
    connect(reply, &QNetworkReply::metaDataChanged, this, [self, reply, listId]() {
        if (!self) return;
        const QByteArray cr = reply->rawHeader(QByteArrayLiteral("Content-Range"));
        if (!cr.isEmpty()) {
            // 形如 "bytes 0-12345/67890"
            static const QRegularExpression re(QStringLiteral("bytes\\s+\\d+-\\d+/(\\d+)"));
            const QRegularExpressionMatch m = re.match(QString::fromLatin1(cr));
            if (m.hasMatch()) self->m_javaTotal[listId] = m.captured(1).toLongLong();
        } else {
            const QVariant cl = reply->header(QNetworkRequest::ContentLengthHeader);
            if (cl.isValid()) self->m_javaTotal[listId] = cl.toLongLong();
        }
    });

    // 边下边落盘：每个 readyRead 把收到的分块追加到部分文件，并刷新断点偏移
    connect(reply, &QNetworkReply::readyRead, this, [self, reply, file, written, listId]() {
        if (!self) return;
        const QByteArray buf = reply->readAll();
        if (buf.isEmpty()) return;
        const qint64 n = file->write(buf);
        if (n > 0) {
            *written += n;
            self->m_javaOffset[listId] = *written;   // 断点：暂停后可据此续传
        }
    });

    connect(reply, &QNetworkReply::downloadProgress, this,
            [self, feature, host, progT, listId, resumed](qint64 recv, qint64 total) mutable {
        Q_UNUSED(total);
        if (!self) return;
        // 限流：大文件下载时进度信号可能极频繁，限到 ~5Hz，避免事件风暴造成 UI 卡死/不稳
        if (progT.elapsed() < 200) return;
        progT.restart();
        // 续传时 recv 仅表示本段已收到字节，整体进度需叠加已落盘偏移
        const qint64 got = resumed + recv;
        const qint64 fileTotal = self->m_javaTotal.value(listId, 0);
        const qreal p = fileTotal > 0
            ? (0.1 + 0.8 * qreal(got) / fileTotal)
            : qMin(qreal(0.9), 0.1 + qreal(got) / (180.0 * 1024 * 1024));
        self->setProgressFor(feature, p);
        // 同步到统一下载列表（百分比 0~100），多个 Java 并行时各自的 listId 互不干扰
        self->m_list->setProgress(listId, p * 100.0);
        self->setStatus(QStringLiteral("正在从 %1 下载 Java %2 安装程序（%3 MB）")
                                .arg(host).arg(feature).arg(got / (1024 * 1024)));
    });
    connect(reply, &QNetworkReply::finished, this,
            [self, reply, file, written, partPath, url, feature, cb, listId, resumed, resuming, root]() {
        if (!self) { reply->deleteLater(); delete file; delete written; return; } // JavaManager 已销毁，安全退出
        // 收尾前把残留字节落盘（极少数情况下最后一块只在 finished 时才可读到）
        {
            const QByteArray last = reply->readAll();
            if (!last.isEmpty()) {
                const qint64 n = file->write(last);
                if (n > 0) { *written += n; self->m_javaOffset[listId] = *written; }
            }
        }
        file->close();
        delete file;
        delete written;
        reply->deleteLater();
        self->m_javaReplies.remove(listId);   // 取消映射，避免悬空指针

        // 用户主动暂停/取消：保留部分文件与断点，不重试/不报错
        if (self->m_javaAborted.value(listId, false)) {
            self->m_javaAborted.remove(listId);
            if (self->m_javaPaused.contains(listId)) {
                // 暂停态：状态已由 pause() 置为 StPaused 并保持不变；保留 m_javaPaused 供 resume() 判定，
                // 部分文件与断点均已就绪，resume() 会从 m_javaOffset 处继续。
            } else {
                // 取消：清掉部分文件，避免残留
                QFile::remove(partPath);
                self->m_javaOffset.remove(listId);
                self->m_javaTotal.remove(listId);
                self->m_list->setState(listId, DownloadListModel::StCanceled, QStringLiteral("已取消"));
            }
            return;
        }

        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

        // 续传请求却被服务器以 200(整文件)/416(超出范围) 回应：不支持 Range 或偏移无效，
        // 截断后整文件重下一次（本次不再续传，但行为仍正确）
        if (resuming && (status == 200 || status == 416)) {
            QFile::remove(partPath);
            self->m_javaOffset.remove(listId);
            self->m_javaTotal.remove(listId);
            self->fetchInstaller(feature, url, listId, cb);
            return;
        }

        // 手动处理重定向（ManualRedirectPolicy）：拦截 Adoptium 302 到被墙的 github.com，
        // 改写为清华 TUNA 镜像；镜像失败再回退原始 github 直链（对非国内用户不退化）
        const QVariant redir = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redir.isValid()) {
            const QUrl target = reply->url().resolved(redir.toUrl());
            if (target.host() == QStringLiteral("github.com")) {
                QUrl tuna = target;
                tuna.setScheme(QStringLiteral("https"));
                tuna.setHost(QStringLiteral("mirrors.tuna.tsinghua.edu.cn"));
                tuna.setPath(QStringLiteral("/github-release") + target.path());
                self->fetchInstaller(feature, tuna, listId, [self, feature, cb, target, listId](bool ok, QString path) {
                    if (ok) { cb(true, path); return; }
                    // TUNA 镜像失败（如境外不可达）：回退到原始 github 直链
                    self->fetchInstaller(feature, target, listId, cb);
                });
                return;
            }
            self->fetchInstaller(feature, target, listId, cb);
            return;
        }

        if (reply->error() != QNetworkReply::NoError) {
            self->setError(reply->errorString());
            self->setStatus(QStringLiteral("下载 Java %1 安装程序失败：%2").arg(feature).arg(reply->errorString()));
            // 源失败 -> 切换下一个源前清理部分文件与断点
            QFile::remove(partPath);
            self->m_javaOffset.remove(listId);
            self->m_javaTotal.remove(listId);
            cb(false, QString());
            return;
        }

        // 到这里：安装包已完整下载到 partPath。读取文件头魔数校验类型，再改名并安装/解压
        QFile f(partPath);
        if (!f.open(QIODevice::ReadOnly)) {
            self->setError(QStringLiteral("无法读取已下载的临时文件"));
            cb(false, QString());
            return;
        }
        const QByteArray head = f.read(4);
        const qint64 size = f.size();
        f.close();
        qDebug() << "[JavaManager] downloaded url=" << reply->url().toString()
                 << "status=" << status << "size=" << size;
        if (size == 0) {
            self->setError(QStringLiteral("下载内容为空"));
            QFile::remove(partPath);
            self->m_javaOffset.remove(listId);
            self->m_javaTotal.remove(listId);
            cb(false, QString());
            return;
        }
        // 甲骨文对需登录的版本（历史上 JDK 8 等）会返回许可/登录 HTML 页（Content-Type: text/html），
        // 这种"假安装包"必须判为下载失败，以便 download() 换下一个源重试，而不是把 HTML 当 exe 执行。
        const bool isHtml = head.startsWith("<!DO") || head.startsWith("<!do") || head.startsWith("<htm");
        if (isHtml) {
            self->setError(QStringLiteral("源返回的是网页而非安装包（可能需要登录或已被拦截），正在尝试下一个源"));
            self->setStatus(QStringLiteral("Java %1 的下载源返回了网页而非安装包，尝试下一个源").arg(feature));
            QFile::remove(partPath);
            self->m_javaOffset.remove(listId);
            self->m_javaTotal.remove(listId);
            cb(false, QString()); // 空 path -> download() 换下一个源重试
            return;
        }
        // 防护：JDK 安装包/压缩包体积必然很大（通常 > 30MB）。若只下到几 KB，说明拿到的是
        // 许可页/错误页/被拦截的占位文件（例如 Adoptium 的 302 跳转到被墙的 GitHub 只取到小页面），
        // 不能当作安装包解压，必须判失败以便换源重试。
        const qint64 minSize = 20 * 1024 * 1024;
        if (size < minSize) {
            self->setError(QStringLiteral("下载到的文件过小（%1 KB），疑似许可页/错误页/被拦截，非有效安装包")
                                   .arg(size / 1024));
            self->setStatus(QStringLiteral("Java %1 源返回的文件过小（%2 KB），尝试下一个源")
                                    .arg(feature).arg(size / 1024));
            QFile::remove(partPath);
            self->m_javaOffset.remove(listId);
            self->m_javaTotal.remove(listId);
            cb(false, QString());   // 空 path -> download() 换下一个源
            return;
        }
        // 以"文件头魔数"判定安装包类型，比 URL 扩展名可靠：
        // java.com 的 AutoDL 直链路径不含 .exe，且 Adoptium 302 后路径才是真实扩展名，
        // 若仅看 URL 会把 exe 误判成 zip 而用 tar 解压失败。
        const bool isMsi = head.startsWith(QByteArray("\xD0\xCF\x11\xE0")); // OLE 复合文档
        const bool isExe = head.startsWith(QByteArray("MZ"));               // DOS/PE 可执行
        const bool isZip = head.startsWith(QByteArray("PK"));               // zip 压缩包
        // 在状态栏明确告知用户下载到的是"安装程序"还是"压缩包"，便于排查卡死/失败
        const QString pkgType = isMsi ? QStringLiteral("MSI 安装包")
                                      : (isExe ? QStringLiteral("EXE 安装程序")
                                               : (isZip ? QStringLiteral("压缩包(zip)") : QStringLiteral("未知文件")));
        self->setStatus(QStringLiteral("已下载 Java %1（%2，约 %3 MB），开始处理…")
                                .arg(feature).arg(pkgType).arg(size / (1024 * 1024)));
        const QString ext = isMsi ? QStringLiteral("installer.msi")
                                  : (isExe ? QStringLiteral("installer.exe") : QStringLiteral("archive.zip"));
        const QString tmp = root + QStringLiteral("/jdk-%1-%2").arg(feature).arg(ext);
        // 魔数校验：防止把错误页/重定向页/被拦截的占位文件当安装包
        if (isMsi && !head.startsWith(QByteArray("\xD0\xCF\x11\xE0"))) {
            self->setBusy(false);
            self->setError(QStringLiteral("下载到的不是有效的 MSI 安装包（可能被拦截或重定向失败）"));
            cb(false, QString());
            return;
        }
        if (isExe && !head.startsWith(QByteArray("MZ"))) {
            self->setBusy(false);
            self->setError(QStringLiteral("下载到的不是有效的 EXE 安装包（可能被拦截或重定向失败）"));
            cb(false, QString());
            return;
        }
        // 改名（.partial -> 带类型的文件名），供安装/解压使用
        QFile::remove(tmp); // 覆盖同名旧文件
        if (!QFile::rename(partPath, tmp)) {
            self->setBusy(false);
            self->setError(QStringLiteral("写入临时文件失败"));
            cb(false, QString());
            return;
        }
        self->setProgressFor(feature, 0.92);

        if (isMsi || isExe) {
            // MSI / EXE 安装包：静默安装（exe 也走 launchInstaller，内部按扩展名区分）
            self->setStatus(QStringLiteral("正在安装 Java %1…").arg(feature));
            self->launchInstaller(feature, tmp, [self, tmp, feature, cb](bool ok) {
                if (!self) return;
                if (ok) {
                    const QString java = self->javaPathFor(QString::number(feature));
                    self->setBusy(false);
                    self->setProgressFor(feature, 1);
                    self->setStatus(QStringLiteral("Java %1 安装完成").arg(feature));
                    cb(true, java);
                } else {
                    self->setBusy(false);
                    self->setError(QStringLiteral("安装程序已启动但未在限定时间内完成；如已安装请刷新，或手动运行：") + tmp);
                    cb(false, tmp); // 文件已下载，勿重复下载
                }
            });
        } else {
            // 压缩包（官方 green/portable JDK）解压到管理目录，无需安装、无需管理员权限
            self->setStatus(QStringLiteral("正在解压 Java %1…").arg(feature));
            // 如果 setInstallBase 已设了覆盖路径（如 {path}/jvm-21），就直接解压到那里，
            // 不再追加 /{feature} 子层（与 installedJava() 的判断保持一致）。
            const QString dest = self->installBase().isEmpty()
                ? (root + QStringLiteral("/%1").arg(feature))
                : root;
            QDir().mkpath(dest);
            const QString nativeTmp = QDir::toNativeSeparators(tmp);
            const QString nativeDest = QDir::toNativeSeparators(dest);
            // 优先用系统自带的 tar.exe（Win10+ 内置，对大 zip 更可靠），失败再回退 PowerShell ZipFile
            const QString tar = QStandardPaths::findExecutable(QStringLiteral("tar"),
                    { QStringLiteral("C:/Windows/System32") });
            QString program;
            QStringList args;
            if (!tar.isEmpty()) {
                program = tar;
                args << QStringLiteral("-xf") << nativeTmp << QStringLiteral("-C") << nativeDest;
            } else {
                program = QStringLiteral("powershell");
                args << QStringLiteral("-NoProfile") << QStringLiteral("-ExecutionPolicy") << QStringLiteral("Bypass")
                     << QStringLiteral("-Command")
                     << QStringLiteral("Add-Type -AssemblyName System.IO.Compression.FileSystem; "
                                        "[System.IO.Compression.ZipFile]::ExtractToDirectory($args[0],$args[1])")
                     << nativeTmp << nativeDest;
            }
            QProcess *ps = new QProcess(self.data());
            ps->setProcessChannelMode(QProcess::MergedChannels);
            ps->start(program, args);
            // 解压超时保护：tar/PowerShell 卡死时给出明确错误，避免界面永久"一动不动"
            QTimer *guard = new QTimer(self.data());
            guard->setSingleShot(true);
            guard->setInterval(10 * 60 * 1000);
            connect(guard, &QTimer::timeout, self.data(), [self, ps, guard, program, feature, cb]() {
                if (!self) { guard->deleteLater(); ps->kill(); return; }
                guard->deleteLater();
                ps->kill();
                self->setBusy(false);
                self->setError(QStringLiteral("解压 Java %1 超时（%2 可能卡住），请手动解压后刷新，或检查网络")
                                 .arg(feature).arg(program));
                cb(false, QString());
            });
            connect(ps, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                    self.data(), [self, ps, guard, tmp, nativeDest, program, feature, cb](int code, QProcess::ExitStatus) {
                if (!self) { guard->deleteLater(); ps->deleteLater(); return; }
                guard->deleteLater();
                const QByteArray out = ps->readAllStandardOutput();
                ps->deleteLater();
                // 即使 tar 返回非零（如权限/已存在），若 java.exe 真的被解出来了，就视为成功。
                // 很多 Windows tar 对某些 zip 会警告但仍然完成提取——用户的目录里看到了
                // jdk-21.0.11+10/bin/java.exe，但 code != 0，按原逻辑直接报错太激进。
                QString java = self->javaPathFor(QString::number(feature));

                // 某些 JDK zip 顶层是 jdk-21.0.11+10/，解压后是 jvm/21/jdk-21.0.11+10/bin/java.exe。
                // 上移一层：把唯一的子目录里的内容平铺到 jvm/21/，得到 jvm/21/bin/java.exe。
                if (java.isEmpty() || java.contains(QStringLiteral("/jdk-"))) {
                    self->setStatus(QStringLiteral("正在展平 Java %1 目录…").arg(feature));
                    QDir d(nativeDest);
                    const QStringList subdirs = d.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
                    if (subdirs.size() == 1) {
                        const QString inner = nativeDest + QLatin1Char('/') + subdirs.first();
                        QDir id(inner);
                        const QStringList items = id.entryList(QDir::AllEntries | QDir::NoDotAndDotDot);
                        for (const QString &item : items) {
                            const QString src = inner + QLatin1Char('/') + item;
                            const QString dst = nativeDest + QLatin1Char('/') + item;
                            if (QFile::exists(dst)) QFile::remove(dst);
                            if (!QFile::rename(src, dst)) {
                                // rename 失败（跨设备/被占用）用 copy+delete 兜底
                                QFile::remove(dst);
                                if (QFile::copy(src, dst)) QFile::remove(src);
                            }
                        }
                        id.rmdir(QStringLiteral("."));
                    }
                    java = self->javaPathFor(QString::number(feature));
                }

                QFile::remove(tmp);
                if (java.isEmpty()) {
                    self->setBusy(false);
                    self->setError(QStringLiteral("解压失败（%1 返回 %2）：%3")
                                     .arg(program).arg(code).arg(QString::fromLocal8Bit(out).trimmed()));
                    cb(false, tmp);
                    return;
                }
                QSettings s;
                s.setValue(QStringLiteral("java/installed/%1").arg(feature),
                           QDir::fromNativeSeparators(QFileInfo(java).absolutePath()));
                self->setBusy(false);
                self->setProgressFor(feature, 1);
                self->setStatus(QStringLiteral("Java %1 安装完成（%2）").arg(feature).arg(java));
                cb(true, java);
                return;
            });
        }
    });
}

void JavaManager::setBusy(bool b) { if (m_busy != b) { m_busy = b; emit busyChanged(); } }
void JavaManager::setProgressFor(int feature, qreal p)
{
    // 各特性独立记录进度，避免并行下载时互相覆盖导致进度条乱跳
    m_progressByFeature[feature] = p;
    qreal sum = 0; int n = 0;
    for (qreal v : m_progressByFeature) { sum += v; ++n; }
    const qreal avg = n ? sum / n : 0;
    if (m_progress != avg) { m_progress = avg; emit progressChanged(); }
}
void JavaManager::setStatus(const QString &s) { if (m_status != s) { m_status = s; emit statusTextChanged(); } }
void JavaManager::setError(const QString &e) { if (m_error != e) { m_error = e; emit errorTextChanged(); } }

bool JavaManager::busy() const { return m_busy; }
qreal JavaManager::progress() const { return m_progress; }
QString JavaManager::statusText() const { return m_status; }
QString JavaManager::errorText() const { return m_error; }

// ============ 临时 Java：可移植解压到临时目录，用完即删，不留下环境改动 ============

QString JavaManager::findExtractedJava(const QString &dir) const
{
    QDir d(dir);
    if (!d.exists())
        return QString();
    const QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : entries) {
        if (fi.isDir()) {
            const QString r = findExtractedJava(fi.absoluteFilePath());
            if (!r.isEmpty())
                return r;
        } else if (fi.fileName().compare(QStringLiteral("java.exe"), Qt::CaseInsensitive) == 0
                   || fi.fileName().compare(QStringLiteral("java"), Qt::CaseInsensitive) == 0) {
            return fi.absoluteFilePath();
        }
    }
    return QString();
}

QString JavaManager::adoptiumZipLinkFor(int feature, const QJsonDocument &doc) const
{
    const QJsonArray releases = doc.array();
    if (releases.isEmpty())
        return QString();
    const QJsonObject rel = releases.first().toObject();
    const QJsonArray binaries = rel.value(QStringLiteral("binaries")).toArray();
    for (const auto &b : binaries) {
        const QJsonObject bo = b.toObject();
        if (bo.value(QStringLiteral("archive_type")).toString() == QStringLiteral("zip"))
            return bo.value(QStringLiteral("package")).toObject().value(QStringLiteral("link")).toString();
    }
    if (!binaries.isEmpty())
        return binaries.first().toObject().value(QStringLiteral("package")).toObject().value(QStringLiteral("link")).toString();
    return QString();
}

QString JavaManager::oracleZipLinkFor(int feature) const
{
    // 甲骨文公开“latest”永久别名直链（无需登录/otn cookie），指向该特性版本最新的
    // Windows x64 压缩包(zip)。用 latest 别名而非具体版本号，避免硬编码版本过期后 404。
    // 国内下载快（用户实测 5~6 MB/s）。feature=8 等不提供公开 zip 时返回空，走 Adoptium 兜底。
    switch (feature) {
    case 21: return QStringLiteral("https://download.oracle.com/java/21/latest/jdk-21_windows-x64_bin.zip");
    case 17: return QStringLiteral("https://download.oracle.com/java/17/latest/jdk-17_windows-x64_bin.zip");
    case 11: return QStringLiteral("https://download.oracle.com/java/11/latest/jdk-11_windows-x64_bin.zip");
    default: return QString();   // 25/26 等无公开 zip 直链，回退 TUNA/Adoptium 官方镜像
    }
}

void JavaManager::downloadToTemp(const QString &url, const QString &dest, std::function<void(bool, QString)> cb,
                                 const QList<QPair<QByteArray, QByteArray>> &extraHeaders,
                                 std::function<void(qint64, qint64)> onProgress)
{
    // 用系统自带的 curl.exe 下载大文件，彻底绕开 Qt6+MinGW+Schannel 在下载上百 MB 的
    // JDK 压缩包（Oracle/Adoptium）时出现的卡死/崩溃（之前用 QNetworkAccessManager 直接拉会卡死）。
    // 与 extractZip 用 PowerShell 解压是同一思路：把 Qt 不擅长的大文件网络/解压工作交给系统成熟工具。
    // 用户的 Oracle 源实测 5~6 MB/s，curl 能跑满，且出错信息清晰便于排查。
    QStringList args;
    args << QStringLiteral("-L")                       // 跟随重定向（Adoptium 302 到 GitHub 等）
         << QStringLiteral("-sS")                      // 静默但保留错误信息
         << QStringLiteral("--retry") << QStringLiteral("3")
         << QStringLiteral("--retry-delay") << QStringLiteral("2")
         << QStringLiteral("-m") << QStringLiteral("600")   // 总超时 10 分钟（足够 196MB@5MB/s≈40s）
         << QStringLiteral("-o") << QDir::toNativeSeparators(dest);
    for (const QPair<QByteArray, QByteArray> &h : extraHeaders)
        args << QStringLiteral("-H") << (QString::fromUtf8(h.first) + QStringLiteral(": ") + QString::fromUtf8(h.second));
    args << url;

    QPointer<JavaManager> self(this);
    auto *p = new QProcess(this);
    QTimer *progTimer = nullptr;
    if (onProgress) {
        progTimer = new QTimer(this);
        QObject::connect(progTimer, &QTimer::timeout, this, [self, p, dest, onProgress]() {
            if (!self || !p || p->state() != QProcess::Running) return;
            const qint64 sz = QFileInfo(dest).size();
            if (sz > 0) onProgress(sz, 0);
        });
        progTimer->start(500);
    }
    QObject::connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), p,
                     [self, p, dest, cb, onProgress, progTimer](int code, QProcess::ExitStatus) {
                         if (progTimer) { progTimer->stop(); progTimer->deleteLater(); }
                         const QByteArray errOut = p->readAllStandardError();
                         p->deleteLater();
                         if (!self) return;
                         const bool ok = (code == 0) && QFile::exists(dest) && (QFileInfo(dest).size() > 0);
                         if (ok) {
                             const qint64 sz = QFileInfo(dest).size();
                             if (onProgress) onProgress(sz, sz);
                             cb(true, QString());
                         } else {
                             cb(false, QString::fromLocal8Bit(errOut).trimmed());
                         }
                     });
    p->start(QStringLiteral("curl.exe"), args);
    if (!p->waitForStarted(10000)) {
        if (progTimer) { progTimer->stop(); progTimer->deleteLater(); }
        const QString e = p->errorString();
        p->deleteLater();
        cb(false, QStringLiteral("无法启动 curl.exe 下载器：") + e);
    }
}

void JavaManager::extractZip(const QString &zip, const QString &dest, std::function<void(bool)> cb)
{
    QDir().mkpath(dest);
    // 写一段临时 PowerShell 脚本，用 Expand-Archive 纯解压（不安装、不动注册表）
    const QString scriptPath = QDir::cleanPath(dest + QStringLiteral("/../__msm_extract__.ps1"));
    {
        QFile sf(scriptPath);
        if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) { cb(false); return; }
        QTextStream ts(&sf);
        // 路径经单引号转义写入脚本（单引号字符串中 ' 转义为 ''），避免路径含特殊字符导致命令注入
        const QString zipN = QDir::toNativeSeparators(zip).replace(QLatin1Char('\''), QStringLiteral("''"));
        const QString destN = QDir::toNativeSeparators(dest).replace(QLatin1Char('\''), QStringLiteral("''"));
        ts << "Expand-Archive -Force -LiteralPath '" << zipN
           << "' -DestinationPath '" << destN << "'\n";
        sf.close();
    }
    auto *p = new QProcess(this);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), p,
            [p, cb, scriptPath]() {
                QFile::remove(scriptPath);
                const bool ok = (p->exitCode() == 0);
                p->deleteLater();
                cb(ok);
            });
    p->start(QStringLiteral("powershell"),
             QStringList() << QStringLiteral("-NoProfile") << QStringLiteral("-ExecutionPolicy")
                           << QStringLiteral("Bypass") << QStringLiteral("-File") << scriptPath);
    if (!p->waitForStarted(5000)) {
        QFile::remove(scriptPath);
        p->deleteLater();
        cb(false);
    }
}

void JavaManager::ensureTemp(int feature, std::function<void(bool, QString)> cb)
{
    if (!m_dm) { setStatus(QStringLiteral("下载管理器未初始化，无法准备临时 Java")); return; }
    const QString base = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    const QString dir = QDir::cleanPath(base + QStringLiteral("/msm-jdk-temp-") + QString::number(feature));
    QDir().mkpath(dir);

    // 已解压过则直接复用
    {
        const QString existing = findExtractedJava(dir);
        if (!existing.isEmpty()) { m_tempJavaDir = dir; emit hasTempJavaChanged(); cb(true, existing); return; }
    }

    QPointer<JavaManager> self(this);

    const QList<QPair<QByteArray, QByteArray>> oracleHeaders = {
        { QByteArrayLiteral("Referer"), QByteArrayLiteral("https://www.oracle.com/") },
        { QByteArrayLiteral("Cookie"),  QByteArrayLiteral("oraclelicense=accept-license") }
    };

    // 通用步骤：经统一 DownloadManager 下载压缩包（进入下载面板，带 90s 超时与进度/暂停/取消）
    // -> 解压 -> 校验 java 可用（能跑 -version 且主版本匹配）。失败则 onFail 切换下一个下载源。
    auto tryDownloadAndVerify = [self, dir, feature, cb](const QString &link,
                                                         const QList<QPair<QByteArray, QByteArray>> &headers,
                                                         const std::function<void()> &onFail) {
        Q_UNUSED(headers); // DownloadManager 不支持自定义请求头；甲骨文 latest 公开直链无需鉴权
        if (link.isEmpty()) { onFail(); return; }
        if (!self->m_dm) { onFail(); return; }
        const QString zipName = QStringLiteral("adoptium.zip");
        self->setStatus(QStringLiteral("正在准备临时 Java %1（下载压缩包）…").arg(feature));
        const QString id = self->m_dm->download(link, dir, zipName,
                                                QStringLiteral("Java %1 压缩包").arg(feature));
        // 登记续跑：下载成功则解压+校验，失败则走 onFail 尝试下一个源
        self->m_pending.insert(id, [self, dir, feature, cb, onFail](bool ok, QString) {
            if (!ok) { onFail(); return; }
            self->setStatus(QStringLiteral("正在解压临时 Java %1…").arg(feature));
            const QString zipPath = QDir::cleanPath(dir + QStringLiteral("/adoptium.zip"));
            self->extractZip(zipPath, dir, [self, dir, feature, cb, onFail](bool ok2) {
                if (!ok2) { onFail(); return; }
                const QString java = self->findExtractedJava(dir);
                if (java.isEmpty()) { onFail(); return; }
                self->probeJava(java, feature, [self, dir, feature, cb, onFail](const QString &probed) {
                    if (probed.isEmpty()) { onFail(); return; }
                    self->m_tempJavaDir = dir;
                    emit self->hasTempJavaChanged();
                    cb(true, probed);
                });
            });
        });
    };

    // 下载源顺序（国内按速度/可用性，优先 TUNA 镜像）：
    //   1) 清华 TUNA 镜像（Adoptium 二进制镜像，国内直连快）
    //   2) 甲骨文官方 latest 压缩包直链（公开、无需鉴权）
    //   3) Adoptium 官方 package.link（直连/302，龟速但可达）
    // resolveInstaller 已对 TUNA/腾讯镜像做候选排序，返回官方直链 tunaUrl。
    resolveInstaller(feature, [self, dir, feature, cb, oracleHeaders, tryDownloadAndVerify](bool ok, QString tunaUrl, QString fallback) {
        Q_UNUSED(fallback);
        if (!ok || tunaUrl.isEmpty()) {
            const QString oracleLink = self->oracleZipLinkFor(feature);
            if (!oracleLink.isEmpty())
                tryDownloadAndVerify(oracleLink, oracleHeaders, [cb]() {
                    cb(false, QStringLiteral("所有下载源均失败（甲骨文/Adoptium）"));
                });
            else
                cb(false, QStringLiteral("无法获取 Java 下载链接"));
            return;
        }
        const QString zipFname = QUrl(tunaUrl).fileName();
        QUrl tuna; tuna.setScheme(QStringLiteral("https")); tuna.setHost(QStringLiteral("mirrors.tuna.tsinghua.edu.cn"));
        tuna.setPath(QStringLiteral("/Adoptium/%1/jdk/x64/windows/%2").arg(feature).arg(zipFname));
        const QString oracleLink = self->oracleZipLinkFor(feature);
        // Adoptium 官方“latest binary”直链（302 到实际二进制，龟速但可达），作为最后的兜底源
        const QString official = QStringLiteral(
            "https://api.adoptium.net/v3/binary/latest/%1/ga/windows/x64/jdk/hotspot/normal/eclipse?project=jdk")
                .arg(feature);
        // 1) TUNA 镜像 2) 甲骨文直链 3) Adoptium 官方直链
        tryDownloadAndVerify(tuna.toString(), {}, [self, feature, cb, oracleLink, oracleHeaders, official, tryDownloadAndVerify]() {
            if (!oracleLink.isEmpty())
                tryDownloadAndVerify(oracleLink, oracleHeaders, [self, feature, cb, official, tryDownloadAndVerify]() {
                    tryDownloadAndVerify(official, {}, [cb]() {
                        cb(false, QStringLiteral("所有下载源均失败（TUNA/甲骨文/Adoptium）"));
                    });
                });
            else
                tryDownloadAndVerify(official, {}, [cb]() {
                    cb(false, QStringLiteral("所有下载源均失败（TUNA/Adoptium）"));
                });
        });
    });
}

void JavaManager::cleanupTemp()
{
    if (m_tempJavaDir.isEmpty())
        return;
    QDir(m_tempJavaDir).removeRecursively();
    m_tempJavaDir.clear();
    emit hasTempJavaChanged();
}

bool JavaManager::hasTempJava() const
{
    return !m_tempJavaDir.isEmpty() && QDir(m_tempJavaDir).exists();
}

JavaManager::~JavaManager()
{
    // 关闭应用时自动清理残留的临时 Java 目录，不留下环境改动
    cleanupTemp();
}
