#include "bar_connection.h"

#include <algorithm>
#include <array>

namespace headsup
{
    namespace
    {
        constexpr double kBytesPerKb = 1024.0;

        std::string Rate(const std::optional<NetCounters>& counters, int32_t NetCounters::*field)
        {
            return counters ? Grouped(static_cast<uint32_t>((*counters).*field)) : "-";
        }

        Span Figure(std::string text, LinkState state)
        {
            switch (state)
            {
            case LinkState::Quiet: return Colored(std::move(text), kWarningColor);
            case LinkState::Silent: return Colored(std::move(text), kAlarmColor);
            case LinkState::Normal: break;
            }
            return Text(std::move(text));
        }

        std::string PerSecond(const std::optional<NetCounters>& counters, int32_t NetCounters::*field)
        {
            return counters ? Rate(counters, field) + " B/s" : "-";
        }

        // The time since the last server data, amber or red as the bar's figures are.
        PopupRow LastData(const ConnectionReport& r)
        {
            const std::string when = r.silence ? SpanText(*r.silence) : "-";
            const LinkState state  = StateOf(r);
            if (state == LinkState::Normal) return Labeled("Last server data", Text(when));
            return Line(Cell(ListOf(Figure("Last server data ", state), Figure(when, state))));
        }

        bool Waiting(const std::optional<PlayOnlineCounts>& counts)
        {
            return counts && (counts->messages > 0 || counts->mail > 0);
        }
    }

    LinkState StateOf(const ConnectionReport& r)
    {
        if (r.zoning) return LinkState::Normal;
        if (r.counters && r.counters->health < kFullHealth) return LinkState::Silent;
        // Not every datagram the server sends carries a packet HeadsUp is shown, so while the game's counters still
        // see data arrive, a quiet spell is not one.
        if (r.counters && r.counters->received > 0) return LinkState::Normal;
        if (r.silence && *r.silence > kQuietSeconds) return LinkState::Quiet;
        return LinkState::Normal;
    }

    void ConnectionMeter::Received(double now) { m_LastReceived = now; }

