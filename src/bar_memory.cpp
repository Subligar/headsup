#include "bar_memory.h"

#include <string>

namespace headsup
{
    namespace
    {
        constexpr int kLuaColumns  = 4; // two addons a row, each a name and its size

        std::string MbText(uint32_t mb) { return Grouped(mb) + " MB"; }

        // A figure in the color of how little space is left.
        Span HeadroomSpan(std::string text, Headroom headroom)
        {
            switch (headroom)
            {
            case Headroom::Plenty: break;
            case Headroom::Low: return Colored(std::move(text), kWarningColor);
            case Headroom::Critical: return Colored(std::move(text), kAlarmColor);
            }
            return Text(std::move(text));
        }

        // One fix's figures; what it is when it never started or is switched off.
        Span FixSpan(uint32_t flags, uint32_t started, uint32_t on, std::string figures)
        {
            if ((flags & started) == 0) return Text("not in use", Tone::Dim);
            if ((flags & on) == 0) return Colored("switched off", kWarningColor);
            return Text(std::move(figures));
        }

        void AddLinuxXI(std::vector<PopupSection>& sections, const LinuxXIReport& r)
        {
            PopupSection fixes = Section(CardKind::PerfLinuxXI, "LinuxXI");
            fixes.rows.push_back(Labeled("Packet Copies",
                FixSpan(r.flags, kPoolHooked, kPoolOn, Grouped(r.copiesHeld) + " (" + Grouped(r.copiesPeak) + " at most)")));
            if ((r.flags & kPoolHooked) != 0)
                fixes.rows.push_back(Labeled("Pool", Text(MbText(r.chunks * r.chunkMb) + " (" + MbText(r.chunksPeak * r.chunkMb) + " at most)")));
            fixes.rows.push_back(Labeled("Releases Held Back",
                FixSpan(r.flags, kGraceActive, kGraceOn, Grouped(r.releasesHeldBack) + " (" + Grouped(r.releasesWaiting) + " waiting)")));
            fixes.rows.push_back(Labeled("Addons Stepped", Text(Grouped(r.addonsStepped) + " (" + Grouped(r.addonsNotLoaded) + " not loaded)")));
            fixes.rows.push_back(Labeled("Registry Compares", FixSpan(r.flags, kLocaleActive, kLocaleOn, "C Locale")));
            sections.push_back(std::move(fixes));
            if (r.addonCount == 0) return;
            PopupSection lua = Section(CardKind::PerfLua, "Lua Memory");
            uint64_t total = 0;
            for (uint32_t i = 0; i < r.addonCount; ++i)
                total += r.lua[i].kilobytes;
            lua.rows.push_back(Labeled("All Addons", Text(KbText(total))));
            PopupRow grid = Grid(kLuaColumns, 2 * r.addonCount);
            for (uint32_t i = 0; i < r.addonCount; ++i)
            {
                grid.cells.push_back(Cell(Text(r.lua[i].name)));
                grid.cells.push_back(Cell(Text(KbText(r.lua[i].kilobytes), Tone::Dim)));
            }
            lua.rows.push_back(std::move(grid));
            sections.push_back(std::move(lua));
        }
    }

    void CpuMeter::Add(double now, double cpuSeconds)
    {
        if (!Wants(now)) return;
        if (m_At && cpuSeconds >= *m_Cpu)
            m_Percent = 100.0 * (cpuSeconds - *m_Cpu) / (now - *m_At);
        else if (m_At)
            m_Percent.reset();
        m_At  = now;
        m_Cpu = cpuSeconds;
    }

    Headroom HeadroomOf(const MemoryReport& memory)
    {
        if (memory.leftMb < kCriticalLeftMb) return Headroom::Critical;
        if (memory.leftMb < kLowLeftMb) return Headroom::Low;
        return Headroom::Plenty;
    }

    std::vector<Span> MemoryFigures(const std::optional<MemoryReport>& memory)
    {
        return ListOf(memory ? HeadroomSpan(MbText(memory->usedMb), HeadroomOf(*memory)) : Text("-"));
    }

    std::vector<PopupSection> MemorySections(const std::optional<MemoryReport>& memory, std::optional<double> cpuPercent,
        const std::optional<FrameReport>& frames, const LinuxXIState& linuxxi)
    {
        std::vector<PopupSection> sections;
        if (memory)
        {
            const Headroom headroom = HeadroomOf(*memory);
            PopupSection space = Section(CardKind::PerfMemory, "Memory");
            space.rows.push_back(Labeled("Used", Text(MbText(memory->usedMb))));
            space.rows.push_back(Labeled("Left", HeadroomSpan(MbText(memory->leftMb), headroom)));
            space.rows.push_back(Labeled("Of", Text(MbText(memory->ceilingMb))));
            if (headroom != Headroom::Plenty) space.rows.push_back(LineOf("The game closes once none is left.", Tone::Dim));
            sections.push_back(std::move(space));
        }
        PopupSection processor = Section(CardKind::PerfProcessor, "Processor");
        if (cpuPercent) processor.rows.push_back(Labeled("CPU", Text(Printed("%.0f%%", *cpuPercent) + " of one core")));
        if (frames)
        {
            processor.rows.push_back(Labeled("Frame Rate", Text(std::to_string(FramesPerSecond(frames->averageMs)) + " fps")));
            processor.rows.push_back(Labeled("Frame Time", Text(Printed("%.1f ms", frames->averageMs))));
            processor.rows.push_back(Labeled("Slowest Frame", Text(Printed("%.1f ms", frames->slowestMs))));
        }
        if (!processor.rows.empty()) sections.push_back(std::move(processor));
        if (linuxxi.report)
            AddLinuxXI(sections, *linuxxi.report);
        else if (linuxxi.otherVersion)
            sections.push_back(Section(CardKind::PerfLinuxXI, "LinuxXI", ListOf(LineOf("It sends report version " + std::to_string(*linuxxi.otherVersion) +
                                                                     "; this HeadsUp reads " + std::to_string(kLinuxXIReportVersion) + ".", Tone::Dim))));
        return sections;
    }
}
