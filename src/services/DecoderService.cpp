#include "ptapro/services/DecoderService.h"

#include <ZXing/BarcodeFormat.h>
#include <ZXing/ImageView.h>
#include <ZXing/ReadBarcode.h>
#include <ZXing/ReaderOptions.h>
#include <ZXing/Result.h>

#include <QSet>
#include <QtGlobal>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ptapro {
namespace {

struct ImageVariant {
    QImage image;
    bool preprocessed{false};
};

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
    options.setFormats(
        ZXing::BarcodeFormat::QRCode |
        ZXing::BarcodeFormat::Code128 |
        ZXing::BarcodeFormat::EAN13 |
        ZXing::BarcodeFormat::UPCA |
        ZXing::BarcodeFormat::Code39
    );
    options.setTryHarder(true);
    options.setTryRotate(true);
    options.setTryInvert(true);
    options.setMaxNumberOfSymbols(32);
    return options;
}

QVector<DecodedSymbol> readSymbols(const QImage& image)
{
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
        if (result.isValid()) {
            symbols.append(toDecodedSymbol(result));
        }
    }
    return symbols;
}

QImage toGrayscale(const QImage& image)
{
    return image.convertToFormat(QImage::Format_Grayscale8);
}

int otsuThreshold(const QImage& grayscale)
{
    std::array<int, 256> histogram{};
    for (int y = 0; y < grayscale.height(); ++y) {
        const uchar* row = grayscale.constScanLine(y);
        for (int x = 0; x < grayscale.width(); ++x) {
            ++histogram[row[x]];
        }
    }

    const int totalPixels = grayscale.width() * grayscale.height();
    double totalIntensity = 0.0;
    for (int value = 0; value < 256; ++value) {
        totalIntensity += value * histogram[value];
    }

    int backgroundWeight = 0;
    double backgroundIntensity = 0.0;
    double bestVariance = -1.0;
    int bestThreshold = 127;

    for (int threshold = 0; threshold < 256; ++threshold) {
        backgroundWeight += histogram[threshold];
        if (backgroundWeight == 0) {
            continue;
        }

        const int foregroundWeight = totalPixels - backgroundWeight;
        if (foregroundWeight == 0) {
            break;
        }

        backgroundIntensity += threshold * histogram[threshold];
        const double backgroundMean = backgroundIntensity / backgroundWeight;
        const double foregroundMean = (totalIntensity - backgroundIntensity) / foregroundWeight;
        const double meanDifference = backgroundMean - foregroundMean;
        const double betweenClassVariance = backgroundWeight * foregroundWeight * meanDifference * meanDifference;

        if (betweenClassVariance > bestVariance) {
            bestVariance = betweenClassVariance;
            bestThreshold = threshold;
        }
    }

    return bestThreshold;
}

QImage thresholdBinary(const QImage& grayscale, int threshold)
{
    QImage binary(grayscale.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < grayscale.height(); ++y) {
        const uchar* inputRow = grayscale.constScanLine(y);
        uchar* outputRow = binary.scanLine(y);
        for (int x = 0; x < grayscale.width(); ++x) {
            outputRow[x] = inputRow[x] <= threshold ? 0 : 255;
        }
    }
    return binary;
}

QImage adaptiveThreshold(const QImage& grayscale, int radius, int bias)
{
    const int width = grayscale.width();
    const int height = grayscale.height();
    std::vector<qint64> integral(static_cast<size_t>((width + 1) * (height + 1)), 0);

    for (int y = 1; y <= height; ++y) {
        const uchar* row = grayscale.constScanLine(y - 1);
        int rowSum = 0;
        for (int x = 1; x <= width; ++x) {
            rowSum += row[x - 1];
            integral[static_cast<size_t>(y * (width + 1) + x)] =
                integral[static_cast<size_t>((y - 1) * (width + 1) + x)] + rowSum;
        }
    }

    QImage binary(grayscale.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < height; ++y) {
        const uchar* inputRow = grayscale.constScanLine(y);
        uchar* outputRow = binary.scanLine(y);
        const int top = qMax(0, y - radius);
        const int bottom = qMin(height - 1, y + radius);

        for (int x = 0; x < width; ++x) {
            const int left = qMax(0, x - radius);
            const int right = qMin(width - 1, x + radius);
            const int area = (right - left + 1) * (bottom - top + 1);
            const qint64 sum =
                integral[static_cast<size_t>((bottom + 1) * (width + 1) + right + 1)] -
                integral[static_cast<size_t>(top * (width + 1) + right + 1)] -
                integral[static_cast<size_t>((bottom + 1) * (width + 1) + left)] +
                integral[static_cast<size_t>(top * (width + 1) + left)];

            // 局部均值减去偏置，能在阴影或光照不均时保留更多黑色码块/条纹。
            const int localThreshold = static_cast<int>(sum / area) - bias;
            outputRow[x] = inputRow[x] <= localThreshold ? 0 : 255;
        }
    }
    return binary;
}

