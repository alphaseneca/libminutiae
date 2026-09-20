#pragma once

#include "types.hpp"

namespace minutiae {
namespace quality {

// Breakdown of biometric quality components as defined in ISO/IEC 29794-4 & NIST NFIQ 2
struct NFIQ2Report {
    uint8_t overall_score = 0;       // NIST NFIQ 2 overall quality: 0 (unusable) to 100 (excellent)
    float local_clarity_score = 0.0f;// Dynamic range / ridge-valley contrast
    float orientation_certainty = 0.0f;// Directional coherence of ridge flow
    uint16_t minutiae_count = 0;     // Number of valid minutiae extracted
    float spatial_spread = 0.0f;     // Spatial coverage across the active sensor area
    bool acceptable = false;         // Meets minimum biometric quality threshold (>= 40)
};

// Computes NIST NFIQ 2 / ISO 29794-4 biometric quality from a grayscale frame and extracted minutiae
NFIQ2Report evaluate_nfiq2(
    const RawImage& image,
    const std::vector<Minutia>& minutiae
);

} // namespace quality
} // namespace minutiae
