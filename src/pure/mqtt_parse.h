// Pure parser for FRIYAY commit-sync MQTT messages.
// Format: "FRIYAY:<friendIndex>:<0|1>"

#ifndef PURE_MQTT_PARSE_H
#define PURE_MQTT_PARSE_H

#include <cstring>
#include <cstdlib>

namespace pure {

// Returns true if the message is structurally valid, in range, and not our own
// echo. On true: outIdx and outCommitted are populated. On false: ignore.
//
// Pass ownIdx = -1 to disable the echo skip (useful when caller wants to
// validate format only).
inline bool parseMqttCommit(const char* text,
                            int numFriends,
                            int ownIdx,
                            int& outIdx,
                            bool& outCommitted) {
    if (!text) return false;

    static const char kPrefix[] = "FRIYAY:";
    const int prefixLen = (int)sizeof(kPrefix) - 1;
    if (std::strncmp(text, kPrefix, prefixLen) != 0) return false;

    const char* idxStart = text + prefixLen;
    const char* colon = std::strchr(idxStart, ':');
    if (!colon || colon == idxStart) return false;

    // Index segment must be all digits (allow no whitespace, no signs).
    for (const char* p = idxStart; p < colon; ++p) {
        if (*p < '0' || *p > '9') return false;
    }

    int idx = std::atoi(idxStart);
    if (idx < 0 || idx >= numFriends) return false;
    if (ownIdx >= 0 && idx == ownIdx) return false;

    const char* valStart = colon + 1;
    if (*valStart != '0' && *valStart != '1') return false;
    // Anything after the value digit is acceptable (whitespace, nothing).
    // Strict: require either end-of-string or a single digit only. We accept
    // any trailing chars to mirror the existing toInt() permissiveness.

    outIdx = idx;
    outCommitted = (*valStart == '1');
    return true;
}

}  // namespace pure

#endif  // PURE_MQTT_PARSE_H
