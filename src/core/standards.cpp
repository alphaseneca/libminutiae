#include "../minutiae/standards.hpp"
#include <cstring>
#include <algorithm>
#include <cmath>

namespace minutiae {

// Helper Big-Endian writers
static inline void write_be16(uint8_t* p, uint16_t val) {
    p[0] = static_cast<uint8_t>((val >> 8) & 0xFF);
    p[1] = static_cast<uint8_t>(val & 0xFF);
}

static inline void write_be32(uint8_t* p, uint32_t val) {
    p[0] = static_cast<uint8_t>((val >> 24) & 0xFF);
    p[1] = static_cast<uint8_t>((val >> 16) & 0xFF);
    p[2] = static_cast<uint8_t>((val >> 8) & 0xFF);
    p[3] = static_cast<uint8_t>(val & 0xFF);
}

static inline uint16_t read_be16(const uint8_t* p) {
    return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);
}

static inline uint32_t read_be32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8)  |
            static_cast<uint32_t>(p[3]);
}

// ----------------------------------------------------------------------------
// BiometricTemplate Standards Encoders
// ----------------------------------------------------------------------------

size_t BiometricTemplate::export_iso_19794_2_2005(uint8_t* out_buf, size_t max_len) const {
    size_t count = minutiae.size();
    size_t record_len = 24 + 4 + 2 + (count * 6) + 2; // Header (24) + FingerView (6) + Minutiae (N*6) + Extended (2)

    if (max_len < record_len) return 0;

    uint8_t* p = out_buf;

    // Record Header (24 Bytes)
    memcpy(p, "FMR\0", 4); p += 4;
    memcpy(p, " 20\0", 4); p += 4;
    write_be32(p, static_cast<uint32_t>(record_len)); p += 4;
    write_be16(p, 0x0000); p += 2; // Capture device ID
    write_be16(p, sensor_width_px); p += 2;
    write_be16(p, sensor_height_px); p += 2;
    write_be16(p, resolution_x_dpcm); p += 2;
    write_be16(p, resolution_y_dpcm); p += 2;
    *p++ = 1; // Number of finger views
    *p++ = 0; // Reserved

    // Single Finger View Header (6 Bytes)
    *p++ = static_cast<uint8_t>(position);
    *p++ = ((impression_type & 0x0F) << 4); // View #0 & Impression
    *p++ = overall_quality;
    *p++ = static_cast<uint8_t>(count);

    // Minutiae Records (6 Bytes each)
    for (const auto& m : minutiae) {
        uint8_t type_bits = (m.type == MinutiaeType::ENDING) ? 0x01 :
                            (m.type == MinutiaeType::BIFURCATION) ? 0x02 : 0x00;
        uint16_t x_field = ((type_bits & 0x03) << 14) | (m.x & 0x3FFF);
        write_be16(p, x_field); p += 2;
        write_be16(p, m.y & 0x3FFF); p += 2;

        // Angle in units of 2*pi / 256 (0..255)
        uint8_t angle_unit = static_cast<uint8_t>(std::round(m.angle_deg * 256.0f / 360.0f)) % 256;
        *p++ = angle_unit;
        *p++ = m.quality;
    }

    // Extended Data Block Length (2 Bytes = 0)
    write_be16(p, 0x0000); p += 2;

    return record_len;
}

