#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QHash>
#include <QMap>
#include <QProcess>
#include <QSet>
#include <QPair>
#include <QDateTime>
#include <QTimer>

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
    // 看门狗：防止 IO 卡死导致无法控制。双检测机制：
    //  1) 被动静默超时：进程存活但连续 silenceSec 秒无 stdout 输出（疑似 IO 管道阻塞/世界卡死）
    //  2) 主动心跳超时：周期性发 list 指令，heartbeatTimeoutSec 秒内无新 stdout 响应（进程无响应）
    // 两者任一触发即判定卡死，自动强关并走异常纠错重启（若 autoRestart 开启）。
    Q_PROPERTY(bool watchdogEnabled READ watchdogEnabled WRITE setWatchdogEnabled NOTIFY watchdogEnabledChanged)
    Q_PROPERTY(int watchdogHeartbeatSec READ watchdogHeartbeatSec WRITE setWatchdogHeartbeatSec NOTIFY watchdogHeartbeatSecChanged)
    Q_PROPERTY(int watchdogSilenceSec READ watchdogSilenceSec WRITE setWatchdogSilenceSec NOTIFY watchdogSilenceSecChanged)
    Q_PROPERTY(int watchdogTimeoutSec READ watchdogTimeoutSec WRITE setWatchdogTimeoutSec NOTIFY watchdogTimeoutSecChanged)
    // 异常纠错中心：当前存在活动异常（未恢复）的服务器记录列表；QML 据此渲染纠错界面。
    // 记录会在服务器成功重拉起后自动清除（视为已纠错）。
    Q_PROPERTY(QVariantList errorRecords READ errorRecords NOTIFY errorRecordsChanged)
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

    bool watchdogEnabled() const { return m_watchdogEnabled; }
    void setWatchdogEnabled(bool v);
    int watchdogHeartbeatSec() const { return m_watchdogHeartbeatSec; }
    void setWatchdogHeartbeatSec(int v);
    int watchdogSilenceSec() const { return m_watchdogSilenceSec; }
    void setWatchdogSilenceSec(int v);
    int watchdogTimeoutSec() const { return m_watchdogTimeoutSec; }
    void setWatchdogTimeoutSec(int v);

    // 判断指定服务器当前是否在运行（按 path 身份键区分，同名不同目录互不干扰）
    Q_INVOKABLE bool isRunning(const QString &path) const;
    // 返回当前所有运行中的服务器名称（供 QML/WebUI 判断运行时状态）
    Q_INVOKABLE QStringList runningServerNames() const;
    // 判断路径是否存在（文件或目录），供 QML 判断服务器是否含 mods/ 等子目录
    Q_INVOKABLE bool pathExists(const QString &path) const;
    // 以给定的 java 路径与内存参数（最小/最大，单位 MB）启动一台服务器。
    // path 为服务端根目录（含核心 jar），同时作为唯一进程键（同名不同目录互不串台）；
    // name 仅用于日志/控制台展示。
    Q_INVOKABLE void start(const QString &name, const QString &path,
                           const QString &javaPath = QStringLiteral("java"),
                           int minMem = 1024, int maxMem = 2048,
                           bool resetRetry = true);
    // 向服务端发送 stop 指令（优雅停止，等待存档保存后退出）
    Q_INVOKABLE void stop(const QString &path);
    // 强制终止进程（TerminateProcess），不等待存档；仅在无响应时兜底使用
    Q_INVOKABLE void forceStop(const QString &path);
    // 退出时强制终止所有运行中的服务器进程，避免残留孤儿进程
    Q_INVOKABLE void stopAll();
    // 向运行中的服务端发送一条控制台指令（如 op、gamemode、whitelist 等）
    Q_INVOKABLE void send(const QString &path, const QString &cmd);
    // 取回指定服务器的完整控制台历史文本（进程结束后仍保留缓存）
    Q_INVOKABLE QString getConsole(const QString &path) const;
    // 取回当前在线玩家名列表
    Q_INVOKABLE QStringList players(const QString &path) const;
    // 列举指定服务端目录下的 mods（文件名列表）
    Q_INVOKABLE QStringList listMods(const QString &path) const;
    // 读取 server.properties 为键值映射（便于 QML 表单双向编辑）
    Q_INVOKABLE QVariantMap readProperties(const QString &path);
    // 将键值映射写回 server.properties（仅覆盖提供过的键，保留其余原值）
    Q_INVOKABLE void writeProperties(const QString &path, const QVariantMap &map);
    // 运行中服务器的资源占用快照（CPU%/内存MB/在线人数/运行时长）
    Q_INVOKABLE QVariantList runningServerUsages() const;
    // 取某服务器的看门狗健康状态（供 QML/WebUI 展示）：
    // { enabled, running, lastStdoutSec, lastHeartbeatSec, pendingHeartbeat, healthy }
    Q_INVOKABLE QVariantMap watchdogStatus(const QString &path) const;

    // ---- 异常纠错中心 ----
    // 返回当前所有“活动异常”（未恢复）的服务器纠错记录列表，每条含：
    //  name, path, type(crash/io/heartbeat/eula), typeLabel, time, logTail,
    //  autoRestart, retryCount, maxRetries, retrying, nextRetryInSec, fatal(eula 等不可自动恢复)
    Q_INVOKABLE QVariantList errorRecords() const;
    // 立即强制重拉起（取消 pending 重试计时器），用于界面“现在重试”按钮
    Q_INVOKABLE void retryNow(const QString &path);
    // 停止该服务器的自动重拉起（保留记录，标记 retrying=false），用于“停止重试”
    Q_INVOKABLE void stopRetries(const QString &path);
    // 清除该服务器的纠错记录（已人工处理完毕），用于“标记已解决”
    Q_INVOKABLE void clearError(const QString &path);

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
    void watchdogEnabledChanged();
    void watchdogHeartbeatSecChanged();
    void watchdogSilenceSecChanged();
    void watchdogTimeoutSecChanged();
    // 看门狗触发：服务器被判定卡死并强制终止，附带原因（"io"=被动静默 / "heartbeat"=心跳无响应）
    void watchdogTriggered(const QString &name, const QString &reason);

    // 异常纠错记录变化（新增/更新/清除任一记录后发出），供 QML 刷新纠错中心
    void errorRecordsChanged();

