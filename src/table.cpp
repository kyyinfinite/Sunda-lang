#include "table.h"

#include "object.h"

namespace sunda {

namespace {

constexpr size_t InitialCapacity = 8;
constexpr size_t NoSlot = static_cast<size_t>(-1);

bool isLiveKey(Value key) {
    return key.bits != EmptyKeyValue.bits && key.bits != TombstoneValue.bits;
}

}

uint32_t hashValue(Value value) {
    if (isString(value)) return asString(value)->hash;
    uint64_t bits = value.bits;
    if (isNumber(value) && asNumber(value) == 0) bits = 0;
    bits ^= bits >> 33;
    bits *= 0xff51afd7ed558ccdULL;
    bits ^= bits >> 33;
    return static_cast<uint32_t>(bits);
}

size_t Table::findSlot(const std::vector<Entry>& entries, Value key) {
    size_t mask = entries.size() - 1;
    size_t index = hashValue(key) & mask;
    size_t tombstone = NoSlot;
    for (;;) {
        const Entry& entry = entries[index];
        if (entry.key.bits == EmptyKeyValue.bits) return tombstone != NoSlot ? tombstone : index;
        if (entry.key.bits == TombstoneValue.bits) {
            if (tombstone == NoSlot) tombstone = index;
        } else if (valuesEqual(entry.key, key)) {
            return index;
        }
        index = (index + 1) & mask;
    }
}

void Table::grow() {
    size_t capacity = entries.empty() ? InitialCapacity : entries.size() * 2;
    std::vector<Entry> fresh(capacity, Entry{EmptyKeyValue, NilValue});
    size_t count = 0;
    for (const Entry& entry : entries) {
        if (!isLiveKey(entry.key)) continue;
        fresh[findSlot(fresh, entry.key)] = entry;
        ++count;
    }
    entries = std::move(fresh);
    used = count;
}

bool Table::set(Value key, Value value) {
    if (entries.empty() || (used + 1) * 4 > entries.size() * 3) grow();
    size_t slot = findSlot(entries, key);
    Entry& entry = entries[slot];
    bool isNew = !isLiveKey(entry.key);
    if (entry.key.bits == EmptyKeyValue.bits) ++used;
    if (isNew) ++live;
    entry.key = key;
    entry.value = value;
    return isNew;
}

bool Table::get(Value key, Value& value) const {
    if (entries.empty()) return false;
    const Entry& entry = entries[findSlot(entries, key)];
    if (!isLiveKey(entry.key)) return false;
    value = entry.value;
    return true;
}

bool Table::remove(Value key) {
    if (entries.empty()) return false;
    Entry& entry = entries[findSlot(entries, key)];
    if (!isLiveKey(entry.key)) return false;
    entry.key = TombstoneValue;
    entry.value = NilValue;
    --live;
    return true;
}

void Table::removeIf(bool (*predicate)(Value key)) {
    for (Entry& entry : entries) {
        if (!isLiveKey(entry.key)) continue;
        if (predicate(entry.key)) {
            entry.key = TombstoneValue;
            entry.value = NilValue;
            --live;
        }
    }
}

ObjString* Table::findString(std::string_view chars, uint32_t hash) const {
    if (entries.empty()) return nullptr;
    size_t mask = entries.size() - 1;
    size_t index = hash & mask;
    for (;;) {
        const Entry& entry = entries[index];
        if (entry.key.bits == EmptyKeyValue.bits) return nullptr;
        if (isString(entry.key)) {
            ObjString* candidate = asString(entry.key);
            if (candidate->hash == hash && candidate->chars == chars) return candidate;
        }
        index = (index + 1) & mask;
    }
}

}
