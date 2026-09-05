/*
 * portmapper.cpp —— A3 公网暴露（UPnP IGD 端口映射，实验性）
 * 流程：SSDP M-SEARCH → 设备描述 XML → WANIPConnection controlURL
 *       → SOAP AddPortMapping / DeletePortMapping / GetExternalIPAddress
 */
#include "portmapper.h"
#include "httpclient.h"

#include <QUdpSocket>
#include <QHostAddress>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QXmlStreamReader>
#include <QRegularExpression>
#include <QTimer>

namespace {
const char *kSsdpAddr = "239.255.255.250";
const quint16 kSsdpPort = 1900;
// 依次尝试的服务类型（IGD1/IGD2、IP/PPP 拨号）
const QStringList kWanServices = {
    QStringLiteral("urn:schemas-upnp-org:service:WANIPConnection:1"),
    QStringLiteral("urn:schemas-upnp-org:service:WANIPConnection:2"),
    QStringLiteral("urn:schemas-upnp-org:service:WANPPPConnection:1"),
};
} // namespace

PortMapper::PortMapper(QObject *parent) : QObject(parent)
{
    m_nam.setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
}

void PortMapper::setStatus(const QString &s)
{
    if (s == m_status)
        return;
    m_status = s;
    emit statusChanged();
}

void PortMapper::setBusy(bool b)
{
    if (b == m_busy)
        return;
    m_busy = b;
    emit busyChanged();
}

// ---------- 第 1 步：SSDP 发现 ----------
void PortMapper::discover()
{
    if (m_busy)
        return;
    setBusy(true);
    m_descPending = false;
    m_controlUrl.clear();
    m_gatewayName.clear();
    emit availableChanged();
    setStatus(QStringLiteral("正在搜索支持 UPnP 的网关…"));

    if (m_ssdp) {
        m_ssdp->deleteLater();
        m_ssdp = nullptr;
    }
    m_ssdp = new QUdpSocket(this);
    m_ssdp->bind(QHostAddress::AnyIPv4, 0);
    connect(m_ssdp, &QUdpSocket::readyRead, this, &PortMapper::onSsdpReadyRead);

    // 对 IGD 根设备与 WAN 服务各发一轮 M-SEARCH，提高兼容性
    const QStringList targets = {
        QStringLiteral("urn:schemas-upnp-org:device:InternetGatewayDevice:1"),
        QStringLiteral("urn:schemas-upnp-org:service:WANIPConnection:1"),
    };
    for (const QString &st : targets) {
        const QByteArray msg =
            "M-SEARCH * HTTP/1.1\r\n"
            "HOST: 239.255.255.250:1900\r\n"
            "MAN: \"ssdp:discover\"\r\n"
            "MX: 2\r\n"
            "ST: " + st.toUtf8() + "\r\n\r\n";
        m_ssdp->writeDatagram(msg, QHostAddress(QString::fromLatin1(kSsdpAddr)), kSsdpPort);
        m_ssdp->writeDatagram(msg, QHostAddress(QString::fromLatin1(kSsdpAddr)), kSsdpPort);
    }

    // 4 秒无应答判定失败
    QTimer::singleShot(4000, this, [this]() {
        if (!m_descPending && m_controlUrl.isEmpty()) {
            setBusy(false);
            setStatus(QStringLiteral("未发现支持 UPnP 的网关（请确认路由器已开启 UPnP）"));
        }
        if (m_ssdp) {
            m_ssdp->deleteLater();
            m_ssdp = nullptr;
        }
    });
}

void PortMapper::onSsdpReadyRead()
{
    while (m_ssdp && m_ssdp->hasPendingDatagrams()) {
        QByteArray buf;
        buf.resize(int(m_ssdp->pendingDatagramSize()));
        m_ssdp->readDatagram(buf.data(), buf.size());
        // 从应答头中提取 LOCATION
        const QRegularExpression re(QStringLiteral("(?im)^LOCATION:\\s*(\\S+)"));
        const auto m = re.match(QString::fromLatin1(buf));
        if (!m.hasMatch())
            continue;
        const QUrl loc(m.captured(1));
        if (!loc.isValid() || m_descPending || !m_controlUrl.isEmpty())
            continue;
        m_descPending = true;
        fetchDescription(loc);
    }
}

