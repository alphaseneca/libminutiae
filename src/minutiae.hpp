#pragma once

// Master include for libminutiae
// The Global C++ Biometric Fingerprint Engine & Hardware Driver Framework

#include "minutiae/types.hpp"
#include "minutiae/standards.hpp"
#include "minutiae/quality.hpp"
#include "minutiae/matcher.hpp"
#include "minutiae/driver.hpp"
#include "minutiae/transport.hpp"
#include "drivers/r558s/r558s.hpp"

namespace minutiae {

// High-level Biometric Processing Engine
class Engine {
public:
    explicit Engine(const MatcherConfig& config = MatcherConfig());

    // 1. Minutiae Extraction (Zhang-Suen thinning + Rutovitz Crossing Number)
    std::vector<Minutia> extract_minutiae(const RawImage& image) const;

    // 2. NIST NFIQ 2 / ISO/IEC 29794-4 Quality Scoring
    quality::NFIQ2Report evaluate_quality(
        const RawImage& image,
        const std::vector<Minutia>& minutiae
    ) const;

    // 3. Build a standard BiometricTemplate from raw image & extracted minutiae
    BiometricTemplate create_template(
        const RawImage& image,
        const std::vector<Minutia>& minutiae,
        FingerPosition position = FingerPosition::RIGHT_INDEX,
        standards::StandardFormat format = standards::StandardFormat::ISO_19794_2_2005
    ) const;

    // 4. Biometric Matching (physical coordinate normalization)
    bool verify(
        const BiometricTemplate& candidate,
        const BiometricTemplate& enrolled,
        uint8_t& out_score
    ) const;

    // 5. Template Adaptation / Self-learning
    bool adapt(
        BiometricTemplate& enrolled,
        const BiometricTemplate& candidate,
        uint8_t match_score
    ) const;

    // 6. Anti-Replay Detection
    bool check_replay(const RawImage& current, const RawImage& previous) const;

    // 7. File I/O for standard .fmr files
    static bool save_fmr_file(
        const std::string& filepath,
        const BiometricTemplate& tmpl,
        standards::StandardFormat format = standards::StandardFormat::ISO_19794_2_2005
    );

    static bool load_fmr_file(
        const std::string& filepath,
        BiometricTemplate& out_tmpl
    );

private:
    Matcher m_matcher;
};

} // namespace minutiae
