#include "../minutiae/types.hpp"
#include <cmath>
#include <algorithm>

namespace minutiae {
namespace internal {

// 1. Adaptive Local Mean Binarization using an Integral Image
void binarize_integral(const uint8_t* grayscale, uint16_t width, uint16_t height, uint8_t* binary_out) {
    std::vector<uint32_t> integral((width + 1) * (height + 1), 0);

    for (uint16_t y = 0; y < height; ++y) {
        uint32_t row_sum = 0;
        for (uint16_t x = 0; x < width; ++x) {
            row_sum += grayscale[y * width + x];
            integral[(y + 1) * (width + 1) + (x + 1)] =
                integral[y * (width + 1) + (x + 1)] + row_sum;
        }
    }

    const int window = 15;
    const int half_w = window / 2;
    const int c = 5;

    for (uint16_t y = 0; y < height; ++y) {
        int y1 = std::max(0, static_cast<int>(y) - half_w);
        int y2 = std::min(static_cast<int>(height) - 1, static_cast<int>(y) + half_w);

        for (uint16_t x = 0; x < width; ++x) {
            int x1 = std::max(0, static_cast<int>(x) - half_w);
            int x2 = std::min(static_cast<int>(width) - 1, static_cast<int>(x) + half_w);

            int area = (y2 - y1 + 1) * (x2 - x1 + 1);

            uint32_t sum = integral[(y2 + 1) * (width + 1) + (x2 + 1)]
                         - integral[y1 * (width + 1) + (x2 + 1)]
                         - integral[(y2 + 1) * (width + 1) + x1]
                         + integral[y1 * (width + 1) + x1];

            uint8_t mean = static_cast<uint8_t>(sum / area);
            // Black ridges = 1, white valleys = 0
            binary_out[y * width + x] = (grayscale[y * width + x] < (mean - c)) ? 1 : 0;
        }
    }
}

// 2. Zhang-Suen Morphological Skeleton Thinning
void zhang_suen_thinning(uint8_t* binary, uint16_t width, uint16_t height) {
    bool has_changed = true;
    std::vector<std::pair<int, int>> to_clear;
    to_clear.reserve(1024);

    while (has_changed) {
        has_changed = false;

        // Sub-iteration 1
        to_clear.clear();
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                if (binary[y * width + x] == 0) continue;

                uint8_t p2 = binary[(y - 1) * width + x];
                uint8_t p3 = binary[(y - 1) * width + (x + 1)];
                uint8_t p4 = binary[y * width + (x + 1)];
                uint8_t p5 = binary[(y + 1) * width + (x + 1)];
                uint8_t p6 = binary[(y + 1) * width + x];
                uint8_t p7 = binary[(y + 1) * width + (x - 1)];
                uint8_t p8 = binary[y * width + (x - 1)];
                uint8_t p9 = binary[(y - 1) * width + (x - 1)];

                int b = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
                if (b < 2 || b > 6) continue;

                int a = 0;
                if (!p2 && p3) a++;
                if (!p3 && p4) a++;
                if (!p4 && p5) a++;
                if (!p5 && p6) a++;
                if (!p6 && p7) a++;
                if (!p7 && p8) a++;
                if (!p8 && p9) a++;
                if (!p9 && p2) a++;
                if (a != 1) continue;

                if ((p2 * p4 * p6) == 0 && (p4 * p6 * p8) == 0) {
                    to_clear.emplace_back(x, y);
                }
            }
        }
        for (const auto& pt : to_clear) {
            binary[pt.second * width + pt.first] = 0;
            has_changed = true;
        }

        // Sub-iteration 2
        to_clear.clear();
        for (int y = 1; y < height - 1; ++y) {
            for (int x = 1; x < width - 1; ++x) {
                if (binary[y * width + x] == 0) continue;

                uint8_t p2 = binary[(y - 1) * width + x];
                uint8_t p3 = binary[(y - 1) * width + (x + 1)];
                uint8_t p4 = binary[y * width + (x + 1)];
                uint8_t p5 = binary[(y + 1) * width + (x + 1)];
                uint8_t p6 = binary[(y + 1) * width + x];
                uint8_t p7 = binary[(y + 1) * width + (x - 1)];
                uint8_t p8 = binary[y * width + (x - 1)];
                uint8_t p9 = binary[(y - 1) * width + (x - 1)];

                int b = p2 + p3 + p4 + p5 + p6 + p7 + p8 + p9;
                if (b < 2 || b > 6) continue;

                int a = 0;
                if (!p2 && p3) a++;
                if (!p3 && p4) a++;
                if (!p4 && p5) a++;
                if (!p5 && p6) a++;
                if (!p6 && p7) a++;
                if (!p7 && p8) a++;
                if (!p8 && p9) a++;
                if (!p9 && p2) a++;
                if (a != 1) continue;

                if ((p2 * p4 * p8) == 0 && (p2 * p6 * p8) == 0) {
                    to_clear.emplace_back(x, y);
                }
            }
        }
        for (const auto& pt : to_clear) {
            binary[pt.second * width + pt.first] = 0;
            has_changed = true;
        }
    }
}

