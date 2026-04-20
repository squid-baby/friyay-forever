// Pure Telegram-id-to-friend-index lookup.

#ifndef PURE_FRIEND_LOOKUP_H
#define PURE_FRIEND_LOOKUP_H

#include <cstdint>

namespace pure {

inline int getFriendIdx(int64_t id, const int64_t* ids, int numFriends) {
    for (int i = 0; i < numFriends; ++i) {
        if (ids[i] == id) return i;
    }
    return -1;
}

}  // namespace pure

#endif  // PURE_FRIEND_LOOKUP_H
