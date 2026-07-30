/*
 * servercontroller.cpp —— 单台服务端进程管理
 * -------------------------------------------------
 * 启动/停止/强停/发送指令，捕获控制台输出、追踪在线玩家、读写
 * server.properties、列举 mods。每个 name 对应一个 QProcess，
 * 控制台历史在进程结束后仍缓存供回看。
 */
#include "servercontroller.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QRegularExpression>
#include <QTcpServer>
#include <QHostAddress>
#include <QTimer>
#include <QSettings>

ServerController::ServerController(QObject *parent) : QObject(parent)
{
    QSettings s(QStringLiteral("MSM"), QStringLiteral("MSM"));
    m_autoRestart = s.value(QStringLiteral("server/autoRestart"), true).toBool();
    m_maxRetries = s.value(QStringLiteral("server/maxRetries"), 5).toInt();
    m_backoffSec = s.value(QStringLiteral("server/backoffSec"), 5).toInt();
}

void ServerController::setAutoRestart(bool v)
{
    if (v == m_autoRestart)
        return;
    m_autoRestart = v;
    QSettings(QStringLiteral("MSM"), QStringLiteral("MSM")).setValue(QStringLiteral("server/autoRestart"), v);
    emit autoRestartChanged();
}

void ServerController::setMaxRetries(int v)
{
    if (v == m_maxRetries)
        return;
    m_maxRetries = v;
    QSettings(QStringLiteral("MSM"), QStringLiteral("MSM")).setValue(QStringLiteral("server/maxRetries"), v);
    emit maxRetriesChanged();
}

void ServerController::setBackoffSec(int v)
{
    if (v == m_backoffSec)
        return;
    m_backoffSec = v;
    QSettings(QStringLiteral("MSM"), QStringLiteral("MSM")).setValue(QStringLiteral("server/backoffSec"), v);
    emit backoffSecChanged();
}

bool ServerController::isRunning(const QString &name) const
{
    return m_procs.contains(name);
}

