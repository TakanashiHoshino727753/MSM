#pragma once
#include <QObject>
#include <QString>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>
#include <QMap>
#include <functional>
#include <memory>

// 统一 HTTP 工具：消除 createserver / downloadcatalog / proxycontroller 中多份重复的
// fetchJson / fetchText 实现。提供超时兜底（TransferTimeout 只覆盖“已连接无数据”，
// 不覆盖“握手阶段被墙”的卡死，故额外用单发 QTimer 强制结束）。
//
// 设计要点：
// - 使用 shared_ptr<bool> 闸门保证 finish 回调只触发一次（finished 与 guard 谁先到都安全）。
// - guard 到时直接调用 onErr，不依赖 abort() 后 finished 一定触发（Qt6+MinGW 下 TLS 握手
//   阶段 abort 偶尔不发出 finished，会造成永久卡死）。
class HttpClient
{
public:
    static constexpr int kDefaultTimeoutMs = 30000;
    static constexpr const char *kUserAgent =
        "MinecraftServerManager/1.0 (https://github.com/MinecraftServerManager; contact: msm@example.com)";

    // 取一份带通用头的请求对象（UA + 跟随重定向 + 传输超时）。
    static QNetworkRequest makeRequest(const QString &url,
                                       int transferTimeoutMs = kDefaultTimeoutMs)
    {
        QNetworkRequest req{QUrl(url)};
        req.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
        req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
        req.setTransferTimeout(transferTimeoutMs);
        return req;
    }

    // 发起 GET，返回的 reply 由本函数负责 deleteLater（guard 以 reply 为父）。
    // onOk/onErr 只会触发一次。timeoutMs 为总超时兜底（<=0 时用 kDefaultTimeoutMs）。
    static void getJson(QNetworkAccessManager *nam, const QString &url,
                        std::function<void(const QJsonDocument &)> onOk,
                        std::function<void(const QString &)> onErr,
                        int timeoutMs = kDefaultTimeoutMs)
    {
        get(nam, url,
            [onOk, onErr](bool ok, const QByteArray &data, const QString &err) {
                if (!ok) { onErr(err); return; }
                QJsonParseError pe;
                const QJsonDocument doc = QJsonDocument::fromJson(data, &pe);
                if (doc.isNull()) { onErr(QStringLiteral("JSON 解析失败: ") + pe.errorString()); return; }
                onOk(doc);
            },
            timeoutMs);
    }

    // URL 百分比解码（%20 → 空格等）。
    static QString urlDecode(const QString &s)
    {
        return QUrl::fromPercentEncoding(s.toUtf8());
    }

    // 解析 "a=1&b=2" 形式的 query 串为键值表（值已做百分比解码）。
    static QMap<QString, QString> parseQuery(const QString &q)
    {
        QMap<QString, QString> out;
        if (q.isEmpty())
            return out;
        for (const QString &p : q.split(QLatin1Char('&'), Qt::SkipEmptyParts)) {
            const int eq = p.indexOf(QLatin1Char('='));
            if (eq < 0)
                out[urlDecode(p)] = QString();
            else
                out[urlDecode(p.left(eq))] = urlDecode(p.mid(eq + 1));
        }
        return out;
    }

    // 文本 GET（不解析 JSON）。onOk/onErr 只会触发一次。
    static void getText(QNetworkAccessManager *nam, const QString &url,
                        std::function<void(const QString &)> onOk,
                        std::function<void(const QString &)> onErr,
                        int timeoutMs = kDefaultTimeoutMs)
    {
        get(nam, url,
            [onOk, onErr](bool ok, const QByteArray &data, const QString &err) {
                if (ok) onOk(QString::fromUtf8(data));
                else    onErr(err);
            },
            timeoutMs);
    }

private:
    // 底层 GET：完成/出错统一经 finish(bool,data,err) 闸门；JSON 校验留给上层回调。
    static void get(QNetworkAccessManager *nam, const QString &url,
                    std::function<void(bool, const QByteArray &, const QString &)> finish,
                    int timeoutMs)
    {
        if (timeoutMs <= 0)
            timeoutMs = kDefaultTimeoutMs;
        QNetworkReply *reply = nam->get(makeRequest(url, timeoutMs));
        auto done = std::make_shared<bool>(false);
        auto finishOnce = [done, finish](bool ok, const QByteArray &data, const QString &err) {
            if (*done) return;
            *done = true;
            finish(ok, data, err);
        };
        QTimer *guard = new QTimer(reply);
        guard->setSingleShot(true);
        guard->setInterval(timeoutMs);
        QObject::connect(guard, &QTimer::timeout, reply, [reply, done, finishOnce, url]() {
            if (*done) return;
            if (reply->isRunning())
                reply->abort();
            finishOnce(false, QByteArray(),
                       QStringLiteral("请求超时（%1ms 无响应）").arg(url));
        });
        guard->start();
        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, done, finishOnce]() {
            const QByteArray data = reply->readAll();
            const bool ok = reply->error() == QNetworkReply::NoError;
            const QString err = reply->errorString();
            reply->deleteLater();
            finishOnce(ok, data, err);
        });
    }
};
