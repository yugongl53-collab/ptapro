#include "ptapro/services/EncoderService.h"

#include <ZXing/BarcodeFormat.h>
#include <ZXing/BitMatrix.h>
#include <ZXing/CharacterSet.h>
#include <ZXing/MultiFormatWriter.h>

#include <QBuffer>
#include <QIODevice>
#include <QPainter>
#include <QXmlStreamWriter>
#include <QtGlobal>

#include <stdexcept>
#include <string>

namespace ptapro {
namespace {

constexpr int kMinimumImageSize = 160;
constexpr int kMaximumImageSize = 2048;
constexpr int kMinimumBarcodeHeight = 120;
constexpr int kReadableTextHeight = 44;

struct MatrixPayload {
    ZXing::BitMatrix matrix;
    QString readableText;
};

QSize normalizedImageSize(QSize requestedSize)
{
    // 外部调用可能传入过小、过大或无效尺寸，这里统一收敛，避免生成不可见图片或占用异常内存。
    return {
        qBound(kMinimumImageSize, requestedSize.width(), kMaximumImageSize),
        qBound(kMinimumImageSize, requestedSize.height(), kMaximumImageSize),
    };
}

std::string toUtf8StdString(const QString& text)
{
    const QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<size_t>(bytes.size()));
}

ZXing::BarcodeFormat toZxingFormat(Symbology symbology)
{
    switch (symbology) {
    case Symbology::QrCode:
        return ZXing::BarcodeFormat::QRCode;
    case Symbology::Code128:
        return ZXing::BarcodeFormat::Code128;
    case Symbology::Ean13:
        return ZXing::BarcodeFormat::EAN13;
    }

    // 防御性兜底：未来新增码制但未适配时，先按 QR Code 生成，避免未定义行为。
    return ZXing::BarcodeFormat::QRCode;
}

int toZxingEccLevel(ErrorCorrectionLevel level)
{
    switch (level) {
    case ErrorCorrectionLevel::Low:
        return 0;
    case ErrorCorrectionLevel::Medium:
        return 1;
    case ErrorCorrectionLevel::Quartile:
        return 2;
    case ErrorCorrectionLevel::High:
        return 3;
    }

    // ZXing 的 QR Code 纠错等级按 0-3 映射到 L/M/Q/H；默认使用 M 平衡容量和容错。
    return 1;
}

int ean13CheckDigit(const QString& firstTwelveDigits)
{
    int sum = 0;
    for (int index = 0; index < firstTwelveDigits.size(); ++index) {
        const int digit = firstTwelveDigits.at(index).digitValue();
        sum += (index % 2 == 0) ? digit : digit * 3;
    }

    return (10 - (sum % 10)) % 10;
}

QString normalizedPayload(const EncodeRequest& request)
{
    const QString payload = request.payload.trimmed();
    if (payload.isEmpty()) {
        throw std::runtime_error("请输入需要编码的文本或 URL。");
    }

    if (request.symbology != Symbology::Ean13) {
        return payload;
    }

    QString digits;
    digits.reserve(payload.size());
    for (const QChar ch : payload) {
        if (ch.isSpace()) {
            continue;
        }
        if (!ch.isDigit()) {
            throw std::runtime_error("EAN-13 只能包含 12 或 13 位数字。");
        }
        digits.append(ch);
    }

    if (digits.size() == 12) {
        digits.append(QString::number(ean13CheckDigit(digits)));
        return digits;
    }

    if (digits.size() != 13) {
        throw std::runtime_error("EAN-13 需要 12 位数据数字，或 13 位含校验位数字。");
    }

    const QString firstTwelveDigits = digits.left(12);
    if (digits.at(12).digitValue() != ean13CheckDigit(firstTwelveDigits)) {
        throw std::runtime_error("EAN-13 校验位不正确，请检查最后一位数字。");
    }

    return digits;
}

MatrixPayload buildMatrix(const EncodeRequest& request, QSize targetSize)
{
    const QString content = normalizedPayload(request);
    const QSize imageSize = normalizedImageSize(targetSize);
    const bool linearCode = request.symbology != Symbology::QrCode;
    const int matrixHeight = linearCode && request.showReadableText
        ? qMax(kMinimumBarcodeHeight, imageSize.height() - kReadableTextHeight)
        : imageSize.height();

    ZXing::MultiFormatWriter writer(toZxingFormat(request.symbology));
    writer.setMargin(qBound(0, request.marginSize, 64));

    if (request.symbology == Symbology::QrCode) {
        // QR Code 文本按 UTF-8 写入，保证中文、URL 和普通文本都使用一致编码。
        writer.setEncoding(ZXing::CharacterSet::UTF8);
        writer.setEccLevel(toZxingEccLevel(request.errorCorrection));
    }

    return {
        writer.encode(toUtf8StdString(content), imageSize.width(), matrixHeight),
        content,
    };
}

void paintMatrix(QPainter& painter, const ZXing::BitMatrix& matrix, const QPoint& origin)
{
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    for (int y = 0; y < matrix.height(); ++y) {
        int runStart = -1;
        for (int x = 0; x <= matrix.width(); ++x) {
            const bool dark = x < matrix.width() && matrix.get(x, y);
            if (dark && runStart < 0) {
                runStart = x;
            } else if (!dark && runStart >= 0) {
                // 连续黑点合并为一段矩形，减少 QPainter 调用次数并避免逐点绘制的性能抖动。
                painter.drawRect(origin.x() + runStart, origin.y() + y, x - runStart, 1);
                runStart = -1;
            }
        }
    }
}

void paintReadableText(QPainter& painter, const QRect& imageRect, const QString& readableText)
{
    QFont font = painter.font();
    font.setPointSize(qMax(10, imageRect.height() / 28));
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(
        imageRect.adjusted(12, imageRect.height() - kReadableTextHeight, -12, -8),
        Qt::AlignCenter,
        readableText
    );
}

void paintLogo(QPainter& painter, const QRect& imageRect, const QImage& logo)
{
    if (logo.isNull()) {
        return;
    }

    const int logoSide = qMax(32, qMin(imageRect.width(), imageRect.height()) / 5);
    const QSize logoSize(logoSide, logoSide);
    const QPoint logoTopLeft(
        imageRect.center().x() - logoSide / 2,
        imageRect.center().y() - logoSide / 2
    );
    const QRect logoRect(logoTopLeft, logoSize);
    const QRect backgroundRect = logoRect.adjusted(-8, -8, 8, 8);

    // Logo 下方留白，减少中心图片对 QR Code 模块的干扰；高纠错等级下可读性更稳。
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawRoundedRect(backgroundRect, 8, 8);
    const QImage scaledLogo = logo.scaled(logoSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    const QPoint scaledLogoTopLeft(
        logoRect.center().x() - scaledLogo.width() / 2,
        logoRect.center().y() - scaledLogo.height() / 2
    );
    painter.drawImage(QRect(scaledLogoTopLeft, scaledLogo.size()), scaledLogo);
}

QImage renderImage(const EncodeRequest& request, const MatrixPayload& payload)
{
    const QSize imageSize = normalizedImageSize(request.imageSize);
    QImage image(imageSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    paintMatrix(painter, payload.matrix, QPoint(0, 0));

    if (request.symbology == Symbology::QrCode) {
        paintLogo(painter, image.rect(), request.logo);
    } else if (request.showReadableText) {
        paintReadableText(painter, image.rect(), payload.readableText);
    }

    return image;
}

void writeSvgMatrix(QXmlStreamWriter& writer, const ZXing::BitMatrix& matrix)
{
    for (int y = 0; y < matrix.height(); ++y) {
        int runStart = -1;
        for (int x = 0; x <= matrix.width(); ++x) {
            const bool dark = x < matrix.width() && matrix.get(x, y);
            if (dark && runStart < 0) {
                runStart = x;
            } else if (!dark && runStart >= 0) {
                writer.writeEmptyElement(QStringLiteral("rect"));
                writer.writeAttribute(QStringLiteral("x"), QString::number(runStart));
                writer.writeAttribute(QStringLiteral("y"), QString::number(y));
                writer.writeAttribute(QStringLiteral("width"), QString::number(x - runStart));
                writer.writeAttribute(QStringLiteral("height"), QStringLiteral("1"));
                runStart = -1;
            }
        }
    }
}

void writeSvgLogo(QXmlStreamWriter& writer, const QRect& imageRect, const QImage& logo)
{
    if (logo.isNull()) {
        return;
    }

    const int logoSide = qMax(32, qMin(imageRect.width(), imageRect.height()) / 5);
    const QPoint logoTopLeft(
        imageRect.center().x() - logoSide / 2,
        imageRect.center().y() - logoSide / 2
    );
    const QRect logoRect(logoTopLeft, QSize(logoSide, logoSide));
    const QRect backgroundRect = logoRect.adjusted(-8, -8, 8, 8);

    writer.writeEmptyElement(QStringLiteral("rect"));
    writer.writeAttribute(QStringLiteral("x"), QString::number(backgroundRect.x()));
    writer.writeAttribute(QStringLiteral("y"), QString::number(backgroundRect.y()));
    writer.writeAttribute(QStringLiteral("width"), QString::number(backgroundRect.width()));
    writer.writeAttribute(QStringLiteral("height"), QString::number(backgroundRect.height()));
    writer.writeAttribute(QStringLiteral("rx"), QStringLiteral("8"));
    writer.writeAttribute(QStringLiteral("fill"), QStringLiteral("#ffffff"));

    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    buffer.open(QIODevice::WriteOnly);
    logo.save(&buffer, "PNG");

    writer.writeEmptyElement(QStringLiteral("image"));
    writer.writeAttribute(QStringLiteral("x"), QString::number(logoRect.x()));
    writer.writeAttribute(QStringLiteral("y"), QString::number(logoRect.y()));
    writer.writeAttribute(QStringLiteral("width"), QString::number(logoRect.width()));
    writer.writeAttribute(QStringLiteral("height"), QString::number(logoRect.height()));
    writer.writeAttribute(QStringLiteral("href"), QStringLiteral("data:image/png;base64,%1").arg(QString::fromLatin1(pngBytes.toBase64())));
}

QString renderSvgDocument(const EncodeRequest& request, const MatrixPayload& payload)
{
    const QSize imageSize = normalizedImageSize(request.imageSize);
    QString svg;
    QXmlStreamWriter writer(&svg);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement(QStringLiteral("svg"));
    writer.writeDefaultNamespace(QStringLiteral("http://www.w3.org/2000/svg"));
    writer.writeAttribute(QStringLiteral("width"), QString::number(imageSize.width()));
    writer.writeAttribute(QStringLiteral("height"), QString::number(imageSize.height()));
    writer.writeAttribute(
        QStringLiteral("viewBox"),
        QStringLiteral("0 0 %1 %2").arg(imageSize.width()).arg(imageSize.height())
    );

    writer.writeEmptyElement(QStringLiteral("rect"));
    writer.writeAttribute(QStringLiteral("width"), QStringLiteral("100%"));
    writer.writeAttribute(QStringLiteral("height"), QStringLiteral("100%"));
    writer.writeAttribute(QStringLiteral("fill"), QStringLiteral("#ffffff"));

    writer.writeStartElement(QStringLiteral("g"));
    writer.writeAttribute(QStringLiteral("fill"), QStringLiteral("#000000"));
    writeSvgMatrix(writer, payload.matrix);
    writer.writeEndElement();

    if (request.symbology == Symbology::QrCode) {
        writeSvgLogo(writer, QRect(QPoint(0, 0), imageSize), request.logo);
    } else if (request.showReadableText) {
        writer.writeStartElement(QStringLiteral("text"));
        writer.writeAttribute(QStringLiteral("x"), QString::number(imageSize.width() / 2));
        writer.writeAttribute(QStringLiteral("y"), QString::number(imageSize.height() - 14));
        writer.writeAttribute(QStringLiteral("text-anchor"), QStringLiteral("middle"));
        writer.writeAttribute(QStringLiteral("font-family"), QStringLiteral("Arial, Helvetica, sans-serif"));
        writer.writeAttribute(QStringLiteral("font-size"), QString::number(qMax(12, imageSize.height() / 18)));
        writer.writeCharacters(payload.readableText);
        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndDocument();
    return svg;
}

EncodeResult makeResult(const EncodeRequest& request)
{
    const MatrixPayload payload = buildMatrix(request, normalizedImageSize(request.imageSize));
    return {
        true,
        renderImage(request, payload),
        QStringLiteral("已生成可扫描的 %1。").arg(displayName(request.symbology)),
    };
}

} // namespace

EncodeResult EncoderService::encode(const EncodeRequest& request) const
{
    try {
        switch (request.symbology) {
        case Symbology::QrCode:
            return renderQrCode(request);
        case Symbology::Code128:
            return renderCode128(request);
        case Symbology::Ean13:
            return renderEan13(request);
        }
    } catch (const std::exception& error) {
        return {
            false,
            {},
            QString::fromUtf8(error.what()),
        };
    }

    return {
        false,
        {},
        QStringLiteral("暂不支持当前码制。"),
    };
}

QString EncoderService::renderSvg(const EncodeRequest& request, QString* errorMessage) const
{
    try {
        const MatrixPayload payload = buildMatrix(request, normalizedImageSize(request.imageSize));
        if (errorMessage) {
            errorMessage->clear();
        }
        return renderSvgDocument(request, payload);
    } catch (const std::exception& error) {
        if (errorMessage) {
            *errorMessage = QString::fromUtf8(error.what());
        }
        return {};
    }
}

EncodeResult EncoderService::renderQrCode(const EncodeRequest& request) const
{
    return makeResult(request);
}

EncodeResult EncoderService::renderCode128(const EncodeRequest& request) const
{
    return makeResult(request);
}

EncodeResult EncoderService::renderEan13(const EncodeRequest& request) const
{
    return makeResult(request);
}

} // namespace ptapro
