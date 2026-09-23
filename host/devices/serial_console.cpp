#include "serial_console.h"

namespace vmarea {

void SerialConsole::handleWrite(uint16_t port, uint8_t data) {
    if (port == COM1_BASE) {
        char c = static_cast<char>(data);
        buffer_ += c;
        if (callback_) {
            callback_(c);
        }
    }
    // Phase 1: Ignore writes to other COM1 registers (0x3F9–0x3FF)
}

bool SerialConsole::ownsPort(uint16_t port) const {
    return port >= COM1_BASE && port <= COM1_END;
}

const std::string& SerialConsole::output() const {
    return buffer_;
}

void SerialConsole::clear() {
    buffer_.clear();
}

void SerialConsole::setOutputCallback(OutputCallback cb) {
    callback_ = std::move(cb);
}

} // namespace vmarea
