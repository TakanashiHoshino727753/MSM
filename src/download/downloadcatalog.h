#pragma once
#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QList>
#include <QStringList>
#include <QHash>
#include <QUrl>
#include <functional>

class DownloadManager;
class DownloadItem;
class JavaManager;
class QNetworkAccessManager;
class QNetworkReply;

// 单个可下载条目（数据模型行）。所有“状态”变化通过 stateChanged 通知，
// 由 DownloadCatalog 转发为 model 的 dataChanged，从而驱动 QML 列表刷新。
class DownloadItem : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString subtitle READ subtitle CONSTANT)
    Q_PROPERTY(QString url READ url CONSTANT)
    Q_PROPERTY(QString filename READ filename CONSTANT)
    Q_PROPERTY(QString modrinthId READ modrinthId CONSTANT)
    Q_PROPERTY(QString taskId READ taskId NOTIFY stateChanged)
    Q_PROPERTY(bool downloading READ downloading NOTIFY stateChanged)
    Q_PROPERTY(qreal percent READ percent NOTIFY stateChanged)
    Q_PROPERTY(bool done READ done NOTIFY stateChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
public:
    explicit DownloadItem(const QString &title, const QString &subtitle,
                          const QString &url, const QString &filename,
                          const QString &modrinthId = QString(), QObject *parent = nullptr)
        : QObject(parent), m_title(title), m_subtitle(subtitle), m_url(url),
          m_filename(filename), m_modrinthId(modrinthId) {}

    QString title() const { return m_title; }
    QString subtitle() const { return m_subtitle; }
    QString url() const { return m_url; }
    QString filename() const { return m_filename; }
    QString modrinthId() const { return m_modrinthId; }
    QString taskId() const { return m_taskId; }
    bool downloading() const { return m_downloading; }
    qreal percent() const { return m_percent; }
    bool done() const { return m_done; }
    QString filePath() const { return m_filePath; }
    QString errorText() const { return m_errorText; }

    void setTaskId(const QString &id) { m_taskId = id; emit stateChanged(); }
    void setDownloading(bool b) { m_downloading = b; emit stateChanged(); }
    void setPercent(qreal p) { m_percent = p; emit stateChanged(); }
    void setDone(bool b) { m_done = b; emit stateChanged(); }
    void setFilePath(const QString &p) { m_filePath = p; emit stateChanged(); }
    void setErrorText(const QString &t) { m_errorText = t; emit stateChanged(); }

signals:
    void stateChanged();

private:
    QString m_title, m_subtitle, m_url, m_filename, m_modrinthId, m_taskId, m_errorText, m_filePath;
    bool m_downloading = false;
    qreal m_percent = 0;
    bool m_done = false;
};

