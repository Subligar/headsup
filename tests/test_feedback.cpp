#include "feedback.h"
#include "test.h"

using namespace headsup;

TEST(feedback_needs_its_text_and_an_other_topic_its_own_words)
{
    FeedbackDraft draft;
    CHECK(!FeedbackFilled(draft));
    draft.text = " \n\t ";
    CHECK(!FeedbackFilled(draft));
    draft.text = "Names flicker.";
    CHECK(FeedbackFilled(draft));
    draft.other = "unused";
    CHECK(FeedbackFilled(draft));

    draft.topic = FeedbackTopic::Other;
    draft.other = "  ";
    CHECK(!FeedbackFilled(draft));
    draft.other = "Fishing";
    CHECK(FeedbackFilled(draft));
    draft.text = "";
    CHECK(!FeedbackFilled(draft));
}

TEST(feedback_is_sent_as_the_service_reads_it)
{
    FeedbackDraft draft;
    draft.type  = FeedbackType::BugReport;
    draft.topic = FeedbackTopic::TopPanel;
    draft.other = "kept for Other only";
    draft.text  = "It says \"hi\"\\\nthen\ttabs\x01 and caf\xc3\xa9";
    CHECK_EQ(FeedbackBody(draft, "0.8"),
        std::string(R"({"type":"Bug Report","topic":"Top Panel","other":"","text":"It says \"hi\"\\\nthen\ttabs\u0001 and caf)"
                    "\xc3\xa9"
                    R"(","version":"0.8"})"));

    draft.topic = FeedbackTopic::Other;
    draft.other = "Fish";
    draft.text  = "x";
    CHECK_EQ(FeedbackBody(draft, "1"),
        std::string(R"({"type":"Bug Report","topic":"Other","other":"Fish","text":"x","version":"1"})"));
}
