/*
 * proxymanager.cpp —— P2 多代理列表模型实现
 */
#include "proxymanager.h"
#include "proxycontroller.h"

#include <QSettings>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>

ProxyManager::ProxyManager(ServerManager *sm, ServerController *sc,
                           JavaManager *java, QObject *parent)
    : QObject(parent), m_sm(sm), m_sc(sc), m_java(java)
{
    // 索引 0：默认实例（空 id，兼容旧版单代理的目录与设置）
    m_list << createInstance(QString());
    // 其余实例：按持久化清单恢复
    m_ids = QSettings().value(QStringLiteral("proxy/instances")).toStringList();
    for (const QString &id : std::as_const(m_ids))
        m_list << createInstance(id);
}

// 构建实例并挂接摘要刷新信号（QObject 父子关系保证 QML 不会回收）
ProxyController *ProxyManager::createInstance(const QString &id)
{
    auto *p = new ProxyController(m_sm, m_sc, m_java, id, this);
    connect(p, &ProxyController::runningChanged, this, &ProxyManager::proxiesChanged);
    connect(p, &ProxyController::statusChanged, this, &ProxyManager::proxiesChanged);
    connect(p, &ProxyController::nameChanged, this, &ProxyManager::proxiesChanged);
    connect(p, &ProxyController::proxyPortChanged, this, &ProxyManager::proxiesChanged);
    connect(p, &ProxyController::playerCountChanged, this, &ProxyManager::proxiesChanged);
    return p;
}

QVariantList ProxyManager::proxies() const
{
    QVariantList out;
    for (int i = 0; i < m_list.size(); ++i) {
        const ProxyController *p = m_list.at(i);
        QVariantMap m;
        m[QStringLiteral("index")] = i;
        m[QStringLiteral("id")] = p->instanceId();
        m[QStringLiteral("name")] = p->name();
        m[QStringLiteral("port")] = p->proxyPort();
        m[QStringLiteral("running")] = p->running();
        m[QStringLiteral("status")] = p->status();
        m[QStringLiteral("players")] = p->playerCount();
        m[QStringLiteral("isDefault")] = p->instanceId().isEmpty();
        out << m;
    }
    return out;
}

ProxyController *ProxyManager::proxyAt(int index) const
{
    return m_list.value(index, nullptr);
}

int ProxyManager::addProxy(const QString &name)
{
    // 生成不重复的短 id（时间戳 36 进制）
    QString id = QStringLiteral("p") + QString::number(
                     QDateTime::currentMSecsSinceEpoch(), 36);
    while (m_ids.contains(id))
        id += QLatin1Char('x');

    ProxyController *p = createInstance(id);
    if (!name.trimmed().isEmpty())
        p->setName(name.trimmed());
    // 默认端口：现有最大端口 +1，避免冲突
    int maxPort = 0;
    for (const ProxyController *e : std::as_const(m_list))
        maxPort = qMax(maxPort, e->proxyPort());
    p->setProxyPort(qMin(65535, maxPort + 1));

    m_ids << id;
    m_list << p;
    saveIds();
    emit proxiesChanged();
    return m_list.size() - 1;
}

bool ProxyManager::removeProxy(int index)
{
    // 默认实例与运行中的实例不可删除
    if (index <= 0 || index >= m_list.size())
        return false;
    ProxyController *p = m_list.at(index);
    if (p->running())
        return false;

    const QString id = p->instanceId();
    m_list.removeAt(index);
    m_ids.removeAll(id);
    saveIds();

    // 清理该实例的设置与数据目录
    QSettings().remove(QStringLiteral("proxy/") + id);
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                        + QStringLiteral("/MSM/Velocity/instances/") + id;
    QDir(dir).removeRecursively();

    p->deleteLater();
    emit proxiesChanged();
    return true;
}

void ProxyManager::saveIds() const
{
    QSettings().setValue(QStringLiteral("proxy/instances"), m_ids);
}
