#include "bar_server_time.h"

namespace headsup
{
    namespace
    {
        constexpr int kDaysPerWeek = 7;
        constexpr int64_t kEpochWeekday = 3; // 1970-01-01, day 0, was a Thursday: 3 days after a Monday
        const char* const kWeekdays[kDaysPerWeek] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday",
            "Saturday", "Sunday"};

        int64_t FloorDiv(int64_t a, int64_t b) { return a / b - ((a % b != 0) && ((a < 0) != (b < 0)) ? 1 : 0); }

        // The days since 1970-01-01 of a date, and a date of them (Howard Hinnant's days_from_civil and
        // civil_from_days).
        int64_t DaysFromCivil(int64_t y, int m, int d)
        {
            y -= m <= 2 ? 1 : 0;
            const int64_t era = FloorDiv(y, 400);
            const int64_t yoe = y - era * 400;
            const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
            const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            return era * 146097 + doe - 719468;
        }
        struct Date
        {
            int64_t year;
            int month, day;
        };
        Date DateOf(int64_t days)
        {
            const int64_t z = days + 719468, era = FloorDiv(z, 146097), doe = z - era * 146097;
            const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
            const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
            return Date{yoe + era * 400 + (mp >= 10 ? 1 : 0), static_cast<int>(mp < 10 ? mp + 3 : mp - 9),
                static_cast<int>(doy - (153 * mp + 2) / 5 + 1)};
        }
        int WeekdayOf(int64_t days) { return static_cast<int>(((days + kEpochWeekday) % kDaysPerWeek + kDaysPerWeek) % kDaysPerWeek); }
        constexpr int kSunday = 6;
        // The day of the nth Sunday of a month (n 1 up), or its last (n 0).
        int64_t SundayOf(int64_t year, int month, int n)
        {
            if (n == 0)
            {
                const int64_t next = DaysFromCivil(month == 12 ? year + 1 : year, month == 12 ? 1 : month + 1, 1) - 1;
                return next - (WeekdayOf(next) - kSunday + kDaysPerWeek) % kDaysPerWeek;
            }
            const int64_t first = DaysFromCivil(year, month, 1);
            return first + (kSunday - WeekdayOf(first) + kDaysPerWeek) % kDaysPerWeek + (n - 1) * kDaysPerWeek;
        }

        enum class Rule : uint8_t
        {
            None,
            UnitedStates, // second Sunday of March 02:00 to the first Sunday of November 02:00, local
            Europe,       // last Sunday of March to the last Sunday of October, 01:00 UTC
            Australia,    // first Sunday of October 02:00 to the first Sunday of April 03:00, local
        };
        struct ZoneRule
        {
            const char* name;
            int standardMinutes; // ahead of UTC
            const char *standard, *daylight;
            Rule rule;
        };
        constexpr int kHourMinutes = 60;
        const ZoneRule kZones[kClockZoneCount] = {
            {"Japan (JST)", 9 * kHourMinutes, "JST", "JST", Rule::None},
            {"UTC", 0, "UTC", "UTC", Rule::None},
            {"US Eastern (ET)", -5 * kHourMinutes, "EST", "EDT", Rule::UnitedStates},
            {"US Central (CT)", -6 * kHourMinutes, "CST", "CDT", Rule::UnitedStates},
            {"US Mountain (MT)", -7 * kHourMinutes, "MST", "MDT", Rule::UnitedStates},
            {"Arizona (MST)", -7 * kHourMinutes, "MST", "MST", Rule::None},
            {"US Pacific (PT)", -8 * kHourMinutes, "PST", "PDT", Rule::UnitedStates},
            {"Alaska (AKT)", -9 * kHourMinutes, "AKST", "AKDT", Rule::UnitedStates},
            {"Hawaii (HST)", -10 * kHourMinutes, "HST", "HST", Rule::None},
            {"UK and Ireland (GMT)", 0, "GMT", "BST", Rule::Europe},
            {"Central Europe (CET)", 1 * kHourMinutes, "CET", "CEST", Rule::Europe},
            {"Eastern Europe (EET)", 2 * kHourMinutes, "EET", "EEST", Rule::Europe},
            {"Australia Eastern (AET)", 10 * kHourMinutes, "AEST", "AEDT", Rule::Australia},
        };

