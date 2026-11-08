#pragma once

#include <string>
#include <string_view>

namespace headsup
{
    // The Feedback page's form, and what is sent of it to the team's feedback service.
    enum class FeedbackType : int
    {
        FeatureRequest,
        Suggestion,
        BugReport,
    };
    constexpr int kFeedbackTypeCount = static_cast<int>(FeedbackType::BugReport) + 1;
    enum class FeedbackTopic : int
    {
        General,
        Nameplates,
        Outlines,
        TopPanel,
        UserInterface,
        Other,
    };
    constexpr int kFeedbackTopicCount = static_cast<int>(FeedbackTopic::Other) + 1;
    // The most characters each box takes; the service turns away more.
    constexpr size_t kMostFeedbackText = 4000, kMostFeedbackOther = 60;
    constexpr const wchar_t* kFeedbackHost = L"feedback.subligar.dev";
    constexpr const wchar_t* kFeedbackPath = L"/feedback";

    struct FeedbackDraft
    {
        FeedbackType type   = FeedbackType::FeatureRequest;
        FeedbackTopic topic = FeedbackTopic::General;
        std::string other, text; // other: what an Other topic is
    };
    // Where Send Feedback has got to: it says so under the button.
    enum class FeedbackStatus
    {
        None,
        Missing,
        Sending,
        Sent,
        Failed,
    };

    // Whether it can be sent: its text, and an Other topic's own words, are more than spaces.
    bool FeedbackFilled(const FeedbackDraft& draft);
    // The JSON the service takes, with the plugin's version as it prints it.
    std::string FeedbackBody(const FeedbackDraft& draft, std::string_view version);
}
