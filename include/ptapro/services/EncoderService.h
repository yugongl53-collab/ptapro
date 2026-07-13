#pragma once

#include "ptapro/core/CodecTypes.h"

namespace ptapro {

class EncoderService {
public:
    EncodeResult encode(const EncodeRequest& request) const;
    QString renderSvg(const EncodeRequest& request, QString* errorMessage = nullptr) const;

private:
    EncodeResult renderQrCode(const EncodeRequest& request) const;
    EncodeResult renderCode128(const EncodeRequest& request) const;
    EncodeResult renderEan13(const EncodeRequest& request) const;
};

} // namespace ptapro
