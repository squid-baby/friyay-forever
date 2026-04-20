// Strip non-printable / non-ASCII characters from incoming Telegram messages.
// Preserves printable ASCII (32-126), \n, and \r.

#ifndef PURE_MESSAGE_SANITIZE_H
#define PURE_MESSAGE_SANITIZE_H

#include <string>

namespace pure {

inline std::string sanitizeMessage(const char* msg) {
    std::string out;
    if (!msg) return out;
    for (const char* p = msg; *p; ++p) {
        unsigned char c = (unsigned char)*p;
        if ((c >= 32 && c <= 126) || c == '\n' || c == '\r') {
            out += (char)c;
        }
    }
    return out;
}

}  // namespace pure

#endif  // PURE_MESSAGE_SANITIZE_H
