#ifndef NETWORK_ID_H
#define NETWORK_ID_H

#include <cstdint>
#include <functional>
#include <compare>
#include <string>
#include <ostream>

/*
=============================================================================
DEMONSTRATION & EXTENSION: NetworkID Wrapper
=============================================================================

NOTE: This header is NOT coupled to AppState or used by default.
It is provided as an example of how a custom networking layer can manage
strongly-typed network identities if you choose to build an entity-based
multiplayer architecture.

-----------------------------------------------------------------------------
WHERE AND HOW TO USE THIS:
-----------------------------------------------------------------------------
1. Where it goes:
   - Place this in your game's networking subsystem (e.g., alongside your
     packet serialization, replication managers, or network entity tables).
   - AppState should remain pure application logic; your networking driver
     or network manager maps incoming NetworkIDs to your game objects.

2. Why use a NetworkID wrapper instead of raw integers?
   - Explicit Invalid State (0 is invalid / unassigned).
   - Authority Partitioning:
       * Static / Map entities: 1 .. 999 (pre-placed in level)
       * Server dynamic entities: 1000 .. 0x7FFFFFFF (authoritative objects)
       * Client-predicted entities: >= 0x80000000 (spawned locally before server ACK)
   - Type safety: Prevents confusing entity IDs with client IDs or packet numbers.
   - Standard hash support for std::unordered_map<NetworkID, ...>.
=============================================================================
*/

class NetworkID {
public:
    using ValueType = uint32_t;

    static constexpr ValueType INVALID_VAL          = 0;
    static constexpr ValueType STATIC_MAP_START     = 1;
    static constexpr ValueType SERVER_DYNAMIC_START = 1000;
    static constexpr ValueType LOCAL_PREDICT_START  = 0x80000000; // High bit set

    constexpr NetworkID() noexcept : id_(INVALID_VAL) {}
    constexpr explicit NetworkID(ValueType id) noexcept : id_(id) {}

    // Factory methods
    [[nodiscard]] static constexpr NetworkID invalid() noexcept {
        return NetworkID(INVALID_VAL);
    }

    [[nodiscard]] static constexpr NetworkID server(ValueType id) noexcept {
        return NetworkID(id);
    }

    [[nodiscard]] static constexpr NetworkID predicted(ValueType local_counter) noexcept {
        return NetworkID(LOCAL_PREDICT_START | (local_counter & ~LOCAL_PREDICT_START));
    }

    // State queries
    [[nodiscard]] constexpr bool is_valid() const noexcept {
        return id_ != INVALID_VAL;
    }

    [[nodiscard]] constexpr bool is_static() const noexcept {
        return is_valid() && (id_ < SERVER_DYNAMIC_START);
    }

    [[nodiscard]] constexpr bool is_server_allocated() const noexcept {
        return (id_ >= SERVER_DYNAMIC_START) && (id_ < LOCAL_PREDICT_START);
    }

    [[nodiscard]] constexpr bool is_predicted() const noexcept {
        return id_ >= LOCAL_PREDICT_START;
    }

    [[nodiscard]] constexpr ValueType raw() const noexcept {
        return id_;
    }

    constexpr explicit operator bool() const noexcept {
        return is_valid();
    }

    constexpr auto operator<=>(const NetworkID&) const noexcept = default;
    constexpr bool operator==(const NetworkID&) const noexcept = default;

    [[nodiscard]] std::string to_string() const {
        if (!is_valid()) return "NetworkID(Invalid)";
        if (is_static()) return "NetworkID(Static:" + std::to_string(id_) + ")";
        if (is_predicted()) return "NetworkID(Predicted:" + std::to_string(id_ & ~LOCAL_PREDICT_START) + ")";
        return "NetworkID(" + std::to_string(id_) + ")";
    }

private:
    ValueType id_{ INVALID_VAL };
};

inline std::ostream& operator<<(std::ostream& os, const NetworkID& id) {
    return os << id.to_string();
}

// Specialization for std::unordered_map and std::unordered_set
namespace std {
    template <>
    struct hash<NetworkID> {
        size_t operator()(const NetworkID& id) const noexcept {
            return std::hash<NetworkID::ValueType>{}(id.raw());
        }
    };
}

#endif // NETWORK_ID_H
