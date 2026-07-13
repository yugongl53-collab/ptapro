#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>

namespace ptapro {

// 统一维护界面和业务层都要使用的码制，避免各模块用字符串硬编码。
enum class Symbology {
    QrCode,
    Code128,
    Ean13
};

struct EncodeRequest {
    QString payload;
    Symbology symbology{Symbology::QrCode};
    QSize imageSize{320, 320};
};

struct EncodeResult {
    bool success{false};
    QImage image;
    QString message;
};

struct DecodeResult {
    bool success{false};
    QString payload;
    QString formatName;
    QString message;
};

QString displayName(Symbology symbology);
QStringList supportedSymbologyNames();
Symbology symbologyFromIndex(int index);

} // namespace ptapro
