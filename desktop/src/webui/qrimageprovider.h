#ifndef QRIMAGEPROVIDER_H
#define QRIMAGEPROVIDER_H

#include <QQuickImageProvider>

// 用内嵌的 qrcode-generator (qrcode_lib.h) 在 C++ 侧把任意文本渲染成二维码位图，
// 供 QML 通过 image://qr/<text> 直接显示（无需 QtSvg 插件，兼容性最好）。
class QrImageProvider : public QQuickImageProvider
{
public:
    QrImageProvider();
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};

#endif // QRIMAGEPROVIDER_H
