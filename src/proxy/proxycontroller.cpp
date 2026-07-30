/*
 * proxycontroller.cpp —— Velocity 反向代理聚合
 * -------------------------------------------------
 * 下载 velocity.jar → 依据服务器列表生成 velocity.toml/forwarding.secret
 * → 修补后端配置 → 管理代理进程。玩家统一连接代理端口，
 * 游戏内用 /server <名称> 在多台同时运行的服务器之间切换。
 */
#include "proxycontroller.h"
#include "servermanager.h"
#include "servercontroller.h"
#include "../java/javamanager.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QVersionNumber>

ProxyController::ProxyController(ServerManager *sm, ServerController *sc,
                                 JavaManager *java, const QString &instanceId,
                                 QObject *parent)
    : QObject(parent), m_sm(sm), m_sc(sc), m_java(java), m_instanceId(instanceId)
{
    m_nam.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    QSettings s;
    m_proxyPort = s.value(key(QStringLiteral("port")), 25577).toInt();
    m_motd = s.value(key(QStringLiteral("motd")),
                     QStringLiteral("&3MSM 聚合服务器")).toString();
    m_offlineMode = s.value(key(QStringLiteral("offlineMode")), true).toBool();
    m_stopBackendsWithProxy = s.value(key(QStringLiteral("stopBackendsWithProxy")), true).toBool();
    m_autoRestart = s.value(key(QStringLiteral("autoRestart")), true).toBool();
    m_maxRetries = s.value(key(QStringLiteral("maxRetries")), 5).toInt();
    m_backoffSec = s.value(key(QStringLiteral("backoffSec")), 5).toInt();
    m_name = s.value(key(QStringLiteral("name")),
                     m_instanceId.isEmpty() ? QStringLiteral("默认代理") : m_instanceId).toString();
    m_serverFilter = s.value(key(QStringLiteral("servers"))).toStringList();
    m_status = installed() ? QStringLiteral("已安装，未运行") : QStringLiteral("未安装");
}

// 实例化设置键：默认实例保持旧版 proxy/xxx，其余实例 proxy/<id>/xxx
QString ProxyController::key(const QString &k) const
{
    return m_instanceId.isEmpty()
               ? QStringLiteral("proxy/") + k
               : QStringLiteral("proxy/") + m_instanceId + QLatin1Char('/') + k;
}

// 主目录：velocity.jar 在此共享（所有实例复用同一 jar）
QString ProxyController::baseDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
           + QStringLiteral("/MSM/Velocity");
}

QString ProxyController::jarPath() const
{
    return baseDir() + QStringLiteral("/velocity.jar");
}

QString ProxyController::proxyDir() const
{
    return m_instanceId.isEmpty()
               ? baseDir()
               : baseDir() + QStringLiteral("/instances/") + m_instanceId;
}

bool ProxyController::installed() const
{
    return QFile::exists(jarPath());
}

void ProxyController::setProxyPort(int p)
{
    if (p <= 0 || p >= 65536 || p == m_proxyPort)
        return;
    m_proxyPort = p;
    QSettings().setValue(key(QStringLiteral("port")), p);
    emit proxyPortChanged();
}

void ProxyController::setMotd(const QString &m)
{
    if (m == m_motd)
        return;
    m_motd = m;
    QSettings().setValue(key(QStringLiteral("motd")), m);
    emit motdChanged();
}

void ProxyController::setOfflineMode(bool v)
{
    if (v == m_offlineMode)
        return;
    m_offlineMode = v;
    QSettings().setValue(key(QStringLiteral("offlineMode")), v);
    emit offlineModeChanged();
}

void ProxyController::setStopBackendsWithProxy(bool v)
{
    if (v == m_stopBackendsWithProxy)
        return;
    m_stopBackendsWithProxy = v;
    QSettings().setValue(key(QStringLiteral("stopBackendsWithProxy")), v);
    emit stopBackendsWithProxyChanged();
}

void ProxyController::setAutoRestart(bool v)
{
    if (v == m_autoRestart)
        return;
    m_autoRestart = v;
    QSettings().setValue(key(QStringLiteral("autoRestart")), v);
    emit autoRestartChanged();
}

void ProxyController::setName(const QString &n)
{
    const QString t = n.trimmed();
    if (t.isEmpty() || t == m_name)
        return;
    m_name = t;
    QSettings().setValue(key(QStringLiteral("name")), t);
    emit nameChanged();
}

