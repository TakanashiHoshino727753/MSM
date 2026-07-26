#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QUrl>
#include <QSet>
#include <QList>
#include <QPair>
#include <QByteArray>
#include <functional>

class DownloadListModel;   // 显式依赖：Java 下载统一进入此列表（由构造注入，避免隐式 parent 取空）
class DownloadManager;     // 显式依赖：Java 下载统一经它下载（进度/暂停/取消/超时），并由此取得列表模型

class QNetworkAccessManager;
class QNetworkReply;

// 按 Minecraft 版本自动选择并安装合适的 Java（8/17/21…）。
// 优先使用系统 PATH 上匹配的 Java；否则从 Adoptium（Temurin）下载 JDK 安装程序（MSI）并安装，
// 仅在无安装包时退化为下载压缩包并解压到本地管理目录。
class JavaManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY errorTextChanged)
    // 是否存在尚未清理的临时 Java 目录（供 UI 决定是否显示“清理临时 Java”按钮）
    Q_PROPERTY(bool hasTempJava READ hasTempJava NOTIFY hasTempJavaChanged)

public:
    // dm：统一下载管理器（必填）。Java 下载统一经它下载，从而进入下载面板（并行/进度/暂停/取消），
    // 并复用其镜像选择与 90s 超时保护；由其还可取得统一下载列表模型。
    // 显式注入而非从 parent() 偷取，避免父对象缺失时静默失效。
    explicit JavaManager(DownloadManager *dm, QObject *parent = nullptr);
    // 析构时自动删除残留的临时 Java 目录，确保关闭应用后不留下环境改动。
    ~JavaManager() override;

    // 根据 MC 版本号返回所需 Java 特性版本（8 / 17 / 21 …）
    Q_INVOKABLE static int requiredFeature(const QString &mcVersion);

    // 同步查找：返回可直接使用的 java 可执行文件路径（已管理安装或 PATH 上匹配版本），否则空串
    Q_INVOKABLE QString javaPathFor(const QString &mcVersion);

    // 异步确保：若已可用立即回调；否则下载并解压后回调 (ok, path)
    void ensure(const QString &mcVersion, std::function<void(bool, QString)> cb);
    // 按 JDK 特性版本（如 8/11/17/21）直接确保；下载中心用此入口指定大版本
    void ensureFeature(int feature, std::function<void(bool, QString)> cb);

    // 临时准备一个可移植 JDK 到临时目录（不解压安装到系统、不登记），供安装器一次性使用；
    // 完成后务必调用 cleanupTemp() 删除临时目录，从而不留下环境改动。若已有可用 Java 则直接复用。
    void ensureTemp(int feature, std::function<void(bool, QString)> cb);
    void cleanupTemp();

    // 仅解析 Java 安装包直链（不下载、不安装），供下载中心"只下载原文件、不解压不安装"场景使用。
    // 成功回调 (true, url, 建议文件名)；失败则回调 (false, QString(), QString())，调用方仍可尝试下载。
    void resolveDownloadUrl(int feature, std::function<void(bool, QString, QString)> cb);

    // 覆盖默认安装根目录（默认 Downloads/jvm/）。由 CreateServerController 在创建服务器时
    // 设置为 "{服务器路径}/jvm"，让 Java 与服务端核心同目录、自带、可单独删除。空串恢复默认。
    Q_INVOKABLE void setInstallBase(const QString &path);
    Q_INVOKABLE QString installBase() const;

    // 供 WebUI 轮询当前状态（mcVersion 形如 "1.21.1"）
    Q_INVOKABLE QVariantMap statusFor(const QString &mcVersion);
    // 按 JDK 特性版本（如 8/17/21）查询状态，避免把纯数字误当作 MC 版本解析
    Q_INVOKABLE QVariantMap statusForFeature(int feature);

    // 手动指定 JDK 根目录（含 bin/java 的目录，或直接的 java.exe），避免在无网络时仍能创建服务器
    Q_INVOKABLE QString manualJavaHome() const;
    Q_INVOKABLE void setManualJavaHome(const QString &dir);

    bool busy() const;
    qreal progress() const;
    QString statusText() const;
    QString errorText() const;
    // 是否存在尚未清理的临时 Java 目录（下载中心 / 安装器用完即删后会变回 false）
    bool hasTempJava() const;

