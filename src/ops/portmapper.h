/*
 * portmapper.h —— A3 公网暴露（UPnP IGD 端口映射，实验性）
 * -------------------------------------------------
 * 纯 Qt 实现，无第三方 native 依赖：
 *  - SSDP 发现：QUdpSocket 组播 M-SEARCH（239.255.255.250:1900）
 *  - 描述解析：QNetworkAccessManager 拉取设备 XML，QXmlStreamReader
 *    找 WANIPConnection/WANPPPConnection 的 controlURL
 *  - 控制操作：SOAP POST（AddPortMapping / DeletePortMapping /
 *    GetExternalIPAddress）
 * 注意：需要路由器开启 UPnP；运营商级 NAT（CG-NAT）下映射成功
 * 也可能无法从公网访问 —— 状态文案会给出提示。
 */
#pragma once

#include <QObject>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QNetworkAccessManager>
#include <functional>

class QUdpSocket;

class PortMapper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)     // 已找到可用网关
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(QString externalIp READ externalIp NOTIFY externalIpChanged)
    Q_PROPERTY(QString gatewayName READ gatewayName NOTIFY availableChanged)
    Q_PROPERTY(QString lanIp READ lanIp NOTIFY availableChanged)
    Q_PROPERTY(QVariantList mappings READ mappings NOTIFY mappingsChanged) // 本次会话添加的映射
public:
    explicit PortMapper(QObject *parent = nullptr);

    bool available() const { return !m_controlUrl.isEmpty(); }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QString externalIp() const { return m_externalIp; }
    QString gatewayName() const { return m_gatewayName; }
    QString lanIp() const { return m_lanIp; }
    QVariantList mappings() const { return m_mappings; }

    Q_INVOKABLE void discover();                                  // 发现网关
    Q_INVOKABLE void addMapping(int externalPort, int internalPort,
                                const QString &protocol = QStringLiteral("TCP"),
                                const QString &description = QStringLiteral("MSM"));
    Q_INVOKABLE void removeMapping(int externalPort,
                                   const QString &protocol = QStringLiteral("TCP"));
    Q_INVOKABLE void refreshExternalIp();

signals:
    void availableChanged();
    void busyChanged();
    void statusChanged();
    void externalIpChanged();
    void mappingsChanged();
    void mappingResult(bool ok, const QString &message);          // 供 UI toast

private:
    void setStatus(const QString &s);
    void setBusy(bool b);
    void onSsdpReadyRead();
    void fetchDescription(const QUrl &location);                  // 拉设备描述 XML
    void soapRequest(const QString &action, const QString &argsXml,
                     std::function<void(bool, const QString &)> done);
    static QString detectLanIp(const QString &gatewayHost);       // 探测到达网关所用本机 IP

    QNetworkAccessManager m_nam;
    QUdpSocket *m_ssdp = nullptr;
    bool m_busy = false;
    QString m_status = QStringLiteral("未发现网关，点击\u201c发现网关\u201d开始");
    QString m_externalIp;
    QString m_gatewayName;
    QString m_lanIp;
    QUrl m_controlUrl;         // WANIPConnection 控制端点
    QString m_serviceType;     // 实际匹配到的 serviceType（IP/PPP Connection）
    QVariantList m_mappings;
    bool m_descPending = false; // 防止多个 SSDP 应答重复拉取描述
};
