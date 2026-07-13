#pragma once

#include "ptapro/core/CodecTypes.h"

namespace ptapro {

class EncoderService {
public:
    EncodeResult encode(const EncodeRequest& request) const;

private:
    QImage renderQrPlaceholder(const EncodeRequest& request) const;
    QImage renderBarcodePlaceholder(const EncodeRequest& request) const;
};

} // namespace ptapro
