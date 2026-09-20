#include "../minutiae.hpp"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace minutiae {

namespace internal {
    void binarize_integral(const uint8_t* grayscale, uint16_t width, uint16_t height, uint8_t* binary_out);
    void zhang_suen_thinning(uint8_t* binary, uint16_t width, uint16_t height);
    uint8_t compute_crossing_number(const uint8_t* skeleton, uint16_t width, int x, int y);
    float estimate_orientation(const uint8_t* skeleton, uint16_t width, uint16_t height, int x, int y, MinutiaeType type);
    void filter_minutiae(std::vector<Minutia>& minutiae, uint16_t width, uint16_t height, uint8_t margin_px);
}

Matcher::Matcher(const MatcherConfig& config) : m_config(config) {}

bool Matcher::match(
    const std::vector<Minutia>& candidate_minutiae,
    uint16_t candidate_dpcm,
    const BiometricTemplate& enrolled_template,
    uint8_t& out_score
) const {
    out_score = 0;
    if (candidate_minutiae.empty() || enrolled_template.minutiae.empty()) {
        return false;
    }

    uint16_t enrolled_dpcm = enrolled_template.resolution_x_dpcm;
    if (enrolled_dpcm == 0) enrolled_dpcm = DPCM_508_DPI;
    if (candidate_dpcm == 0) candidate_dpcm = DPCM_508_DPI;

    // Physical coordinate conversion:
    // x_um = x_px * (10000.0 / dpcm)
    float cand_scale_um = 10000.0f / static_cast<float>(candidate_dpcm);
    float enr_scale_um  = 10000.0f / static_cast<float>(enrolled_dpcm);

    size_t best_matches = 0;

    // Pairwise alignment with rotation and translation
    for (size_t i = 0; i < candidate_minutiae.size(); ++i) {
        const auto& c_ref = candidate_minutiae[i];
        float c_ref_x_um = c_ref.x * cand_scale_um;
        float c_ref_y_um = c_ref.y * cand_scale_um;

        for (size_t j = 0; j < enrolled_template.minutiae.size(); ++j) {
            const auto& e_ref = enrolled_template.minutiae[j];
            if (c_ref.type != e_ref.type && c_ref.type != MinutiaeType::OTHER && e_ref.type != MinutiaeType::OTHER) {
                continue;
            }

            float e_ref_x_um = e_ref.x * enr_scale_um;
            float e_ref_y_um = e_ref.y * enr_scale_um;

            float d_angle_deg = c_ref.angle_deg - e_ref.angle_deg;
            while (d_angle_deg > 180.0f) d_angle_deg -= 360.0f;
            while (d_angle_deg < -180.0f) d_angle_deg += 360.0f;

            if (std::abs(d_angle_deg) > m_config.max_rotation_deg) {
                continue;
            }

            float rad = d_angle_deg * 3.14159265f / 180.0f;
            float cos_a = std::cos(rad);
            float sin_a = std::sin(rad);

            size_t current_matches = 0;
            std::vector<bool> enrolled_matched(enrolled_template.minutiae.size(), false);

            for (size_t ci = 0; ci < candidate_minutiae.size(); ++ci) {
                const auto& c = candidate_minutiae[ci];
                float cx_rel_um = (c.x * cand_scale_um) - c_ref_x_um;
                float cy_rel_um = (c.y * cand_scale_um) - c_ref_y_um;

                // Rotate candidate relative coordinate
                float rot_x_um = cx_rel_um * cos_a - cy_rel_um * sin_a;
                float rot_y_um = cx_rel_um * sin_a + cy_rel_um * cos_a;

                float cand_aligned_x_um = rot_x_um + e_ref_x_um;
                float cand_aligned_y_um = rot_y_um + e_ref_y_um;

                float best_dist_um = m_config.physical_distance_threshold_um;
                int best_idx = -1;

                for (size_t ej = 0; ej < enrolled_template.minutiae.size(); ++ej) {
                    if (enrolled_matched[ej]) continue;

                    const auto& e = enrolled_template.minutiae[ej];
                    float ex_um = e.x * enr_scale_um;
                    float ey_um = e.y * enr_scale_um;

                    float dist_um = std::hypot(cand_aligned_x_um - ex_um, cand_aligned_y_um - ey_um);
                    if (dist_um < best_dist_um) {
                        best_dist_um = dist_um;
                        best_idx = static_cast<int>(ej);
                    }
                }

                if (best_idx >= 0) {
                    enrolled_matched[best_idx] = true;
                    current_matches++;
                }
            }

            if (current_matches > best_matches) {
                best_matches = current_matches;
            }
        }
    }

    size_t min_count = std::min(candidate_minutiae.size(), enrolled_template.minutiae.size());
    if (min_count == 0) return false;

    float score_ratio = static_cast<float>(best_matches) / static_cast<float>(min_count);
    out_score = static_cast<uint8_t>(std::clamp(std::round(score_ratio * 100.0f), 0.0f, 100.0f));

    return out_score >= m_config.match_score_threshold;
}

