#pragma once

#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVector>

namespace ptapro {

// 统一维护界面和业务层都要使用的码制，避免各模块用字符串硬编码。
enum class Symbology {
    QrCode,
    Code128,
    Ean13
};

enum class ErrorCorrectionLevel {
    Low,
    Medium,
    Quartile,
    High
};

struct EncodeRequest {
    QString payload;
    Symbology symbology{Symbology::QrCode};
    QSize imageSize{320, 320};
    int marginSize{4};
    ErrorCorrectionLevel errorCorrection{ErrorCorrectionLevel::Medium};
    bool showReadableText{true};
    QImage logo;
};

struct EncodeResult {
    bool success{false};
    QImage image;
    QString message;
};

struct DecodedSymbol {
    QString payload;
    QString formatName;
    QVector<QPointF> corners;
};

struct DecodeResult {
    bool success{false};
    QString payload;
    QString formatName;
    QVector<DecodedSymbol> symbols;
    QString message;
};

QString displayName(Symbology symbology);
QStringList supportedSymbologyNames();
Symbology symbologyFromIndex(int index);
QString displayName(ErrorCorrectionLevel level);
QStringList supportedErrorCorrectionNames();
ErrorCorrectionLevel errorCorrectionFromIndex(int index);

} // namespace ptapro