void ProxyController::setServerFilter(const QStringList &f)
{
    if (f == m_serverFilter)
        return;
    m_serverFilter = f;
    QSettings().setValue(key(QStringLiteral("servers")), f);
    emit serverFilterChanged();
}

// QML 便捷开关：把某台后端加入/移出本实例聚合范围。
// 语义：filter 为空 = 聚合全部；勾掉一台时把"当前全部"落成显式名单再移除；
// 勾满全部后回落为空（跟随新增服务器）。
void ProxyController::setServerEnabled(const QString &serverName, bool on)
{
    if (!m_sm)
        return;
    QStringList all;
    for (const QVariant &v : m_sm->serverSummary())
        all << v.toMap().value(QStringLiteral("name")).toString();
    QStringList cur = m_serverFilter.isEmpty() ? all : m_serverFilter;
    if (on) {
        if (!cur.contains(serverName))
            cur << serverName;
    } else {
        cur.removeAll(serverName);
    }
    // 全选 → 存空（跟随全部）；否则存显式名单
    bool coversAll = true;
    for (const QString &n : all)
        if (!cur.contains(n)) { coversAll = false; break; }
    setServerFilter(coversAll ? QStringList() : cur);
}

void ProxyController::setPlayerCount(int n)
{
    n = qMax(0, n);
    if (n == m_playerCount)
        return;
    m_playerCount = n;
    emit playerCountChanged();
}

// 经 serverFilter 过滤后的后端列表（filter 为空 = 全部）
QVariantList ProxyController::filteredServers() const
{
    QVariantList out;
    if (!m_sm)
        return out;
    for (const QVariant &v : m_sm->serverSummary()) {
        const QString name = v.toMap().value(QStringLiteral("name")).toString();
        if (m_serverFilter.isEmpty() || m_serverFilter.contains(name))
            out << v;
    }
    return out;
}

void ProxyController::setStatus(const QString &s)
{
    if (m_status == s)
        return;
    m_status = s;
    emit statusChanged();
}

void ProxyController::setBusy(bool b)
{
    if (m_busy == b)
        return;
    m_busy = b;
    emit busyChanged();
}

void ProxyController::appendConsole(const QString &line)
{
    m_console.append(line + QLatin1Char('\n'));
    if (m_console.size() > 200000)
        m_console = m_console.right(200000);
    emit consoleAppended(line);
}

// ---------------- 下载 ----------------

void ProxyController::fetchJson(const QString &url,
                                std::function<void(const QJsonDocument &)> ok,
                                std::function<void(const QString &)> fail)
{
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MSM/1.0"));
    QNetworkReply *rep = m_nam.get(req);
    connect(rep, &QNetworkReply::finished, this, [rep, ok, fail]() {
        rep->deleteLater();
        if (rep->error() != QNetworkReply::NoError) {
            fail(rep->errorString());
            return;
        }
        ok(QJsonDocument::fromJson(rep->readAll()));
    });
}

void ProxyController::install()
{
    if (m_busy)
        return;
    setBusy(true);
    m_installProgress = 0;
    emit installProgressChanged();
    setStatus(QStringLiteral("正在获取 Velocity 版本信息…"));
    // Fill v3：取版本列表 → 最新版本 → 最新构建 → downloads["server:default"].url
    fetchJson(QStringLiteral("https://fill.papermc.io/v3/projects/velocity/versions"),
        [this](const QJsonDocument &d) {
            QStringList vers;
            const QJsonArray vs = d.object().value(QStringLiteral("versions")).toArray();
            for (const auto &e : vs) {
                const QString id = e.toObject().value(QStringLiteral("version"))
                                        .toObject().value(QStringLiteral("id")).toString();
                if (!id.isEmpty()) vers << id;
            }
            std::sort(vers.begin(), vers.end(), [](const QString &a, const QString &b) {
                return QVersionNumber::fromString(a) > QVersionNumber::fromString(b);
            });
            if (vers.isEmpty()) {
                setStatus(QStringLiteral("获取 Velocity 版本列表失败"));
                setBusy(false);
                return;
            }
            const QString ver = vers.first();
            setStatus(QStringLiteral("正在获取 Velocity %1 下载地址…").arg(ver));
            fetchJson(QStringLiteral("https://fill.papermc.io/v3/projects/velocity/versions/%1/builds").arg(ver),
                [this, ver](const QJsonDocument &bd) {
                    const QJsonArray builds = bd.array();
                    if (builds.isEmpty()) {
                        setStatus(QStringLiteral("Velocity %1 无可用构建").arg(ver));
                        setBusy(false);
                        return;
                    }
                    const QString url = builds.first().toObject()
                                            .value(QStringLiteral("downloads")).toObject()
                                            .value(QStringLiteral("server:default")).toObject()
                                            .value(QStringLiteral("url")).toString();
                    if (url.isEmpty()) {
                        setStatus(QStringLiteral("解析下载地址失败"));
                        setBusy(false);
                        return;
                    }
                    setStatus(QStringLiteral("正在下载 Velocity %1…").arg(ver));
                    downloadJar(url);
                },
                [this](const QString &e) {
                    setStatus(QStringLiteral("获取构建列表失败：") + e);
                    setBusy(false);
                });
        },
        [this](const QString &e) {
            setStatus(QStringLiteral("获取版本列表失败（网络异常）：") + e);
            setBusy(false);
        });
}

