#include "bar_job.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <utility>
#include <vector>

namespace headsup
{
    namespace
    {
        const char* const kJobNames[] = {"", "WAR", "MNK", "WHM", "BLM", "RDM", "THF", "PLD", "DRK", "BST", "BRD", "RNG",
            "SAM", "NIN", "DRG", "SMN", "BLU", "COR", "PUP", "DNC", "SCH", "GEO", "RUN"};
        constexpr auto kHour = static_cast<double>(kSecondsPerHour);

        uint32_t Rise(uint64_t before, uint64_t after) { return after > before ? static_cast<uint32_t>(after - before) : 0; }

        uint64_t LimitTotal(const JobSample& s) { return static_cast<uint64_t>(s.merits) * kLimitPointsPerMerit + s.limitPoints; }

        bool AtCap(const JobSample& s) { return s.mainLevel >= kLevelCap; }

        std::string OutOf(uint32_t value, uint32_t whole) { return Grouped(value) + " / " + Grouped(whole); }

        Span PerHourText(const std::optional<double>& perHour)
        {
            return perHour ? Text(Grouped(static_cast<uint32_t>(std::lround(*perHour)))) : Text("-", Tone::Dim);
        }

        std::string Jobs(const JobSample& s) { return JobsText(s.mainJob, s.mainLevel, s.subJob, s.subLevel); }

        uint32_t ToNext(const JobSample& s) { return s.expNeeded > s.exp ? s.expNeeded - s.exp : 0; }

        constexpr int kWholePercent    = 100;

        // What one text says of points earned out of whole, in unit (EXP or LP); a percent is never above what is
        // earned.
        std::string ExpPart(uint32_t earned, uint32_t whole, const char* unit, ExpText text)
        {
            switch (text)
            {
            case ExpText::Earned: return OutOf(earned, whole) + " " + unit;
            case ExpText::ToNext: return Grouped(whole > earned ? whole - earned : 0) + " til next level";
            case ExpText::Percent:
                return std::to_string(whole > 0 ? static_cast<uint64_t>(std::min(earned, whole)) * kWholePercent / whole : 0) + "%";
            case ExpText::Jobs: break;
            }
            return {};
        }

        std::string JobsAs(const JobSample& s, JobText text)
        {
            const std::string main = JobName(s.mainJob), sub = s.subLevel > 0 ? JobName(s.subJob) : "";
            if (main.empty()) return "-";
            const std::string level = std::to_string(s.mainLevel);
            switch (text)
            {
            case JobText::MainSub: return Jobs(s);
            case JobText::Main: return main + " " + level;
            case JobText::LevelFirst: return level + " " + main + (sub.empty() ? "" : " / " + sub);
            }
            return {};
        }

        // What a text says of the EXP, or the jobs as job writes them; at the cap the points are limit points and the
        // merits stand for what is left to the next.
        std::string ExpSaid(const JobSample& s, ExpText text, JobText job = JobText::MainSub)
        {
            if (text == ExpText::Jobs) return JobsAs(s, job);
            if (!AtCap(s)) return ExpPart(s.exp, s.expNeeded, "EXP", text);
            if (text == ExpText::ToNext) return std::to_string(s.merits) + (s.merits == 1 ? " merit" : " merits");
            return ExpPart(std::min(s.limitPoints, kLimitPointsPerMerit), kLimitPointsPerMerit, "LP", text);
        }

        // How full the bar is: of the level, or of the next merit at the cap; nothing without a level's worth.
        std::optional<float> ExpDone(const JobSample& s)
        {
            if (AtCap(s)) return static_cast<float>(std::min(s.limitPoints, kLimitPointsPerMerit)) / kLimitPointsPerMerit;
            if (s.expNeeded == 0) return std::nullopt;
            return static_cast<float>(std::min(s.exp, s.expNeeded)) / static_cast<float>(s.expNeeded);
        }
    }

    const char* JobName(uint8_t job) { return job < std::size(kJobNames) ? kJobNames[job] : ""; }

    std::string JobsText(uint8_t mainJob, uint8_t mainLevel, uint8_t subJob, uint8_t subLevel)
    {
        const std::string main = JobName(mainJob);
        if (main.empty()) return "-";
        std::string text = main + " " + std::to_string(mainLevel);
        if (const std::string sub = JobName(subJob); !sub.empty() && subLevel > 0)
            text += " / " + sub + " " + std::to_string(subLevel);
        return text;
    }

