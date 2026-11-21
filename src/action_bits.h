#pragma once

#include <cstdint>
#include <optional>

namespace headsup
{
    // An action packet's (0x028) packing, as the server's 0x028_battle2.cpp packs it: fields of bits from byte
    // kActionBits / 8, each from its lowest bit.
    constexpr uint32_t kActionBits = 40;
    constexpr uint32_t kActorBits = 32, kTargetsBits = 6, kResultsSumBits = 4, kCommandBits = 4, kArgumentBits = 32,
                       kInfoBits = 32, kTargetIdBits = 32, kResultsBits = 4;
    // A result: miss, kind, animation, info, distortion and knockback, then value, message and modifier; an
    // additional effect's kind and info, value and message, and spikes' kind and info, value and message, after a bit
    // each that says they are there.
    constexpr uint32_t kResultLeadBits = 3 + 2 + 12 + 5 + 2 + 3, kValueBits = 17, kMessageBits = 10, kModifierBits = 31,
                       kAddedLeadBits = 6 + 4, kSpikesLeadBits = 6 + 4, kSpikesValueBits = 14,
                       kSpikesBits = kSpikesLeadBits + kSpikesValueBits + kMessageBits;

    class BitReader
    {
    public:
        BitReader(const uint8_t* data, uint32_t size) : m_Data(data), m_Bits(size * 8) {}
        // Nothing once past the end.
        std::optional<uint32_t> Read(uint32_t bits)
        {
            if (m_Bit + bits > m_Bits) return std::nullopt;
            uint32_t value = 0;
            for (uint32_t i = 0; i < bits; ++i, ++m_Bit)
                value |= static_cast<uint32_t>(m_Data[m_Bit / 8] >> m_Bit % 8 & 1) << i;
            return value;
        }
        void Skip(uint32_t bits) { m_Bit += bits; }

    private:
        const uint8_t* m_Data;
        uint32_t m_Bits, m_Bit = kActionBits;
    };
}
