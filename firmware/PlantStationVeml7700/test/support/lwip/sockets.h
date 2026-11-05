#pragma once
#include <stddef.h>
#include <errno.h>
#include <string>
// Match the installed ESP32-S3 lwIP contract; the Python test also checks it.
#define MSG_DONTWAIT 0x08
namespace host {
struct SocketSend {
    int result=0, error=0, calls=0, fd=-1, flags=0;
    size_t amount=0;
    std::string bytes;
};
inline SocketSend& socketSend() {static SocketSend state;return state;}
}
inline int send(int fd,const void* data,size_t size,int flags) {
    auto& state=host::socketSend();++state.calls;state.fd=fd;state.flags=flags;state.amount=size;
    const int result=state.result>int(size)?int(size):state.result;
    if(result>0) state.bytes.append(static_cast<const char*>(data),result);
    errno=state.error;return result;
}