bool Matcher::adapt(
    BiometricTemplate& enrolled_template,
    const std::vector<Minutia>& candidate_minutiae,
    uint16_t candidate_dpcm,
    uint8_t match_score
) const {
    if (match_score < m_config.adaptation_score_threshold) {
        return false;
    }

    uint16_t enr_dpcm = enrolled_template.resolution_x_dpcm;
    if (enr_dpcm == 0) enr_dpcm = DPCM_508_DPI;
    if (candidate_dpcm == 0) candidate_dpcm = DPCM_508_DPI;

    float cand_to_enr = static_cast<float>(enr_dpcm) / static_cast<float>(candidate_dpcm);

    // Merge high-quality new minutiae that don't already exist
    for (const auto& c : candidate_minutiae) {
        if (c.quality < 60) continue;

        uint16_t mapped_x = static_cast<uint16_t>(c.x * cand_to_enr);
        uint16_t mapped_y = static_cast<uint16_t>(c.y * cand_to_enr);

        bool exists = false;
        for (const auto& e : enrolled_template.minutiae) {
            int dx = static_cast<int>(mapped_x) - static_cast<int>(e.x);
            int dy = static_cast<int>(mapped_y) - static_cast<int>(e.y);
            if ((dx * dx + dy * dy) < 36) { // within 6 pixels
                exists = true;
                break;
            }
        }

        if (!exists && enrolled_template.minutiae.size() < m_config.max_template_minutiae) {
            Minutia new_m = c;
            new_m.x = mapped_x;
            new_m.y = mapped_y;
            enrolled_template.minutiae.push_back(new_m);
        }
    }

    enrolled_template.adaptation_count++;
    return true;
}

bool Matcher::detect_replay(const RawImage& current, const RawImage& previous, float max_identical_ratio) {
    if (!current.is_valid() || !previous.is_valid()) return false;
    if (current.data.size() != previous.data.size()) return false;

    size_t identical = 0;
    size_t total = current.data.size();

    for (size_t i = 0; i < total; ++i) {
        if (current.data[i] == previous.data[i]) {
            identical++;
        }
    }

    float ratio = static_cast<float>(identical) / static_cast<float>(total);
    return ratio >= max_identical_ratio;
}

// ----------------------------------------------------------------------------
// High-Level Engine Implementation
// ----------------------------------------------------------------------------

Engine::Engine(const MatcherConfig& config) : m_matcher(config) {}

std::vector<Minutia> Engine::extract_minutiae(const RawImage& image) const {
    std::vector<Minutia> results;
    if (!image.is_valid()) return results;

    uint16_t w = image.width;
    uint16_t h = image.height;

    std::vector<uint8_t> binary(w * h, 0);
    internal::binarize_integral(image.data.data(), w, h, binary.data());

    std::vector<uint8_t> skeleton = binary;
    internal::zhang_suen_thinning(skeleton.data(), w, h);

    for (int y = 2; y < h - 2; ++y) {
        for (int x = 2; x < w - 2; ++x) {
            if (skeleton[y * w + x] == 0) continue;

            uint8_t cn = internal::compute_crossing_number(skeleton.data(), w, x, y);
            if (cn == 1 || cn == 3) {
                Minutia m;
                m.x = static_cast<uint16_t>(x);
                m.y = static_cast<uint16_t>(y);
                m.type = (cn == 1) ? MinutiaeType::ENDING : MinutiaeType::BIFURCATION;
                m.angle_deg = internal::estimate_orientation(skeleton.data(), w, h, x, y, m.type);
                m.quality = 80;
                results.push_back(m);
            }
        }
    }

    internal::filter_minutiae(results, w, h, 8);
    return results;
}

quality::NFIQ2Report Engine::evaluate_quality(
    const RawImage& image,
    const std::vector<Minutia>& minutiae
) const {
    return quality::evaluate_nfiq2(image, minutiae);
}

BiometricTemplate Engine::create_template(
    const RawImage& image,
    const std::vector<Minutia>& minutiae,
    FingerPosition position,
    standards::StandardFormat format
) const {
    BiometricTemplate tmpl;
    tmpl.sensor_width_px = image.width;
    tmpl.sensor_height_px = image.height;
    tmpl.resolution_x_dpcm = image.resolution_dpcm;
    tmpl.resolution_y_dpcm = image.resolution_dpcm;
    tmpl.position = position;
    tmpl.minutiae = minutiae;

    auto q = quality::evaluate_nfiq2(image, minutiae);
    tmpl.overall_quality = q.overall_score;

    if (format == standards::StandardFormat::ISO_19794_2_2011) {
        tmpl.format_version = ISO_19794_2_VERSION_2011;
    } else if (format == standards::StandardFormat::ANSI_378_2009) {
        tmpl.format_version = ANSI_378_VERSION_2009;
    } else {
        tmpl.format_version = ISO_19794_2_VERSION_2005;
    }

    return tmpl;
}

bool Engine::verify(
    const BiometricTemplate& candidate,
    const BiometricTemplate& enrolled,
    uint8_t& out_score
) const {
    return m_matcher.match(candidate.minutiae, candidate.resolution_x_dpcm, enrolled, out_score);
}

bool Engine::adapt(
    BiometricTemplate& enrolled,
    const BiometricTemplate& candidate,
    uint8_t match_score
) const {
    return m_matcher.adapt(enrolled, candidate.minutiae, candidate.resolution_x_dpcm, match_score);
}

bool Engine::check_replay(const RawImage& current, const RawImage& previous) const {
    return Matcher::detect_replay(current, previous);
}

bool Engine::save_fmr_file(const std::string& filepath, const BiometricTemplate& tmpl, standards::StandardFormat format) {
    std::vector<uint8_t> buffer(4096);
    size_t written = standards::encode_fmr(tmpl, format, buffer.data(), buffer.size());
    if (written == 0) return false;

    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) return false;

    ofs.write(reinterpret_cast<const char*>(buffer.data()), written);
    return ofs.good();
}

bool Engine::load_fmr_file(const std::string& filepath, BiometricTemplate& out_tmpl) {
    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs) return false;

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    if (buffer.empty()) return false;

    return standards::decode_fmr(buffer.data(), buffer.size(), out_tmpl);
}

} // namespace minutiae