void ServerController::start(const QString &name, const QString &path,
                             const QString &javaPath, int minMem, int maxMem)
{
    if (m_procs.contains(name)) {
        emit consoleAppended(name, QStringLiteral("[MSM] 服务器已在运行"));
        return;
    }
    m_args[name] = {path, javaPath, minMem, maxMem};
    const QString jar = path + QStringLiteral("/server.jar");
    if (!QFile::exists(jar)) {
        emit consoleAppended(name, QStringLiteral("[MSM] 未找到 server.jar，无法启动：") + jar);
        return;
    }

    // ---- 多开端口冲突检测：与其他运行中的受管服务器或系统进程抢占同一端口时取消启动 ----
    const int port = serverPort(path);
    QString holder;
    bool conflict = false;
    for (auto it = m_ports.cbegin(); it != m_ports.cend(); ++it) {
        if (it.value() == port && it.key() != name) {
            holder = it.key();
            conflict = true;
            break;
        }
    }
    if (!conflict && !isPortFree(port))
        conflict = true;    // holder 留空 = 被系统其他程序占用
    if (conflict) {
        emit consoleAppended(name, QStringLiteral("[MSM] 端口 %1 已被%2占用，启动已取消（可自动分配空闲端口后重试）")
                                       .arg(port)
                                       .arg(holder.isEmpty() ? QStringLiteral("其他程序")
                                                             : QStringLiteral("服务器“%1”").arg(holder)));
        emit portConflict(name, path, port, holder);
        return;
    }

    // 解析 Java 路径：默认 "java" 时优先使用创建服务器时记录的版本专属 Java
    QString effectiveJava = javaPath;
    if (effectiveJava.isEmpty() || effectiveJava == QLatin1String("java")) {
        const QString stored = path + QStringLiteral("/.msm/java.txt");
        QFile f(stored);
        if (f.open(QIODevice::ReadOnly)) {
            const QString p2 = QString::fromUtf8(f.readAll()).trimmed();
            if (!p2.isEmpty() && QFile::exists(p2))
                effectiveJava = p2;
        }
        // 兜底：递归扫描 {path}/ 下所有以 jvm 开头的目录里的 java.exe
        // （兼容 jvm/、jvm-8/、jvm-21/、jvm-17/ 等不同命名）
        if (effectiveJava.isEmpty() || effectiveJava == QLatin1String("java")) {
            QDir baseDir(path);
            const QFileInfoList jvms = baseDir.entryInfoList(
                    QStringList() << QStringLiteral("jvm*"),
                    QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QFileInfo &fi : jvms) {
                QDirIterator it(fi.absoluteFilePath(),
                                QStringList() << QStringLiteral("java.exe") << QStringLiteral("java"),
                                QDir::Files, QDirIterator::Subdirectories);
                if (it.hasNext()) {
                    effectiveJava = it.next();
                    break;
                }
            }
        }
    }

    Proc p;
    p.proc = new QProcess(this);
    p.proc->setWorkingDirectory(path);
    p.proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(p.proc, &QProcess::readyReadStandardOutput, this, [this, name]() {
        handleOutput(name);
    });
    connect(p.proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, name](int code, QProcess::ExitStatus s) { onFinished(name, code, s); });

    QStringList args;
    args << QStringLiteral("-Xms%1M").arg(minMem)
         << QStringLiteral("-Xmx%1M").arg(maxMem)
         << QStringLiteral("-jar") << QStringLiteral("server.jar")
         << QStringLiteral("nogui");
    p.proc->start(effectiveJava, args);
    if (!p.proc->waitForStarted(5000)) {
        emit consoleAppended(name, QStringLiteral("[MSM] 启动失败：无法执行 ") + effectiveJava +
                                   QStringLiteral("（请确认已安装并加入 PATH）"));
        p.proc->deleteLater();
        return;
    }

    m_procs.insert(name, p);
    m_retryCount.remove(name);
    m_startTime.insert(name, QDateTime::currentMSecsSinceEpoch());
    m_ports.insert(name, port);
    m_intentionalKill.remove(name);
    m_tps[name] = 20.0;
    m_lastOverload.remove(name);
    emit consoleAppended(name, QStringLiteral("[MSM] 正在启动服务器…"));
    emit stateChanged(name, true);
    emit runningCountChanged();
}