void ProxyController::downloadJar(const QString &url)
{
    QDir().mkpath(baseDir());
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("MSM/1.0"));
    QNetworkReply *rep = m_nam.get(req);
    connect(rep, &QNetworkReply::downloadProgress, this, [this](qint64 recv, qint64 total) {
        if (total > 0) {
            m_installProgress = qreal(recv) / qreal(total) * 100.0;
            emit installProgressChanged();
        }
    });
    connect(rep, &QNetworkReply::finished, this, [this, rep]() {
        rep->deleteLater();
        if (rep->error() != QNetworkReply::NoError) {
            setStatus(QStringLiteral("下载失败：") + rep->errorString());
            setBusy(false);
            return;
        }
        // 先写临时文件再替换，避免中断留下半截 jar（jar 为全部实例共享）
        const QString target = jarPath();
        QFile f(target + QStringLiteral(".part"));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            setStatus(QStringLiteral("无法写入 velocity.jar"));
            setBusy(false);
            return;
        }
        f.write(rep->readAll());
        f.close();
        QFile::remove(target);
        QFile::rename(f.fileName(), target);
        m_installProgress = 100;
        emit installProgressChanged();
        setStatus(QStringLiteral("Velocity 下载完成"));
        setBusy(false);
        emit installedChanged();
    });
}

// ---------------- 配置生成 ----------------

QString ProxyController::forwardingMode() const
{
    const QVariantList servers = filteredServers();
    if (servers.isEmpty())
        return QStringLiteral("none");
    for (const QVariant &v : servers) {
        // type 存储为显示名（Paper/Vanilla/Forge…），大小写不敏感比较
        if (v.toMap().value(QStringLiteral("type")).toString()
                .compare(QStringLiteral("paper"), Qt::CaseInsensitive) != 0)
            return QStringLiteral("none");
    }
    return QStringLiteral("modern");
}

