#include "case_click.h"
#include "test.h"

#include <cstdlib>
#include <vector>

using namespace headsup;

namespace
{
    size_t At(float seconds) { return static_cast<size_t>(seconds * kClickRate) * 2; } // a left sample's place
    bool SilentBetween(const std::vector<int16_t>& track, float from, float to)
    {
        for (size_t i = At(from); i < At(to) && i < track.size(); ++i)
            if (track[i] != 0) return false;
        return true;
    }
    size_t Loudest(const std::vector<int16_t>& track, float from, float to)
    {
        size_t best = At(from);
        for (size_t i = At(from); i < At(to); i += 2)
            if (std::abs(track[i]) > std::abs(track[best])) best = i;
        return best;
    }
}

TEST(a_click_sounds_at_each_moment_and_nowhere_else)
{
    const std::vector<int16_t> track = ClickTrack({1.0f, 2.5f}, 4.0f);
    CHECK_EQ(track.size(), size_t{4 * 44100 * 2});
    CHECK(SilentBetween(track, 0.0f, 0.99f) && SilentBetween(track, 1.1f, 2.49f) && SilentBetween(track, 2.6f, 4.0f));
    CHECK(!SilentBetween(track, 0.99f, 1.1f) && !SilentBetween(track, 2.49f, 2.6f));
    CHECK(std::abs(static_cast<float>(Loudest(track, 0.99f, 1.1f)) / 2.0f / kClickRate - 1.0f) < 0.005f);
    CHECK(std::abs(static_cast<float>(Loudest(track, 2.49f, 2.6f)) / 2.0f / kClickRate - 2.5f) < 0.005f);
    for (size_t i = 0; i < track.size(); i += 2) CHECK(track[i] == track[i + 1]);
}

TEST(clicks_that_overlap_add_and_never_wrap)
{
    const std::vector<int16_t> one = ClickTrack({1.0f}, 2.0f), many = ClickTrack(std::vector<float>(40, 1.0f), 2.0f);
    bool louder = false;
    for (size_t i = 0; i < one.size(); ++i)
    {
        CHECK((one[i] > 0) == (many[i] > 0) && (one[i] < 0) == (many[i] < 0)); // clamped, not wrapped
        louder |= std::abs(many[i]) > std::abs(one[i]);
    }
    CHECK(louder);
}

TEST(a_click_past_the_end_is_cut_not_written_outside)
{
    CHECK_EQ(ClickTrack({3.99f}, 4.0f).size(), size_t{4 * 44100 * 2});
    CHECK_EQ(ClickTrack({9.0f}, 4.0f).size(), size_t{4 * 44100 * 2});
}
