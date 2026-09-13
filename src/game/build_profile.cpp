#include "pch.h"
#include "game/build_profile.h"

namespace TWHT {

using cameraunlock::memory::FingerprintMismatch;
using cameraunlock::memory::PeFingerprint;
using cameraunlock::memory::ReadPeFingerprint;

const BuildProfile* ResolveBuildProfile(void* moduleBase) {
    PeFingerprint running{};
    if (!ReadPeFingerprint(moduleBase, running)) return nullptr;

    const BuildProfile* const* profiles = KnownProfiles();
    const int count = KnownProfileCount();
    for (int i = 0; i < count; ++i) {
        if (running.Matches(profiles[i]->Fingerprint)) return profiles[i];
    }
    return nullptr;
}

const char* DescribeMismatch(void* moduleBase) {
    PeFingerprint running{};
    if (!ReadPeFingerprint(moduleBase, running)) {
        return "the game EXE's PE header could not be read";
    }
    switch (cameraunlock::memory::ClassifyMismatch(running, KnownProfiles()[0]->Fingerprint)) {
        case FingerprintMismatch::Newer:
            return "the game is newer than any build this mod knows about - "
                   "check the releases page for an update";
        case FingerprintMismatch::Older:
            return "the game is older than any build this mod knows about - "
                   "let the store finish updating";
        case FingerprintMismatch::Differs:
        default:
            return "the game EXE has been modified - this mod will not engage "
                   "on a repacked binary";
    }
}

} // namespace TWHT