QString ProxyController::ensureSecret()
{
    const QString file = proxyDir() + QStringLiteral("/forwarding.secret");
    QFile f(file);
    if (f.open(QIODevice::ReadOnly)) {
        const QString s = QString::fromUtf8(f.readAll()).trimmed();
        if (!s.isEmpty())
            return s;
    }
    static const QString chars = QStringLiteral(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    QString secret;
    for (int i = 0; i < 16; ++i)
        secret += chars.at(int(QRandomGenerator::global()->bounded(chars.size())));
    QDir().mkpath(proxyDir());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(secret.toUtf8());
    return secret;
}

QVariantList ProxyController::backendSummary() const
{
    QVariantList out;
    if (!m_sm || !m_sc)
        return out;
    const QString mode = forwardingMode();
    for (const QVariant &v : m_sm->serverSummary()) {
        QVariantMap m = v.toMap();
        const QString name = m.value(QStringLiteral("name")).toString();
        m[QStringLiteral("port")] = m_sc->serverPort(m.value(QStringLiteral("path")).toString());
        m[QStringLiteral("host")] = QStringLiteral("127.0.0.1");
        m[QStringLiteral("forwarding")] = mode;
        // P2 多代理：标记该后端是否纳入本实例的聚合范围（供 UI 勾选）
        m[QStringLiteral("enabled")] = m_serverFilter.isEmpty() || m_serverFilter.contains(name);
        out << m;
    }
    return out;
}

bool ProxyController::syncConfig()
{
    if (!m_sm || !m_sc) {
        setStatus(QStringLiteral("服务器管理器未初始化"));
        return false;
    }
    const QVariantList servers = filteredServers();
    if (servers.isEmpty()) {
        setStatus(QStringLiteral("没有可聚合的服务器，请先创建服务器或勾选后端"));
        return false;
    }
    QDir().mkpath(proxyDir());
    const QString secret = ensureSecret();
    const QString mode = forwardingMode();

    // ---- velocity.toml ----
    QString toml;
    QTextStream ts(&toml);
    ts << "# 由 MSM 自动生成，请勿手动编辑（每次启动代理前会重新生成）\n";
    ts << "config-version = \"2.7\"\n";
    ts << "bind = \"0.0.0.0:" << m_proxyPort << "\"\n";
    ts << "motd = \"" << QString(m_motd).replace(QLatin1Char('"'), QStringLiteral("\\\"")) << "\"\n";
    ts << "show-max-players = 500\n";
    ts << "online-mode = " << (m_offlineMode ? "false" : "true") << "\n";
    ts << "force-key-authentication = " << (m_offlineMode ? "false" : "true") << "\n";
    ts << "prevent-client-proxy-connections = false\n";
    ts << "player-info-forwarding-mode = \"" << mode << "\"\n";
    ts << "forwarding-secret-file = \"forwarding.secret\"\n";
    ts << "announce-forge = false\n";
    ts << "kick-existing-players = false\n";
    ts << "ping-passthrough = \"DISABLED\"\n";
    ts << "enable-player-address-logging = true\n\n";
    ts << "[servers]\n";
    QStringList names;
    for (const QVariant &v : servers) {
        const QVariantMap m = v.toMap();
        const QString name = m.value(QStringLiteral("name")).toString();
        const int port = m_sc->serverPort(m.value(QStringLiteral("path")).toString());
        ts << "\"" << QString(name).replace(QLatin1Char('"'), QString()) << "\" = \"127.0.0.1:" << port << "\"\n";
        names << name;
    }
    ts << "try = [";
    for (int i = 0; i < names.size(); ++i)
        ts << (i ? ", " : "") << "\"" << names.at(i) << "\"";
    ts << "]\n\n";
    ts << "[forced-hosts]\n\n";
    ts << "[advanced]\n\n";
    ts << "[query]\nenabled = false\n";
    ts.flush();

    QFile f(proxyDir() + QStringLiteral("/velocity.toml"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        setStatus(QStringLiteral("无法写入 velocity.toml"));
        return false;
    }
    f.write(toml.toUtf8());
    f.close();

    patchBackends(secret, mode);
    setStatus(QStringLiteral("配置已同步（%1 台后端，转发模式 %2）").arg(servers.size()).arg(mode));
    appendConsole(QStringLiteral("[MSM] velocity.toml 已生成：%1 台后端，入口端口 %2，转发模式 %3")
                      .arg(servers.size()).arg(m_proxyPort).arg(mode));
    if (mode == QStringLiteral("none"))
        appendConsole(QStringLiteral("[MSM] 提示：存在非 Paper 后端，已使用 none 转发模式（后端将看到代理 IP，且必须保持 online-mode=false）"));
    return true;
}

void ProxyController::patchBackends(const QString &secret, const QString &mode)
{
    for (const QVariant &v : filteredServers()) {
        const QVariantMap m = v.toMap();
        const QString path = m.value(QStringLiteral("path")).toString();
        const QString name = m.value(QStringLiteral("name")).toString();
        // 代理接管正版验证，后端必须关闭 online-mode
        QVariantMap props;
        props.insert(QStringLiteral("online-mode"), QStringLiteral("false"));
        m_sc->writeProperties(path, props);
        // Paper 后端启用 modern forwarding（拿到真实 IP/UUID）
        if (mode == QStringLiteral("modern")
            && m.value(QStringLiteral("type")).toString()
                   .compare(QStringLiteral("paper"), Qt::CaseInsensitive) == 0) {
            patchPaperGlobal(path, secret);
        }
        appendConsole(QStringLiteral("[MSM] 已修补后端 %1（online-mode=false%2）")
                          .arg(name, mode == QStringLiteral("modern")
                                         ? QStringLiteral("，velocity modern forwarding")
                                         : QString()));
    }
}

void ProxyController::patchPaperGlobal(const QString &serverPath, const QString &secret)
{
    const QString file = serverPath + QStringLiteral("/config/paper-global.yml");
    QStringList lines;
    QFile f(file);
    const bool exists = f.exists() && f.open(QIODevice::ReadOnly | QIODevice::Text);
    if (exists) {
        while (!f.atEnd())
            lines << QString::fromUtf8(f.readLine()).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).chopped(0);
        f.close();
        // 去掉行尾换行统一处理
        for (QString &l : lines)
            while (l.endsWith(QLatin1Char('\n')) || l.endsWith(QLatin1Char('\r')))
                l.chop(1);
    }

    bool patched = false;
    if (exists) {
        // 在 proxies: → velocity: 块内就地替换 enabled / online-mode / secret
        bool inVelocity = false;
        int velocityIndent = -1;
        for (int i = 0; i < lines.size(); ++i) {
            const QString &line = lines.at(i);
            const QString trimmed = line.trimmed();
            const int indent = line.size() - QString(line).remove(QRegularExpression(QStringLiteral("^\\s+"))).size();
            if (trimmed == QStringLiteral("velocity:")) {
                inVelocity = true;
                velocityIndent = indent;
                continue;
            }
            if (inVelocity) {
                if (!trimmed.isEmpty() && indent <= velocityIndent) {
                    inVelocity = false;    // 离开 velocity 块
                    continue;
                }
                const QString pad = QString(indent, QLatin1Char(' '));
                if (trimmed.startsWith(QStringLiteral("enabled:"))) {
                    lines[i] = pad + QStringLiteral("enabled: true");
                    patched = true;
                } else if (trimmed.startsWith(QStringLiteral("online-mode:"))) {
                    lines[i] = pad + QStringLiteral("online-mode: true");
                } else if (trimmed.startsWith(QStringLiteral("secret:"))) {
                    lines[i] = pad + QStringLiteral("secret: '") + secret + QStringLiteral("'");
                }
            }
        }
    }

    if (!patched) {
        // 文件不存在或没有 velocity 块：写入/追加完整块（Paper 启动时会补全其余默认值）
        if (!exists)
            lines.clear();
        // 若已有 proxies: 顶层键则直接在其后插入 velocity 子块，否则追加整个 proxies 块
        int proxiesIdx = -1;
        for (int i = 0; i < lines.size(); ++i)
            if (lines.at(i).trimmed() == QStringLiteral("proxies:") && !lines.at(i).startsWith(QLatin1Char(' ')))
                { proxiesIdx = i; break; }
        const QStringList block = {
            QStringLiteral("  velocity:"),
            QStringLiteral("    enabled: true"),
            QStringLiteral("    online-mode: true"),
            QStringLiteral("    secret: '") + secret + QStringLiteral("'"),
        };
        if (proxiesIdx >= 0) {
            for (int j = 0; j < block.size(); ++j)
                lines.insert(proxiesIdx + 1 + j, block.at(j));
        } else {
            lines << QStringLiteral("proxies:") << block;
        }
    }

    QDir().mkpath(QFileInfo(file).path());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream out(&f);
        out.setEncoding(QStringConverter::Utf8);
        for (const QString &l : lines)
            out << l << QLatin1Char('\n');
        f.close();
    }
}