private:
    // 在 dir 根目录（不含子目录）查找首个文件名以 prefix 开头、且不在 exclude 列表中的文件
    // （exclude 用于剔除 installer/日志等，如 "*-installer*.jar"）。找不到返回空串。
    static QString findLaunchJar(const QString &dir, const QString &prefix,
                                 const QStringList &exclude);
    // 查找 NeoForge/Forge 1.17+ 的参数文件（win_args.txt/unix_args.txt），返回相对 dir 的路径；
    // 优先根目录，否则递归扫描 libraries/（NeoForge 21.x+ 把 args 放在 libraries 深层）。找不到返回空串。
    static QString findArgsFile(const QString &dir);
    // 读取并解析进程的标准输出/错误，逐行发出 consoleAppended 并提取玩家名单
    void handleOutput(const QString &name);
    // 进程结束回调：清理资源、发出 stateChanged(false)，保留控制台缓存
    void onFinished(const QString &name, int exitCode, QProcess::ExitStatus status);
    // 看门狗：每 tickSec 秒扫描所有运行进程，检测静默超时 / 心跳超时
    void watchdogTick();
    // 对指定进程发送一次心跳指令（list），记录发送时刻并标记 pending
    void sendHeartbeat(const QString &name);
    // 看门狗触发：判定卡死，强制终止并走异常纠错重启
    void triggerWatchdog(const QString &path, const QString &reason);
    // 记录一条异常（serverError / watchdogTriggered 统一入口），分类并刷新待重试计时
    void recordError(const QString &path, const QString &type, const QString &logTail);
    // 清除某服务器的异常记录（成功重拉起 / 人工标记已解决）
    void clearErrorRecord(const QString &path);
    // 每秒刷新待重试倒计时（nextRetryInSec），到期则触发自动重拉起
    void errorTick();

    // 单个服务器异常纠错记录
    struct ErrorInfo {
        QString name;
        QString path;
        QString type;            // crash / io / heartbeat / eula
        QString typeLabel;       // 中文分类标签
        QString time;            // 首次发生时间（本地可读）
        QString logTail;         // 尾部日志（供界面展示）
        bool autoRestart = true; // 是否配置了自动重拉起
        int retryCount = 0;      // 已重试次数
        int maxRetries = 5;      // 最大重试次数
        bool retrying = false;   // 仍在自动重拉起尝试中（false = 已停止重试或已放弃）
        int nextRetryInSec = 0;  // 距下次自动重拉起的剩余秒数（>0 表示 pending）
        bool fatal = false;      // 致命且不可自动恢复（如 eula 未同意）
        qint64 retryDeadlineMs = 0; // 计划重拉起的墙钟时刻
    };

    // 单个服务器进程运行态：保存进程指针、完整控制台、在线玩家
    struct Proc {
        QProcess *proc = nullptr;
        QString console;
        QStringList playerList;
        // ---- 看门狗运行态 ----
        qint64 lastStdoutMs = 0;     // 最近一次收到 stdout 的时间
        qint64 lastHeartbeatMs = 0;  // 最近一次发出心跳指令的时间
        bool heartbeatPending = false; // 心跳已发出、尚未收到新 stdout 响应
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
    struct StartArgs { QString name; QString path; QString javaPath; int minMem = 1024; int maxMem = 2048; };
    QHash<QString, StartArgs> m_args;
    QMap<QString, int> m_retryCount;
    bool m_autoRestart = true;
    int m_maxRetries = 5;
    int m_backoffSec = 5;
    // 每个服务器的估算 TPS（A2）：解析 “Can't keep up! ... Running Nms behind” 得出；
    // 若过去 60s 内没有过载日志，则回退为满速 20。
    QMap<QString, double> m_tps;
    QMap<QString, qint64> m_lastOverload;

    // 看门狗配置
    bool m_watchdogEnabled = true;       // 总开关（默认开）
    int m_watchdogHeartbeatSec = 60;     // 心跳指令周期（秒）
    int m_watchdogSilenceSec = 300;      // 被动静默上限（秒，无 stdout 即疑似卡死）
    int m_watchdogTimeoutSec = 20;       // 心跳发出后等待响应的超时（秒）
    QTimer *m_watchdogTimer = nullptr;    // 周期性扫描定时器
    QTimer *m_errorTimer = nullptr;       // 异常重试倒计时定时器（每秒）
    // 活动异常记录（key = path）；成功重拉起后移除
    QHash<QString, ErrorInfo> m_errors;
};
