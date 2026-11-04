#include "bar_fps.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace headsup
{
    void FrameStats::Add(double now, double ms)
    {
        const bool newSecond = m_Frames.empty() || std::floor(now) != std::floor(m_Frames.back().end);
        m_Frames.push_back(Frame{now, ms});
        while (!m_Frames.empty() && m_Frames.front().end <= now - kSlowestWindow)
            m_Frames.pop_front();
        if (!newSecond) return;
        double sum = 0.0, slowest = 0.0;
        int recent = 0;
        for (const Frame& f : m_Frames)
        {
            slowest = std::max(slowest, f.ms);
            if (f.end <= now - kAverageWindow) continue;
            sum += f.ms;
            ++recent;
        }
        m_Report = FrameReport{sum / recent, slowest};
    }

    int FramesPerSecond(double averageMs)
    {
        return averageMs > 0.0 ? static_cast<int>(std::lround(static_cast<double>(kMsPerSecond) / averageMs)) : 0;
    }

    std::vector<Span> FpsFigures(const std::optional<FrameReport>& frames)
    {
        return ListOf(Text(frames ? std::to_string(FramesPerSecond(frames->averageMs)) + " fps" : "-"));
    }
}
