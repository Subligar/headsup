#pragma once

#include "bar_content.h"

#include <cstdint>
#include <string_view>

namespace headsup
{
    // Phoenix keeps Japan's time (earth_time.h's jst namespace): UTC+9, without daylight saving.
    constexpr int64_t kJstOffsetMs = 9 * kMsPerHour;

    // The day in Japan a time is in, counted from the Unix epoch.
    int64_t JstDay(int64_t unixMs);
    // 0 Monday to 6 Sunday, as Phoenix's get_game_weekday counts.
    const char* JstWeekdayName(int weekday);
    // Earth seconds to the next JST midnight, Phoenix's daily reset: more than 0, at most a day.
    double SecondsToDailyReset(int64_t unixMs);
    // Earth seconds to the next Monday 00:00 JST, Phoenix's conquest tally (get_next_game_week): more than 0, at most
    // a week.
    double SecondsToTally(int64_t unixMs);

    // The zones the Clock widget can show its clock in, with their daylight saving rules as of 2026.
    enum class ClockZone : uint8_t
    {
        Japan, // the server's, which the resets go by
        Utc,
        UsEastern,
        UsCentral,
        UsMountain,
        UsArizona,
        UsPacific,
        UsAlaska,
        UsHawaii,
        UkIreland,
        CentralEurope,
        EasternEurope,
        AustraliaEastern,
    };
    constexpr int kClockZoneCount = static_cast<int>(ClockZone::AustraliaEastern) + 1;
    // As the menu lists it: "Japan (JST)".
    const char* ClockZoneName(ClockZone zone);

    struct ZoneTime
    {
        int weekday = 0; // 0 Monday to 6 Sunday
        int hour = 0, minute = 0;
        const char* abbreviation = ""; // "JST", "EDT", "AEST"
        int second               = 0;  // left out of ==, as the date is
        int year = 0, month = 0, day = 0;
        bool operator==(const ZoneTime& o) const
        {
            return weekday == o.weekday && hour == o.hour && minute == o.minute &&
                   std::string_view(abbreviation) == o.abbreviation;
        }
    };
    ZoneTime ZoneAt(ClockZone zone, int64_t unixMs);
    // The computer's zone by the local date and time it gives for unixMs: the abbreviation of a zone as far ahead of
    // UTC now ("CEST"), one in daylight saving first, else that offset ("UTC+5:30").
    std::string LocalZoneName(int64_t unixMs, int year, int month, int day, int hour, int minute);
    // A zone's clock as a widget titles it: "JST 15:30", "PDT 11:30:42 PM".
    std::string ZoneClockText(const ZoneTime& t, bool twentyFour, bool seconds);

    // What the Clock's popup lists: the server's clock, "Server Time Friday 15:30 JST", and the time to the daily reset
    // and to the conquest tally, which go by it.
    PopupSection ServerTimeRows(int64_t unixMs);
}
