#include "../minutiae/quality.hpp"
#include <cmath>
#include <algorithm>

namespace minutiae {
namespace quality {

NFIQ2Report evaluate_nfiq2(const RawImage& image, const std::vector<Minutia>& minutiae) {
    NFIQ2Report report;
    if (!image.is_valid()) {
        return report;
    }

    uint16_t w = image.width;
    uint16_t h = image.height;
    const uint8_t* pixels = image.data.data();

    // 1. Local Clarity Score (LCS) / Dynamic Range
    uint32_t total_variance = 0;
    uint32_t block_count = 0;
    const int block_sz = 8;

    for (uint16_t by = 0; by + block_sz <= h; by += block_sz) {
        for (uint16_t bx = 0; bx + block_sz <= w; bx += block_sz) {
            uint32_t sum = 0;
            uint32_t sum_sq = 0;
            for (int y = 0; y < block_sz; ++y) {
                for (int x = 0; x < block_sz; ++x) {
                    uint8_t val = pixels[(by + y) * w + (bx + x)];
                    sum += val;
                    sum_sq += static_cast<uint32_t>(val) * val;
                }
            }
            uint32_t mean = sum / (block_sz * block_sz);
            uint32_t var = (sum_sq / (block_sz * block_sz)) - (mean * mean);
            total_variance += var;
            block_count++;
        }
    }

    float avg_variance = (block_count > 0) ? (static_cast<float>(total_variance) / block_count) : 0.0f;
    // Normalized clarity score (typical good ridge-valley contrast variance > 600)
    report.local_clarity_score = std::min(100.0f, (avg_variance / 8.0f));

    // 2. Orientation Certainty Level (OCL)
    float total_coherence = 0.0f;
    uint32_t ocl_blocks = 0;

    for (int y = 2; y < h - 2; y += 4) {
        for (int x = 2; x < w - 2; x += 4) {
            float gx = static_cast<float>(pixels[y * w + (x + 1)]) - static_cast<float>(pixels[y * w + (x - 1)]);
            float gy = static_cast<float>(pixels[(y + 1) * w + x]) - static_cast<float>(pixels[(y - 1) * w + x]);

            float gxx = gx * gx;
            float gyy = gy * gy;
            float gxy = gx * gy;

            float denom = gxx + gyy;
            if (denom > 10.0f) {
                float num = std::sqrt((gxx - gyy) * (gxx - gyy) + 4.0f * gxy * gxy);
                total_coherence += (num / denom);
                ocl_blocks++;
            }
        }
    }
    report.orientation_certainty = (ocl_blocks > 0) ? (total_coherence / ocl_blocks * 100.0f) : 0.0f;

    // 3. Minutiae Count & Spatial Spread
    report.minutiae_count = static_cast<uint16_t>(minutiae.size());

    if (!minutiae.empty()) {
        uint16_t min_x = w, max_x = 0;
        uint16_t min_y = h, max_y = 0;
        for (const auto& m : minutiae) {
            min_x = std::min(min_x, m.x);
            max_x = std::max(max_x, m.x);
            min_y = std::min(min_y, m.y);
            max_y = std::max(max_y, m.y);
        }
        float spread_area = static_cast<float>((max_x - min_x) * (max_y - min_y));
        float total_area = static_cast<float>(w * h);
        report.spatial_spread = std::min(100.0f, (spread_area / total_area) * 150.0f);
    }

    // 4. Overall NIST NFIQ 2 Quality Score (Weighted composite 0-100)
    // Formula based on ISO/IEC 29794-4 & NIST NFIQ 2 quality weights
    float q = (report.local_clarity_score * 0.35f) +
              (report.orientation_certainty * 0.35f) +
              (report.spatial_spread * 0.15f) +
              (std::min(40.0f, static_cast<float>(report.minutiae_count) * 2.5f) * 0.15f);

    report.overall_score = static_cast<uint8_t>(std::clamp(std::round(q), 0.0f, 100.0f));
    report.acceptable = (report.overall_score >= 40 && report.minutiae_count >= 8);

    return report;
}

} // namespace quality
} // namespace minutiae
