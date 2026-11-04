#include "pose.h"
#include "boxes.h"
#include "nameplate.h"
#include "test.h"

#include <cmath>

using namespace headsup;

namespace
{
    // The scene camera in a capture of a town (2560x1440 back buffer).
    const float kView[16]       = {-0.53312f, 0.0461647f, -0.844779f, 0.0f, -0.0f, -0.99851f, -0.0545656f, 0.0f, -0.84604f,
              -0.02909f, 0.532325f, 0.0f, -47.0549f, -6.67921f, 100.925f, 1.0f};
    const float kProjection[16] = {0.747742f, 0.0f, 0.0f, 0.0f, 0.0f, 1.26042f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -1.0f, 0.0f, 0.0f,
        -0.1f, 0.0f};

    constexpr float kScreenHeight = 1440.0f;
    Camera TownCamera() { return MakeCamera(kView, kProjection, kScreenHeight); }

    bool Near(float a, float b) { return test::Near(a, b, 0.5f); } // a pixel row

    // The row of a point this high over the feet, as Direct3D projects it through kView and then kProjection.
    float RowAt(const WorldPoint& feet, float height)
    {
        const float point[4] = {feet.x, feet.y - height, feet.z, 1.0f};
        float view[4] = {}, clip[4] = {};
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                view[j] += point[k] * kView[k * 4 + j];
        for (int j = 0; j < 4; ++j)
            for (int k = 0; k < 4; ++k)
                clip[j] += view[k] * kProjection[k * 4 + j];
        return (1.0f - clip[1] / clip[3]) * 0.5f * kScreenHeight;
    }

    // The height over the feet at this row, by halving: a higher point is a smaller row.
    float HeightAt(const WorldPoint& feet, float row)
    {
        constexpr float kTallest = 10.0f; // yalms
        constexpr int kHalvings  = 40;
        float low = 0.0f, high = kTallest;
        for (int i = 0; i < kHalvings; ++i)
        {
            const float middle = (low + high) / 2.0f;
            (RowAt(feet, middle) > row ? low : high) = middle;
        }
        return (low + high) / 2.0f;
    }
}

TEST(a_camera_no_draw_gives_any_more_stops_counting)
{
    // A tester's client drew nothing that carries the camera for a whole frame, and the one kept from earlier was 73
    // yalms from the player and facing away: a kept camera is only good for the moment after its last draw.
    CHECK(CameraStillSeen(500, 500));
    CHECK(CameraStillSeen(500 + kCameraGraceFrames, 500));
    CHECK(!CameraStillSeen(500 + kCameraGraceFrames + 1, 500));
    CHECK(!CameraStillSeen(90000, 500));
}

TEST(a_camera_counts_for_seated_names_once_it_has_stayed)
{
    // A live client with shadows off had cameras from other draws for 9 to 42 frames at a time: none of those moves
    // a seated name, while a client that draws shadows has one every frame and lowers them after three seconds.
    uint32_t run = 0;
    for (int frame = 0; frame < 42; ++frame)
        run = CameraRun(run, true);
    CHECK(!CameraSteady(run));
    run = CameraRun(run, false);
    CHECK_EQ(run, 0u);
    for (int frame = 0; frame < 90; ++frame)
        run = CameraRun(run, true);
    CHECK(CameraSteady(run));
}

TEST(statuses_give_the_pose)
{
    CHECK(PoseFromStatus(0) == Pose::Standing);
    CHECK(PoseFromStatus(1) == Pose::Standing); // fighting
    CHECK(PoseFromStatus(33) == Pose::Resting);
    CHECK(PoseFromStatus(47) == Pose::Sitting);
    CHECK(PoseFromStatus(62) == Pose::Standing);
    CHECK(PoseFromStatus(63) == Pose::Chair); // the default chair
    CHECK(PoseFromStatus(83) == Pose::Chair); // the last chair key item
    CHECK(PoseFromStatus(84) == Pose::Standing);
}

TEST(sitting_for_aggro_is_phoenixs_rule)
{
    // Phoenix's isSitting: resting, sitting and the first eleven chairs let Too Weak aggressive mobs aggro; later chairs
    // only look seated.
    for (const uint32_t status : {33u, 47u, 63u, 73u})
        CHECK(IsSittingStatus(status));
    for (const uint32_t status : {0u, 1u, 62u, 74u, 83u})
        CHECK(!IsSittingStatus(status));
}

