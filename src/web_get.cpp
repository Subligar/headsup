#include "web_get.h"

#include <windows.h>
#include <winhttp.h>

namespace headsup
{
    namespace
    {
        constexpr int kResolveMs = 5000, kConnectMs = 5000, kSendMs = 5000, kReceiveMs = 10000;
        constexpr DWORD kOk      = 200;
        constexpr size_t kChunk  = 4096;

        // A WinHTTP handle closed when it goes.
        struct Handle
        {
            HINTERNET h = nullptr;
            ~Handle()
            {
                if (h != nullptr) WinHttpCloseHandle(h);
            }
        };
    }

    WebGet::~WebGet() { Stop(); }

    void WebGet::Start(const wchar_t* host, const wchar_t* path)
    {
        {
            std::lock_guard lock(m_Lock);
            if (m_Busy) return;
            m_Busy = true;
        }
        if (m_Thread.joinable()) m_Thread.join(); // the last fetch, which has ended
        m_Thread = std::thread(&WebGet::Run, this, std::wstring(host), std::wstring(path));
    }

    void WebGet::Stop()
    {
        {
            std::lock_guard lock(m_Lock);
            m_Stopping = true;
            if (m_Request != nullptr) WinHttpCloseHandle(std::exchange(m_Request, nullptr));
        }
        if (m_Thread.joinable()) m_Thread.join();
    }

    std::optional<WebGet::Answer> WebGet::Take()
    {
        std::lock_guard lock(m_Lock);
        return std::exchange(m_Answer, std::nullopt);
    }

    void WebGet::Run(std::wstring host, std::wstring path)
    {
        Answer answer;
        const Handle session{WinHttpOpen(L"HeadsUp", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS, 0)};
        if (session.h != nullptr) WinHttpSetTimeouts(session.h, kResolveMs, kConnectMs, kSendMs, kReceiveMs);
        const Handle connect{session.h != nullptr ? WinHttpConnect(session.h, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr};
        HINTERNET request = connect.h != nullptr ? WinHttpOpenRequest(connect.h, L"GET", path.c_str(), nullptr,
                                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                                 : nullptr;
        {
            std::lock_guard lock(m_Lock);
            if (m_Stopping && request != nullptr) WinHttpCloseHandle(std::exchange(request, nullptr));
            m_Request = request;
        }
        // Closed by Stop, the request fails whatever call it is in.
        DWORD status = 0, size = sizeof(status);
        if (request != nullptr &&
            WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
            WinHttpReceiveResponse(request, nullptr) &&
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                &status, &size, WINHTTP_NO_HEADER_INDEX))
        {
            char chunk[kChunk];
            DWORD got = 0;
            while (WinHttpReadData(request, chunk, sizeof(chunk), &got) && got > 0)
                answer.body.append(chunk, got);
            answer.ok = status == kOk;
        }
        std::lock_guard lock(m_Lock);
        if (m_Request != nullptr) WinHttpCloseHandle(std::exchange(m_Request, nullptr));
        m_Answer = std::move(answer);
        m_Busy   = false;
    }
}
