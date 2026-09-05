#include "qrimageprovider.h"
#include "qrcode_lib.h"

#include <QJSEngine>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPaintEngine>
#include <QPainter>
#include <QUrl>

QrImageProvider::QrImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Image)
{
}

QImage QrImageProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    // id 由 QML 端 encodeURIComponent 编码后传入，这里还原成原始配对 URI
    const QString data = QUrl::fromPercentEncoding(id.toUtf8());
    if (data.isEmpty())
        return QImage();

    QJSEngine eng;
    eng.globalObject().setProperty("__qrdata", data);
    // 注意：必须把「加载 QRCODE_JS 库」和「调用生成」放在同一次 evaluate 中。
    // QJSEngine 每次 evaluate 的顶层 var 作用域相互隔离，分两次 evaluate 会导致第二次
    // 找不到第一次定义的 qrcode 全局对象，从而 evaluate 报错、返回空图。
    // 该 qrcode-generator 版本不支持 typeNumber=0 自动选版本，故固定用版本 10（纠错 M，
    // 容量 ~270 字节，足以容纳配对 URI）。
    const QString js = QStringLiteral(
        "(function(){"
        "%1"
        "  var qr = qrcode(10, 'M');"
        "  qr.addData(__qrdata);"
        "  qr.make();"
        "  var n = qr.getModuleCount();"
        "  var rows = [];"
        "  for (var r = 0; r < n; r++) {"
        "    var row = [];"
        "    for (var c = 0; c < n; c++) { row.push(qr.isDark(r, c) ? 1 : 0); }"
        "    rows.push(row);"
        "  }"
        "  return JSON.stringify({ n: n, rows: rows });"
        "})()")
        .arg(QLatin1String(QRCODE_JS));
    QJSValue res = eng.evaluate(js);
    if (res.isError()) {
        qWarning() << "[QrImageProvider] evaluate error:" << res.toString();
        return QImage();
    }

    const QJsonObject obj = QJsonDocument::fromJson(res.toString().toUtf8()).object();
    const int n = obj.value(QStringLiteral("n")).toInt();
    const QJsonArray rows = obj.value(QStringLiteral("rows")).toArray();
    if (n <= 0 || rows.size() != n)
        return QImage();

    const int scale = 8;
    const int margin = scale * 4;
    const int dim = n * scale + margin * 2;
    QImage img(dim, dim, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::black);
    for (int r = 0; r < n; ++r) {
        const QJsonArray row = rows.at(r).toArray();
        for (int c = 0; c < n; ++c) {
            if (row.at(c).toInt() == 1)
                p.drawRect(margin + c * scale, margin + r * scale, scale, scale);
        }
    }
    p.end();

    if (size)
        *size = img.size();
    Q_UNUSED(requestedSize)
    return img;
}