QImage dilateBlack(const QImage& binary)
{
    QImage output(binary.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < binary.height(); ++y) {
        uchar* outputRow = output.scanLine(y);
        for (int x = 0; x < binary.width(); ++x) {
            bool hasBlackNeighbor = false;
            for (int dy = -1; dy <= 1 && !hasBlackNeighbor; ++dy) {
                const int yy = y + dy;
                if (yy < 0 || yy >= binary.height()) {
                    continue;
                }
                const uchar* inputRow = binary.constScanLine(yy);
                for (int dx = -1; dx <= 1; ++dx) {
                    const int xx = x + dx;
                    if (xx >= 0 && xx < binary.width() && inputRow[xx] == 0) {
                        hasBlackNeighbor = true;
                        break;
                    }
                }
            }
            outputRow[x] = hasBlackNeighbor ? 0 : 255;
        }
    }
    return output;
}

QImage erodeBlack(const QImage& binary)
{
    QImage output(binary.size(), QImage::Format_Grayscale8);
    for (int y = 0; y < binary.height(); ++y) {
        uchar* outputRow = output.scanLine(y);
        for (int x = 0; x < binary.width(); ++x) {
            bool allNeighborsBlack = true;
            for (int dy = -1; dy <= 1 && allNeighborsBlack; ++dy) {
                const int yy = y + dy;
                if (yy < 0 || yy >= binary.height()) {
                    allNeighborsBlack = false;
                    break;
                }
                const uchar* inputRow = binary.constScanLine(yy);
                for (int dx = -1; dx <= 1; ++dx) {
                    const int xx = x + dx;
                    if (xx < 0 || xx >= binary.width() || inputRow[xx] != 0) {
                        allNeighborsBlack = false;
                        break;
                    }
                }
            }
            outputRow[x] = allNeighborsBlack ? 0 : 255;
        }
    }
    return output;
}

QImage closeBlackGaps(const QImage& binary)
{
    // 黑色为前景：先膨胀再腐蚀，能连接轻微断裂的条码线条或二维码模块边缘。
    return erodeBlack(dilateBlack(binary));
}

QVector<ImageVariant> buildImageVariants(const QImage& image, const DecodeOptions& options)
{
    QVector<ImageVariant> variants;
    variants.append({image, false});

    if (!options.enablePreprocessing || options.maxPreprocessedVariants <= 0) {
        return variants;
    }

    const QImage grayscale = toGrayscale(image);
    const QImage otsu = thresholdBinary(grayscale, otsuThreshold(grayscale));
    variants.append({otsu, true});
    if (variants.size() - 1 >= options.maxPreprocessedVariants) {
        return variants;
    }

    const int adaptiveRadius = qBound(8, qMin(grayscale.width(), grayscale.height()) / 32, 24);
    const QImage adaptive = adaptiveThreshold(grayscale, adaptiveRadius, 7);
    variants.append({adaptive, true});
    if (variants.size() - 1 >= options.maxPreprocessedVariants) {
        return variants;
    }

    variants.append({closeBlackGaps(adaptive), true});
    return variants;
}

QString dedupeKey(const DecodedSymbol& symbol)
{
    return symbol.formatName + QStringLiteral("\n") + symbol.payload;
}

} // namespace

DecodeResult DecoderService::decodeImage(const QImage& image, const DecodeOptions& options) const
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

    QVector<DecodedSymbol> symbols;
    QSet<QString> seenSymbols;
    bool usedPreprocessing = false;

    for (const ImageVariant& variant : buildImageVariants(image, options)) {
        const QVector<DecodedSymbol> variantSymbols = readSymbols(variant.image);
        if (!variantSymbols.isEmpty() && variant.preprocessed) {
            usedPreprocessing = true;
        }

        for (const DecodedSymbol& symbol : variantSymbols) {
            const QString key = dedupeKey(symbol);
            if (seenSymbols.contains(key)) {
                continue;
            }
            seenSymbols.insert(key);
            symbols.append(symbol);
        }
    }

    if (symbols.isEmpty()) {
        return {
            false,
            {},
            {},
            {},
            options.enablePreprocessing
                ? QStringLiteral("未识别到二维码或条形码，已尝试图像预处理。")
                : QStringLiteral("未识别到二维码或条形码。"),
        };
    }

    const DecodedSymbol& firstSymbol = symbols.first();
    return {
        true,
        firstSymbol.payload,
        firstSymbol.formatName,
        symbols,
        usedPreprocessing
            ? QStringLiteral("识别到 %1 个码（含预处理增强）。").arg(symbols.size())
            : QStringLiteral("识别到 %1 个码。").arg(symbols.size()),
    };
}

} // namespace ptapro