size_t BiometricTemplate::export_iso_19794_2_2011(uint8_t* out_buf, size_t max_len) const {
    size_t count = minutiae.size();
    size_t record_len = 24 + 4 + 2 + (count * 6) + 2;

    if (max_len < record_len) return 0;

    uint8_t* p = out_buf;

    // ISO 19794-2:2011 uses " 30\0"
    memcpy(p, "FMR\0", 4); p += 4;
    memcpy(p, " 30\0", 4); p += 4;
    write_be32(p, static_cast<uint32_t>(record_len)); p += 4;
    write_be16(p, 0x0000); p += 2;
    write_be16(p, sensor_width_px); p += 2;
    write_be16(p, sensor_height_px); p += 2;
    write_be16(p, resolution_x_dpcm); p += 2;
    write_be16(p, resolution_y_dpcm); p += 2;
    *p++ = 1;
    *p++ = 0;

    *p++ = static_cast<uint8_t>(position);
    *p++ = ((impression_type & 0x0F) << 4);
    *p++ = overall_quality;
    *p++ = static_cast<uint8_t>(count);

    for (const auto& m : minutiae) {
        uint8_t type_bits = (m.type == MinutiaeType::ENDING) ? 0x01 :
                            (m.type == MinutiaeType::BIFURCATION) ? 0x02 : 0x00;
        uint16_t x_field = ((type_bits & 0x03) << 14) | (m.x & 0x3FFF);
        write_be16(p, x_field); p += 2;
        write_be16(p, m.y & 0x3FFF); p += 2;
        uint8_t angle_unit = static_cast<uint8_t>(std::round(m.angle_deg * 256.0f / 360.0f)) % 256;
        *p++ = angle_unit;
        *p++ = m.quality;
    }

    write_be16(p, 0x0000); p += 2;
    return record_len;
}

size_t BiometricTemplate::export_ansi_378_2004(uint8_t* out_buf, size_t max_len) const {
    size_t count = minutiae.size();
    size_t record_len = 26 + (count * 6) + 2;

    if (max_len < record_len) return 0;

    uint8_t* p = out_buf;

    // ANSI INCITS 378-2004 Header
    memcpy(p, "FMR\0", 4); p += 4;
    memcpy(p, " 20\0", 4); p += 4;
    write_be32(p, static_cast<uint32_t>(record_len)); p += 4;
    write_be16(p, 0x0000); p += 2; // Product ID
    write_be16(p, 0x0000); p += 2; // Equipment Compliance
    write_be16(p, sensor_width_px); p += 2;
    write_be16(p, sensor_height_px); p += 2;
    write_be16(p, resolution_x_dpcm); p += 2;
    write_be16(p, resolution_y_dpcm); p += 2;
    *p++ = 1;
    *p++ = 0;

    // Finger View Header
    *p++ = static_cast<uint8_t>(position);
    *p++ = ((impression_type & 0x0F) << 4);
    *p++ = overall_quality;
    *p++ = static_cast<uint8_t>(count);

    for (const auto& m : minutiae) {
        uint8_t type_bits = (m.type == MinutiaeType::ENDING) ? 0x01 :
                            (m.type == MinutiaeType::BIFURCATION) ? 0x02 : 0x00;
        uint16_t x_field = ((type_bits & 0x03) << 14) | (m.x & 0x3FFF);
        write_be16(p, x_field); p += 2;
        write_be16(p, m.y & 0x3FFF); p += 2;
        uint8_t angle_unit = static_cast<uint8_t>(std::round(m.angle_deg * 256.0f / 360.0f)) % 256;
        *p++ = angle_unit;
        *p++ = m.quality;
    }

    write_be16(p, 0x0000); p += 2;
    return record_len;
}

size_t BiometricTemplate::export_ansi_378_2009(uint8_t* out_buf, size_t max_len) const {
    size_t count = minutiae.size();
    size_t record_len = 26 + (count * 6) + 2;

    if (max_len < record_len) return 0;

    uint8_t* p = out_buf;

    // ANSI INCITS 378-2009 Header uses " 35\0"
    memcpy(p, "FMR\0", 4); p += 4;
    memcpy(p, " 35\0", 4); p += 4;
    write_be32(p, static_cast<uint32_t>(record_len)); p += 4;
    write_be16(p, 0x0000); p += 2;
    write_be16(p, 0x0000); p += 2;
    write_be16(p, sensor_width_px); p += 2;
    write_be16(p, sensor_height_px); p += 2;
    write_be16(p, resolution_x_dpcm); p += 2;
    write_be16(p, resolution_y_dpcm); p += 2;
    *p++ = 1;
    *p++ = 0;

    *p++ = static_cast<uint8_t>(position);
    *p++ = ((impression_type & 0x0F) << 4);
    *p++ = overall_quality;
    *p++ = static_cast<uint8_t>(count);

    for (const auto& m : minutiae) {
        uint8_t type_bits = (m.type == MinutiaeType::ENDING) ? 0x01 :
                            (m.type == MinutiaeType::BIFURCATION) ? 0x02 : 0x00;
        uint16_t x_field = ((type_bits & 0x03) << 14) | (m.x & 0x3FFF);
        write_be16(p, x_field); p += 2;
        write_be16(p, m.y & 0x3FFF); p += 2;
        uint8_t angle_unit = static_cast<uint8_t>(std::round(m.angle_deg * 256.0f / 360.0f)) % 256;
        *p++ = angle_unit;
        *p++ = m.quality;
    }

    write_be16(p, 0x0000); p += 2;
    return record_len;
}

