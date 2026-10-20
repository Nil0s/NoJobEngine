#pragma once

#include <any>
#include <algorithm>
#include <functional>
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace NoJob {

// Data owned by one BTContext/BTInstance, never by a shared BTTree.
// Keys are case-sensitive. Types must match exactly (no implicit conversions).
class BTBlackboard {
public:
    template <typename T>
    void Set(std::string key, T&& value) {
        using Stored = std::decay_t<T>;
        static_assert(std::is_copy_constructible_v<Stored>,
            "Blackboard values must be copy constructible");
        if (key.empty()) throw std::invalid_argument("Blackboard key cannot be empty");
        const auto existing = m_Values.find(key);
        if (existing != m_Values.end() && existing->second.type() != typeid(Stored))
            throw std::invalid_argument("Blackboard type mismatch for key: " + key);
        m_Values.insert_or_assign(key, std::any(std::forward<T>(value)));
        Notify(key);
    }

    // C-string literals are stored as std::string, not const char*.
    void Set(std::string key, const char* value) {
        if (!value) throw std::invalid_argument("Blackboard string cannot be null");
        Set(std::move(key), std::string(value));
    }

    template <typename T>
    const T* TryGet(std::string_view key) const {
        const auto it = m_Values.find(std::string(key));
        return it == m_Values.end() ? nullptr : std::any_cast<T>(&it->second);
    }

    template <typename T>
    T* TryGet(std::string_view key) {
        const auto it = m_Values.find(std::string(key));
        return it == m_Values.end() ? nullptr : std::any_cast<T>(&it->second);
    }

    template <typename T>
    const T& Get(std::string_view key) const {
        const T* value = TryGet<T>(key);
        if (!value) throw std::runtime_error(
            "Blackboard key missing or type mismatch: " + std::string(key));
        return *value;
    }

    template <typename T>
    T& Get(std::string_view key) {
        T* value = TryGet<T>(key);
        if (!value) throw std::runtime_error(
            "Blackboard key missing or type mismatch: " + std::string(key));
        return *value;
    }

    template <typename T>
    T GetOr(std::string_view key, T fallback) const {
        if (const T* value = TryGet<T>(key)) return *value;
        return fallback;
    }

    bool Contains(std::string_view key) const {
        return m_Values.find(std::string(key)) != m_Values.end();
    }

    bool Remove(std::string_view key) {
        const bool removed = m_Values.erase(std::string(key)) != 0;
        if (removed) Notify(std::string(key));
        return removed;
    }

    void Clear() {
        std::vector<std::string> keys;
        for (const auto& entry : m_Values) keys.push_back(entry.first);
        m_Values.clear();
        for (const auto& key : keys) Notify(key);
    }
    using ObserverID = std::uint64_t;
    ObserverID Observe(std::string key, std::function<void(std::string_view)> callback) {
        if (!callback) throw std::invalid_argument("Blackboard observer requires callback");
        const auto id = ++m_NextObserver;
        m_Observers.emplace(id, Observer{std::move(key), std::move(callback)});
        return id;
    }
    bool Unobserve(ObserverID id) { return m_Observers.erase(id) != 0; }
    std::uint64_t Revision() const noexcept { return m_Revision; }
    std::size_t Size() const { return m_Values.size(); }

    struct DebugEntry { std::string Key, Type, Value; };
    std::vector<DebugEntry> DebugEntries() const {
        std::vector<DebugEntry> entries;
        entries.reserve(m_Values.size());
        for (const auto& [key, value] : m_Values) {
            DebugEntry e{key, value.type().name(), "<unsupported type>"};
            if (const auto* boolValue = std::any_cast<bool>(&value)) {
                e.Type = "bool"; e.Value = *boolValue ? "true" : "false";
            } else if (const auto* intValue = std::any_cast<int>(&value)) {
                e.Type = "int"; e.Value = std::to_string(*intValue);
            } else if (const auto* uintValue = std::any_cast<std::uint32_t>(&value)) {
                e.Type = "uint32"; e.Value = std::to_string(*uintValue);
            } else if (const auto* floatValue = std::any_cast<float>(&value)) {
                e.Type = "float"; e.Value = std::to_string(*floatValue);
            } else if (const auto* doubleValue = std::any_cast<double>(&value)) {
                e.Type = "double"; e.Value = std::to_string(*doubleValue);
            } else if (const auto* stringValue = std::any_cast<std::string>(&value)) {
                e.Type = "string"; e.Value = *stringValue;
            }
            entries.push_back(std::move(e));
        }
        std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) { return a.Key < b.Key; });
        return entries;
    }

    // For debugging and future editor tooling; returns typeid(void) if absent.
    std::type_index TypeOf(std::string_view key) const {
        const auto it = m_Values.find(std::string(key));
        return it == m_Values.end() ? std::type_index(typeid(void))
                                    : std::type_index(it->second.type());
    }

private:
    struct Observer { std::string Key; std::function<void(std::string_view)> Callback; };
    void Notify(const std::string& key) {
        ++m_Revision;
        // Snapshot IDs so callbacks may safely unsubscribe.
        std::vector<ObserverID> pending;
        for (const auto& [id, observer] : m_Observers)
            if (observer.Key.empty() || observer.Key == key) pending.push_back(id);
        for (auto id : pending) {
            auto it = m_Observers.find(id);
            if (it != m_Observers.end()) it->second.Callback(key);
        }
    }
    std::unordered_map<ObserverID, Observer> m_Observers;
    ObserverID m_NextObserver = 0;
    std::uint64_t m_Revision = 0;
    std::unordered_map<std::string, std::any> m_Values;
};

} // namespace NoJob
