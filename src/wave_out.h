#pragma once

#include <cstdint>
#include <vector>

#include <windows.h>

namespace headsup
{
    // A sound of HeadsUp's own on Windows' wave output, so it cuts off nobody else's: PlaySound keeps one sound for
    // the whole process, which an addon's alert would share.
    class WaveOut
    {
    public:
        WaveOut() = default;
        WaveOut(const WaveOut&)            = delete;
        WaveOut& operator=(const WaveOut&) = delete;
        ~WaveOut() { Stop(); }
        // In place of what it was playing; silent when no device opens.
        void Play(std::vector<int16_t> stereo, int rate);
        void Stop();

    private:
        HWAVEOUT m_Device = nullptr;
        WAVEHDR m_Header{};
        std::vector<int16_t> m_Samples; // kept until the device lets go of them
    };
}
