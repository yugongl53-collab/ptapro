#include "ptapro/services/DecoderService.h"

namespace ptapro {

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

#if defined(PTAPRO_HAS_OPENCV)
    // 这里是 OpenCV/ZXing/ZBar 等识别适配层的预留入口。
    // 初始架构阶段先保持明确失败，防止用户误以为占位逻辑已经具备真实识别能力。
    return {
        false,
        {},
        {},
        QStringLiteral("已检测到 OpenCV，但真实识别适配层尚未实现。"),
    };
#else
    return {
        false,
        {},
        {},
        QStringLiteral("当前环境未启用真实识别适配层，请后续接入 OpenCV 或条码识别库。"),
    };
#endif
}

} // namespace ptapro
