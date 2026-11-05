#pragma once
#include <stdint.h>
#include <stddef.h>
#include <PlantSoilProtocol.h>
namespace plant {
constexpr uint64_t kHistoryMs = 30ULL * 24 * 60 * 60 * 1000;
struct HistoryRecord {
    uint32_t boot = 0, session = 0, sequence = 0;
    uint64_t uptimeMs = 0, utcMs = 0;
    int16_t raw = 0;
    uint8_t node = 0, status = 0;
};
// All calls belong to the storage/network worker, never the radio loop.
class HistoryFlash {
public:
    virtual ~HistoryFlash() {}
    virtual size_t size() const = 0;
    virtual bool read(size_t offset, void* data, size_t length) = 0;
    virtual bool write(size_t offset, const void* data, size_t length) = 0;
    virtual bool erase(size_t offset, size_t length) = 0;
};
class PlantHistory {
public:
    enum class State { Ready, UnknownData, IoError, TooSmall };
    explicit PlantHistory(HistoryFlash& flash) : flash_(flash) {}
    bool begin();
    bool append(const HistoryRecord& value);
    // Cursor is a physical monotonic record number. Requests may scan bounded flash,
    // and are limited to 96 returned records; interrupted writes remain gaps.
    size_t page(uint8_t node, uint64_t after, uint64_t nowUtc, uint64_t periodMs,
                HistoryRecord* out, size_t limit, uint64_t& next, bool& more);
    State state() const { return state_; }
    uint32_t corruptRecords() const { return corrupt_; }
    static const char* stateName(State state);
    static uint32_t crc(const uint8_t* bytes, size_t size);
private:
    static constexpr size_t kSector = 4096, kHeader = 32, kRecord = 40;
    static constexpr size_t kPerSector = (kSector - kHeader) / kRecord;
    bool startSector(uint32_t generation);
    bool sectorGeneration(size_t sector, uint32_t& generation);
    bool decode(const uint8_t* bytes, HistoryRecord& value) const;
    HistoryFlash& flash_;
    State state_ = State::IoError;
    uint32_t generation_ = 0, corrupt_ = 0;
    size_t slot_ = kPerSector, sectors_ = 0;
};
}
