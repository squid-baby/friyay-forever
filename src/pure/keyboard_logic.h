// Pure character lookup for the on-screen WiFi password keyboard.
// Special keys (space, period, backspace, caps, close) are layout/state and
// handled by the caller; this only resolves letter/symbol presses.

#ifndef PURE_KEYBOARD_LOGIC_H
#define PURE_KEYBOARD_LOGIC_H

#include <cstring>

namespace pure {

// Returns the character at (x, y), or 0 if the touch is not on a key.
// capsOn=false lowercases A-Z; symbols and digits are unaffected.
inline char keyboardCharAt(int x, int y, bool capsOn) {
    static const char* rows[] = {
        "!@#$%^&*()",
        "1234567890",
        "QWERTYUIOP",
        "ASDFGHJKL",
        "ZXCVBNM",
    };
    static const int rowY[]  = { 200, 245, 290, 335, 380 };
    static const int rowX[]  = { 35, 35, 35, 70, 115 };
    static const int rowH    = 40;
    static const int keyW    = 68;

    for (int r = 0; r < 5; ++r) {
        if (y < rowY[r] || y >= rowY[r] + rowH) continue;
        int kx = x - rowX[r];
        if (kx < 0) continue;
        int k = kx / keyW;
        int len = (int)std::strlen(rows[r]);
        if (k >= len) continue;
        char c = rows[r][k];
        if (!capsOn && c >= 'A' && c <= 'Z') c = (char)(c + 32);
        return c;
    }
    return 0;
}

}  // namespace pure

#endif  // PURE_KEYBOARD_LOGIC_H