// 下载中心数据/逻辑层（C++）。负责：分类列表拉取与解析（Java / 服务端 / Modrinth）、
// 下载触发与进度联动、搜索。QML 只负责把本模型的角色绑定到界面。
class DownloadCatalog : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString currentKey READ currentKey WRITE setCurrentKey NOTIFY currentKeyChanged)
    Q_PROPERTY(QString serverType READ serverType WRITE setServerType NOTIFY serverTypeChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
    Q_PROPERTY(QString saveDir READ saveDir WRITE setSaveDir NOTIFY saveDirChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // 是否存在尚未清理的临时 Java 目录（供 QML 决定是否显示“清理临时 Java”按钮）
    Q_PROPERTY(bool hasTempJava READ hasTempJava NOTIFY hasTempJavaChanged)
    // 模组服（多加载器）相关状态：MC 版本、已勾选加载器、可选项版本列表
    Q_PROPERTY(QString modVersion READ modVersion WRITE setModVersion NOTIFY modVersionChanged)
    Q_PROPERTY(QStringList selectedLoaders READ selectedLoaders WRITE setSelectedLoaders NOTIFY selectedLoadersChanged)
    Q_PROPERTY(QStringList mcReleases READ mcReleases NOTIFY mcReleasesChanged)
    // 模组服子项（Forge/Fabric/NeoForge）当前选中的加载器；每个加载器对应一个独立服务端实例
    Q_PROPERTY(QString modLoaderType READ modLoaderType WRITE setModLoaderType NOTIFY modLoaderTypeChanged)

public:
    enum Role {
        TitleRole = Qt::UserRole + 1,
        SubtitleRole,
        UrlRole,
        FilenameRole,
        ModrinthIdRole,
        TaskIdRole,
        DownloadingRole,
        PercentRole,
        DoneRole,
        FilePathRole,
        ErrorRole
    };

    explicit DownloadCatalog(DownloadManager *dm, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_items.size(); }
    bool hasTempJava() const;


    Q_INVOKABLE void refresh();
    Q_INVOKABLE void download(int index);
    Q_INVOKABLE void cancel(int index);
    // 按列表索引暂停 / 继续下载（与本地端下载列表的暂停、继续一致）
    Q_INVOKABLE void pause(int index);
    Q_INVOKABLE void resume(int index);
    Q_INVOKABLE void openFile(int index);
    Q_INVOKABLE QVariantList items() const;

    QString status() const;
    Q_INVOKABLE void setStatus(const QString &s);
    // 界面语言（"简体中文" / "English"），由 QML 绑定 I18n.lang；变化时状态栏实时重译
    Q_PROPERTY(QString language READ language WRITE setLanguage NOTIFY languageChanged)
    QString language() const { return m_language; }
    void setLanguage(const QString &v);
    bool loading() const { return m_loading; }
    QString currentKey() const { return m_currentKey; }
    void setCurrentKey(const QString &k);
    QString serverType() const { return m_serverType; }
    void setServerType(const QString &t);
    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &t);
    QString saveDir() const { return m_saveDir; }
    void setSaveDir(const QString &d);
    void setJavaManager(JavaManager *j);
    // 注入“打包器”委托：下载中心“下载并打包选中加载器”不再自己下载安装器，而是调用
    // 此委托。由上层（webui/本地）注入复用 CreateServerController（setSkipAddList=true）的
    // 打包逻辑，与“创建服务器”完全一致，确保打出的包已准备 Java + 运行安装器，可直接运行。
    void setPackager(const std::function<void()> &f);

    // 模组服（多加载器）
    QString modVersion() const { return m_modVersion; }
    void setModVersion(const QString &v);
    QStringList selectedLoaders() const { return m_modLoaders; }
    void setSelectedLoaders(const QStringList &v);
    QStringList mcReleases() const { return m_mcReleases; }
    QString modLoaderType() const { return m_modLoaderType; }
    void setModLoaderType(const QString &t);

    Q_INVOKABLE QStringList modLoaders() const;
    Q_INVOKABLE QString loaderLabel(const QString &loader) const;
    Q_INVOKABLE bool loaderCompatible(const QString &loader, const QString &version) const;
    Q_INVOKABLE void toggleLoader(const QString &loader);
    Q_INVOKABLE void downloadSelectedLoaders();
    // 单个加载器：下载安装器到下载目录（安装到服务端由应用层安装协调器负责）
    Q_INVOKABLE void downloadLoader(const QString &loader);
    Q_INVOKABLE void cleanupTempJava();

signals:
    void statusChanged();
    void languageChanged();
    void loadingChanged();
    void currentKeyChanged();
    void serverTypeChanged();
    void searchTextChanged();
    void saveDirChanged();
    void countChanged();
    void hasTempJavaChanged();
    void modVersionChanged();
    void selectedLoadersChanged();
    void modLoaderTypeChanged();
    void mcReleasesChanged();

private:
    void setLoading(bool b);
    void clearItems();
    void appendItem(DownloadItem *it);

    void loadJava();
    void loadServer();
    void loadModrinth();
    void loadPaper();
    void loadVanilla();
    void loadMod();

    void fetchJson(const QString &url,
                   std::function<void(const QJsonDocument &)> onOk,
                   std::function<void(const QString &)> onErr);

    // 按当前语言返回译文（简体中文原样返回）
    QString ts(const QString &zh) const;
    void fetchText(const QString &url,
                   std::function<void(const QString &)> onOk,
                   std::function<void(const QString &)> onErr);
    void fetchMcReleases(std::function<void(const QStringList &)> cb);
    void resolveVanillaUrl(const QString &version, std::function<void(const QString &)> cb);
    void resolveLoaderUrl(const QString &loader, const QString &version,
                          std::function<void(const QString &)> cb);

    void startDownload(DownloadItem *it, const QString &url, const QString &filename);
    DownloadItem *findByTaskId(const QString &id) const;

    void onDownloadProgress(const QString &id, qreal percent, qint64, qint64);
    void onDownloadFinished(const QString &id, const QString &path);
    void onDownloadError(const QString &id, const QString &message);

    // 依次尝试多个镜像源，直到某个返回可解析的 JSON（Modrinth：国内 MCIM 优先，回退官方）
    void fetchFirst(const QStringList &urls,
                    const std::function<void(const QJsonDocument &)> &onOk,
                    const std::function<void(const QString &)> &onErr,
                    int idx = 0);
    static QStringList modrinthBases();

    JavaManager *m_java = nullptr;
    DownloadManager *m_dm = nullptr;
    std::function<void()> m_packager;   // 注入的打包委托（复用创建服务器引擎）
    QNetworkAccessManager *m_nam = nullptr;
    QList<DownloadItem *> m_items;
    QString m_statusRaw;
    QString m_language = QStringLiteral("简体中文");
    bool m_loading = false;
    int m_inflight = 0;
    QString m_currentKey = QStringLiteral("java");
    QString m_serverType = QStringLiteral("paper");
    QString m_searchText;
    QString m_saveDir;

    // 模组服（多加载器）状态
    QString m_modVersion;
    QStringList m_modLoaders;
    QString m_modLoaderType;       // 当前选中的子项加载器（forge/fabric/neoforge）
    QStringList m_mcReleases;
    bool m_mcReleasesFetching = false;
};
