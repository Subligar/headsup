#pragma once

#include "bar_content.h"
#include "native_locate.h" // NetCounters

#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace headsup
{
    constexpr double kQuietSeconds     = 2.0;  // server silence the bar shows in amber
    constexpr double kLongestFrame     = 1.0;  // seconds: a longer step between fed frames counts nothing to the totals
    constexpr double kCounterSampling  = 0.25; // seconds between reads of the game's counters, which change every 3 s

    struct ConnectionReport
    {
        std::optional<NetCounters> counters; // the game's, while it has a session and they could be read
        std::optional<double> silence;       // seconds since the last server packet; nothing before one
        bool zoning = false;                 // the game is changing zones, when silence is expected
        double receivedBytes = 0.0, sentBytes = 0.0; // since HeadsUp first read the counters
        double countedSeconds = 0.0;                 // how long those totals ran
        std::optional<PlayOnlineCounts> playOnline;
        bool operator==(const ConnectionReport&) const = default;
    };

    enum class LinkState : uint8_t
    {
        Normal,
        Quiet,  // nothing from the server for more than kQuietSeconds
        Silent, // the game's own meter is under 100: 10 seconds or more without server data
    };
    // Normal while zoning.
    LinkState StateOf(const ConnectionReport& report);

    // What HeadsUp measures of the connection itself.
    class ConnectionMeter
    {
    public:
        // Each packet the game receives, at now (seconds).
        void Received(double now);
        // Each frame, with the game's counters when they could be read. Fed only while the bar runs with Connection
        // shown, so a step longer than kLongestFrame is time away (a logout, the widget off) and is not counted.
        void Frame(double now, const std::optional<NetCounters>& counters);
        ConnectionReport Report(double now, bool zoning) const;

    private:
        std::optional<double> m_LastReceived;
        std::optional<double> m_LastFrame;
        std::optional<NetCounters> m_Counters;
        double m_ReceivedBytes = 0.0, m_SentBytes = 0.0, m_CountedSeconds = 0.0;
    };

    // "820 B", "34.5 KB", "1.2 MB", a KB being 1,024 bytes.
    std::string ByteText(double bytes);

    // The parts of parts, by PerfBit, that are the connection's: the rates sent and received, the game's own meter as its
    // readout has it, and PlayOnline's waiting messages.
    std::vector<Span> ConnectionFigures(const std::optional<ConnectionReport>& report, uint8_t parts);
    // The Connection card first, when there is a report.
    std::vector<PopupSection> ConnectionSections(const std::optional<ConnectionReport>& report);
}
