#pragma once

// 下载任务列表模型（低层、可独立复用）。
// 被下载管理器、Java 管理等模块共用，作为 UI 统一下载面板的唯一数据源。
// 设计为无外部依赖（仅 QtCore），以便于各业务模块单独引用而不产生循环依赖。
#include <QAbstractListModel>
#include <QList>
#include <QHash>
#include <QVariant>

class DownloadListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    enum State { StDownloading, StDone, StError, StCanceled, StPaused };
    enum Role { TitleRole = Qt::UserRole + 1, PercentRole, StateRole, TaskIdRole, ErrorRole, PathRole, UrlRole };

    explicit DownloadListModel(QObject *parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    { Q_UNUSED(parent); return m_rows.size(); }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
            return {};
        const Row &r = m_rows.at(index.row());
        switch (role) {
        case TitleRole:   return r.title;
        case PercentRole: return r.percent;
        case StateRole:   return stateString(r.state);
        case TaskIdRole:  return r.id;
        case ErrorRole:   return r.error;
        case PathRole:    return r.path;
        case UrlRole:     return r.url;
        }
        return {};
    }

    QHash<int, QByteArray> roleNames() const override
    {
        static const QHash<int, QByteArray> r = {
            {TitleRole, "title"}, {PercentRole, "percent"}, {StateRole, "state"},
            {TaskIdRole, "taskId"}, {ErrorRole, "errorText"}, {PathRole, "path"},
            {UrlRole, "url"}
        };
        return r;
    }

    int count() const { return m_rows.size(); }

    // 序列化为 JSON 友好的变体列表，供 WebUI 后端暴露“下载任务”列表
    Q_INVOKABLE QVariantList items() const
    {
        QVariantList out;
        for (const Row &r : m_rows) {
            QVariantMap m;
            m[QStringLiteral("title")] = r.title;
            m[QStringLiteral("percent")] = r.percent;
            m[QStringLiteral("state")] = stateString(r.state);
            m[QStringLiteral("errorText")] = r.error;
            m[QStringLiteral("path")] = r.path;
            m[QStringLiteral("taskId")] = r.id;
            out << m;
        }
        return out;
    }

    // 新增一行下载任务（标题 / 路径 / 下载地址）。url 用于“重新下载”时找回源地址。
    void add(const QString &id, const QString &title, const QString &path, const QString &url = QString())
    {
        beginInsertRows(QModelIndex(), m_rows.size(), m_rows.size());
        m_rows.append({id, title, QString(), path, url, 0, StDownloading});
        endInsertRows();
        emit countChanged();
    }

    // 更新进度（百分比 0~100）
    void setProgress(const QString &id, qreal p)
    {
        const int row = findRow(id);
        if (row < 0) return;
        m_rows[row].percent = p;
        emit dataChanged(index(row), index(row), {PercentRole});
    }

    // 更新任务状态（成功/失败/取消/暂停）
    void setState(const QString &id, State s, const QString &err = QString())
    {
        const int row = findRow(id);
        if (row < 0) return;
        m_rows[row].state = s;
        m_rows[row].error = err;
        if (s == StDone) m_rows[row].percent = 100;
        emit dataChanged(index(row), index(row));
    }

    Q_INVOKABLE void cancelAt(int row)
    {
        if (row < 0 || row >= m_rows.size()) return;
        emit cancelRequested(m_rows.at(row).id);
    }

    Q_INVOKABLE void pauseAt(int row)
    {
        if (row < 0 || row >= m_rows.size()) return;
        emit pauseRequested(m_rows.at(row).id);
    }

    Q_INVOKABLE void resumeAt(int row)
    {
        if (row < 0 || row >= m_rows.size()) return;
        emit resumeRequested(m_rows.at(row).id);
    }

    Q_INVOKABLE void restartAt(int row)
    {
        if (row < 0 || row >= m_rows.size()) return;
        emit restartRequested(m_rows.at(row).id);
    }

    Q_INVOKABLE void removeAt(int row)
    {
        if (row < 0 || row >= m_rows.size()) return;
        beginRemoveRows(QModelIndex(), row, row);
        m_rows.removeAt(row);
        endRemoveRows();
        emit countChanged();
    }

    Q_INVOKABLE void clearFinished()
    {
        for (int i = m_rows.size() - 1; i >= 0; --i) {
            const State s = m_rows.at(i).state;
            if (s == StDone || s == StError || s == StCanceled) {
                beginRemoveRows(QModelIndex(), i, i);
                m_rows.removeAt(i);
                endRemoveRows();
            }
        }
        emit countChanged();
    }

    // 供 DownloadManager 在“重新下载”时取回源地址 / 路径 / 标题
    QString urlForId(const QString &id) const
    { const int r = findRow(id); return r < 0 ? QString() : m_rows.at(r).url; }
    QString pathForId(const QString &id) const
    { const int r = findRow(id); return r < 0 ? QString() : m_rows.at(r).path; }
    QString titleForId(const QString &id) const
    { const int r = findRow(id); return r < 0 ? QString() : m_rows.at(r).title; }

signals:
    void cancelRequested(const QString &id);
    void pauseRequested(const QString &id);
    void resumeRequested(const QString &id);
    void restartRequested(const QString &id);
    void countChanged();

private:
    struct Row {
        QString id, title, error, path, url;
        qreal percent = 0;
        State state = StDownloading;
    };
    static QString stateString(State s)
    {
        switch (s) {
        case StDone:     return QStringLiteral("done");
        case StError:    return QStringLiteral("error");
        case StCanceled: return QStringLiteral("canceled");
        case StPaused:   return QStringLiteral("paused");
        default:         return QStringLiteral("downloading");
        }
    }
    int findRow(const QString &id) const
    {
        for (int i = 0; i < m_rows.size(); ++i)
            if (m_rows.at(i).id == id) return i;
        return -1;
    }
    QList<Row> m_rows;
};
