#pragma once
#include <QObject>
#include <QString>
#include <QList>
#include <QQmlListProperty>
#include <QDir>
#include <QDirIterator>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QDebug>

// 单个被管理的服务器（数据行）。属性变化通过 NOTIFY 信号驱动 QML 绑定。
class Server : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
public:
    explicit Server(QObject *parent = nullptr) : QObject(parent) {}
    Server(const QString &name, const QString &version, const QString &type,
           const QString &path, QObject *parent = nullptr)
        : QObject(parent), m_name(name), m_version(version), m_type(type), m_path(path) {}

    QString name() const { return m_name; }
    void setName(const QString &v) { if (m_name != v) { m_name = v; emit nameChanged(); } }
    QString version() const { return m_version; }
    void setVersion(const QString &v) { if (m_version != v) { m_version = v; emit versionChanged(); } }
    QString type() const { return m_type; }
    void setType(const QString &v) { if (m_type != v) { m_type = v; emit typeChanged(); } }
    QString path() const { return m_path; }
    void setPath(const QString &v) { if (m_path != v) { m_path = v; emit pathChanged(); } }
signals:
    void nameChanged();
    void versionChanged();
    void typeChanged();
    void pathChanged();
private:
    QString m_name, m_version, m_type, m_path;
};

// 服务器集合（C++ 逻辑层）：提供 QML 列表并支持持久化、扫描文档文件夹。
class ServerManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QQmlListProperty<Server> servers READ servers NOTIFY serversChanged)
    Q_PROPERTY(int count READ count NOTIFY serversChanged)
public:
    explicit ServerManager(QObject *parent = nullptr) : QObject(parent) {
        // 不再硬编码示例服务器，改从持久化加载
        loadServers();
        // 第一次使用自动扫描文档文件夹
        if (m_servers.isEmpty())
            scanServers();
    }

    QQmlListProperty<Server> servers() {
        return QQmlListProperty<Server>(this, nullptr,
                                        &ServerManager::appendServer,
                                        &ServerManager::serverCount,
                                        &ServerManager::serverAt,
                                        &ServerManager::clearServers);
    }

    int count() const { return m_servers.size(); }

    Q_INVOKABLE QVariantList serverSummary() const {
        QVariantList out;
        for (Server *s : m_servers) {
            QVariantMap m;
            m[QStringLiteral("name")] = s->name();
            m[QStringLiteral("version")] = s->version();
            m[QStringLiteral("type")] = s->type();
            m[QStringLiteral("path")] = s->path();
            out << m;
        }
        return out;
    }
    Q_INVOKABLE Server *serverByName(const QString &name) {
        for (Server *s : m_servers)
            if (s->name() == name) return s;
        return nullptr;
    }

    Q_INVOKABLE void addServer(const QString &name, const QString &version,
                               const QString &type = QString(),
                               const QString &path = QString()) {
        auto *s = new Server(name, version, type, path, this);
        m_servers.append(s);
        saveServers();
        emit serversChanged();
    }
    Q_INVOKABLE void removeServer(int index) {
        if (index < 0 || index >= m_servers.size())
            return;
        m_servers.takeAt(index)->deleteLater();
        saveServers();
        emit serversChanged();
    }
    Q_INVOKABLE void clear() {
        qDeleteAll(m_servers);
        m_servers.clear();
        saveServers();
        emit serversChanged();
    }

    // 扫描"我的文档"和"文档/MSM"文件夹，自动发现 Minecraft 服务端并加入列表
    Q_INVOKABLE void scanServers() {
        const QString docDir = QStandardPaths::writableLocation(
            QStandardPaths::DocumentsLocation);
        const QString msmDir = docDir + QStringLiteral("/MSM");
        // 优先扫 MSM 子目录（创建服务器的默认位置），再扫文档根目录
        QStringList roots = {msmDir};
        if (msmDir != docDir)
            roots << docDir;
        int added = 0;
        for (const QString &root : roots) {
            QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                const QString dirPath = it.next();
                QDir dir(dirPath);
                // 判断是否为 Minecraft 服务器目录：含有 server.properties
                if (!dir.exists(QStringLiteral("server.properties")))
                    continue;
                const QString absPath = dir.absolutePath();
                // 跳过已录入的
                bool known = false;
                for (Server *s : m_servers) {
                    if (QDir(s->path()).absolutePath() == absPath) {
                        known = true;
                        break;
                    }
                }
                if (known) continue;

                // 名称 = 类型-版本（规范化命名，如 Paper-1.21.1）
                const QString folderName = dir.dirName();

                // 从文件夹名提取 Minecraft 版本号（第一个 X.Y / X.Y.Z 模式）
                static const QRegularExpression verRe(
                    QStringLiteral("(\\d+\\.\\d+(\\.\\d+)?)"),
                    QRegularExpression::CaseInsensitiveOption);
                QString version = QStringLiteral("unknown");
                auto verMatch = verRe.match(folderName);
                if (verMatch.hasMatch())
                    version = verMatch.captured(1);

                // 从文件夹名前缀判断服务端类型（优于 jar 文件名检测）
                QString type = QStringLiteral("vanilla");
                const QString fn = folderName.toLower();
                if (fn.startsWith(QStringLiteral("paper")))
                    type = QStringLiteral("paper");
                else if (fn.startsWith(QStringLiteral("fabric")))
                    type = QStringLiteral("fabric");
                else if (fn.startsWith(QStringLiteral("forge")) ||
                         fn.startsWith(QStringLiteral("minecraftforge")))
                    type = QStringLiteral("forge");
                else if (fn.startsWith(QStringLiteral("neoforge")))
                    type = QStringLiteral("neoforge");
                else {
                    // 后备：从 jar 文件名检测
                    const QStringList jars = dir.entryList({QStringLiteral("*.jar")}, QDir::Files);
                    for (const QString &j : jars) {
                        const QString lower = j.toLower();
                        if (lower.startsWith(QStringLiteral("paper"))) {
                            type = QStringLiteral("paper"); break;
                        }
                        if (lower.startsWith(QStringLiteral("fabric"))) {
                            type = QStringLiteral("fabric"); break;
                        }
                        if (lower.startsWith(QStringLiteral("forge")) ||
                            lower.startsWith(QStringLiteral("minecraftforge"))) {
                            type = QStringLiteral("forge"); break;
                        }
                        if (lower.startsWith(QStringLiteral("neoforge"))) {
                            type = QStringLiteral("neoforge"); break;
                        }
                    }
                }

                addServer(type + QStringLiteral("-") + version, version, type, absPath);
                added++;
            }
        }
        if (added > 0) {
            qDebug() << "[ServerManager] 扫描发现" << added << "台服务器";
        }
    }

