#pragma once
#include <stdint.h>
#include <functional>
#include <assert.h>
namespace host {
extern uint32_t now;
struct Task {void(*entry)(void*)=nullptr;void* argument=nullptr;unsigned iterations=0,limit=0;bool deleted=false;std::function<void(unsigned)> tick;};
inline Task& task(){static Task t;return t;}
inline void runTask(unsigned limit,const std::function<void(unsigned)>& tick){auto& t=task();assert(t.entry);t.iterations=0;t.limit=limit;t.deleted=false;t.tick=tick;tick(0);t.entry(t.argument);t.tick={};assert(t.deleted);}
}
inline int xTaskCreatePinnedToCore(void(*entry)(void*),const char*,unsigned,void* argument,unsigned,void*,int){host::task().entry=entry;host::task().argument=argument;return 1;}
inline void vTaskDelay(unsigned ms){host::now+=ms;auto& t=host::task();assert(++t.iterations<=t.limit);if(t.tick)t.tick(t.iterations);}
inline void vTaskDelete(void*){host::task().deleted=true;}
