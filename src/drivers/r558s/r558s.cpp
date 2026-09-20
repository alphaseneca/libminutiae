#include "r558s.hpp"
#include <cstring>
#include <iostream>

namespace minutiae {
namespace drivers {

// R558-S Protocol Constants
constexpr uint16_t PACKET_HEADER = 0xEF01;
constexpr uint8_t PID_COMMAND    = 0x01;
constexpr uint8_t PID_DATA       = 0x02;
constexpr uint8_t PID_ACK        = 0x07;
constexpr uint8_t PID_END_DATA   = 0x08;

// Opcodes
constexpr uint8_t CMD_GET_IMG       = 0x01;
constexpr uint8_t CMD_UP_IMG        = 0x0A;
constexpr uint8_t CMD_READ_SYS_PARA = 0x0F;
constexpr uint8_t CMD_SEARCH        = 0x04;
constexpr uint8_t CMD_EMPTY         = 0x0D;
constexpr uint8_t CMD_GET_RANDOM    = 0x14;
constexpr uint8_t CMD_AUTO_ENROLL   = 0x31;
constexpr uint8_t CMD_READ_SN       = 0x34;
constexpr uint8_t CMD_CONTROL_LED   = 0x3C;

// Return Codes
constexpr uint8_t RC_OK             = 0x00;
constexpr uint8_t RC_NO_FINGER      = 0x02;

R558S::R558S(uint32_t device_address) : m_address(device_address) {}

R558S::~R558S() {
    release();
}

bool R558S::initialize(ITransport& transport) {
    m_transport = &transport;
    m_transport->purge_buffers();

    // Verify communication with ReadSysPara
    DeviceInfo info;
    return get_device_info(info);
}

void R558S::release() {
    if (m_transport) {
        m_transport->close();
        m_transport = nullptr;
    }
}

bool R558S::send_packet(uint8_t pid, const uint8_t* payload, size_t len) {
    if (!m_transport || !m_transport->is_open()) return false;

    uint16_t packet_len = static_cast<uint16_t>(len + 2); // payload + 2 checksum bytes
    std::vector<uint8_t> frame(9 + len + 2);

    frame[0] = static_cast<uint8_t>((PACKET_HEADER >> 8) & 0xFF);
    frame[1] = static_cast<uint8_t>(PACKET_HEADER & 0xFF);
    frame[2] = static_cast<uint8_t>((m_address >> 24) & 0xFF);
    frame[3] = static_cast<uint8_t>((m_address >> 16) & 0xFF);
    frame[4] = static_cast<uint8_t>((m_address >> 8) & 0xFF);
    frame[5] = static_cast<uint8_t>(m_address & 0xFF);
    frame[6] = pid;
    frame[7] = static_cast<uint8_t>((packet_len >> 8) & 0xFF);
    frame[8] = static_cast<uint8_t>(packet_len & 0xFF);

    uint16_t csum = pid + (packet_len >> 8) + (packet_len & 0xFF);
    if (payload && len > 0) {
        memcpy(&frame[9], payload, len);
        for (size_t i = 0; i < len; ++i) {
            csum += payload[i];
        }
    }

    frame[9 + len]     = static_cast<uint8_t>((csum >> 8) & 0xFF);
    frame[9 + len + 1] = static_cast<uint8_t>(csum & 0xFF);

    return m_transport->write(frame.data(), frame.size()) == frame.size();
}

bool R558S::receive_ack(uint8_t& out_rc, std::vector<uint8_t>& out_payload, uint32_t timeout_ms) {
    if (!m_transport || !m_transport->is_open()) return false;

    uint8_t header[9];
    size_t r = m_transport->read(header, 9, timeout_ms);
    if (r < 9) return false;

    uint16_t magic = (static_cast<uint16_t>(header[0]) << 8) | header[1];
    if (magic != PACKET_HEADER) return false;

    uint8_t pid = header[6];
    if (pid != PID_ACK) return false;

    uint16_t packet_len = (static_cast<uint16_t>(header[7]) << 8) | header[8];
    if (packet_len < 3) return false;

    size_t payload_len = packet_len - 2; // contains confirmation code + remaining data
    std::vector<uint8_t> body(packet_len);
    size_t read_body = m_transport->read(body.data(), packet_len, timeout_ms);
    if (read_body < packet_len) return false;

    // Verify Checksum
    uint16_t calc_csum = pid + header[7] + header[8];
    for (size_t i = 0; i < payload_len; ++i) {
        calc_csum += body[i];
    }
    uint16_t rx_csum = (static_cast<uint16_t>(body[payload_len]) << 8) | body[payload_len + 1];
    if (calc_csum != rx_csum) return false;

    out_rc = body[0];
    out_payload.clear();
    if (payload_len > 1) {
        out_payload.assign(body.begin() + 1, body.begin() + payload_len);
    }
    return true;
}

bool R558S::receive_data_stream(std::vector<uint8_t>& out_raw, uint32_t timeout_ms) {
    out_raw.clear();
    bool finished = false;

    while (!finished) {
        uint8_t header[9];
        size_t r = m_transport->read(header, 9, timeout_ms);
        if (r < 9) return false;

        uint16_t magic = (static_cast<uint16_t>(header[0]) << 8) | header[1];
        if (magic != PACKET_HEADER) return false;

        uint8_t pid = header[6];
        if (pid != PID_DATA && pid != PID_END_DATA) return false;

        uint16_t packet_len = (static_cast<uint16_t>(header[7]) << 8) | header[8];
        if (packet_len < 2) return false;

        size_t payload_len = packet_len - 2;
        std::vector<uint8_t> body(packet_len);
        size_t read_body = m_transport->read(body.data(), packet_len, timeout_ms);
        if (read_body < packet_len) return false;

        out_raw.insert(out_raw.end(), body.begin(), body.begin() + payload_len);

        if (pid == PID_END_DATA) {
            finished = true;
        }
    }
    return true;
}

bool R558S::read_chip_unique_id(std::string& out_uid) {
    uint8_t cmd = CMD_READ_SN;
    if (!send_packet(PID_COMMAND, &cmd, 1)) return false;

    uint8_t rc = 0xFF;
    std::vector<uint8_t> payload;
    if (!receive_ack(rc, payload, 1000) || rc != RC_OK) {
        return false;
    }

    out_uid.clear();
    for (uint8_t b : payload) {
        if (b >= 32 && b <= 126) {
            out_uid.push_back(static_cast<char>(b));
        } else {
            char hex[8];
            snprintf(hex, sizeof(hex), "\\x%02X", b);
            out_uid += hex;
        }
    }
    return true;
}

bool R558S::get_device_info(DeviceInfo& out_info) {
    uint8_t cmd = CMD_READ_SYS_PARA;
    if (!send_packet(PID_COMMAND, &cmd, 1)) return false;

    uint8_t rc = 0xFF;
    std::vector<uint8_t> payload;
    if (!receive_ack(rc, payload, 1000) || rc != RC_OK || payload.size() < 16) {
        return false;
    }

    out_info.driver_name = get_driver_name();
    out_info.model_name = get_model_name();
    out_info.width_px = 96;
    out_info.height_px = 100;
    out_info.resolution_dpi = 508;
    out_info.resolution_dpcm = DPCM_508_DPI; // 200 dpcm

    out_info.capacity = (static_cast<uint16_t>(payload[4]) << 8) | payload[5];
    out_info.security_level = (static_cast<uint16_t>(payload[6]) << 8) | payload[7];

    read_chip_unique_id(out_info.chip_unique_id);
    return true;
}

bool R558S::is_finger_present() {
    uint8_t cmd = CMD_GET_IMG;
    if (!send_packet(PID_COMMAND, &cmd, 1)) return false;

    uint8_t rc = 0xFF;
    std::vector<uint8_t> payload;
    if (!receive_ack(rc, payload, 500)) return false;

    return rc == RC_OK;
}

bool R558S::capture_raw_image(RawImage& out_image) {
    // 1. Capture Image in Sensor RAM (0x01)
    uint8_t cmd_get = CMD_GET_IMG;
    if (!send_packet(PID_COMMAND, &cmd_get, 1)) return false;

    uint8_t rc = 0xFF;
    std::vector<uint8_t> payload;
    if (!receive_ack(rc, payload, 1500) || rc != RC_OK) {
        return false;
    }

    // 2. Upload Image over UART (0x0A)
    uint8_t cmd_up = CMD_UP_IMG;
    if (!send_packet(PID_COMMAND, &cmd_up, 1)) return false;

    if (!receive_ack(rc, payload, 1500) || rc != RC_OK) {
        return false;
    }

    // 3. Receive 4,800-byte data stream
    std::vector<uint8_t> packed_stream;
    if (!receive_data_stream(packed_stream, 2500)) {
        return false;
    }

    if (packed_stream.size() < 4800) {
        return false;
    }

    // 4. Unpack 4-bit nibbles into 96x100 8-bit grayscale
    out_image.width = 96;
    out_image.height = 100;
    out_image.resolution_dpcm = DPCM_508_DPI;
    out_image.data.resize(96 * 100);

    size_t out_idx = 0;
    for (size_t i = 0; i < 4800 && out_idx + 1 < out_image.data.size(); ++i) {
        uint8_t b = packed_stream[i];
        out_image.data[out_idx++] = static_cast<uint8_t>(((b >> 4) & 0x0F) * 17);
        out_image.data[out_idx++] = static_cast<uint8_t>((b & 0x0F) * 17);
    }

    return true;
}

bool R558S::set_led(LedMode mode, LedColor start_color, LedColor end_color, uint8_t cycles) {
    if (mode == LedMode::OFF) {
        start_color = LedColor::NONE;
        end_color   = LedColor::NONE;
    } else if (end_color == LedColor::NONE) {
        end_color = start_color;
    }
    uint8_t payload[5];
    payload[0] = CMD_CONTROL_LED;
    payload[1] = static_cast<uint8_t>(mode);
    payload[2] = static_cast<uint8_t>(start_color);
    payload[3] = static_cast<uint8_t>(end_color);
    payload[4] = cycles;

    if (!send_packet(PID_COMMAND, payload, sizeof(payload))) return false;

    uint8_t rc = 0xFF;
    std::vector<uint8_t> resp;
    return receive_ack(rc, resp, 800) && (rc == RC_OK);
}

bool R558S::get_hardware_random(uint32_t& out_random) {
    uint8_t cmd = CMD_GET_RANDOM;
    if (!send_packet(PID_COMMAND, &cmd, 1)) return false;

    uint8_t rc = 0xFF;
    std::vector<uint8_t> payload;
    if (!receive_ack(rc, payload, 800) || rc != RC_OK || payload.size() < 4) {
        return false;
    }

    out_random = (static_cast<uint32_t>(payload[0]) << 24) |
                 (static_cast<uint32_t>(payload[1]) << 16) |
                 (static_cast<uint32_t>(payload[2]) << 8)  |
                  static_cast<uint32_t>(payload[3]);
    return true;
}

bool R558S::auto_enroll(uint16_t page_id, uint8_t num_samples) {
    uint8_t payload[5];
    payload[0] = CMD_AUTO_ENROLL;
    payload[1] = static_cast<uint8_t>((page_id >> 8) & 0xFF);
    payload[2] = static_cast<uint8_t>(page_id & 0xFF);
    payload[3] = num_samples;
    payload[4] = 0x00; // auto-return control flags

    if (!send_packet(PID_COMMAND, payload, sizeof(payload))) return false;

    uint8_t rc = 0xFF;
    std::vector<uint8_t> resp;
    return receive_ack(rc, resp, 10000) && (rc == RC_OK);
}

bool R558S::search_database(uint16_t& out_page_id, uint16_t& out_score) {
    // 0x04 Search: buffer_id (0x01), start_page (0), page_num (200)
    uint8_t payload[6];
    payload[0] = CMD_SEARCH;
    payload[1] = 0x01; // Buffer 1
    payload[2] = 0x00; // Start Page High
    payload[3] = 0x00; // Start Page Low
    payload[4] = 0x00; // Page Num High
    payload[5] = 0xC8; // Page Num Low (200)

    if (!send_packet(PID_COMMAND, payload, sizeof(payload))) return false;

    uint8_t rc = 0xFF;
    std::vector<uint8_t> resp;
    if (!receive_ack(rc, resp, 2000) || rc != RC_OK || resp.size() < 4) {
        return false;
    }

    out_page_id = (static_cast<uint16_t>(resp[0]) << 8) | resp[1];
    out_score   = (static_cast<uint16_t>(resp[2]) << 8) | resp[3];
    return true;
}

bool R558S::empty_database() {
    uint8_t cmd = CMD_EMPTY;
    if (!send_packet(PID_COMMAND, &cmd, 1)) return false;

    uint8_t rc = 0xFF;
    std::vector<uint8_t> resp;
    return receive_ack(rc, resp, 3000) && (rc == RC_OK);
}

} // namespace drivers
} // namespace minutiae
