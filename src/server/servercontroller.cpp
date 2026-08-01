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
#include <QProcessEnvironment>

// 判断 JVM 参数字符串列表里是否已包含某前缀（用于避免重复追加 -D 参数）
static bool ulinesContains(const QStringList &lines, const QString &prefix)
{
    for (const QString &l : lines)
        if (l.startsWith(prefix))
            return true;
    return false;
}

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
    // 注意：不再前置硬校验 server.jar 是否存在——模组服（Forge/NeoForge/Fabric）的真实启动核心
    // 可能是 fabric-server-launch.jar / forge-*.jar / unix_args.txt 等，并不一定是 server.jar。
    // 是否存在可识别核心由下方启动探测逻辑统一判断并在缺失时报错，避免误拒模组服启动。

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
    // Linux 下主进程若在 C locale 中，派生的 java 子进程会因路径含中文解码成 '?' 而打不开
    // server.jar / 无法写含中文目录。强制注入 UTF-8 locale（仅当环境未指定时），与安装器一致。
#ifdef Q_OS_LINUX
    {
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        if (env.value(QStringLiteral("LC_ALL")).isEmpty() && env.value(QStringLiteral("LANG")).isEmpty())
            env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C.UTF-8"));
        p.proc->setProcessEnvironment(env);
    }
#endif

    connect(p.proc, &QProcess::readyReadStandardOutput, this, [this, name]() {
        handleOutput(name);
    });
    connect(p.proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, name](int code, QProcess::ExitStatus s) { onFinished(name, code, s); });

    QStringList args;
    // 自适应探测启动方式：不同加载器（NeoForge/Forge 1.17+、Fabric、Forge 1.16-）的安装产物结构
    // 各不相同，不能硬编码成 `java -jar server.jar`。探测顺序（args 文件优先，因其内含 libraries
    // 路径与 -jar 目标，裸 jar 启动会找不到主类）：
    //   1) unix_args.txt/win_args.txt（NeoForge、Forge 1.17+）→ `java @user_jvm_args.txt @win_args.txt`
    //   2) fabric-server-launch.jar（Fabric）→ `java -Xms -Xmx -jar fabric-server-launch.jar nogui`
    //   3) forge-*.jar 胖 jar（Forge 1.16-）→ `java -Xms -Xmx -jar forge-X.Y.Z.jar nogui`
    //   4) neoforge-*.jar（兜底）→ 同上
    //   5) server.jar（原版/Vanilla 或纯核心）→ `java -Xms -Xmx -jar server.jar nogui`
    // NeoForge / Forge 1.17+ 的 args 文件位置有两种可能：
    //   · 旧版（Forge 1.17-1.20.x）放在服务端根目录（unix_args.txt / win_args.txt）
    //   · 新版（NeoForge 21.x+、Forge 1.21+）放在 libraries/net/neoforged/neoforge/{version}/
    //     下（如 libraries/net/neoforged/neoforge/26.2.0.41-beta/win_args.txt）。
    // 这里优先根目录，否则递归搜索 libraries/ 找到 args 文件，返回相对工作目录的路径（供 @ 引用）。
    const QString argsFile = findArgsFile(path);
    const bool hasArgs = !argsFile.isEmpty();
    if (hasArgs) {
        // NeoForge / Forge 1.17+：把 UI 内存设置写入 user_jvm_args.txt（替换已有 -Xms/-Xmx 行），
        // 再 @引用参数文件（nogui 已在参数文件内，无需追加）。user_jvm_args.txt 若不存在则兜底创建。
        // 注意：user_jvm_args.txt 一律在根目录（安装器始终在此生成），与 args 文件位置无关。
        const QString ujvm = path + QStringLiteral("/user_jvm_args.txt");
        QFile uf(ujvm);
        QStringList ulines;
        if (uf.open(QIODevice::ReadOnly)) {
            const QStringList old = QString::fromUtf8(uf.readAll())
                    .split(QLatin1Char('\n'), Qt::SkipEmptyParts);
            for (const QString &l : old)
                if (!l.contains(QLatin1String("-Xms")) && !l.contains(QLatin1String("-Xmx")))
                    ulines << l;
            uf.close();
        }
        ulines << QStringLiteral("-Xms%1M").arg(minMem) << QStringLiteral("-Xmx%1M").arg(maxMem);
        // NeoForge/Forge 1.17+ 的 args 文件（win_args.txt/unix_args.txt）里不带 nogui，
        // 主类 net.neoforged.fml.startup.Server / net.minecraftforge.fml.server.Server 会弹
        // 出早期显示（earlydisplay）GUI 窗口。该窗口由 JVM 的 AWT 决定：置 headless=true 即
        // 抑制（日志可见 “Not loading early display in headless mode”）。否则 Windows 上会多
        // 出一个关不掉的服务器 GUI 窗口，必须手动关掉才不卡进程。
        if (!ulinesContains(ulines, QStringLiteral("-Djava.awt.headless")))
            ulines << QStringLiteral("-Djava.awt.headless=true");
        if (uf.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            uf.write(ulines.join(QLatin1Char('\n')).toUtf8());
            uf.close();
        }
        args << QStringLiteral("@user_jvm_args.txt");
        // args 文件可能是根目录（win_args.txt）或 libraries 下的深层路径——统一用相对工作目录的引用。
        args << (QStringLiteral("@") + argsFile);
    } else {
        // 探测实际启动 jar
        static const QStringList installerExclude = {
            QStringLiteral("*-installer*.jar"), QStringLiteral("installer*.jar.log") };
        QString launch;
        // Fabric 优先：fabric-server-launch.jar 才是真正的模组服启动器（server.jar 只是原版核心备份）
        const QString fabricJar = findLaunchJar(path, QStringLiteral("fabric-server-launch.jar"),
                                                installerExclude);
        if (!fabricJar.isEmpty()) {
            launch = fabricJar;
        } else {
            const QString forgeJar = findLaunchJar(path, QStringLiteral("forge-"), installerExclude);
            if (!forgeJar.isEmpty()) {
                launch = forgeJar;
            } else {
                const QString neoJar = findLaunchJar(path, QStringLiteral("neoforge-"),
                                                     installerExclude);
                if (!neoJar.isEmpty())
                    launch = neoJar;
            }
        }
        if (launch.isEmpty() && QFile::exists(path + QStringLiteral("/server.jar")))
            launch = QDir(path).absoluteFilePath(QStringLiteral("server.jar"));
        if (launch.isEmpty()) {
            emit consoleAppended(name, QStringLiteral("[MSM] 启动失败：未找到任何可识别的服务端核心"
                "（server.jar / fabric-server-launch.jar / forge-*.jar / neoforge-*.jar / args 文件）。"
                "请重新创建服务器。"));
            p.proc->deleteLater();
            return;
        }
        args << QStringLiteral("-Xms%1M").arg(minMem)
             << QStringLiteral("-Xmx%1M").arg(maxMem)
             << QStringLiteral("-jar") << launch
             << QStringLiteral("nogui");
    }
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