        // Whether daylight saving is on at unixMs: each change at its local time, the standard time's before it.
        bool Daylight(const ZoneRule& z, int64_t unixMs)
        {
            const int64_t standardMs = static_cast<int64_t>(z.standardMinutes) * kMsPerMinute;
            const int64_t year       = DateOf(FloorDiv(unixMs + standardMs, kMsPerDay)).year;
            const auto at            = [&](int64_t day, int hour, int64_t offsetMs) {
                return day * kMsPerDay + hour * kMsPerHour - offsetMs;
            };
            switch (z.rule)
            {
            case Rule::None: return false;
            case Rule::UnitedStates:
                return unixMs >= at(SundayOf(year, 3, 2), 2, standardMs) &&
                       unixMs < at(SundayOf(year, 11, 1), 2, standardMs + kMsPerHour);
            case Rule::Europe: return unixMs >= at(SundayOf(year, 3, 0), 1, 0) && unixMs < at(SundayOf(year, 10, 0), 1, 0);
            case Rule::Australia:
                return unixMs >= at(SundayOf(year, 10, 1), 2, standardMs) ||
                       unixMs < at(SundayOf(year, 4, 1), 3, standardMs + kMsPerHour);
            }
            return false;
        }

        // The Earth seconds from unixMs to the start of JST day `day`.
        double SecondsTo(int64_t day, int64_t unixMs)
        {
            return static_cast<double>(day * kMsPerDay - kJstOffsetMs - unixMs) / static_cast<double>(kMsPerSecond);
        }
    }

    int64_t JstDay(int64_t unixMs) { return FloorDiv(unixMs + kJstOffsetMs, kMsPerDay); }

    const char* ClockZoneName(ClockZone zone) { return kZones[static_cast<int>(zone)].name; }

    ZoneTime ZoneAt(ClockZone zone, int64_t unixMs)
    {
        const ZoneRule& z     = kZones[static_cast<int>(zone)];
        const bool daylight   = Daylight(z, unixMs);
        const int64_t local   = unixMs + (static_cast<int64_t>(z.standardMinutes) + (daylight ? kHourMinutes : 0)) * kMsPerMinute;
        const int64_t day     = FloorDiv(local, kMsPerDay);
        const int64_t inDay   = local - day * kMsPerDay;
        const Date date       = DateOf(day);
        return ZoneTime{WeekdayOf(day), static_cast<int>(inDay / kMsPerHour),
            static_cast<int>(inDay % kMsPerHour / kMsPerMinute), daylight ? z.daylight : z.standard,
            static_cast<int>(inDay % kMsPerMinute / kMsPerSecond), static_cast<int>(date.year), date.month, date.day};
    }

    std::string LocalZoneName(int64_t unixMs, int year, int month, int day, int hour, int minute)
    {
        const int64_t localMs = DaysFromCivil(year, month, day) * kMsPerDay + hour * kMsPerHour + minute * kMsPerMinute;
        const auto ahead      = static_cast<int>(FloorDiv(localMs - FloorDiv(unixMs, kMsPerMinute) * kMsPerMinute, kMsPerMinute));
        // A zone in daylight saving first: US Pacific's summer is Arizona's whole year, and more play on the coast.
        for (const bool summer : {true, false})
            for (const ZoneRule& z : kZones)
            {
                const bool daylight = Daylight(z, unixMs);
                if (daylight == summer && z.standardMinutes + (daylight ? kHourMinutes : 0) == ahead)
                    return daylight ? z.daylight : z.standard;
            }
        const int minutes = ahead < 0 ? -ahead : ahead;
        std::string name  = std::string("UTC") + (ahead < 0 ? "-" : "+") + std::to_string(minutes / kHourMinutes);
        if (minutes % kHourMinutes != 0) name += ":" + std::to_string(minutes % kHourMinutes);
        return name;
    }

    const char* JstWeekdayName(int weekday) { return kWeekdays[((weekday % kDaysPerWeek) + kDaysPerWeek) % kDaysPerWeek]; }

    double SecondsToDailyReset(int64_t unixMs) { return SecondsTo(JstDay(unixMs) + 1, unixMs); }

    double SecondsToTally(int64_t unixMs)
    {
        return SecondsTo(JstDay(unixMs) + kDaysPerWeek - ZoneAt(ClockZone::Japan, unixMs).weekday, unixMs);
    }

    std::string ZoneClockText(const ZoneTime& t, bool twentyFour, bool seconds)
    {
        return std::string(t.abbreviation) + " " + ClockText(t.hour, t.minute, twentyFour, seconds ? t.second : -1);
    }

    PopupSection ServerTimeRows(int64_t unixMs)
    {
        const ZoneTime t = ZoneAt(ClockZone::Japan, unixMs);
        PopupSection rows;
        rows.rows.push_back(Labeled("Server Time", Text(std::string(JstWeekdayName(t.weekday)) + " " +
                                                        HourMinute(t.hour, t.minute) + " " + t.abbreviation)));
        rows.rows.push_back(Labeled("Daily Reset in", Text(SpanText(SecondsToDailyReset(unixMs)))));
        rows.rows.push_back(Labeled("Conquest Tally in", Text(SpanText(SecondsToTally(unixMs)))));
        return rows;
    }
}