TEST(a_standing_players_name_stays_where_the_game_puts_it)
{
    const WorldPoint feet = FromEntityPosition(85.296f, -128.863f, -1.105f);
    CHECK(Near(PosedNameRow(TownCamera(), feet, 624.4f, Pose::Standing), 624.4f));
}

TEST(a_posed_players_name_comes_down_to_their_head)
{
    // In the capture the game put Shio's name 2.92 yalms above their feet in a chair (its bottom at row -51.4, above
    // the screen), Maryel's 1.65 sitting and Justcool's 2.21 resting, found by bisection on the same camera. Each comes
    // down to a share of that height and stays above the feet.
    const Camera camera = TownCamera();
    struct Case
    {
        WorldPoint feet;
        float gameRow;
        float gameHeight;
        Pose pose;
    };
    const Case cases[] = {{FromEntityPosition(61.957f, -97.996f, -0.917f), -51.4f, 2.9226f, Pose::Chair},
        {FromEntityPosition(83.920f, -99.868f, 0.297f), 662.7f, 1.6548f, Pose::Sitting},
        {FromEntityPosition(68.954f, -130.954f, -0.474f), 619.1f, 2.2062f, Pose::Resting}};
    for (const Case& c : cases)
    {
        const float game = HeightAt(c.feet, c.gameRow);
        CHECK(std::fabs(game - c.gameHeight) < 0.01f);
        const float share = HeightAt(c.feet, PosedNameRow(camera, c.feet, c.gameRow, c.pose)) / game;
        CHECK(share > 0.0f && share < 1.0f);
        // A share of the height, not a drop by so much: the same pose at another height comes down in the same share.
        const float higher = HeightAt(c.feet, PosedNameRow(camera, c.feet, RowAt(c.feet, 2.0f * game), c.pose)) / (2.0f * game);
        CHECK(std::fabs(higher - share) < 0.001f);
    }
}

TEST(a_seated_players_name_is_placed_on_screen_when_the_games_is_above_it)
{
    // Shio in a chair, with the game's name above the top of the screen and their head in view.
    const Camera camera     = TownCamera();
    const WorldPoint feet   = FromEntityPosition(61.957f, -97.996f, -0.917f);
    const ScreenBox letters = test::Box(1945.6f, -102.1f, 2221.6f, -51.4f);
    const ScreenBox placed  = PlaceName(letters, &camera, feet, Pose::Chair);
    CHECK(!NameOnScreen(letters, 2560.0f, 1440.0f));
    CHECK(NameOnScreen(placed, 2560.0f, 1440.0f));
    CHECK(std::fabs(placed.Height() - letters.Height()) < 0.01f && std::fabs(placed.CenterX() - letters.CenterX()) < 0.01f);
    CHECK(Near(placed.maxY, PosedNameRow(camera, feet, letters.maxY, Pose::Chair)));
}

TEST(a_name_that_cannot_be_projected_stays_put)
{
    // Behind the camera.
    const WorldPoint behind = FromEntityPosition(40.0f, -80.0f, 0.0f);
    CHECK(Near(PosedNameRow(TownCamera(), behind, 300.0f, Pose::Chair), 300.0f));
}

TEST(a_camera_knows_where_it_is)
{
    // A view of a camera at (10, -5, 20), turned a quarter about the height axis: its last row is the eye, turned
    // and negated.
    const float view[16] = {0.0f, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, -20.0f, 5.0f, 10.0f, 1.0f};
    const Camera camera  = MakeCamera(view, kProjection, 1440.0f);
    CHECK(test::Near(camera.eye.x, 10.0f, 1e-4f) && test::Near(camera.eye.y, -5.0f, 1e-4f) &&
          test::Near(camera.eye.z, 20.0f, 1e-4f));
    CHECK(test::Near(Distance(camera.eye, WorldPoint{10.0f, -5.0f, 17.0f}), 3.0f, 1e-4f));
    CHECK(test::Near(Distance(WorldPoint{0.0f, 0.0f, 0.0f}, WorldPoint{3.0f, 4.0f, 12.0f}), 13.0f, 1e-4f));
}

TEST(status_4_is_the_games_event_status_and_nothing_else_is)
{
    CHECK(IsEventStatus(4));
    CHECK(!IsEventStatus(0));
    CHECK(!IsEventStatus(1));
    CHECK(!IsEventStatus(3));
    CHECK(!IsEventStatus(5));
    CHECK(!IsEventStatus(33));
    CHECK(!IsEventStatus(47));
}
