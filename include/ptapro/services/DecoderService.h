#pragma once

#include "ptapro/core/CodecTypes.h"

namespace ptapro {

class DecoderService {
public:
    DecodeResult decodeImage(const QImage& image) const;
};

} // namespace ptapro
