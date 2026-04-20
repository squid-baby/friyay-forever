// Pure countdown to next Friday 3pm local time.
// Takes an injectable "now" so tests don't depend on the wall clock.

#ifndef PURE_COUNTDOWN_H
#define PURE_COUNTDOWN_H

#include <ctime>

namespace pure {

// Compute seconds remaining until the next Friday 15:00 local from `now`.
// `local` must be `now` already broken down into local time (gmtime/localtime).
// On Friday before 15:00, returns seconds remaining today; at/after 15:00,
// rolls forward to the following Friday.
inline long calcCountdownSeconds(time_t now, const struct tm& local) {
    struct tm fri = local;
    int dayOfWeek = local.tm_wday;  // 0=Sun .. 5=Fri .. 6=Sat
    int days = (5 - dayOfWeek + 7) % 7;
    if (days == 0 && local.tm_hour >= 15) days = 7;

    fri.tm_mday += days;
    fri.tm_hour = 15;
    fri.tm_min  = 0;
    fri.tm_sec  = 0;
    // mktime() normalizes overflowed mday and re-derives tm_isdst, so DST
    // boundaries land on the right wall-clock Friday 3pm.
    fri.tm_isdst = -1;

    long secs = (long)difftime(mktime(&fri), now);
    return secs < 0 ? 0 : secs;
}

}  // namespace pure

#endif  // PURE_COUNTDOWN_H
