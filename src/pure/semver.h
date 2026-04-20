// Pure semver comparison. No Arduino deps; safe for host tests.
// Used by both ota_updates.h (Arduino) and test_semver (host).

#ifndef PURE_SEMVER_H
#define PURE_SEMVER_H

#include <cstring>

namespace pure {

// Parse "X.Y.Z" or "vX.Y.Z" into components. Missing parts default to 0.
inline void parseVersion(const char* ver, int& major, int& minor, int& patch) {
    major = minor = patch = 0;
    if (!ver) return;
    if (ver[0] == 'v' || ver[0] == 'V') ver++;

    int* parts[3] = { &major, &minor, &patch };
    int slot = 0;
    int acc = 0;
    bool any = false;
    for (const char* p = ver; ; ++p) {
        if (*p >= '0' && *p <= '9') {
            acc = acc * 10 + (*p - '0');
            any = true;
        } else if (*p == '.' || *p == '\0') {
            if (any) *parts[slot] = acc;
            acc = 0;
            any = false;
            slot++;
            if (slot >= 3 || *p == '\0') break;
        } else {
            // Unknown char terminates parsing of this component.
            if (any) *parts[slot] = acc;
            return;
        }
    }
}

// Returns true if `newer` is strictly greater than `current`.
// "unknown" current is treated as older than anything.
inline bool isNewerVersion(const char* newer, const char* current) {
    if (current && std::strcmp(current, "unknown") == 0) return true;

    int nMaj, nMin, nPat;
    int cMaj, cMin, cPat;
    parseVersion(newer, nMaj, nMin, nPat);
    parseVersion(current, cMaj, cMin, cPat);

    if (nMaj != cMaj) return nMaj > cMaj;
    if (nMin != cMin) return nMin > cMin;
    return nPat > cPat;
}

}  // namespace pure

#endif  // PURE_SEMVER_H
