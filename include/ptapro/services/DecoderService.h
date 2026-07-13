#pragma once

#include "ptapro/core/CodecTypes.h"

namespace ptapro {

struct DecodeOptions {
    bool enablePreprocessing{true};
    int maxPreprocessedVariants{3};
};

class DecoderService {
public:
    DecodeResult decodeImage(const QImage& image, const DecodeOptions& options = {}) const;
};

} // namespace ptapro
