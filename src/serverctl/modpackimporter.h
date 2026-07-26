#pragma once
#include <QObject>
#include <QString>
#include <QProcess>

class DownloadManager;
class ServerManager;

// 导入整合包逻辑层（C++）：选择 .zip 整合包 -> 解压 -> 识别类型
// (CurseForge / Modrinth) -> 解析所需服务端加载器与游戏版本 -> 拉取服务端核心
// -> 写入 eula 并加入服务器列表。QML 只负责选择文件/目录并展示状态。
class ModpackImporter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString zipPath READ zipPath WRITE setZipPath NOTIFY zipPathChanged)
    Q_PROPERTY(QString targetDir READ targetDir WRITE setTargetDir NOTIFY targetDirChanged)
    Q_PROPERTY(QString modpackName READ modpackName NOTIFY modpackNameChanged)
    Q_PROPERTY(QString detectedType READ detectedType NOTIFY detectedTypeChanged)
    Q_PROPERTY(QString loader READ loader NOTIFY loaderChanged)
    Q_PROPERTY(QString gameVersion READ gameVersion NOTIFY gameVersionChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool done READ done NOTIFY doneChanged)
    Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)

public:
    explicit ModpackImporter(DownloadManager *dm, ServerManager *sm,
                             QObject *parent = nullptr);

    QString zipPath() const { return m_zipPath; }
    void setZipPath(const QString &v);
    QString targetDir() const { return m_targetDir; }
    void setTargetDir(const QString &v);
    QString modpackName() const { return m_name; }
    QString detectedType() const { return m_type; }
    QString loader() const { return m_loader; }
    QString gameVersion() const { return m_gameVersion; }
    QString statusText() const { return m_status; }
    bool busy() const { return m_busy; }
    bool done() const { return m_done; }
    qreal progress() const { return m_progress; }

    Q_INVOKABLE void reset();
    Q_INVOKABLE void import();

signals:
    void zipPathChanged();
    void targetDirChanged();
    void modpackNameChanged();
    void detectedTypeChanged();
    void loaderChanged();
    void gameVersionChanged();
    void statusTextChanged();
    void busyChanged();
    void doneChanged();
    void progressChanged();

private slots:
    void onExtractFinished(int exitCode, QProcess::ExitStatus status);

private:
    void setStatus(const QString &s);
    void setBusy(bool b);
    void setDone(bool d);
    void setProgress(qreal p);
    void parseManifest(const QString &dir);
    void downloadCore();
    void finishImport(bool coreOk);
    QString findFile(const QString &dir, const QString &fileName) const;

    void onDownloadProgress(const QString &id, qreal percent, qint64, qint64);
    void onDownloadFinished(const QString &id, const QString &path);
    void onDownloadError(const QString &id, const QString &message);

    DownloadManager *m_dm = nullptr;
    ServerManager *m_sm = nullptr;
    QString m_zipPath, m_targetDir, m_name, m_type, m_loader, m_loaderVersion, m_gameVersion, m_status;
    bool m_busy = false;
    bool m_done = false;
    qreal m_progress = 0;
    QString m_taskId;
    QProcess *m_proc = nullptr;
};