// 3. Rutovitz Crossing Number (CN)
// CN = 0.5 * sum(|P_i - P_{i+1}|)
uint8_t compute_crossing_number(const uint8_t* skeleton, uint16_t width, int x, int y) {
    uint8_t p[9];
    p[0] = skeleton[(y - 1) * width + x];
    p[1] = skeleton[(y - 1) * width + (x + 1)];
    p[2] = skeleton[y * width + (x + 1)];
    p[3] = skeleton[(y + 1) * width + (x + 1)];
    p[4] = skeleton[(y + 1) * width + x];
    p[5] = skeleton[(y + 1) * width + (x - 1)];
    p[6] = skeleton[y * width + (x - 1)];
    p[7] = skeleton[(y - 1) * width + (x - 1)];
    p[8] = p[0];

    int sum = 0;
    for (int i = 0; i < 8; ++i) {
        sum += std::abs(static_cast<int>(p[i]) - static_cast<int>(p[i + 1]));
    }
    return static_cast<uint8_t>(sum / 2);
}

// 4. Orientation Estimation for Ridge Ending / Bifurcation
float estimate_orientation(const uint8_t* skeleton, uint16_t width, uint16_t height, int x, int y, MinutiaeType type) {
    const int r = 4;
    float dx = 0.0f;
    float dy = 0.0f;

    for (int dy_i = -r; dy_i <= r; ++dy_i) {
        for (int dx_i = -r; dx_i <= r; ++dx_i) {
            if (dx_i == 0 && dy_i == 0) continue;
            int nx = x + dx_i;
            int ny = y + dy_i;
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                if (skeleton[ny * width + nx] != 0) {
                    dx += static_cast<float>(dx_i);
                    dy += static_cast<float>(dy_i);
                }
            }
        }
    }

    float angle_rad = std::atan2(-dy, dx); // Invert Y for Cartesian coordinates
    if (type == MinutiaeType::ENDING) {
        // Points towards the ridge flow
        angle_rad += 3.14159265f;
    }
    float angle_deg = angle_rad * (180.0f / 3.14159265f);
    while (angle_deg < 0.0f) angle_deg += 360.0f;
    while (angle_deg >= 360.0f) angle_deg -= 360.0f;
    return angle_deg;
}

// 5. Spurious Minutiae Filtering
void filter_minutiae(std::vector<Minutia>& minutiae, uint16_t width, uint16_t height, uint8_t margin_px) {
    std::vector<Minutia> valid;
    valid.reserve(minutiae.size());

    // Filter margin
    for (const auto& m : minutiae) {
        if (m.x >= margin_px && m.x < (width - margin_px) &&
            m.y >= margin_px && m.y < (height - margin_px)) {
            valid.push_back(m);
        }
    }

    // Filter paired endings within small distance (broken ridge artifact)
    std::vector<bool> keep(valid.size(), true);
    for (size_t i = 0; i < valid.size(); ++i) {
        if (!keep[i]) continue;
        for (size_t j = i + 1; j < valid.size(); ++j) {
            if (!keep[j]) continue;
            int dx = static_cast<int>(valid[i].x) - static_cast<int>(valid[j].x);
            int dy = static_cast<int>(valid[i].y) - static_cast<int>(valid[j].y);
            if ((dx * dx + dy * dy) < 25) { // within 5 pixels
                keep[i] = false;
                keep[j] = false;
                break;
            }
        }
    }

    minutiae.clear();
    for (size_t i = 0; i < valid.size(); ++i) {
        if (keep[i]) {
            minutiae.push_back(valid[i]);
        }
    }
}

} // namespace internal
} // namespace minutiae