    uint32_t Gained(const JobSample& before, const JobSample& after)
    {
        if (before.mainJob != after.mainJob) return 0;
        if (AtCap(before) && AtCap(after))
        {
            // The server sends merits and limit points apart, so a merit can arrive a frame after its points wrapped.
            const uint32_t points = Rise(LimitTotal(before), LimitTotal(after));
            return (points < kLimitPointsPerMerit ? points : 0) + Rise(before.exp, after.exp);
        }
        if (after.mainLevel == before.mainLevel) return Rise(before.exp, after.exp);
        if (after.mainLevel == before.mainLevel + 1 && before.expNeeded >= before.exp)
            return before.expNeeded - before.exp + after.exp;
        return 0;
    }

    void ExpTracker::Reset() { m_Last.reset(); }

    void ExpTracker::Update(double now, const JobSample& sample, uint32_t who)
    {
        if (!m_Last || m_Last->mainJob != sample.mainJob || m_Who != who || now - m_LastAt > kExpMaxGap ||
            now < m_LastAt)
        {
            m_Gains.clear();
            m_Since = now;
        }
        else if (const uint32_t points = Gained(*m_Last, sample); points > 0)
            m_Gains.push_back(Gain{now, points});
        m_Last   = sample;
        m_Who    = who;
        m_LastAt = now;
        while (!m_Gains.empty() && m_Gains.front().at < std::floor(now) - kExpWindow)
            m_Gains.pop_front();
    }

    std::optional<double> ExpTracker::PerHour(double now) const
    {
        const double second = std::floor(now);
        const double start  = std::max(m_Since, second - kExpWindow);
        double total        = 0.0;
        for (const Gain& g : m_Gains)
            if (g.at >= start && g.at <= second) total += g.points;
        const double span = second - start;
        if (!m_Last || span < kExpLeastSpan || total <= 0.0) return std::nullopt;
        return total * kHour / span;
    }

    std::vector<PartContent> JobParts(const std::optional<JobReport>& job, const WidgetOptions& o, WidgetContent& base)
    {
        if (!job) return {};
        const JobSample& s = job->sample;
        std::vector<PartContent> parts{{WidgetPart::JobJobs, ListOf(Text(JobsAs(s, o.jobText))), o.jobInfoAlign},
            {WidgetPart::JobEarned, ListOf(Text(ExpSaid(s, ExpText::Earned))), o.earnedAlign},
            {WidgetPart::JobToNext, ListOf(Text(ExpSaid(s, ExpText::ToNext))), o.toNextAlign},
            {WidgetPart::JobPercent, ListOf(Text(ExpSaid(s, ExpText::Percent))), o.percentAlign}, {WidgetPart::JobBar, {}}};
        if (const std::optional<float> done = ExpDone(s))
        {
            Span bar = Text("");
            bar.bar  = InlineBar{*done, o.expBarWidth, o.expBarHeight, o.expBarText ? ExpSaid(s, *o.expBarText, o.jobText) : "",
                 o.expBarTextAlign, o.expBarBack, o.expBarFill};
            parts.back().spans.push_back(bar);
        }
        base.hasPopup = true;
        base.figures  = true;
        return parts;
    }

    PopupContent JobPopup(const std::optional<JobReport>& job)
    {
        PopupContent popup;
        if (!job) return popup;
        const JobSample& s = job->sample;
        popup.title = ListOf(Text("Job and EXP")); // the jobs and levels are on the bar, in the detail font
        PopupSection rows = Section(CardKind::JobExp, "");
        if (AtCap(s))
        {
            rows.rows.push_back(Labeled("Merits", Text(std::to_string(s.merits) + " / " + std::to_string(s.meritsMax))));
            rows.rows.push_back(Labeled("Limit Points", Text(OutOf(s.limitPoints, kLimitPointsPerMerit))));
            rows.rows.push_back(Labeled("Per Hour", PerHourText(job->perHour)));
            if (job->perHour && s.limitMode && s.merits < s.meritsMax)
                rows.rows.push_back(Labeled("Next Merit in",
                    Text(SpanText((kLimitPointsPerMerit - std::min(s.limitPoints, kLimitPointsPerMerit)) * kHour /
                                  *job->perHour))));
            if (!s.limitMode) rows.rows.push_back(LineOf("Limit Mode is off, so EXP does not become merits.", Tone::Dim));
        }
        else
        {
            rows.rows.push_back(Labeled("Earned EXP", Text(OutOf(s.exp, s.expNeeded))));
            rows.rows.push_back(Labeled("Til Next Level", Text(Grouped(ToNext(s)))));
            rows.rows.push_back(Labeled("EXP Per Hour", PerHourText(job->perHour)));
            if (job->perHour)
                rows.rows.push_back(Labeled("Next Level in", Text(SpanText(ToNext(s) * kHour / *job->perHour))));
            rows.rows.push_back(Labeled("Limit Points", Text(OutOf(s.limitPoints, kLimitPointsPerMerit))));
        }
        popup.sections = ListOf(std::move(rows));
        return popup;
    }
}
