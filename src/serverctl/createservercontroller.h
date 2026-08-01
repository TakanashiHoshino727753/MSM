#pragma once
#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <functional>
#include <QProcess>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QVector>
#include <QTimer>

class DownloadManager;
class ServerManager;
class JavaManager;

// 创建服务器向导的逻辑层（C++）。负责：服务端版本真实拉取（Paper/Vanilla/模组服）、
// 名称->目录推导、下载触发、完成后写 eula.txt 并入服务器列表。QML 只画表单并绑定属性。
// 模组服可勾选安装 forge/fabric/neoforge 多个加载器；Java 按普通服务器策略装到服务器目录
// 下的 jvm-{feature}/（与下载中心模组服一样只准备服务端产物，不做压缩包打包）。
class CreateServerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QStringList types READ types CONSTANT)
    Q_PROPERTY(QStringList versions READ versions NOTIFY versionsChanged)
    Q_PROPERTY(QString currentType READ currentType WRITE setCurrentType NOTIFY currentTypeChanged)
    Q_PROPERTY(QString currentVersion READ currentVersion WRITE setCurrentVersion NOTIFY currentVersionChanged)
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString saveDir READ saveDir WRITE setSaveDir NOTIFY saveDirChanged)
    Q_PROPERTY(bool eulaAccepted READ eulaAccepted WRITE setEulaAccepted NOTIFY eulaAcceptedChanged)
    Q_PROPERTY(QList<QString> selectedLoaders READ selectedLoaders WRITE setSelectedLoaders NOTIFY selectedLoadersChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool done READ done NOTIFY doneChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(qreal stageProgress READ stageProgress NOTIFY stageProgressChanged)
    Q_PROPERTY(QString stageText READ stageText NOTIFY stageTextChanged)

public:
    explicit CreateServerController(DownloadManager *dm, ServerManager *sm,
                                    JavaManager *java, QObject *parent = nullptr);

    QStringList types() const;
    QStringList versions() const { return m_versions; }
    QString currentType() const { return m_currentType; }
    void setCurrentType(const QString &t);
    QString currentVersion() const { return m_currentVersion; }
    void setCurrentVersion(const QString &v);
    QString name() const { return m_name; }
    void setName(const QString &v);
    QString saveDir() const { return m_saveDir; }
    void setSaveDir(const QString &d);
    bool eulaAccepted() const { return m_eulaAccepted; }
    void setEulaAccepted(bool b);
    bool busy() const { return m_busy; }
    bool done() const { return m_done; }
    QString statusText() const { return m_status; }
    qreal progress() const { return m_progress; }
    qreal stageProgress() const { return m_stageProg; }
    QString stageText() const { return m_stageText; }

    Q_INVOKABLE void loadVersions();
    Q_INVOKABLE void create();
    Q_INVOKABLE void reset();

    // 从已下载/已打包的服务器压缩包导入：解压到保存目录并加入服务器列表。
    // 版本/类型自动探测；要求 EULA 已同意（调用方负责）。
    Q_INVOKABLE void importZip(const QString &zipPath);

    // 下载中心“仅准备服务端目录、不加入服务器列表”时使用：跳过 addServer。
    // 安装/Java 策略与普通创建服务器完全一致（Java 装到 saveDir/jvm-{feature}/）。
    void setSkipAddList(bool b) { m_listServer = b; }

    // 判断保存目录是否已存在且非空（含任意文件/子目录），用于创建前校验，
    // 避免覆盖已有内容/已存在的服务器目录。
    Q_INVOKABLE bool dirOccupied(const QString &path) const {
        QDir d(path);
        if (!d.exists()) return false;
        return !d.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty();
    }

    // 内存分配（MB）。创建时不启动服务器，此处仅记录，供后续启动时读取；
    // 与本地端 CreateServerDialog 的内存字段行为一致（展示 + 记录，未强制回写启动）。
    Q_INVOKABLE void setMinMemory(int v) { m_minMemory = v; }
    Q_INVOKABLE void setMaxMemory(int v) { m_maxMemory = v; }

    // 模组服：可用加载器列表（key），QML 据此绘制勾选项
    Q_INVOKABLE QStringList modLoaders() const;
    Q_INVOKABLE QString loaderLabel(const QString &loader) const;
    // 判断某加载器是否兼容给定 MC 版本（只把兼容的提供给用户勾选）
    Q_INVOKABLE bool loaderCompatible(const QString &loader, const QString &version) const;

    QList<QString> selectedLoaders() const { return m_selectedLoaders; }
    void setSelectedLoaders(const QList<QString> &v);

signals:
    void versionsChanged();
    void currentTypeChanged();
    void currentVersionChanged();
    void nameChanged();
    void saveDirChanged();
    void eulaAcceptedChanged();
    void selectedLoadersChanged();
    void busyChanged();
    void doneChanged();
    void statusTextChanged();
    void progressChanged();
    void stageProgressChanged();
    void stageTextChanged();

public:
    // 供外部（如下载中心打包委托）读取当前状态文案，转发到上层状态栏
    QString status() const { return m_status; }

private:
    QString typeKey() const;
    void setStatus(const QString &s);
    void setBusy(bool b);
    void setDone(bool d);
    void setProgress(qreal p);

    // 分阶段进度管理：把整体进度按权重分配到各阶段（准备 Java / 每个加载器 / 打包），
    // 避免进度条直接跳到 100%。stageProgress 表示当前阶段内部 0-100，progress 为总进度。
    void beginStages(const QVector<qreal> &weights);
    void setStage(int idx);
    void setStageProgress(qreal p);
    void setStageText(const QString &t);

    void fetchPaper();
    void fetchVanilla();
    void fetchModVersions();
    void fetchNeoforgeVersions();
    void resolveUrl(const QString &type, const QString &version,
                    std::function<void(const QString &)> cb);
    // 解析某个加载器（forge/fabric/neoforge）对应 MC 版本的 installer 下载地址
    // cb 的第一个参数为首选 URL，第二个参数为镜像兜底 URL（为空表示无兜底）
    void resolveLoaderUrl(const QString &loader, const QString &version,
                          std::function<void(const QString &url, const QString &fallback)> cb);
    void fetchJson(const QString &url,
                   std::function<void(const QJsonDocument &)> onOk,
                   std::function<void(const QString &)> onErr);
    void fetchText(const QString &url,
                   std::function<void(const QString &)> onOk,
                   std::function<void(const QString &)> onErr);
    void finalizeCreate();
    void resolveJava(std::function<void(const QString &)> cb);

    // 模组服流程：按普通服务器策略安装 Java + 逐个加载器安装 + 完成后整理目录
    void startModCreate();
    void processNextLoader();
    // 启动某个加载器 installer 的下载（主源失败时由 onDownloadError 用镜像兜底重试）
    void startLoaderDownload(const QString &loader, const QString &url);
    void runModInstaller(const QString &loader, bool useMirror = false);
    void onModInstallerFinished(const QString &loader, int exitCode, QProcess::ExitStatus status);
    void finalizeModCreate();
    // ensureNeoForgeJar 补齐核心 jar 完成后执行的收尾（复制目录、写 eula、加入列表）
    void finalizeModCreateAfterNeoforge();
    // 安装完成后删除 Forge/NeoForge/Fabric 安装器额外生成的启动脚本/元数据（仅手动双击脚本时用到），
    // MSM 以 `java -jar server.jar` 从目录启动，这些文件无用，移除以让目录呈现为单个 server.jar。
    void cleanupInstallerLeftovers();
    // 补齐 NeoForge 26.x+ 安装器漏放的核心 jar（neoforge-{ver}.jar）。
    // 新版（beta 分支）安装器仅跑 EXTRACT_FILES + PROCESS_MINECRAFT_JAR 两个 processor，
    // 不会把 universal jar 放置为 libraries/net/neoforged/neoforge/{ver}/neoforge-{ver}.jar，
    // 导致 FML 启动报 "The NeoForge jar is missing"。这里扫描生成的 neoForge 目录，
    // 若该 jar 缺失则异步从 BMCLAPI/官方 maven 下载 universal jar 放置为它（win_args.txt 的
    // -classpath 引用的正是这个文件名），下载完成后再执行收尾回调。
    void ensureNeoForgeJar(const QString &dir, std::function<void()> done);
    // 同步下载文件（阻塞，带超时），用于安装器未产出 universal 时补齐标记 jar。
    bool downloadFileSync(const QString &url, const QString &destPath);
    // 默认服务器命名：类型 + 版本 +（模组服）加载器类型
    QString defaultName() const;
    void refreshSaveDir();
    void regenerateNameIfAuto();
    QString findLoaderJar(const QString &dir, const QString &loader) const;

    void startInstallerProcess(const QString &java, const QString &loader, bool useMirror = false);
    void onInstallerFinished(int exitCode, QProcess::ExitStatus status);
    QString findJava() const;

    // 模组服安装工作目录：位于 MSM 程序同目录下的 Temp/<标识>（纯 ASCII，避免含中文的
    // saveDir 在 C locale 的 java 子进程下路径解码成 '?' 而失败）。安装器在此目录运行并
    // 生成核心，所有加载器装完后整体复制到 saveDir。
    QString modsTempDir() const;
    static bool copyDirRecursive(const QString &src, const QString &dst);

    // 把系统代理转成 JVM 参数，供独立的 java 安装器进程使用。
    // 主程序通过 Qt 使用系统代理（setUseSystemConfiguration），但派生的 java 进程不会继承，
    // 导致安装器下载 Mojang / NeoForge 源时被墙/超时而退出码 1。
    QStringList systemProxyJvmArgs() const;
    // 判断安装器日志是否表现为“网络下载失败”（超时 / 库下载失败），用于触发 BMCLAPI 镜像重试。
    bool installerFailedByNetwork() const;
    // 截取安装器输出末尾，便于在失败时展示真实原因（而非笼统的“请检查网络后重试”）
    QString installerErrorTail() const;

    // 压缩包导入的内部实现：探测版本/类型、解压、写 eula、加入服务器列表
    void importZipCore(const QString &zipPath, const QString &name,
                       const QString &saveDir, const QString &java);
    QString findServerJar(const QString &dir, const QString &type) const;

    void onDownloadProgress(const QString &id, qreal percent, qint64, qint64);
    void onDownloadFinished(const QString &id, const QString &path);
    void onDownloadError(const QString &id, const QString &message);

    DownloadManager *m_dm = nullptr;
    ServerManager *m_sm = nullptr;
    JavaManager *m_java = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    QProcess *m_installer = nullptr;
    QString m_resolvedJava;

    QStringList m_versions;
    QString m_currentType = QStringLiteral("Paper");
    QString m_currentVersion;
    QString m_name;
    QString m_saveDir;
    int m_minMemory = 2048;
    int m_maxMemory = 2048;
    bool m_userSetDir = false;
    bool m_userSetName = false;
    QString m_baseName;        // 自动生成的类型-版本部分（用于保存目录）
    bool m_eulaAccepted = false;
    bool m_busy = false;
    bool m_done = false;
    QString m_status;
    qreal m_progress = 0;
    QString m_taskId;

    // 分阶段进度状态
    bool m_useStages = false;
    QVector<qreal> m_stageWeights;
    qreal m_totalWeight = 0;
    int m_stageIdx = 0;
    qreal m_stageProg = 0;
    QString m_stageText;
    // 安装器运行期间进度不可读：用缓慢爬升动画表示“进行中”
    QTimer *m_installTimer = nullptr;
    qreal m_installAnim = 0;

    // 模组服状态
    QStringList m_selectedLoaders;        // 用户勾选的加载器
    QStringList m_loaderQueue;            // 待安装队列
    QString m_activeLoader;               // 当前正在安装的加载器
    QString m_loaderFallbackUrl;          // 当前加载器下载失败时的镜像兜底地址
    QString m_serverFallbackUrl;          // Vanilla/Paper 主源下载失败时的镜像兜底地址
    QHash<QString, QString> m_loaderByTask; // 下载任务 id -> 加载器
    QString m_modsTemp;                   // 模组服安装工作目录（系统临时目录 Temp/MSM/<标识>，纯 ASCII）
    int m_loaderTotal = 0;                // 总加载器数（用于进度）
    int m_loaderDone = 0;                 // 已完成数

    QString m_installerLog;               // 安装器 stdout/stderr 累积（失败时取末尾展示）
    int m_installAttempt = 0;              // 当前加载器安装重试次数（0=首次不使用镜像，>=1=已切 BMCLAPI 镜像重试）
    bool m_listServer = true;              // 完成时是否加入服务器列表（下载中心“仅准备目录”时为 false）
};
