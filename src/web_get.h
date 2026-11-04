#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace headsup
{
    // One HTTPS GET at a time, on a thread of its own, so the game never waits on the network.
    class WebGet
    {
    public:
        ~WebGet();
        // Fetches https://host/path, unless a fetch is under way.
        void Start(const wchar_t* host, const wchar_t* path);
        // Cancels the fetch under way and waits for its thread.
        void Stop();
        struct Answer
        {
            bool ok = false; // a 200 with its body; anything else is not
            std::string body;
        };
        // The answer of the fetch that ended last, once.
        std::optional<Answer> Take();

    private:
        void Run(std::wstring host, std::wstring path);

        std::thread m_Thread;
        std::mutex m_Lock;
        bool m_Busy = false, m_Stopping = false;
        void* m_Request = nullptr; // the fetch's WinHTTP request, which Stop closes to end a wait on it
        std::optional<Answer> m_Answer;
    };
}
