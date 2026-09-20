#pragma once

#include "types.hpp"

namespace minutiae {

struct MatcherConfig {
    // Physical spatial threshold in micrometers (um)
    // Human epidermal ridge pitch is ~350 to 500 um.
    // 200 um represents approx 4 pixels on a 508 DPI sensor (half ridge pitch).
    float physical_distance_threshold_um = 200.0f;
    float max_rotation_deg = 25.0f;          // Rotation alignment search window (+/- deg)
    float max_angle_tolerance_deg = 20.0f;   // Minutia orientation tolerance (+/- deg)
    uint8_t match_score_threshold = 55;      // Minimum score (0-100) for a positive match
    uint8_t adaptation_score_threshold = 80; // Minimum score required to trigger self-learning
    uint16_t max_template_minutiae = 80;     // Maximum minutiae points stored
    uint8_t border_margin_px = 8;            // Exclude border artifacts
};

class Matcher {
public:
    explicit Matcher(const MatcherConfig& config = MatcherConfig());

    // Matches candidate minutiae against an enrolled template
    // Automatically uses native resolutions to convert coordinates to micrometers (um)
    bool match(
        const std::vector<Minutia>& candidate_minutiae,
        uint16_t candidate_dpcm,
        const BiometricTemplate& enrolled_template,
        uint8_t& out_score
    ) const;

    // Adaptive Self-Learning: integrates newly confirmed candidate minutiae into enrolled template
    bool adapt(
        BiometricTemplate& enrolled_template,
        const std::vector<Minutia>& candidate_minutiae,
        uint16_t candidate_dpcm,
        uint8_t match_score
    ) const;

    // Anti-Replay Detection: detects if candidate image is a static replay of previous image
    static bool detect_replay(
        const RawImage& current,
        const RawImage& previous,
        float max_identical_ratio = 0.990f
    );

private:
    MatcherConfig m_config;
};

} // namespace minutiae
