// Pure MAC-address-to-friend-index lookup. Matches by last 3 octets.

#ifndef PURE_MAC_LOOKUP_H
#define PURE_MAC_LOOKUP_H

#include <cstdint>

struct MacMapping {
    uint8_t mac3, mac4, mac5;  // last 3 octets
    int friendIndex;
};

namespace pure {

inline int lookupMacOwner(const uint8_t mac[6],
                          const MacMapping* table,
                          int tableSize,
                          int defaultIdx) {
    for (int i = 0; i < tableSize; ++i) {
        if (mac[3] == table[i].mac3 &&
            mac[4] == table[i].mac4 &&
            mac[5] == table[i].mac5) {
            return table[i].friendIndex;
        }
    }
    return defaultIdx;
}

}  // namespace pure

#endif  // PURE_MAC_LOOKUP_H
