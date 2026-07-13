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

} // namespace

DecodeResult DecoderService::decodeImage(const QImage& image) const
{
    if (image.isNull()) {
        return {
            false,
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

    ZXing::ReaderOptions options;
    options.setFormats(ZXing::BarcodeFormat::QRCode | ZXing::BarcodeFormat::Code128 | ZXing::BarcodeFormat::EAN13);
    options.setTryHarder(true);
    options.setTryRotate(true);
    options.setTryInvert(true);

    const ZXing::Result result = ZXing::ReadBarcode(imageView, options);
    if (!result.isValid()) {
        return {
            false,
            {},
            {},
            QStringLiteral("未识别到二维码或条形码。"),
        };
    }

    return {
        true,
        fromUtf8StdString(result.text()),
        fromUtf8StdString(ZXing::ToString(result.format())),
        QStringLiteral("识别成功。"),
    };
}

} // namespace ptapro
