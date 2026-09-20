#include "../src/activity.h"

#include <iostream>

namespace {
bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}
}  // namespace

int main() {
    bool ok = true;
#ifdef FLAME_MOUSE_EDITION
    ok &= Expect(SelectPetMode(0, UINT64_MAX, UINT64_MAX) == PetMode::Cute,
                 "startup should be cute");
    ok &= Expect(SelectPetMode(100, 0, UINT64_MAX) == PetMode::Typing,
                 "new key should trigger typing");
    ok &= Expect(SelectPetMode(100, kTypingHoldMs, UINT64_MAX) == PetMode::Typing,
                 "typing boundary should be inclusive");
    ok &= Expect(SelectPetMode(100, kTypingHoldMs + 1, UINT64_MAX) == PetMode::Cute,
                 "typing should settle back to cute");
    ok &= Expect(SelectPetMode(100, UINT64_MAX, 0) == PetMode::Mousing,
                 "new mouse event should trigger mousing");
    ok &= Expect(SelectPetMode(100, 200, 0) == PetMode::Mousing,
                 "most recent active device should win");
    ok &= Expect(SelectPetMode(100, 0, 200) == PetMode::Typing,
                 "newer key event should win");
    ok &= Expect(SelectPetMode(100, 0, 0) == PetMode::Typing,
                 "equal timestamps should prefer typing");
    ok &= Expect(SelectPetMode(kDefaultSleepAfterMs, UINT64_MAX, UINT64_MAX)
                     == PetMode::Sleeping,
                 "idle boundary should sleep");
    ok &= Expect(SelectPetMode(kDefaultSleepAfterMs + 1, 0, 0)
                     == PetMode::Sleeping,
                 "sleep should win for an inconsistent stale input snapshot");
#else
    ok &= Expect(SelectPetMode(0, UINT64_MAX) == PetMode::Cute,
                 "startup should be cute");
    ok &= Expect(SelectPetMode(100, 0) == PetMode::Typing,
                 "new key should trigger typing");
    ok &= Expect(SelectPetMode(100, kTypingHoldMs) == PetMode::Typing,
                 "typing boundary should be inclusive");
    ok &= Expect(SelectPetMode(100, kTypingHoldMs + 1) == PetMode::Cute,
                 "typing should settle back to cute");
    ok &= Expect(SelectPetMode(kDefaultSleepAfterMs, UINT64_MAX) == PetMode::Sleeping,
                 "idle boundary should sleep");
    ok &= Expect(SelectPetMode(kDefaultSleepAfterMs + 1, 0) == PetMode::Sleeping,
                 "sleep should win for an inconsistent stale input snapshot");
#endif
    ok &= Expect(TypingFrameIntervalMs(1) == 155,
                 "light typing interval");
    ok &= Expect(TypingFrameIntervalMs(4) == 105,
                 "medium typing interval");
    ok &= Expect(TypingFrameIntervalMs(8) == 70,
                 "fast typing interval");
    if (ok) {
        std::cout << "activity_unit_test=OK\n";
        return 0;
    }
    return 1;
}
