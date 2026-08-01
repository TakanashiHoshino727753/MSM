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
#include <QUuid>
#include <QDebug>

// 单个被管理的服务器（数据行）。属性变化通过 NOTIFY 信号驱动 QML 绑定。
class Server : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(QString version READ version WRITE setVersion NOTIFY versionChanged)
    Q_PROPERTY(QString type READ type WRITE setType NOTIFY typeChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(QString id READ id WRITE setId NOTIFY idChanged)
public:
    explicit Server(QObject *parent = nullptr) : QObject(parent) {}
    Server(const QString &name, const QString &version, const QString &type,
           const QString &path, const QString &id = QString(), QObject *parent = nullptr)
        : QObject(parent), m_name(name), m_version(version), m_type(type), m_path(path), m_id(id) {}

    QString name() const { return m_name; }
    void setName(const QString &v) { if (m_name != v) { m_name = v; emit nameChanged(); } }
    QString version() const { return m_version; }
    void setVersion(const QString &v) { if (m_version != v) { m_version = v; emit versionChanged(); } }
    QString type() const { return m_type; }
    void setType(const QString &v) { if (m_type != v) { m_type = v; emit typeChanged(); } }
    QString path() const { return m_path; }
    void setPath(const QString &v) { if (m_path != v) { m_path = v; emit pathChanged(); } }
    QString id() const { return m_id; }
    void setId(const QString &v) { if (m_id != v) { m_id = v; emit idChanged(); } }
signals:
    void nameChanged();
    void versionChanged();
    void typeChanged();
    void pathChanged();
    void idChanged();
private:
    QString m_name, m_version, m_type, m_path, m_id;
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
        // 无论来自加载还是扫描，均按物理路径清洗一次重复登记
        dedupeByPath();
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
            m[QStringLiteral("id")] = s->id();
            out << m;
        }
        return out;
    }
    Q_INVOKABLE Server *serverByName(const QString &name) {
        for (Server *s : m_servers)
            if (s->name() == name) return s;
        return nullptr;
    }
    Q_INVOKABLE int indexOfServer(const QString &name) const {
        for (int i = 0; i < m_servers.size(); ++i)
            if (m_servers.at(i)->name() == name) return i;
        return -1;
    }

    // 为某服务器目录生成/读取稳定的唯一标识，写入目录内的 .msm/id 文件。
    // 即使服务器重名，只要目录不同，id 就不同，从而把"多个服务器"正确区分。
    Q_INVOKABLE QString ensureServerId(const QString &path) {
        const QString dir = QDir(path).absolutePath();
        if (dir.isEmpty() || !QDir().exists(dir))
            return QString();
        const QString msmDir = dir + QStringLiteral("/.msm");
        QDir().mkpath(msmDir);
        const QString idFile = msmDir + QStringLiteral("/id");
        QFile f(idFile);
        if (f.open(QIODevice::ReadOnly)) {
            const QString existing = QString::fromUtf8(f.readAll()).trimmed();
            f.close();
            if (!existing.isEmpty())
                return existing;
        }
        const QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(newId.toUtf8());
            f.close();
        }
        return newId;
    }

    // 路径归一化 + 大小写无关比较：Windows 上 servers.json 存储的路径与运行时扫描到的
    // 路径可能因大小写/斜杠差异被当成不同目录，导致同一个服务器被重复识别。统一用
    // QDir::cleanPath + 小写比较，避免这种重复。
    static bool samePath(const QString &a, const QString &b) {
        if (a.isEmpty() || b.isEmpty())
            return false;
        return QDir::cleanPath(QDir(a).absolutePath()).toLower()
               == QDir::cleanPath(QDir(b).absolutePath()).toLower();
    }
    bool hasServerPath(const QString &path) const {
        for (Server *s : m_servers)
            if (samePath(s->path(), path))
                return true;
        return false;
    }

    Q_INVOKABLE void addServer(const QString &name, const QString &version,
                               const QString &type = QString(),
                               const QString &path = QString(),
                               const QString &id = QString()) {
        // 去重：同一物理目录只登记一次（大小写/斜杠无关），避免单服务器被识别成多个
        const QString norm = QDir::cleanPath(QDir(path).absolutePath());
        if (hasServerPath(norm))
            return;
        const QString realId = id.isEmpty() ? ensureServerId(norm) : id;
        auto *s = new Server(name, version, type, norm, realId, this);
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
        // 只以文档根目录为扫描根（msmDir 是其子目录，递归必覆盖），避免根目录重叠
        // 导致同一目录被两次遍历而依赖去重逻辑。
        QStringList roots = {docDir};
        // 刷新时先移除磁盘上已不存在的已登记服务器（点“刷新服务器”应能反映删除）。
        QList<Server *> stale;
        for (Server *s : m_servers)
            if (!QDir(s->path()).exists())
                stale.append(s);
        for (Server *s : stale) {
            qDebug() << "[ServerManager] 刷新移除失效服务器" << s->name() << s->path();
            m_servers.removeAll(s);
            s->deleteLater();
        }
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
                const QString absPath = QDir::cleanPath(dir.absolutePath());
                // 跳过已录入的（大小写/斜杠无关比较）
                if (hasServerPath(absPath))
                    continue;

                // 跳过“嵌套在另一个服务器目录内”的目录（如 backups/worlds/解包临时目录也含
                // server.properties），否则单个服务器会被识别成多个。仅把“最外层含
                // server.properties 的目录”当作服务器根目录。
                {
                    QDir cur = dir;
                    bool nested = false;
                    while (cur.cdUp()) {
                        if (cur.exists(QStringLiteral("server.properties"))) {
                            nested = true;
                            break;
                        }
                    }
                    if (nested) continue;
                }

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

                addServer(type + QStringLiteral("-") + version, version, type, absPath,
                          ensureServerId(absPath));
                added++;
            }
        }
        if (added > 0) {
            qDebug() << "[ServerManager] 扫描发现" << added << "台服务器";
        }
        dedupeByPath();
    }

    // 按物理路径去重：扫描（或 JSON 加载）可能因大小写/斜杠差异、根目录重叠遍历、
    // 或持久化脏数据导致同一服务器被登记多次。统一用 samePath 清洗，只保留首条。
    void dedupeByPath() {
        QList<Server *> unique;
        for (Server *s : m_servers) {
            bool dup = false;
            for (Server *u : unique)
                if (samePath(u->path(), s->path())) { dup = true; break; }
            if (dup) {
                qDebug() << "[ServerManager] 去重：丢弃重复服务器" << s->name() << s->path();
                s->deleteLater();
            } else {
                unique.append(s);
            }
        }
        if (unique.size() != m_servers.size()) {
            m_servers = unique;
            saveServers();
        }
        // 每次扫描/加载后都通知视图刷新（即使数量未变，也可能有条目增删或失效清理）。
        emit serversChanged();
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
            obj[QStringLiteral("id")] = s->id();
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
            // id 优先取持久化值；若目录内 .msm/id 存在则以目录内为准（跨设备迁移/重建时保持一致）
            QString id = obj[QStringLiteral("id")].toString();
            const QString dirId = ensureServerId(path);
            if (id.isEmpty()) id = dirId;
            auto *s = new Server(
                obj[QStringLiteral("name")].toString(),
                obj[QStringLiteral("version")].toString(),
                obj[QStringLiteral("type")].toString(),
                path,
                id,
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
