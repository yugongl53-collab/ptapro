#include "ptapro/core/CodecTypes.h"

namespace ptapro {

QString displayName(Symbology symbology)
{
    switch (symbology) {
    case Symbology::QrCode:
        return QStringLiteral("二维码 QR Code");
    case Symbology::Code128:
        return QStringLiteral("条形码 Code 128");
    case Symbology::Ean13:
        return QStringLiteral("条形码 EAN-13");
    }

    // 防御性兜底：当枚举未来扩展但 switch 尚未补齐时，界面仍能给出可读名称。
    return QStringLiteral("未知码制");
}

QStringList supportedSymbologyNames()
{
    return {
        displayName(Symbology::QrCode),
        displayName(Symbology::Code128),
        displayName(Symbology::Ean13),
    };
}

Symbology symbologyFromIndex(int index)
{
    switch (index) {
    case 0:
        return Symbology::QrCode;
    case 1:
        return Symbology::Code128;
    case 2:
        return Symbology::Ean13;
    default:
        // 非法索引按二维码处理，避免 UI 状态异常时向下游传递不可控值。
        return Symbology::QrCode;
    }
}

} // namespace ptapro