// ---------- 第 2 步：解析设备描述，找 WAN 服务 controlURL ----------
void PortMapper::fetchDescription(const QUrl &location)
{
    setStatus(QStringLiteral("已发现网关，正在获取设备描述…"));
    QNetworkRequest req{location};
    req.setHeader(QNetworkRequest::UserAgentHeader, HttpClient::kUserAgent);
    QNetworkReply *rep = m_nam.get(req);
    connect(rep, &QNetworkReply::finished, this, [this, rep, location]() {
        rep->deleteLater();
        m_descPending = false;
        if (rep->error() != QNetworkReply::NoError) {
            setBusy(false);
            setStatus(QStringLiteral("获取网关描述失败：") + rep->errorString());
            return;
        }
        const QByteArray xml = rep->readAll();
        // 遍历 <service>，匹配 WANIP/WANPPP Connection
        QXmlStreamReader r(xml);
        QString friendly, curType, curCtl, bestType, bestCtl;
        bool inService = false;
        while (!r.atEnd()) {
            r.readNext();
            if (r.isStartElement()) {
                const auto n = r.name();
                if (n == QLatin1String("friendlyName") && friendly.isEmpty())
                    friendly = r.readElementText();
                else if (n == QLatin1String("service")) {
                    inService = true;
                    curType.clear();
                    curCtl.clear();
                } else if (inService && n == QLatin1String("serviceType"))
                    curType = r.readElementText();
                else if (inService && n == QLatin1String("controlURL"))
                    curCtl = r.readElementText();
            } else if (r.isEndElement() && r.name() == QLatin1String("service")) {
                inService = false;
                if (kWanServices.contains(curType) && !curCtl.isEmpty()
                    && bestCtl.isEmpty()) {
                    bestType = curType;
                    bestCtl = curCtl;
                }
            }
        }
        if (bestCtl.isEmpty()) {
            setBusy(false);
            setStatus(QStringLiteral("网关不提供 WANIPConnection 服务，无法映射端口"));
            return;
        }
        m_serviceType = bestType;
        m_controlUrl = location.resolved(QUrl(bestCtl));
        m_gatewayName = friendly.isEmpty() ? location.host() : friendly;
        m_lanIp = detectLanIp(location.host());
        emit availableChanged();
        setBusy(false);
        setStatus(QStringLiteral("网关就绪：%1").arg(m_gatewayName));
        refreshExternalIp();
    });
}

// 探测到达网关所用的本机 IPv4（UDP connect 不发包，仅让内核选路由）
QString PortMapper::detectLanIp(const QString &gatewayHost)
{
    QUdpSocket probe;
    probe.connectToHost(gatewayHost, 9);
    if (probe.waitForConnected(500)) {
        const QHostAddress a = probe.localAddress();
        if (!a.isNull() && a.protocol() == QAbstractSocket::IPv4Protocol)
            return a.toString();
    }
    return QStringLiteral("127.0.0.1");
}

// ---------- 第 3 步：SOAP 控制 ----------
void PortMapper::soapRequest(const QString &action, const QString &argsXml,
                             std::function<void(bool, const QString &)> done)
{
    if (m_controlUrl.isEmpty()) {
        done(false, QStringLiteral("尚未发现网关"));
        return;
    }
    const QString body = QStringLiteral(
        "<?xml version=\"1.0\"?>"
        "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
        "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
        "<s:Body><u:%1 xmlns:u=\"%2\">%3</u:%1></s:Body></s:Envelope>")
        .arg(action, m_serviceType, argsXml);

    QNetworkRequest req{m_controlUrl};
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("text/xml; charset=\"utf-8\""));
    req.setRawHeader("SOAPAction",
                     QStringLiteral("\"%1#%2\"").arg(m_serviceType, action).toLatin1());
    QNetworkReply *rep = m_nam.post(req, body.toUtf8());
    connect(rep, &QNetworkReply::finished, this, [rep, done]() {
        rep->deleteLater();
        const QString text = QString::fromUtf8(rep->readAll());
        if (rep->error() != QNetworkReply::NoError) {
            // UPnP 错误码在响应体 <errorDescription> 中
            const QRegularExpression re(
                QStringLiteral("<errorDescription>([^<]*)</errorDescription>"));
            const auto m = re.match(text);
            done(false, m.hasMatch() ? m.captured(1) : rep->errorString());
            return;
        }
        done(true, text);
    });
}

