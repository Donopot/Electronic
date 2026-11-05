#pragma once
#ifdef ARDUINO
// Arduino's IPAddress declaration must precede lwIP's INADDR_NONE macro.
#include <WiFiClient.h>
#endif
#include <lwip/sockets.h>
#include <errno.h>
#include <stddef.h>

namespace plant {
struct HttpSendResult {
    size_t written;
    bool closed;
};
// WiFiClient inherits Print::availableForWrite() == 0 in Arduino-ESP32 2.0.17.
// WiFiClient::write() has its own retry/select loop. Use its public socket fd
// for exactly one nonblocking send per worker tick, without either API.
inline HttpSendResult sendHttpChunk(int socket, const char* data, size_t remaining) {
    if (socket < 0) return {0, true};
    if (!remaining) return {0, false};
    const size_t amount = remaining < 512 ? remaining : 512;
    const int count = ::send(socket, data, amount, MSG_DONTWAIT);
    if (count > 0) return {static_cast<size_t>(count), false};
    const int reason = errno;
    if (count < 0 && (reason == EAGAIN || reason == EWOULDBLOCK || reason == EINTR))
        return {0, false};
    return {0, true};
}
}