// 在 dir 根目录查找首个文件名匹配 prefix（前缀）且不在 exclude glob 列表中的文件。
// exclude 用于剔除安装器 jar / 日志（如 "*-installer*.jar"）。仅扫描根目录（不递归），
// 因为服务端核心 jar 一律位于服务端根目录。找不到返回空串。
QString ServerController::findLaunchJar(const QString &dir, const QString &prefix,
                                        const QStringList &exclude)
{
    QDir d(dir);
    if (!d.exists())
        return QString();
    const QFileInfoList files = d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : files) {
        const QString name = fi.fileName();
        // 满足 exclude 之一则跳过（如 neoforge-X-installer.jar）
        bool skipped = false;
        for (const QString &ex : exclude) {
            // ex 形如 "*-installer*.jar"，转成正则：*→[^/]*，?→.，其余按字面（含 . 转义）
            QString pattern = QRegularExpression::anchoredPattern(
                QRegularExpression::wildcardToRegularExpression(ex));
            QRegularExpression re(pattern, QRegularExpression::CaseInsensitiveOption);
            if (re.match(name).hasMatch()) { skipped = true; break; }
        }
        if (skipped)
            continue;
        if (name.startsWith(prefix, Qt::CaseInsensitive))
            return fi.absoluteFilePath();
    }
    return QString();
}

// 在 dir 下查找 NeoForge / Forge 1.17+ 的参数文件（win_args.txt / unix_args.txt），
// 返回相对于 dir 的路径（供 java @ 引用）。优先根目录，否则递归扫描 libraries/
// （NeoForge 21.x+、Forge 1.21+ 把 args 文件放在 libraries/net/neoforged/neoforge/{version}/ 下）。
// 找不到返回空串。
QString ServerController::findArgsFile(const QString &dir)
{
    const QStringList names = { QStringLiteral("win_args.txt"), QStringLiteral("unix_args.txt") };
#ifdef Q_OS_WIN
    const QString wanted = QStringLiteral("win_args.txt");
#else
    const QString wanted = QStringLiteral("unix_args.txt");
#endif
    QDir d(dir);
    if (d.exists()) {
        // 1) 根目录优先
        for (const QString &n : names) {
            if (QFile::exists(dir + QStringLiteral("/") + n))
                return n;
        }
    }
    // 2) libraries/ 递归扫描（NeoForge 实际产物位置）
    const QString libDir = dir + QStringLiteral("/libraries");
    if (QFileInfo::exists(libDir)) {
        QDirIterator it(libDir, QStringList() << QStringLiteral("*.txt"),
                        QDir::Files, QDirIterator::Subdirectories);
        QStringList found;
        while (it.hasNext()) {
            const QString abs = it.next();
            const QString base = QFileInfo(abs).fileName();
            if (names.contains(base, Qt::CaseInsensitive))
                found << d.relativeFilePath(abs);  // 形如 libraries/.../win_args.txt
        }
        if (!found.isEmpty()) {
            // 优先匹配当前平台对应的 args 文件（win/unix），否则任意匹配的 args 文件
            for (const QString &f : found)
                if (QFileInfo(f).fileName().compare(wanted, Qt::CaseInsensitive) == 0)
                    return f;
            return found.first();
        }
    }
    return QString();
}
