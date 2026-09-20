#include "../minutiae/transport.hpp"

#ifdef _WIN32
#include <windows.h>
#include <iostream>

namespace minutiae {

class Win32SerialTransport : public ITransport {
public:
    Win32SerialTransport() : m_handle(INVALID_HANDLE_VALUE) {}

    ~Win32SerialTransport() override {
        close();
    }

    bool open(const std::string& port_name, uint32_t baud_rate) override {
        close();

        std::string full_name = port_name;
        if (full_name.find("\\\\.\\") == std::string::npos && full_name.rfind("COM", 0) == 0) {
            full_name = "\\\\.\\" + full_name;
        }

        m_handle = CreateFileA(
            full_name.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );

        if (m_handle == INVALID_HANDLE_VALUE) {
            return false;
        }

        DCB dcb;
        memset(&dcb, 0, sizeof(dcb));
        dcb.DCBlength = sizeof(dcb);

        if (!GetCommState(m_handle, &dcb)) {
            close();
            return false;
        }

        dcb.BaudRate = baud_rate;
        dcb.ByteSize = 8;
        dcb.Parity   = NOPARITY;
        dcb.StopBits = TWOSTOPBITS; // R558-S standard: 8N2

        dcb.fBinary = TRUE;
        dcb.fDtrControl = DTR_CONTROL_ENABLE;
        dcb.fRtsControl = RTS_CONTROL_ENABLE;
        dcb.fOutxCtsFlow = FALSE;
        dcb.fOutxDsrFlow = FALSE;

        if (!SetCommState(m_handle, &dcb)) {
            close();
            return false;
        }

        COMMTIMEOUTS timeouts;
        memset(&timeouts, 0, sizeof(timeouts));
        timeouts.ReadIntervalTimeout = 50;
        timeouts.ReadTotalTimeoutMultiplier = 2;
        timeouts.ReadTotalTimeoutConstant = 200;
        timeouts.WriteTotalTimeoutMultiplier = 2;
        timeouts.WriteTotalTimeoutConstant = 100;

        SetCommTimeouts(m_handle, &timeouts);
        PurgeComm(m_handle, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);

        return true;
    }

    void close() override {
        if (m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    bool is_open() const override {
        return m_handle != INVALID_HANDLE_VALUE;
    }

    size_t write(const uint8_t* data, size_t len) override {
        if (!is_open() || !data || len == 0) return 0;

        DWORD written = 0;
        if (WriteFile(m_handle, data, static_cast<DWORD>(len), &written, nullptr)) {
            return static_cast<size_t>(written);
        }
        return 0;
    }

    size_t read(uint8_t* buffer, size_t max_len, uint32_t timeout_ms) override {
        if (!is_open() || !buffer || max_len == 0) return 0;

        COMMTIMEOUTS timeouts;
        memset(&timeouts, 0, sizeof(timeouts));
        timeouts.ReadIntervalTimeout = 20;
        timeouts.ReadTotalTimeoutMultiplier = 1;
        timeouts.ReadTotalTimeoutConstant = timeout_ms;
        SetCommTimeouts(m_handle, &timeouts);

        size_t total_read = 0;
        while (total_read < max_len) {
            DWORD bytes_read = 0;
            DWORD to_read = static_cast<DWORD>(max_len - total_read);
            if (!ReadFile(m_handle, buffer + total_read, to_read, &bytes_read, nullptr)) {
                break;
            }
            if (bytes_read == 0) {
                break; // Timeout
            }
            total_read += bytes_read;
        }

        return total_read;
    }

    void flush() override {
        if (is_open()) {
            FlushFileBuffers(m_handle);
        }
    }

    void purge_buffers() override {
        if (is_open()) {
            PurgeComm(m_handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
        }
    }

private:
    HANDLE m_handle;
};

std::unique_ptr<ITransport> create_serial_transport() {
    return std::make_unique<Win32SerialTransport>();
}

} // namespace minutiae

#else
// POSIX Serial fallback
namespace minutiae {
std::unique_ptr<ITransport> create_serial_transport() {
    return nullptr;
}
}
#endif
