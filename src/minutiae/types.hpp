#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <vector>
#include <string>

namespace minutiae {

// Standard Physical Resolutions
// 500 DPI = 196.85 dpcm (approx 197 dpcm) - Standard FBI / ANSI / SecuGen
// 508 DPI = 200.00 dpcm (exact 200 dpcm) - GROW R558-S, Apple TouchID, capacitive sensors
constexpr uint16_t RESOLUTION_500_DPI = 500;
constexpr uint16_t RESOLUTION_508_DPI = 508;
constexpr uint16_t DPCM_500_DPI = 197;
constexpr uint16_t DPCM_508_DPI = 200;

// Conversion factor between DPI and DPCM: DPI = DPCM * 254 / 100
inline uint16_t dpcm_to_dpi(uint16_t dpcm) {
    return static_cast<uint16_t>((static_cast<uint32_t>(dpcm) * 254 + 50) / 100);
}

inline uint16_t dpi_to_dpcm(uint16_t dpi) {
    return static_cast<uint16_t>((static_cast<uint32_t>(dpi) * 100 + 127) / 254);
}

// 8-bit Grayscale Frame Container
struct RawImage {
    uint16_t width = 0;
    uint16_t height = 0;
    uint16_t resolution_dpcm = DPCM_508_DPI; // Native sensor resolution
    std::vector<uint8_t> data;               // Row-major 8-bit grayscale pixels (0=black, 255=white)

    bool is_valid() const {
        return width > 0 && height > 0 && data.size() == static_cast<size_t>(width * height);
    }
};

// Minutiae Types according to ISO/IEC 19794-2:2005/2011 & ANSI INCITS 378
enum class MinutiaeType : uint8_t {
    OTHER       = 0x00, // Undefined / Unknown
    ENDING      = 0x01, // Ridge ending (Rutovitz Crossing Number CN = 1)
    BIFURCATION = 0x02  // Ridge bifurcation (Rutovitz Crossing Number CN = 3)
};

// Standard Finger Positions (ISO/IEC 19794-2 Table 2 & ANSI 378 Section 6.4)
enum class FingerPosition : uint8_t {
    UNKNOWN         = 0,
    RIGHT_THUMB     = 1,
    RIGHT_INDEX     = 2,
    RIGHT_MIDDLE    = 3,
    RIGHT_RING      = 4,
    RIGHT_LITTLE    = 5,
    LEFT_THUMB      = 6,
    LEFT_INDEX      = 7,
    LEFT_MIDDLE     = 8,
    LEFT_RING       = 9,
    LEFT_LITTLE     = 10
};

// Individual Biometric Feature Minutia
struct Minutia {
    uint16_t x;          // Native X coordinate in image pixel grid (0 to 16383)
    uint16_t y;          // Native Y coordinate in image pixel grid (0 to 16383)
    MinutiaeType type;   // ENDING (01b) or BIFURCATION (10b)
    float angle_deg;     // Ridge orientation angle (0.0 to 359.9 deg, counter-clockwise)
    uint8_t quality;     // Feature quality score (0 to 100)
};

// Standard Format Magic & Version Headers
constexpr uint32_t FMR_MAGIC_ID              = 0x464D5200; // "FMR\0"
constexpr uint32_t ISO_19794_2_VERSION_2005  = 0x20323000; // " 20\0" (ISO/IEC 19794-2:2005)
constexpr uint32_t ISO_19794_2_VERSION_2011  = 0x20333000; // " 30\0" (ISO/IEC 19794-2:2011)
constexpr uint32_t ANSI_378_VERSION_2004     = 0x20323000; // " 20\0" (ANSI INCITS 378-2004)
constexpr uint32_t ANSI_378_VERSION_2009     = 0x20333500; // " 35\0" (ANSI INCITS 378-2009)

// Standard Biometric Template Container
struct BiometricTemplate {
    uint32_t magic = FMR_MAGIC_ID;
    uint32_t format_version = ISO_19794_2_VERSION_2005;

    // Physical Sensor Geometry
    uint16_t sensor_width_px = 96;
    uint16_t sensor_height_px = 100;
    uint16_t resolution_x_dpcm = DPCM_508_DPI; // 200 dpcm = 508 DPI
    uint16_t resolution_y_dpcm = DPCM_508_DPI;

    FingerPosition position = FingerPosition::RIGHT_INDEX;
    uint8_t impression_type = 0;         // 0 = Live-scan plain
    uint8_t overall_quality = 80;        // NFIQ 2 Quality Score (0 to 100)
    uint16_t template_id = 1;
    uint32_t timestamp = 0;
    uint16_t adaptation_count = 0;       // Adaptive self-learning updates count
    std::vector<Minutia> minutiae;

    // Standards Export Methods
    size_t export_iso_19794_2_2005(uint8_t* out_buf, size_t max_len) const;
    size_t export_iso_19794_2_2011(uint8_t* out_buf, size_t max_len) const;
    size_t export_ansi_378_2004(uint8_t* out_buf, size_t max_len) const;
    size_t export_ansi_378_2009(uint8_t* out_buf, size_t max_len) const;

    // Standards Import Method (Auto-detects ISO 2005/2011, ANSI 2004/2009)
    bool import_standard_fmr(const uint8_t* in_buf, size_t len);
};

} // namespace minutiae
