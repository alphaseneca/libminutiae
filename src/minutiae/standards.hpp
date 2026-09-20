#pragma once

#include "types.hpp"

namespace minutiae {
namespace standards {

// Standard Format Identifiers
enum class StandardFormat {
    ISO_19794_2_2005, // Aadhaar UIDAI, SecuGen SDK, Global Standard
    ISO_19794_2_2011, // Modern ISO eID, European e-Passports, ICAO 9303
    ANSI_378_2004,    // US Federal PIV, FBI Personal Identity Verification
    ANSI_378_2009,    // INCITS 378 revision
    AUTO_DETECT
};

// Encodes a BiometricTemplate into an ISO/ANSI compliant binary buffer (.fmr)
size_t encode_fmr(
    const BiometricTemplate& tmpl,
    StandardFormat format,
    uint8_t* out_buffer,
    size_t max_len
);

// Decodes any standard FMR record (auto-detects version header)
bool decode_fmr(
    const uint8_t* in_buffer,
    size_t len,
    BiometricTemplate& out_tmpl
);

} // namespace standards
} // namespace minutiae