void PortMapper::refreshExternalIp()
{
    soapRequest(QStringLiteral("GetExternalIPAddress"), QString(),
                [this](bool ok, const QString &resp) {
        if (!ok)
            return;
        const QRegularExpression re(QStringLiteral(
            "<NewExternalIPAddress>([^<]*)</NewExternalIPAddress>"));
        const auto m = re.match(resp);
        if (m.hasMatch() && m.captured(1) != m_externalIp) {
            m_externalIp = m.captured(1);
            emit externalIpChanged();
            // 私网外部 IP → 大概率处于运营商级 NAT，公网不可达
            if (m_externalIp.startsWith(QStringLiteral("10."))
                || m_externalIp.startsWith(QStringLiteral("192.168."))
                || m_externalIp.startsWith(QStringLiteral("100.")))
                setStatus(QStringLiteral(
                    "网关就绪，但外部 IP (%1) 为私网地址——可能处于运营商 NAT，"
                    "公网或无法直连").arg(m_externalIp));
        }
    });
}

void PortMapper::addMapping(int externalPort, int internalPort,
                            const QString &protocol, const QString &description)
{
    if (externalPort <= 0 || externalPort > 65535
        || internalPort <= 0 || internalPort > 65535) {
        emit mappingResult(false, QStringLiteral("端口无效"));
        return;
    }
    const QString args = QStringLiteral(
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>%1</NewExternalPort>"
        "<NewProtocol>%2</NewProtocol>"
        "<NewInternalPort>%3</NewInternalPort>"
        "<NewInternalClient>%4</NewInternalClient>"
        "<NewEnabled>1</NewEnabled>"
        "<NewPortMappingDescription>%5</NewPortMappingDescription>"
        "<NewLeaseDuration>0</NewLeaseDuration>")
        .arg(externalPort).arg(protocol.toUpper()).arg(internalPort)
        .arg(m_lanIp, description);
    setBusy(true);
    soapRequest(QStringLiteral("AddPortMapping"), args,
                [this, externalPort, internalPort, protocol](bool ok, const QString &msg) {
        setBusy(false);
        if (ok) {
            QVariantMap m;
            m[QStringLiteral("externalPort")] = externalPort;
            m[QStringLiteral("internalPort")] = internalPort;
            m[QStringLiteral("protocol")] = protocol.toUpper();
            // 去重后追加
            for (int i = m_mappings.size() - 1; i >= 0; --i) {
                const auto e = m_mappings.at(i).toMap();
                if (e.value(QStringLiteral("externalPort")).toInt() == externalPort
                    && e.value(QStringLiteral("protocol")).toString() == protocol.toUpper())
                    m_mappings.removeAt(i);
            }
            m_mappings << m;
            emit mappingsChanged();
            emit mappingResult(true, QStringLiteral("已映射 %1:%2 → %3:%4")
                               .arg(m_externalIp.isEmpty() ? QStringLiteral("公网") : m_externalIp)
                               .arg(externalPort).arg(m_lanIp).arg(internalPort));
        } else {
            emit mappingResult(false, QStringLiteral("映射失败：") + msg);
        }
    });
}

void PortMapper::removeMapping(int externalPort, const QString &protocol)
{
    const QString args = QStringLiteral(
        "<NewRemoteHost></NewRemoteHost>"
        "<NewExternalPort>%1</NewExternalPort>"
        "<NewProtocol>%2</NewProtocol>")
        .arg(externalPort).arg(protocol.toUpper());
    setBusy(true);
    soapRequest(QStringLiteral("DeletePortMapping"), args,
                [this, externalPort, protocol](bool ok, const QString &msg) {
        setBusy(false);
        if (ok) {
            for (int i = m_mappings.size() - 1; i >= 0; --i) {
                const auto e = m_mappings.at(i).toMap();
                if (e.value(QStringLiteral("externalPort")).toInt() == externalPort
                    && e.value(QStringLiteral("protocol")).toString() == protocol.toUpper())
                    m_mappings.removeAt(i);
            }
            emit mappingsChanged();
            emit mappingResult(true, QStringLiteral("已删除端口 %1 的映射").arg(externalPort));
        } else {
            emit mappingResult(false, QStringLiteral("删除映射失败：") + msg);
        }
    });
}