// ---------------- 进程管理 ----------------

QString ProxyController::detectJava() const
{
    // 1) 代理自身目录的 JDK（用户手动放置或之前为代理下载的 jvm-*/bin/java.exe）
    QDir baseDir(proxyDir());
    const QFileInfoList jvms = baseDir.entryInfoList(
        QStringList() << QStringLiteral("jvm*"), QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : jvms) {
        QDirIterator it(fi.absoluteFilePath(),
                        QStringList() << QStringLiteral("bin/java.exe") << QStringLiteral("bin/java"),
                        QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext())
            return it.next();
    }
    // 2) 已为代理托管、可复用的独立 JDK（feature 21，落地在 managedDir，与后端目录隔离）
    if (m_java) {
        const QString j = m_java->javaPathFor(21);
        if (!j.isEmpty())
            return j;
    }
    return QString();   // 没有现成独立 JDK，交由 start() 触发下载
}

// 取某后端服务器专属的 JDK 路径（.msm/java.txt 优先，其次目录内 jvm*）
QString ProxyController::serverJavaPath(const QString &path) const
{
    QFile f(path + QStringLiteral("/.msm/java.txt"));
    if (f.open(QIODevice::ReadOnly)) {
        const QString j = QString::fromUtf8(f.readAll()).trimmed();
        if (!j.isEmpty() && QFile::exists(j))
            return j;
    }
    QDir baseDir(path);
    const QFileInfoList jvms = baseDir.entryInfoList(
        QStringList() << QStringLiteral("jvm*"), QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : jvms) {
        QDirIterator it(fi.absoluteFilePath(),
                        QStringList() << QStringLiteral("java.exe") << QStringLiteral("java"),
                        QDir::Files, QDirIterator::Subdirectories);
        if (it.hasNext())
            return it.next();
    }
    return QString();
}

