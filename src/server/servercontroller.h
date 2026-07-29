#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QHash>
#include <QProcess>
#include <QSet>
#include <QPair>
#include <QDateTime>

#ifdef Q_OS_WIN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#endif

// 服务器管理逻辑层（C++）：负责单个 Minecraft 服务端进程的
// 启动 / 停止 / 强制停止 / 发送控制台指令，并实时捕获控制台输出、
// 跟踪在线玩家、读写 server.properties、列举 mods。
// 该层不依赖任何 UI 模块，QML 仅通过上下文属性 serverController 调用其
// Q_INVOKABLE 方法并订阅信号来刷新界面，从而实现逻辑与界面解耦。
class ServerController : public QObject
{
    Q_OBJECT
    // 当前正在运行的服务器数量；QML 可据此显示“N 台运行中”或调整界面状态。
    Q_PROPERTY(int runningCount READ runningCount NOTIFY runningCountChanged)
    // 后端崩溃（非主动停止）后是否自动拉起；重试次数与退避基数可配。
    Q_PROPERTY(bool autoRestart READ autoRestart WRITE setAutoRestart NOTIFY autoRestartChanged)
    Q_PROPERTY(int maxRetries READ maxRetries WRITE setMaxRetries NOTIFY maxRetriesChanged)
    Q_PROPERTY(int backoffSec READ backoffSec WRITE setBackoffSec NOTIFY backoffSecChanged)
public:
    explicit ServerController(QObject *parent = nullptr);

    // 正在运行的服务器数量（即 m_procs 中的进程数）
    int runningCount() const { return m_procs.size(); }

    bool autoRestart() const { return m_autoRestart; }
    void setAutoRestart(bool v);
    int maxRetries() const { return m_maxRetries; }
    void setMaxRetries(int v);
    int backoffSec() const { return m_backoffSec; }
    void setBackoffSec(int v);

    // 判断指定服务器当前是否在运行
    Q_INVOKABLE bool isRunning(const QString &name) const;
    // 以给定的 java 路径与内存参数（最小/最大，单位 MB）启动一台服务器。
    // name 用作进程键（控制台/状态索引），path 为服务端根目录（含核心 jar）。
    Q_INVOKABLE void start(const QString &name, const QString &path,
                           const QString &javaPath = QStringLiteral("java"),
                           int minMem = 1024, int maxMem = 2048);
    // 向服务端发送 stop 指令（优雅停止，等待存档保存后退出）
    Q_INVOKABLE void stop(const QString &name);
    // 强制终止进程（TerminateProcess），不等待存档；仅在无响应时兜底使用
    Q_INVOKABLE void forceStop(const QString &name);
    // 向运行中的服务端发送一条控制台指令（如 op、gamemode、whitelist 等）
    Q_INVOKABLE void send(const QString &name, const QString &cmd);
    // 取回指定服务器的完整控制台历史文本（进程结束后仍保留缓存）
    Q_INVOKABLE QString getConsole(const QString &name) const;
    // 取回当前在线玩家名列表
    Q_INVOKABLE QStringList players(const QString &name) const;
    // 列举指定服务端目录下的 mods（文件名列表）
    Q_INVOKABLE QStringList listMods(const QString &path) const;
    // 读取 server.properties 为键值映射（便于 QML 表单双向编辑）
    Q_INVOKABLE QVariantMap readProperties(const QString &path);
    // 将键值映射写回 server.properties（仅覆盖提供过的键，保留其余原值）
    Q_INVOKABLE void writeProperties(const QString &path, const QVariantMap &map);
    // 读取服务端目录中保存的、专用于该服务器的 Java 路径（可为空=用系统默认）
    Q_INVOKABLE QString readServerJavaPath(const QString &path) const;
    // 为该服务器单独设置/清空 Java 路径（写入 eula/启动配置或单独记录文件）
    Q_INVOKABLE void setServerJavaPath(const QString &path, const QString &javaPath);
    // 运行中服务器的资源占用快照（CPU%/内存MB/在线人数/运行时长）
    Q_INVOKABLE QVariantList runningServerUsages() const;

    // ---- 多开端口管理 ----
    // 读取 server.properties 中的 server-port（缺失/非法时返回默认 25565）
    Q_INVOKABLE int serverPort(const QString &path) const;
    // 探测端口是否空闲（尝试绑定 0.0.0.0:port）
    Q_INVOKABLE bool isPortFree(int port) const;
    // 为该服务器自动分配一个空闲端口并写回 server.properties；返回新端口（失败返回 -1）
    Q_INVOKABLE int assignFreePort(const QString &path);

signals:
    // 某服务器新增一行控制台输出（QML 用于增量追加，避免整段重绘）
    void consoleAppended(const QString &name, const QString &line);
    // 服务器运行状态变化（运行中/已停止），QML 据此刷新卡片状态与 runningCount
    void stateChanged(const QString &name, bool running);
    // 在线玩家列表变化
    void playersChanged(const QString &name, const QStringList &players);
    // 运行服务器数量变化
    void runningCountChanged();
    // 服务器异常退出（崩溃 / 非 0 退出且非主动强关），携带尾部日志供上报
    void serverError(const QString &name, const QString &logTail);
    // 有玩家进入服务器（用于 Webhook 推送），who 为玩家名
    void playerJoined(const QString &name, const QString &who);
    // 启动前发现端口冲突（holder=占用端口的另一台受管服务器名；为空表示被系统其他程序占用）。
    // 收到该信号说明本次 start 已被取消，由 UI 决定是否 assignFreePort 后重新 start。
    void portConflict(const QString &name, const QString &path, int port, const QString &holder);
    void autoRestartChanged();
    void maxRetriesChanged();
    void backoffSecChanged();

private:
    // 读取并解析进程的标准输出/错误，逐行发出 consoleAppended 并提取玩家名单
    void handleOutput(const QString &name);
    // 进程结束回调：清理资源、发出 stateChanged(false)，保留控制台缓存
    void onFinished(const QString &name, int exitCode, QProcess::ExitStatus status);

    // 单个服务器进程运行态：保存进程指针、完整控制台、在线玩家
    struct Proc {
        QProcess *proc = nullptr;
        QString console;
        QStringList playerList;
    };
    // name -> 运行进程信息；同一时间同名仅允许一个进程
    QHash<QString, Proc> m_procs;
    // name -> 历史控制台文本；进程结束后仍保留，供用户事后查看日志
    QHash<QString, QString> m_consoleCache;
    // name -> 启动时刻（毫秒），用于计算运行时长
    QHash<QString, qint64> m_startTime;
    // name -> 启动时占用的 server-port（用于多开端口冲突检测）
    QHash<QString, int> m_ports;
    // name -> 上一次采样的 (CPU 时间, 墙钟时间)，用于增量计算 CPU 占用率
    mutable QHash<QString, QPair<qint64, qint64>> m_usageSamples;
    // 被主动 forceStop 的服务器：退出时不视为报错
    QSet<QString> m_intentionalKill;

    // 崩溃自动重启：记录启动参数用于重拉起，以及每服重试计数与全局配置
    struct StartArgs { QString path; QString javaPath; int minMem = 1024; int maxMem = 2048; };
    QHash<QString, StartArgs> m_args;
    QMap<QString, int> m_retryCount;
    bool m_autoRestart = true;
    int m_maxRetries = 5;
    int m_backoffSec = 5;
};
