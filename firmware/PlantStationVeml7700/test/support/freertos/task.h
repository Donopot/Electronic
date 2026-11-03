#pragma once
#include <assert.h>
namespace host {
struct Task { void (*function)(void*) = nullptr; void* context = nullptr; };
inline Task& task() { static Task pending; return pending; }
inline void finishInitialization() {
    assert(task().function);
    const auto work = task(); task() = Task{};
    work.function(work.context);
}
}
inline int xTaskCreate(void (*function)(void*), const char*, unsigned, void* context, unsigned, void*) {
    assert(!host::task().function); host::task().function = function; host::task().context = context;
    return 1;
}
inline void vTaskDelete(void*) {}