void ServerController::handleOutput(const QString &name)
{
    auto it = m_procs.find(name);
    if (it == m_procs.end())
        return;
    QProcess *proc = it->proc;
    const QString data = QString::fromLocal8Bit(proc->readAllStandardOutput());
    QStringList lines = data.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (QString line : lines) {
        line = line.trimmed();
        if (line.isEmpty())
            continue;
        it->console.append(line + QLatin1Char('\n'));
        if (it->console.size() > 200000)
            it->console = it->console.right(200000);
        emit consoleAppended(name, line);

        // 在线玩家追踪
        QRegularExpression reJoin(QStringLiteral("(\\S+) joined the game"));
        QRegularExpression reLeft(QStringLiteral("(\\S+) left the game"));
        QRegularExpressionMatch m;
        m = reJoin.match(line);
        if (m.hasMatch()) {
            const QString who = m.captured(1);
            if (!it->playerList.contains(who)) {
                it->playerList.append(who);
                emit playersChanged(name, it->playerList);
                emit playerJoined(name, who);
            }
        } else if ((m = reLeft.match(line)).hasMatch()) {
            const QString who = m.captured(1);
            if (it->playerList.removeAll(who) > 0)
                emit playersChanged(name, it->playerList);
        }

        // A2：资源监控 - 估算 TPS。服务端在高负载时输出
        // “Can't keep up! (...) Running Nms behind”，据此推算 TPS。
        static const QRegularExpression reTps(QStringLiteral(
            "Can't keep up.*?Running\\s+(\\d+)ms\\s+behind"),
            QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch tm = reTps.match(line);
        if (tm.hasMatch()) {
            const double behind = tm.captured(1).toDouble();
            // 满速 20 TPS 对应 20000ms/窗口；落后越多 TPS 越低（单调、上限 20）。
            const double tps = qBound(0.0, 20000.0 / (20000.0 + behind), 20.0);
            m_tps[name] = tps;
            m_lastOverload[name] = QDateTime::currentMSecsSinceEpoch();
        }
    }
}

void ServerController::onFinished(const QString &name, int exitCode, QProcess::ExitStatus status)
{
    auto it = m_procs.find(name);
    if (it == m_procs.end())
        return;
    const QString tail = it->console.right(3000);
    const bool intentional = m_intentionalKill.remove(name);
    it->proc->deleteLater();
    m_consoleCache.insert(name, it->console);
    m_procs.erase(it);
    m_startTime.remove(name);
    m_ports.remove(name);
    m_usageSamples.remove(name);
    m_tps.remove(name);
    m_lastOverload.remove(name);
    emit consoleAppended(name, QStringLiteral("[MSM] 服务器已停止"));
    emit stateChanged(name, false);
    emit playersChanged(name, {});
    emit runningCountChanged();
    // 非主动强关却异常退出（崩溃 / 非 0 退出码）→ 上报错误日志
    if (!intentional && (status == QProcess::CrashExit || exitCode != 0)) {
        emit serverError(name, tail);
        // 后端崩溃自动拉起：指数退避，最多 m_maxRetries 次；EULA 未同意或用户已手动停止则不再拉起
        if (m_autoRestart && !tail.contains(QLatin1String("eula"), Qt::CaseInsensitive)
            && m_retryCount[name] < m_maxRetries) {
            const int attempt = ++m_retryCount[name];
            const int delay = m_backoffSec * (1 << (attempt - 1));
            emit consoleAppended(name, QStringLiteral("[MSM] 后端异常退出，%1 秒后自动重启（第 %2/%3 次）")
                                          .arg(delay).arg(attempt).arg(m_maxRetries));
            const StartArgs a = m_args.value(name);
            QTimer::singleShot(delay * 1000, this, [this, name, a, attempt]() {
                if (m_intentionalKill.contains(name)) {
                    m_retryCount.remove(name);
                    return;
                }
                start(name, a.path, a.javaPath, a.minMem, a.maxMem);
            });
        }
    }
}

void ServerController::stop(const QString &name)
{
    auto it = m_procs.find(name);
    if (it == m_procs.end())
        return;
    it->proc->write("stop\n");
}

void ServerController::forceStop(const QString &name)
{
    auto it = m_procs.find(name);
    if (it == m_procs.end())
        return;
    m_intentionalKill.insert(name);
    it->proc->kill();
}

void ServerController::send(const QString &name, const QString &cmd)
{
    auto it = m_procs.find(name);
    if (it == m_procs.end())
        return;
    it->proc->write(cmd.toLocal8Bit() + "\n");
}

QString ServerController::getConsole(const QString &name) const
{
    auto it = m_procs.find(name);
    if (it != m_procs.end())
        return it->console;
    return m_consoleCache.value(name);
}

QStringList ServerController::players(const QString &name) const
{
    auto it = m_procs.find(name);
    if (it != m_procs.end())
        return it->playerList;
    return {};
}

QStringList ServerController::listMods(const QString &path) const
{
    QDir dir(path + QStringLiteral("/mods"));
    if (!dir.exists())
        return {};
    QStringList names;
    for (const QFileInfo &fi : dir.entryInfoList(QDir::Files))
        names << fi.fileName();
    return names;
}

QVariantList ServerController::runningServerUsages() const
{
    QVariantList out;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_procs.constBegin(); it != m_procs.constEnd(); ++it) {
        const QString name = it.key();
        const QProcess *proc = it->proc;
        QVariantMap m;
        m[QStringLiteral("name")] = name;
        m[QStringLiteral("players")] = it->playerList.size();
        const qint64 start = m_startTime.value(name, now);
        m[QStringLiteral("uptimeSec")] = start ? (now - start) / 1000 : 0;
        // A2：若过去 60s 内没有过载日志，视为满速 20 TPS
        double tps = 20.0;
        const qint64 lastOver = m_lastOverload.value(name, 0);
        if (lastOver > 0 && (now - lastOver) < 60000)
            tps = m_tps.value(name, 20.0);
        m[QStringLiteral("tps")] = tps;
        double cpu = 0;
        qint64 memBytes = 0;
        const int pid = proc->processId();
        if (pid > 0) {
#ifdef Q_OS_WIN
            HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, (DWORD)pid);
            if (h) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
                    memBytes = pmc.WorkingSetSize;
                FILETIME c, e, k, u;
                if (GetProcessTimes(h, &c, &e, &k, &u)) {
                    auto toQ = [](FILETIME f) -> qint64 {
                        ULARGE_INTEGER i;
                        i.LowPart = f.dwLowDateTime;
                        i.HighPart = f.dwHighDateTime;
                        return qint64(i.QuadPart);
                    };
                    const qint64 cpuTime = toQ(k) + toQ(u);
                    const qint64 wall = QDateTime::currentMSecsSinceEpoch();
                    QPair<qint64, qint64> &s = m_usageSamples[name];
                    if (s.second > 0) {
                        const qint64 dCpu = cpuTime - s.first;
                        const qint64 dWall = wall - s.second;
                        if (dWall > 0)
                            cpu = double(dCpu) / double(dWall) * 100.0;
                    }
                    s.first = cpuTime;
                    s.second = wall;
                }
                CloseHandle(h);
            }
#endif
        }
        m[QStringLiteral("memMB")] = memBytes / (1024.0 * 1024.0);
        m[QStringLiteral("cpu")] = cpu;
        out << m;
    }
    return out;
}