// ----------------------------------------------------------------------------
// Universal Standards Deserializer
// ----------------------------------------------------------------------------
bool BiometricTemplate::import_standard_fmr(const uint8_t* in_buf, size_t len) {
    if (len < 30) return false;

    // Check Magic "FMR\0"
    if (memcmp(in_buf, "FMR\0", 4) != 0) return false;

    uint32_t ver = read_be32(in_buf + 4);
    format_version = ver;

    size_t header_len = 24;
    // Check if ANSI 378 (which has 2 extra bytes for compliance/product ID)
    if (ver == ANSI_378_VERSION_2009) {
        header_len = 26;
    }

    sensor_width_px = read_be16(in_buf + header_len - 10);
    sensor_height_px = read_be16(in_buf + header_len - 8);
    resolution_x_dpcm = read_be16(in_buf + header_len - 6);
    resolution_y_dpcm = read_be16(in_buf + header_len - 4);

    const uint8_t* fv = in_buf + header_len;
    position = static_cast<FingerPosition>(fv[0]);
    impression_type = (fv[1] >> 4) & 0x0F;
    overall_quality = fv[2];
    uint8_t minutiae_count = fv[3];

    const uint8_t* m_ptr = fv + 4;
    minutiae.clear();
    minutiae.reserve(minutiae_count);

    for (uint8_t i = 0; i < minutiae_count; ++i) {
        if (m_ptr + 6 > in_buf + len) break;

        uint16_t x_field = read_be16(m_ptr);
        uint16_t y_field = read_be16(m_ptr + 2);
        uint8_t angle_unit = m_ptr[4];
        uint8_t q = m_ptr[5];

        uint8_t type_bits = (x_field >> 14) & 0x03;
        MinutiaeType m_type = MinutiaeType::OTHER;
        if (type_bits == 0x01) m_type = MinutiaeType::ENDING;
        else if (type_bits == 0x02) m_type = MinutiaeType::BIFURCATION;

        Minutia m;
        m.x = x_field & 0x3FFF;
        m.y = y_field & 0x3FFF;
        m.type = m_type;
        m.angle_deg = static_cast<float>(angle_unit) * 360.0f / 256.0f;
        m.quality = q;

        minutiae.push_back(m);
        m_ptr += 6;
    }

    return true;
}

namespace standards {

size_t encode_fmr(const BiometricTemplate& tmpl, StandardFormat format, uint8_t* out_buffer, size_t max_len) {
    switch (format) {
        case StandardFormat::ISO_19794_2_2005:
            return tmpl.export_iso_19794_2_2005(out_buffer, max_len);
        case StandardFormat::ISO_19794_2_2011:
            return tmpl.export_iso_19794_2_2011(out_buffer, max_len);
        case StandardFormat::ANSI_378_2004:
            return tmpl.export_ansi_378_2004(out_buffer, max_len);
        case StandardFormat::ANSI_378_2009:
            return tmpl.export_ansi_378_2009(out_buffer, max_len);
        default:
            return tmpl.export_iso_19794_2_2005(out_buffer, max_len);
    }
}

bool decode_fmr(const uint8_t* in_buffer, size_t len, BiometricTemplate& out_tmpl) {
    return out_tmpl.import_standard_fmr(in_buffer, len);
}

} // namespace standards
} // namespace minutiae
