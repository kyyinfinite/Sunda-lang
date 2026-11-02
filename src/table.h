#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "value.h"

namespace sunda {

struct Entry {
    Value key;
    Value value;
};

struct ObjString;

uint32_t hashValue(Value value);

class Table {
public:
    bool set(Value key, Value value);
    bool get(Value key, Value& value) const;
    bool remove(Value key);
    ObjString* findString(std::string_view chars, uint32_t hash) const;
    size_t liveCount() const { return live; }
    const std::vector<Entry>& raw() const { return entries; }
    std::vector<Entry>& raw() { return entries; }
    void removeIf(bool (*predicate)(Value key));
    void clear() {
        entries.clear();
        used = 0;
        live = 0;
    }

private:
    static size_t findSlot(const std::vector<Entry>& entries, Value key);
    void grow();

    std::vector<Entry> entries;
    size_t used = 0;
    size_t live = 0;
};

}