// 兜底探测：仅当独立 JDK 不可用/下载失败时，才回退到各后端服务器记录的 JDK 与系统 PATH。
QString ProxyController::fallbackJava() const
{
    if (m_sm) {
        for (const QVariant &v : m_sm->serverSummary()) {
            const QString j = serverJavaPath(v.toMap().value(QStringLiteral("path")).toString());
            if (!j.isEmpty())
                return j;
        }
    }
    return QStringLiteral("java");
}

void ProxyController::start(const QString &javaPath)
{
    if (running()) {
        setStatus(QStringLiteral("代理已在运行"));
        return;
    }
    if (!installed()) {
        setStatus(QStringLiteral("尚未安装 Velocity，请先点击“安装 / 更新”"));
        return;
    }
    if (!syncConfig())
        return;

    // P3：入口端口冲突预检（复用后端 ServerController 的端口探测）
    if (m_sc && !m_sc->isPortFree(quint16(m_proxyPort))) {
        appendConsole(QStringLiteral("[MSM] 错误：代理入口端口 %1 已被占用，无法启动（请改端口或释放占用）").arg(m_proxyPort));
        setStatus(QStringLiteral("端口 %1 被占用").arg(m_proxyPort));
        emit portConflict(m_proxyPort);
        return;
    }

    // 同步配置（改写后端 online-mode）后，自动拉起所有未运行的后端；否则 Velocity 无后端可连。
    m_autoStarted.clear();
    if (m_sm && m_sc) {
        for (const QVariant &v : filteredServers()) {
            const QVariantMap m = v.toMap();
            const QString name = m.value(QStringLiteral("name")).toString();
            const QString path = m.value(QStringLiteral("path")).toString();
            if (name.isEmpty() || path.isEmpty() || m_sc->isRunning(name))
                continue;
            const QString java = serverJavaPath(path);
            m_sc->start(name, path, java.isEmpty() ? QStringLiteral("java") : java, 1024, 2048);
            m_autoStarted << name;
            appendConsole(QStringLiteral("[MSM] 已自动拉起后端 %1").arg(name));
        }
    }

    const QString java = javaPath.isEmpty() ? detectJava() : javaPath;
    if (!java.isEmpty()) {
        launchProcess(java);
        return;
    }
    // 没有现成 Java：为代理专属下载一个独立 JDK（feature 21），与后端目录隔离，
    // 避免某台后端缺失/被删时代理跟着起不来。
    if (m_java) {
        setStatus(QStringLiteral("正在为代理准备独立 Java 运行环境（Java 21）…"));
        setBusy(true);
        m_java->ensureFeature(21, [this](bool ok, QString path) {
            setBusy(false);
            if (ok && !path.isEmpty()) {
                launchProcess(path);
                return;
            }
            // 下载失败：兜底各后端 JDK，再不行系统 java
            const QString fb = fallbackJava();
            if (!fb.isEmpty() && fb != QStringLiteral("java")) {
                launchProcess(fb);
                return;
            }
            appendConsole(QStringLiteral("[MSM] 为代理准备独立 Java 失败：") + m_java->errorText());
            setStatus(QStringLiteral("启动失败：无法准备代理的 Java（") + m_java->errorText()
                      + QStringLiteral("）"));
        });
        return;
    }
    // 未注入 JavaManager：兜底后端/系统
    const QString fb = fallbackJava();
    launchProcess(fb.isEmpty() ? QStringLiteral("java") : fb);
}

