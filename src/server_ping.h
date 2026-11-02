#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace headsup
{
    // Pings the game's map server, on a thread of its own. The game reads its socket once a frame, so timing its own
    // packets can say no better than the frame time; an ICMP echo, which Phoenix's server answers, can. The address is
    // Phoenix's map server's by name until a zone change names the server (ParseZoneOutAddress): nothing of the game's
    // is touched, which virus scanners took for something worse.
    class ServerPing
    {
    public:
        ~ServerPing() { Stop(); }
        // Returns why it could not start, or an empty string.
        std::string Start();
        void Stop();
        // The round trips, in milliseconds, since the last call. It pings only while something takes them.
        std::vector<double> TakeRoundTrips();
        // The game's map server: an IPv4 address as it goes on the wire.
        void SendingTo(uint32_t address);

    private:
        void Run();

        void* m_Icmp  = nullptr; // IcmpCreateFile's handle
        std::thread m_Thread;
        std::mutex m_Lock;
        std::condition_variable m_Wake;
        bool m_Stopping    = false;
        uint32_t m_Address = 0;
        std::chrono::steady_clock::time_point m_TakenAt;
        std::vector<double> m_RoundTrips;
    };
}
