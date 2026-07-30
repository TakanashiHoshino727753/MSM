/*
 * proxymanager.h —— P2 多代理列表模型
 * -------------------------------------------------
 * 管理多个 ProxyController 实例：
 *  - 索引 0 恒为"默认实例"（instanceId 为空，目录/设置键与旧版单代理完全兼容）
 *  - 其余实例目录在 MSM/Velocity/instances/<id>，设置键前缀 proxy/<id>/
 *  - 实例 id 清单持久化于 QSettings("proxy/instances")
 *  - velocity.jar 由全部实例共享（下载一次即可）
 */
#pragma once

#include <QObject>
#include <QList>
#include <QVariantList>

class ProxyController;
class ServerManager;
class ServerController;
class JavaManager;

class ProxyManager : public QObject
{
    Q_OBJECT
    // 供 QML Repeater 使用的实例摘要列表
    Q_PROPERTY(QVariantList proxies READ proxies NOTIFY proxiesChanged)
    Q_PROPERTY(int count READ count NOTIFY proxiesChanged)
public:
    explicit ProxyManager(ServerManager *sm, ServerController *sc,
                          JavaManager *java = nullptr, QObject *parent = nullptr);

    QVariantList proxies() const;
    int count() const { return m_list.size(); }
    ProxyController *defaultProxy() const { return m_list.value(0); }

    Q_INVOKABLE ProxyController *proxyAt(int index) const;
    Q_INVOKABLE int addProxy(const QString &name);   // 返回新实例索引，失败 -1
    Q_INVOKABLE bool removeProxy(int index);         // 默认实例(0)与运行中实例不可删

signals:
    void proxiesChanged();

private:
    ProxyController *createInstance(const QString &id); // 构建并挂接信号
    void saveIds() const;

    ServerManager *m_sm = nullptr;
    ServerController *m_sc = nullptr;
    JavaManager *m_java = nullptr;
    QList<ProxyController *> m_list;
    QStringList m_ids; // 不含默认实例（默认实例 id 为空）
};
