#include "web/NonBlockingHttp.h"
#include <assert.h>
#include <stdio.h>
#include <string>
int main() {
    const std::string response="HTTP/1.1 200 OK\r\nContent-Length: 4\r\n\r\ntest";
    auto& socket=host::socketSend();size_t sent=0;
    // The production sender is compiled and executed; only the SDK syscall is simulated.
    for(const int transient:{EAGAIN,EWOULDBLOCK,EINTR}) {
        socket.result=-1;socket.error=transient;const int before=socket.calls;
        const auto result=plant::sendHttpChunk(7,response.data(),response.size());
        assert(!result.closed && result.written==0 && socket.calls==before+1);
        assert(socket.flags==MSG_DONTWAIT && socket.fd==7);
    }
    while(sent<response.size()) {
        const size_t pending=response.size()-sent;socket.result=pending>3?3:pending;
        const auto result=plant::sendHttpChunk(7,response.data()+sent,pending);
        assert(!result.closed && result.written==size_t(socket.result));sent+=result.written;
    }
    assert(socket.bytes==response);
    const std::string large(2048,'x');socket.result=-1;socket.error=EAGAIN;
    assert(!plant::sendHttpChunk(7,large.data(),large.size()).closed && socket.amount==512);
    socket.error=ECONNRESET;assert(plant::sendHttpChunk(7,response.data(),response.size()).closed);
    socket.result=0;socket.error=EAGAIN;assert(plant::sendHttpChunk(7,response.data(),response.size()).closed);
    const int calls=socket.calls;
    assert(plant::sendHttpChunk(-1,response.data(),response.size()).closed && socket.calls==calls);
    assert(!plant::sendHttpChunk(7,response.data(),0).closed && socket.calls==calls);
    puts("PASS production HTTP socket sender: one MSG_DONTWAIT call/tick, complete partial response, EAGAIN/EWOULDBLOCK/EINTR retry, 512-byte bound, close/error handling (SDK syscall simulated)");
}