signals:
    void busyChanged();
    void progressChanged();
    void statusTextChanged();
    void errorTextChanged();
    void hasTempJavaChanged();
    void ready(int feature, QString path);
    // Java 路径经后台异步探测后解析出来（供需要实时状态的地方更新）
    void javaResolved(int feature, QString path);

private:
    QString managedDir() const;
    QString installedJava(int feature) const;       // 在管理目录中递归查找 java.exe
    // 探测手动目录 / PATH / JAVA_HOME / 常见安装位置，异步回调 (匹配到的 java 路径，空=未找到)
    void detectJava(int feature, std::function<void(const QString &)> cb);
    // 运行 -version 校验主版本是否匹配，异步回调 (验证通过返回 exe，否则空)
    void probeJava(const QString &exe, int feature, std::function<void(const QString &)> cb);
    int majorFromName(const QString &name) const;   // 从目录名推断 JDK 主版本
    QStringList commonJavaBases() const;            // 常见 JDK 安装根目录
    // 扫描常见安装目录 + Minecraft 运行时，异步回调 (匹配到的 java 路径，空=未找到)
    void scanCommonJava(int feature, std::function<void(const QString &)> cb);
    // 同步返回：优先缓存/已安装目录（毫秒级，绝不阻塞）；未知时触发后台异步探测并先返回空
    QString javaPathFor(int feature);
    // 异步权威解析：已安装/已缓存则立即回调，否则完整探测后回调（用于"是否需要下载"的判定）
    void javaPathForAsync(int feature, std::function<void(const QString &)> cb);
    // 后台异步刷新某 feature 的缓存（幂等：同一 feature 不重复探测）
    void refreshJavaCache(int feature);
    void download(int feature, std::function<void(bool, QString)> cb);
    // 解析指定 JDK 特性版本的 Windows x64 安装包直链：
    //   - 8/11：不使用甲骨文源（JDK 8 强制登录、JDK 11 otn 微下载易 SIGSEGV），改走 Adoptium(Temurin)。
    //          通过 resolveAdoptium() 查资产 API 拿到 MSI 文件名，拼出清华 TUNA 直连镜像下载（不走慢速 GitHub），
    //          返回 Windows MSI，由 launchInstaller 经 msiexec 直接安装（运行而非解压）。
    //   - 21/25/26… 使用甲骨文公开 latest 永久直链 download.oracle.com/java/{v}/latest/jdk-{v}_windows-x64_bin.exe
    //   - 17 抓取甲骨文官网归档页提取 exe 直链（公开 archive 路径，无需 otn cookie）
    // 同时把归档页里的 linux-x64_bin.tar.gz 链接缓存到 m_linuxLinks 备用（将来 Linux 部署）。
    // 返回 (国内镜像直链[始终为空，只走官网/Adoptium], 官方直链)
    void resolveInstaller(int feature, std::function<void(bool, QString, QString)> cb);
    // 解析 Adoptium(Temurin) 的 Windows x64 MSI 直链：先查资产 API 拿到 msi 文件名，
    // 再拼出清华 TUNA 的 Adoptium 直连镜像（不走 GitHub，快且稳），msi 由 launchInstaller 直接安装。
    // 成功回调 (true, tunaUrl)；失败回调 (false, QString())，由调用方回退。
    void resolveAdoptium(int feature, std::function<void(bool, QString)> cb);
    // 解析失败时的已知直链兜底
    QString knownOracleLink(int feature) const;
    // 下载安装包：.exe 则静默安装，.msi 则唤起安装向导。listId 为统一下载列表中的任务 ID。
    void fetchInstaller(int feature, const QUrl &url, const QString &listId, std::function<void(bool, QString)> cb);
    // 启动一次完整下载流程（解析源 + 按源顺序下载/重试 + 安装），listId 复用已有行或新建行均可。
    void startDownload(int feature, const QString &listId, std::function<void(bool, QString)> cb);
    // Adoptium latest 重定向兜底源地址
    QString latestRedirectUrl(int feature) const;
    // 下载面板"暂停/继续/重新下载"按钮对应的处理器（仅处理本管理器创建的 Java 行）
    void pause(const QString &id);
    void resume(const QString &id);
    void restart(const QString &id);
    // 收尾：从"正在下载"集合移除 feature 并更新 busy 状态（暂停/取消/重启流程完成时调用）
    void finalizeFeature(int feature);
    // 以管理员权限静默安装甲骨文 .exe / .msi 安装包到管理目录
    void launchInstaller(int feature, const QString &msiPath, std::function<void(bool)> done);
    void setBusy(bool b);
    void setProgressFor(int feature, qreal p);
    void setStatus(const QString &s);
    void setError(const QString &e);

    QNetworkAccessManager *m_nam = nullptr;
    DownloadManager *m_dm = nullptr;          // 统一下载管理器（构造注入，必填）
    DownloadListModel *m_list = nullptr;      // 统一下载列表（由 m_dm->downloadList() 取得）
    // ensureTemp 经 DownloadManager 下载时，DM 任务 id -> 下载完成后的回调整续（提取/校验/上报）
    QHash<QString, std::function<void(bool, QString)>> m_pending;
    bool m_busy = false;
    qreal m_progress = 0;                       // 总体进度（所有特性进度的平均，仅供整体展示）
    QHash<int, qreal> m_progressByFeature;       // 各特性独立进度，避免并行下载时互相覆盖导致进度条乱跳
    QString m_status;
    QString m_error;

    // Linux 版 JDK 直链缓存（备用，将来 Linux 部署使用），key=特性版本
    QMap<int, QString> m_linuxLinks;

    // 正在下载的 feature 集合（支持并行下载，去掉原串行 busy 队列）
    QSet<int> m_downloadingFeatures;
    // 统一下载列表的 listId -> 当前 reply，供取消时 abort
    QHash<QString, QNetworkReply *> m_javaReplies;
    // listId -> JDK 特性版本，供暂停/重启找回对应任务
    QHash<QString, int> m_javaFeature;
    // listId -> 当前下载源地址，供"继续"复用同一源并从断点续传
    QHash<QString, QUrl> m_javaUrl;
    // listId -> 已下载字节数（断点续传偏移）；暂停时即已落盘的部分文件大小
    QHash<QString, qint64> m_javaOffset;
    // listId -> 安装包总字节数（来自 Content-Length / Content-Range），用于进度与 Range 续传
    QHash<QString, qint64> m_javaTotal;
    // listId -> 用户主动暂停/取消标记；finished 时据此跳过重试/报错
    QHash<QString, bool> m_javaAborted;
    // listId -> 已暂停标记（继续按钮依据；finished 时据其区分暂停/取消）
    QHash<QString, bool> m_javaPaused;
    int m_javaSeq = 0;
    // Java 路径解析缓存：feature -> 已解析的 java 路径；后台异步探测填充，避免每次同步阻塞探测
    QHash<int, QString> m_javaCache;
    // 正在进行后台探测的 feature 集合（refreshJavaCache 幂等去重）
    QSet<int> m_detecting;

    // 临时 Java（ensureTemp/cleanupTemp 使用，安装器用完即删，不留下环境改动）
    QString m_tempJavaDir;

    // 当前 Java 安装根目录覆盖（空 = 用 managedDir() 默认 Downloads/jvm/）
    QString m_installBase;
    // 递归查找目录下 java[.exe]
    QString findExtractedJava(const QString &dir) const;
    // 解压 zip 到目标目录（通过 PowerShell Expand-Archive，纯解压、不安装）
    void extractZip(const QString &zip, const QString &dest, std::function<void(bool)> cb);
    // 下载文件到本地临时路径（跟随重定向），回调 (ok, error)。
    // extraHeaders：可选的额外请求头（如甲骨文直链所需的 Referer/Cookie）。
    // onProgress：可选进度回调 (已接收字节, 总字节；总字节未知时为 0)。
    // 内部使用系统 curl.exe 下载，以规避 Qt6+MinGW+Schannel 在下载上百 MB 的 JDK 压缩包时
    // 出现的卡死/崩溃（之前用 QNetworkAccessManager 直接拉会卡死）。
    void downloadToTemp(const QString &url, const QString &dest, std::function<void(bool, QString)> cb,
                        const QList<QPair<QByteArray, QByteArray>> &extraHeaders = {},
                        std::function<void(qint64, qint64)> onProgress = {});
    // DownloadManager 任务完成/出错回调（ensureTemp 路径）：据任务 id 取出 m_pending 并续跑
    void onDmFinished(const QString &id, const QString &path);
    void onDmError(const QString &id, const QString &msg);
    // 从 Adoptium 资产 JSON 中取出 JDK 的 zip 直链（无则回退第一个包链接）
    QString adoptiumZipLinkFor(int feature, const QJsonDocument &doc) const;
    // 甲骨文官方“latest”永久别名压缩包(zip)直链（无需登录，国内快）；不支持的版本返回空。
    QString oracleZipLinkFor(int feature) const;
};
