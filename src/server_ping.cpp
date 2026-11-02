#include "server_ping.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#include "bar_connection.h" // kRoundTrips


namespace headsup
{
    namespace
    {
        constexpr auto kPingEvery     = std::chrono::seconds(1);
        constexpr auto kIdle          = std::chrono::seconds(3); // with nothing taking the round trips, it stops pinging
        constexpr DWORD kTimeoutMs    = 1000;
        constexpr size_t kPayload     = 32;  // as ping.exe sends
        constexpr size_t kReplySlack  = 8;   // IcmpSendEcho's room for an ICMP error
        // Phoenix's map server, every zone's, until a zone change names the server itself.
        constexpr const char* kMapServerHost = "play.phoenix-xi.com";

        uint32_t Resolved(const char* host)
        {
            addrinfo hints{};
            hints.ai_family   = AF_INET;
            addrinfo* found   = nullptr;
            if (getaddrinfo(host, nullptr, &hints, &found) != 0 || found == nullptr) return 0;
            const uint32_t address = reinterpret_cast<const sockaddr_in*>(found->ai_addr)->sin_addr.s_addr;
            freeaddrinfo(found);
            return address;
        }
    }

    std::string ServerPing::Start()
    {
        if (m_Icmp != nullptr) return "";
        const HANDLE icmp = IcmpCreateFile();
        if (icmp == INVALID_HANDLE_VALUE) return "Windows would not open a handle to ping with";
        m_Icmp     = icmp;
        m_Stopping = false;
        m_Thread   = std::thread([this] { Run(); });
        return "";
    }

    void ServerPing::Stop()
    {
        {
            const std::lock_guard lock(m_Lock);
            m_Stopping = true;
            m_Wake.notify_one();
        }
        // An echo out holds the thread until it is answered or kTimeoutMs pass.
        if (m_Thread.joinable()) m_Thread.join();
        if (m_Icmp != nullptr) IcmpCloseHandle(static_cast<HANDLE>(m_Icmp));
        m_Icmp = nullptr;
    }

    std::vector<double> ServerPing::TakeRoundTrips()
    {
        const std::lock_guard lock(m_Lock);
        m_TakenAt = std::chrono::steady_clock::now();
        std::vector<double> taken;
        taken.swap(m_RoundTrips);
        return taken;
    }

    void ServerPing::SendingTo(uint32_t address)
    {
        const std::lock_guard lock(m_Lock);
        m_Address = address;
    }

    void ServerPing::Run()
    {
        char payload[kPayload] = {};
        alignas(ICMP_ECHO_REPLY) uint8_t reply[sizeof(ICMP_ECHO_REPLY) + kPayload + kReplySlack];
        if (const uint32_t address = Resolved(kMapServerHost); address != 0)
        {
            const std::lock_guard lock(m_Lock);
            if (m_Address == 0) m_Address = address;
        }
        std::unique_lock lock(m_Lock);
        while (!m_Wake.wait_for(lock, kPingEvery, [this] { return m_Stopping; }))
        {
            const uint32_t address = m_Address;
            if (address == 0 || std::chrono::steady_clock::now() - m_TakenAt > kIdle) continue;
            lock.unlock();
            const DWORD replies = IcmpSendEcho(static_cast<HANDLE>(m_Icmp), address, payload, sizeof(payload), nullptr, reply,
                sizeof(reply), kTimeoutMs);
            lock.lock();
            const auto* echo = reinterpret_cast<const ICMP_ECHO_REPLY*>(reply);
            if (replies == 0 || echo->Status != IP_SUCCESS) continue;
            m_RoundTrips.push_back(static_cast<double>(echo->RoundTripTime));
            if (m_RoundTrips.size() > kRoundTrips) m_RoundTrips.erase(m_RoundTrips.begin());
        }
    }
}