namespace {
QVariantMap parseProperties(QTextStream &ts)
{
    QVariantMap map;
    while (!ts.atEnd()) {
        const QString line = ts.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int idx = line.indexOf(QLatin1Char('='));
        if (idx <= 0)
            continue;
        map.insert(line.left(idx).trimmed(), line.mid(idx + 1).trimmed());
    }
    return map;
}

void writeDefaultProperties(const QString &file)
{
    QDir().mkpath(QFileInfo(file).path());
    QFile f(file);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
        return;
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    const QStringList defaults = {
        QStringLiteral("#Minecraft server properties (由 MSM 生成的默认配置)"),
        QStringLiteral("allow-flight=false"),
        QStringLiteral("allow-nether=true"),
        QStringLiteral("difficulty=easy"),
        QStringLiteral("enable-command-block=false"),
        QStringLiteral("enable-rcon=false"),
        QStringLiteral("force-gamemode=false"),
        QStringLiteral("gamemode=survival"),
        QStringLiteral("generate-structures=true"),
        QStringLiteral("level-name=world"),
        QStringLiteral("level-seed="),
        QStringLiteral("level-type=minecraft:normal"),
        QStringLiteral("max-build-height=256"),
        QStringLiteral("max-players=20"),
        QStringLiteral("max-world-size=29999984"),
        QStringLiteral("motd=A Minecraft Server"),
        QStringLiteral("online-mode=true"),
        QStringLiteral("op-permission-level=4"),
        QStringLiteral("player-idle-timeout=0"),
        QStringLiteral("pvp=true"),
        QStringLiteral("server-ip="),
        QStringLiteral("server-port=25565"),
        QStringLiteral("spawn-animals=true"),
        QStringLiteral("spawn-monsters=true"),
        QStringLiteral("spawn-protection=16"),
        QStringLiteral("view-distance=10"),
        QStringLiteral("white-list=false")
    };
    for (const QString &l : defaults)
        ts << l << QLatin1Char('\n');
}
}

