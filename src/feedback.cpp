#include "feedback.h"

#include <cstdio>

namespace headsup
{
    namespace
    {
        // The service's own names, whatever the window calls them.
        constexpr const char* kTypeNames[kFeedbackTypeCount]   = {"Feature Request", "Suggestion", "Bug Report"};
        constexpr const char* kTopicNames[kFeedbackTopicCount] = {"General", "Nameplates", "Outlines", "Top Panel",
            "User Interface", "Other"};

        bool Blank(std::string_view text) { return text.find_first_not_of(" \t\r\n") == std::string_view::npos; }

        void AppendJsonString(std::string& out, std::string_view text)
        {
            out += '"';
            for (const char c : text)
            {
                if (c == '"' || c == '\\')
                    (out += '\\') += c;
                else if (c == '\n')
                    out += "\\n";
                else if (c == '\t')
                    out += "\\t";
                else if (static_cast<unsigned char>(c) < 0x20)
                {
                    char escaped[8];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned>(c));
                    out += escaped;
                }
                else
                    out += c;
            }
            out += '"';
        }
    }

    bool FeedbackFilled(const FeedbackDraft& draft)
    {
        return !Blank(draft.text) && (draft.topic != FeedbackTopic::Other || !Blank(draft.other));
    }

    std::string FeedbackBody(const FeedbackDraft& draft, std::string_view version)
    {
        std::string body = "{\"type\":";
        AppendJsonString(body, kTypeNames[static_cast<int>(draft.type)]);
        body += ",\"topic\":";
        AppendJsonString(body, kTopicNames[static_cast<int>(draft.topic)]);
        body += ",\"other\":";
        AppendJsonString(body, draft.topic == FeedbackTopic::Other ? std::string_view(draft.other) : std::string_view());
        body += ",\"text\":";
        AppendJsonString(body, draft.text);
        body += ",\"version\":";
        AppendJsonString(body, version);
        return body + "}";
    }
}
