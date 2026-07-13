#include "ptapro/services/EncoderService.h"

#include <QPainter>
#include <QtGlobal>

namespace ptapro {
namespace {

constexpr int kMinimumImageSize = 160;

QSize normalizedImageSize(QSize requestedSize)
{
    // 外部调用可能传入过小或无效尺寸，这里统一收敛，避免绘制时出现除零或不可见图片。
    return {
        qMax(requestedSize.width(), kMinimumImageSize),
        qMax(requestedSize.height(), kMinimumImageSize),
    };
}

} // namespace

EncodeResult EncoderService::encode(const EncodeRequest& request) const
{
    if (request.payload.trimmed().isEmpty()) {
        return {
            false,
            {},
            QStringLiteral("请输入需要编码的文本或 URL。"),
        };
    }

    const QImage image = request.symbology == Symbology::QrCode
        ? renderQrPlaceholder(request)
        : renderBarcodePlaceholder(request);

    return {
        !image.isNull(),
        image,
        QStringLiteral("已生成架构占位图，后续会替换为真实可扫描编码。"),
    };
}

QImage EncoderService::renderQrPlaceholder(const EncodeRequest& request) const
{
    const QSize size = normalizedImageSize(request.imageSize);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);

    const int margin = size.width() / 12;
    const int side = qMin(size.width(), size.height()) - margin * 2;
    const int cells = 25;
    const int cellSize = qMax(side / cells, 1);
    const int originX = (size.width() - cellSize * cells) / 2;
    const int originY = (size.height() - cellSize * cells) / 2;
    const uint seed = qHash(request.payload);

    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    for (int row = 0; row < cells; ++row) {
        for (int column = 0; column < cells; ++column) {
            const bool finderArea =
                (row < 7 && column < 7) ||
                (row < 7 && column >= cells - 7) ||
                (row >= cells - 7 && column < 7);

            // 占位图采用确定性伪随机图案，让同一输入稳定生成同一预览，方便后续 UI 调试。
            const bool dark = finderArea || (((seed + row * 37 + column * 17) >> ((row + column) % 8)) & 1U);
            if (dark) {
                painter.drawRect(originX + column * cellSize, originY + row * cellSize, cellSize, cellSize);
            }
        }
    }

    painter.setPen(Qt::darkGray);
    painter.drawText(image.rect().adjusted(8, 8, -8, -8), Qt::AlignBottom | Qt::AlignHCenter, QStringLiteral("架构占位"));

    return image;
}

QImage EncoderService::renderBarcodePlaceholder(const EncodeRequest& request) const
{
    const QSize size = normalizedImageSize(request.imageSize);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);

    const QRect barcodeArea = image.rect().adjusted(size.width() / 10, size.height() / 6, -size.width() / 10, -size.height() / 4);
    int x = barcodeArea.left();
    const uint seed = qHash(request.payload + displayName(request.symbology));

    while (x < barcodeArea.right()) {
        // 条宽基于输入内容稳定变化，用于模拟不同文本生成不同条码预览。
        const int width = 2 + static_cast<int>((seed + x * 13U) % 5U);
        painter.drawRect(x, barcodeArea.top(), width, barcodeArea.height());
        x += width + 2 + static_cast<int>((seed + x * 7U) % 4U);
    }

    painter.setPen(Qt::darkGray);
    painter.drawText(image.rect().adjusted(8, 8, -8, -8), Qt::AlignBottom | Qt::AlignHCenter, QStringLiteral("架构占位"));

    return image;
}

} // namespace ptapro