void ProxyController::launchProcess(const QString &java)
{
    m_proc = new QProcess(this);
    m_proc->setWorkingDirectory(proxyDir());
    m_proc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this]() {
        const QString data = QString::fromLocal8Bit(m_proc->readAllStandardOutput());
        const QStringList ls = data.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (QString l : ls) {
            l = l.trimmed();
            if (l.isEmpty())
                continue;
            appendConsole(l);
            if (l.contains(QStringLiteral("Done (")))
                setStatus(QStringLiteral("运行中 · 入口端口 %1").arg(m_proxyPort));
            // 在线人数估算：解析 Velocity 的玩家连接/断开日志
            if (l.contains(QStringLiteral("[connected player]"))) {
                if (l.contains(QStringLiteral("has connected")))
                    setPlayerCount(m_playerCount + 1);
                else if (l.contains(QStringLiteral("has disconnected")))
                    setPlayerCount(m_playerCount - 1);
            }
        }
    });
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int code, QProcess::ExitStatus) {
                m_proc->deleteLater();
                m_proc = nullptr;
                setPlayerCount(0);
                if (m_expectedExit) {
                    // 用户主动停止：不触发崩溃通知/自动重拉起
                    m_expectedExit = false;
                    m_retryCount = 0;
                    appendConsole(QStringLiteral("[MSM] 代理已停止（退出码 %1）").arg(code));
                    setStatus(QStringLiteral("已安装，未运行"));
                    emit runningChanged();
                    return;
                }
                // 异常退出（崩溃）：通知并尝试自动重拉起
                emit crashed();
                appendConsole(QStringLiteral("[MSM] 代理异常退出（退出码 %1）").arg(code));
                setStatus(QStringLiteral("已安装，未运行"));
                emit runningChanged();
                if (m_autoRestart && m_retryCount < m_maxRetries) {
                    const int attempt = ++m_retryCount;
                    const int delay = m_backoffSec * (1 << (attempt - 1));
                    appendConsole(QStringLiteral("[MSM] %1 秒后自动重启代理（第 %2/%3 次）")
                                  .arg(delay).arg(attempt).arg(m_maxRetries));
                    QTimer::singleShot(delay * 1000, this, [this]() {
                        if (m_expectedExit)
                            return;   // 期间已被用户手动停止
                        start();
                    });
                }
            });

    setPlayerCount(0);
    QStringList args;
    args << QStringLiteral("-Xms512M") << QStringLiteral("-Xmx512M")
         << QStringLiteral("-jar") << QDir::toNativeSeparators(jarPath());
    appendConsole(QStringLiteral("[MSM] 正在启动 Velocity（%1）…").arg(java));
    m_proc->start(java, args);
    if (!m_proc->waitForStarted(5000)) {
        appendConsole(QStringLiteral("[MSM] 启动失败：无法执行 ") + java
                      + QStringLiteral("（Velocity 需要 Java 17+）"));
        setStatus(QStringLiteral("启动失败：找不到可用的 Java"));
        m_proc->deleteLater();
        m_proc = nullptr;
        return;
    }
    setStatus(QStringLiteral("正在启动…"));
    emit runningChanged();
}

void ProxyController::stop()
{
    if (!running())
        return;
    m_expectedExit = true;   // 标记本次退出为用户主动停止，避免触发崩溃重拉起
    m_retryCount = 0;
    // 停止代理时一并停止由本代理拉起的后端（可拆卸开关控制）
    if (m_stopBackendsWithProxy && m_sc) {
        const QStringList list = m_autoStarted;
        m_autoStarted.clear();
        for (const QString &name : list) {
            if (m_sc->isRunning(name)) {
                m_sc->stop(name);
                appendConsole(QStringLiteral("[MSM] 已停止后端 %1").arg(name));
            }
        }
    }
    appendConsole(QStringLiteral("[MSM] 正在停止代理…"));
    m_proc->write("shutdown\n");
    // 8 秒未退出则强杀
    QProcess *proc = m_proc;
    QTimer::singleShot(8000, this, [this, proc]() {
        if (m_proc == proc && running())
            m_proc->kill();
    });
}