    void ConnectionMeter::RoundTrip(double ms)
    {
        m_RoundTrips.push_back(ms);
        if (m_RoundTrips.size() > kRoundTrips) m_RoundTrips.pop_front();
        if (m_RoundTrips.size() < kLeastRoundTrips) return;
        std::array<double, kRoundTrips> sorted{};
        const size_t n = m_RoundTrips.size();
        std::copy(m_RoundTrips.begin(), m_RoundTrips.end(), sorted.begin());
        std::sort(sorted.begin(), sorted.begin() + static_cast<std::ptrdiff_t>(n));
        const double median = n % 2 == 1 ? sorted[n / 2] : (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0;
        m_Latency           = Latency{median, sorted[0], sorted[n - 1]};
    }

    void ConnectionMeter::Frame(double now, const std::optional<NetCounters>& counters)
    {
        const double held = m_LastFrame ? now - *m_LastFrame : 0.0;
        if (counters && held > 0.0 && held <= kLongestFrame)
        {
            m_ReceivedBytes += counters->received * held;
            m_SentBytes += counters->sent * held;
            m_CountedSeconds += held;
        }
        m_LastFrame = now;
        m_Counters  = counters;
    }

    ConnectionReport ConnectionMeter::Report(double now, bool zoning) const
    {
        ConnectionReport r;
        r.counters = m_Counters;
        if (m_LastReceived) r.silence = now - *m_LastReceived;
        r.zoning = zoning;
        r.latency = m_Latency;
        r.receivedBytes  = m_ReceivedBytes;
        r.sentBytes      = m_SentBytes;
        r.countedSeconds = m_CountedSeconds;
        return r;
    }

    std::string ByteText(double bytes)
    {
        if (bytes < kBytesPerKb) return Grouped(static_cast<uint32_t>(std::max(0.0, bytes))) + " B";
        if (bytes < kBytesPerKb * kBytesPerKb) return Printed("%.1f KB", bytes / kBytesPerKb);
        return Printed("%.1f MB", bytes / (kBytesPerKb * kBytesPerKb));
    }

    std::vector<Span> ConnectionFigures(const std::optional<ConnectionReport>& report, uint8_t parts)
    {
        if (!report) return ListOf(Text("-"));
        std::vector<Span> line;
        // One line: amber or red figures say what the popup explains.
        const LinkState state = StateOf(*report);
        const auto has        = [parts](PerfPart part) { return (parts & PerfBit(part)) != 0; };
        if (has(PerfPart::Sent))
            AddPart(line, ListOf(Text("S ", Tone::Dim), Figure(Rate(report->counters, &NetCounters::sent), state)));
        if (has(PerfPart::Received))
            AddPart(line, ListOf(Text("R ", Tone::Dim), Figure(Rate(report->counters, &NetCounters::received), state)));
        if (has(PerfPart::Percent) && report->counters)
            AddPart(line, ListOf(Figure(Printed("%d%%", report->counters->health), state)));
        if (has(PerfPart::Ping) && report->latency)
            AddPart(line, ListOf(Text(Printed("%.0f ms", report->latency->medianMs), Tone::Dim)));
        if (has(PerfPart::Messages) && Waiting(report->playOnline))
        {
            std::vector<Span> waiting;
            for (const auto& [picture, count] : {std::pair{GamePicture::PlayOnlineMail, report->playOnline->mail},
                     std::pair{GamePicture::PlayOnlineMessenger, report->playOnline->messages}})
            {
                waiting.push_back(Span{"", Tone::Normal, 0, picture});
                if (count > 0) waiting.push_back(Text(std::to_string(count), Tone::Dim));
            }
            AddPart(line, std::move(waiting));
        }
        return line;
    }

    std::vector<PopupSection> ConnectionSections(const std::optional<ConnectionReport>& report)
    {
        if (!report) return {};
        const ConnectionReport& r = *report;
        PopupSection now{"Connection", {}};
        now.rows.push_back(Labeled("Sent", Text(PerSecond(r.counters, &NetCounters::sent))));
        now.rows.push_back(Labeled("Received", Text(PerSecond(r.counters, &NetCounters::received))));
        if (r.latency)
        {
            now.rows.push_back(Labeled("Ping", Text(Printed("%.0f ms", r.latency->medianMs))));
            now.rows.push_back(LineOf(Printed("%.0f", r.latency->lowestMs) + " to " +
                                          Printed("%.0f ms over recent pings", r.latency->highestMs),
                Tone::Dim));
        }
        else
            now.rows.push_back(Labeled("Ping", Text("-", Tone::Dim)));
        now.rows.push_back(LastData(r));
        if (r.counters && r.counters->health < kFullHealth)
            now.rows.push_back(LineOf("The game's own meter is at " + std::to_string(r.counters->health) +
                                          "%: it falls once 10 s pass without server data.",
                Tone::Dim));
        PopupSection totals{"This session", {}};
        if (r.countedSeconds > 0.0)
        {
            totals.rows.push_back(Labeled("Sent", Text(ByteText(r.sentBytes))));
            totals.rows.push_back(Labeled("Received", Text(ByteText(r.receivedBytes))));
            totals.rows.push_back(Labeled("Over", Text(SpanText(r.countedSeconds))));
        }
        else
            totals.rows.push_back(LineOf("Counted once the game's counters can be read.", Tone::Dim));
        std::vector<PopupSection> sections = ListOf(std::move(now), std::move(totals));
        if (Waiting(r.playOnline))
            sections.push_back(PopupSection{"PlayOnline",
                ListOf(Labeled("Messages", Text(std::to_string(r.playOnline->messages))),
                    Labeled("Mail", Text(std::to_string(r.playOnline->mail))))});
        return sections;
    }
}