QVariantMap ServerController::readProperties(const QString &path)
{
    if (path.isEmpty())
        return {};
    const QString file = path + QStringLiteral("/server.properties");
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        // 文件不存在：写入默认 server.properties，便于查看与编辑
        writeDefaultProperties(file);
        QFile f2(file);
        if (f2.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream ts(&f2);
            ts.setEncoding(QStringConverter::Utf8);
            return parseProperties(ts);
        }
        return {};
    }
    QTextStream ts(&f);
    ts.setEncoding(QStringConverter::Utf8);
    return parseProperties(ts);
}

void ServerController::writeProperties(const QString &path, const QVariantMap &map)
{
    const QString file = path + QStringLiteral("/server.properties");
    QStringList outLines;
    bool changed = false;

    QFile f(file);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        ts.setEncoding(QStringConverter::Utf8);
        while (!ts.atEnd()) {
            QString line = ts.readLine();
            if (line.trimmed().isEmpty() || line.trimmed().startsWith(QLatin1Char('#'))) {
                outLines << line;
                continue;
            }
            const int idx = line.indexOf(QLatin1Char('='));
            if (idx <= 0) { outLines << line; continue; }
            const QString key = line.left(idx).trimmed();
            if (map.contains(key)) {
                const QString val = map.value(key).toString();
                outLines << (key + QStringLiteral("=") + val);
                changed = true;
            } else {
                outLines << line;
            }
        }
        f.close();
    }

    // 追加文件中不存在的键
    for (auto it = map.begin(); it != map.end(); ++it) {
        bool exists = false;
        for (const QString &l : outLines) {
            if (l.startsWith(it.key() + QLatin1Char('=')) || l.startsWith(it.key() + QStringLiteral(" ="))) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            outLines << (it.key() + QStringLiteral("=") + it.value().toString());
            changed = true;
        }
    }

    if (!changed)
        return;
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit consoleAppended(QString(), QStringLiteral("[MSM] 无法写入 server.properties"));
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (const QString &l : outLines)
        out << l << QLatin1Char('\n');
    f.close();
}

int ServerController::serverPort(const QString &path) const
{
    QFile f(path + QStringLiteral("/server.properties"));
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        while (!f.atEnd()) {
            const QString line = QString::fromUtf8(f.readLine()).trimmed();
            if (line.startsWith(QStringLiteral("server-port="))) {
                bool ok = false;
                const int p = line.mid(12).trimmed().toInt(&ok);
                if (ok && p > 0 && p < 65536)
                    return p;
            }
        }
    }
    return 25565;
}

bool ServerController::isPortFree(int port) const
{
    if (port <= 0 || port >= 65536)
        return false;
    QTcpServer probe;
    const bool ok = probe.listen(QHostAddress::AnyIPv4, quint16(port));
    probe.close();
    return ok;
}

int ServerController::assignFreePort(const QString &path)
{
    const int base = serverPort(path);
    QSet<int> used;
    for (auto it = m_ports.cbegin(); it != m_ports.cend(); ++it)
        used.insert(it.value());
    for (int p = base + 1; p < base + 500 && p < 65536; ++p) {
        if (used.contains(p) || !isPortFree(p))
            continue;
        QVariantMap m;
        m.insert(QStringLiteral("server-port"), QString::number(p));
        writeProperties(path, m);
        return p;
    }
    return -1;
}

QString ServerController::readServerJavaPath(const QString &path) const
{
    QFile f(path + QStringLiteral("/.msm/java.txt"));
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    return QString::fromUtf8(f.readAll()).trimmed();
}

void ServerController::setServerJavaPath(const QString &path, const QString &javaPath)
{
    QDir().mkpath(path + QStringLiteral("/.msm"));
    QFile f(path + QStringLiteral("/.msm/java.txt"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return;
    f.write(javaPath.toUtf8());
    if (!javaPath.isEmpty())
        f.write("\n");
    f.close();
}
