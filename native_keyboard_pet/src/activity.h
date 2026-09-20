#pragma once

#include <cstdint>

enum class PetMode : std::uint8_t {
    Cute = 0,
    Typing = 1,
#ifdef FLAME_MOUSE_EDITION
    Mousing = 2,
    Sleeping = 3,
#else
    Sleeping = 2,
#endif
};

constexpr std::uint64_t kTypingHoldMs = 1200;
#ifdef FLAME_MOUSE_EDITION
constexpr std::uint64_t kMouseHoldMs = 850;
#endif
constexpr std::uint64_t kDefaultSleepAfterMs = 45000;

#ifdef FLAME_MOUSE_EDITION
inline PetMode SelectPetMode(std::uint64_t systemIdleMs,
                             std::uint64_t sinceLastKeyMs,
                             std::uint64_t sinceLastMouseMs,
                             std::uint64_t sleepAfterMs = kDefaultSleepAfterMs) {
    if (systemIdleMs >= sleepAfterMs) {
        return PetMode::Sleeping;
    }
    const bool typing = sinceLastKeyMs <= kTypingHoldMs;
    const bool mousing = sinceLastMouseMs <= kMouseHoldMs;
    if (typing && (!mousing || sinceLastKeyMs <= sinceLastMouseMs)) {
        return PetMode::Typing;
    }
    if (mousing) {
        return PetMode::Mousing;
    }
    return PetMode::Cute;
}
#else
inline PetMode SelectPetMode(std::uint64_t systemIdleMs,
                             std::uint64_t sinceLastKeyMs,
                             std::uint64_t sleepAfterMs = kDefaultSleepAfterMs) {
    if (systemIdleMs >= sleepAfterMs) {
        return PetMode::Sleeping;
    }
    if (sinceLastKeyMs <= kTypingHoldMs) {
        return PetMode::Typing;
    }
    return PetMode::Cute;
}
#endif

inline std::uint32_t TypingFrameIntervalMs(unsigned recentKeyCount) {
    if (recentKeyCount >= 8) {
        return 70;
    }
    if (recentKeyCount >= 4) {
        return 105;
    }
    return 155;
}
