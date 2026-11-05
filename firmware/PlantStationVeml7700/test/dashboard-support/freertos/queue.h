#pragma once
#include <deque>
#include <vector>
#include <string.h>
struct FakeQueue {size_t capacity,itemSize;std::deque<std::vector<uint8_t>> items;};
using QueueHandle_t=FakeQueue*;
inline QueueHandle_t xQueueCreate(size_t n,size_t size){return new FakeQueue{n,size,{}};}
inline int xQueueSend(QueueHandle_t q,const void* p,int){if(q->items.size()==q->capacity)return 0;const auto* b=static_cast<const uint8_t*>(p);q->items.emplace_back(b,b+q->itemSize);return 1;}
inline int xQueueOverwrite(QueueHandle_t q,const void* p){q->items.clear();return xQueueSend(q,p,0);}
inline int xQueuePeek(QueueHandle_t q,void* p,int){if(q->items.empty())return 0;memcpy(p,q->items.front().data(),q->itemSize);return 1;}
inline int xQueueReceive(QueueHandle_t q,void* p,int ticks){if(!xQueuePeek(q,p,ticks))return 0;q->items.pop_front();return 1;}