signals:
    void serversChanged();

private:
    static QString configPath() {
        const QString dir = QStandardPaths::writableLocation(
            QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        return dir + QStringLiteral("/servers.json");
    }
    void saveServers() {
        QJsonArray arr;
        for (Server *s : m_servers) {
            QJsonObject obj;
            obj[QStringLiteral("name")] = s->name();
            obj[QStringLiteral("version")] = s->version();
            obj[QStringLiteral("type")] = s->type();
            obj[QStringLiteral("path")] = s->path();
            arr.append(obj);
        }
        QFile f(configPath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    }
    void loadServers() {
        QFile f(configPath());
        if (!f.open(QIODevice::ReadOnly))
            return;
        const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
        f.close();
        if (!doc.isArray())
            return;
        for (const QJsonValue &v : doc.array()) {
            const QJsonObject obj = v.toObject();
            const QString path = obj[QStringLiteral("path")].toString();
            // 跳过路径不在磁盘上的服务器（清理旧版硬编码示例、已删除的服务器）
            if (!QDir().exists(path))
                continue;
            auto *s = new Server(
                obj[QStringLiteral("name")].toString(),
                obj[QStringLiteral("version")].toString(),
                obj[QStringLiteral("type")].toString(),
                path,
                this);
            m_servers.append(s);
        }
        emit serversChanged();
    }

    static void appendServer(QQmlListProperty<Server> *prop, Server *s) {
        auto *self = qobject_cast<ServerManager *>(prop->object);
        if (self) { self->m_servers.append(s); self->saveServers(); emit self->serversChanged(); }
    }
    static long long serverCount(QQmlListProperty<Server> *prop) {
        return qobject_cast<ServerManager *>(prop->object)->m_servers.size();
    }
    static Server *serverAt(QQmlListProperty<Server> *prop, long long i) {
        return qobject_cast<ServerManager *>(prop->object)->m_servers.at(static_cast<int>(i));
    }
    static void clearServers(QQmlListProperty<Server> *prop) {
        auto *self = qobject_cast<ServerManager *>(prop->object);
        if (self) { qDeleteAll(self->m_servers); self->m_servers.clear(); self->saveServers(); emit self->serversChanged(); }
    }
    QList<Server *> m_servers;
};
