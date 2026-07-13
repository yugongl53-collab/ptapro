#include "ptapro/services/DecoderService.h"

#include <ZXing/BarcodeFormat.h>
#include <ZXing/ImageView.h>
#include <ZXing/ReadBarcode.h>
#include <ZXing/ReaderOptions.h>
#include <ZXing/Result.h>

#include <cstdint>
#include <string>

namespace ptapro {
namespace {

QString fromUtf8StdString(const std::string& text)
{
    return QString::fromUtf8(text.data(), static_cast<int>(text.size()));
}

DecodedSymbol toDecodedSymbol(const ZXing::Result& result)
{
    const auto position = result.position();
    return {
        fromUtf8StdString(result.text()),
        fromUtf8StdString(ZXing::ToString(result.format())),
        {
            QPointF(position.topLeft().x, position.topLeft().y),
            QPointF(position.topRight().x, position.topRight().y),
            QPointF(position.bottomRight().x, position.bottomRight().y),
            QPointF(position.bottomLeft().x, position.bottomLeft().y),
        },
    };
}

ZXing::ReaderOptions defaultReaderOptions()
{
    ZXing::ReaderOptions options;
    options.setFormats(ZXing::BarcodeFormat::QRCode | ZXing::BarcodeFormat::Code128 | ZXing::BarcodeFormat::EAN13);
    options.setTryHarder(true);
    options.setTryRotate(true);
    options.setTryInvert(true);
    options.setMaxNumberOfSymbols(32);
    return options;
}

} // namespace

DecodeResult DecoderService::decodeImage(const QImage& image) const
{
    if (image.isNull()) {
        return {
            false,
            {},
            {},
            {},
            QStringLiteral("请先选择一张有效图片。"),
        };
    }

    // ZXing 使用非拥有型 ImageView，必须保证转换后的 QImage 在识别调用期间保持存活。
    const QImage rgbImage = image.convertToFormat(QImage::Format_RGB888);
    const ZXing::ImageView imageView(
        reinterpret_cast<const uint8_t*>(rgbImage.constBits()),
        rgbImage.width(),
        rgbImage.height(),
        ZXing::ImageFormat::RGB,
        rgbImage.bytesPerLine()
    );

    QVector<DecodedSymbol> symbols;
    const ZXing::Results results = ZXing::ReadBarcodes(imageView, defaultReaderOptions());
    symbols.reserve(static_cast<int>(results.size()));

    for (const ZXing::Result& result : results) {
        if (!result.isValid()) {
            continue;
        }
        symbols.append(toDecodedSymbol(result));
    }

    if (symbols.isEmpty()) {
        return {
            false,
            {},
            {},
            {},
            QStringLiteral("未识别到二维码或条形码。"),
        };
    }

    const DecodedSymbol& firstSymbol = symbols.first();
    return {
        true,
        firstSymbol.payload,
        firstSymbol.formatName,
        symbols,
        QStringLiteral("识别到 %1 个码。").arg(symbols.size()),
    };
}

} // namespace ptapro
