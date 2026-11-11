#include "wave_out.h"

#include <mmsystem.h>

namespace headsup
{
    namespace
    {
        constexpr WORD kChannels = 2, kBits = 16;
    }

    void WaveOut::Play(std::vector<int16_t> stereo, int rate)
    {
        Stop();
        if (stereo.empty()) return;
        WAVEFORMATEX format{};
        format.wFormatTag      = WAVE_FORMAT_PCM;
        format.nChannels       = kChannels;
        format.nSamplesPerSec  = static_cast<DWORD>(rate);
        format.wBitsPerSample  = kBits;
        format.nBlockAlign     = kChannels * kBits / 8;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        if (waveOutOpen(&m_Device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL) != MMSYSERR_NOERROR)
        {
            m_Device = nullptr;
            return;
        }
        m_Samples                = std::move(stereo);
        m_Header                 = WAVEHDR{};
        m_Header.lpData          = reinterpret_cast<LPSTR>(m_Samples.data());
        m_Header.dwBufferLength  = static_cast<DWORD>(m_Samples.size() * sizeof(int16_t));
        if (waveOutPrepareHeader(m_Device, &m_Header, sizeof(m_Header)) != MMSYSERR_NOERROR ||
            waveOutWrite(m_Device, &m_Header, sizeof(m_Header)) != MMSYSERR_NOERROR)
            Stop();
    }

    void WaveOut::Stop()
    {
        if (m_Device == nullptr) return;
        waveOutReset(m_Device);
        waveOutUnprepareHeader(m_Device, &m_Header, sizeof(m_Header));
        waveOutClose(m_Device);
        m_Device = nullptr;
        m_Samples.clear();
    }
}
