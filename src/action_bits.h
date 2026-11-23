#pragma once

#include <cstdint>
#include <optional>
#include <vector>

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

    // A result's parts: index is its place among its target's results; no additional effect or spikes reads 0.
    struct ParsedResult
    {
        uint32_t target, index, value, message, addedValue, addedMessage, spikesValue, spikesMessage;
    };
    struct ParsedAction
    {
        uint32_t actor, targets, category, argument;
        std::vector<ParsedResult> results;
    };

    // Nothing when the packet ends early.
    inline std::optional<ParsedAction> ParseAction(const uint8_t* data, uint32_t size)
    {
        if (data == nullptr) return std::nullopt;
        BitReader bits(data, size);
        const std::optional<uint32_t> actor = bits.Read(kActorBits), targets = bits.Read(kTargetsBits);
        bits.Skip(kResultsSumBits);
        const std::optional<uint32_t> category = bits.Read(kCommandBits), argument = bits.Read(kArgumentBits);
        if (!actor || !targets || !category || !argument) return std::nullopt;
        bits.Skip(kInfoBits);
        ParsedAction action{*actor, *targets, *category, *argument, {}};
        for (uint32_t t = 0; t < *targets; ++t)
        {
            const std::optional<uint32_t> target = bits.Read(kTargetIdBits), results = bits.Read(kResultsBits);
            if (!target || !results) return std::nullopt;
            for (uint32_t r = 0; r < *results; ++r)
            {
                bits.Skip(kResultLeadBits);
                const std::optional<uint32_t> value = bits.Read(kValueBits), message = bits.Read(kMessageBits);
                bits.Skip(kModifierBits);
                const std::optional<uint32_t> added = bits.Read(1);
                std::optional<uint32_t> addedValue = 0, addedMessage = 0;
                if (added && *added)
                {
                    bits.Skip(kAddedLeadBits);
                    addedValue   = bits.Read(kValueBits);
                    addedMessage = bits.Read(kMessageBits);
                }
                const std::optional<uint32_t> spikes = bits.Read(1);
                std::optional<uint32_t> spikesValue = 0, spikesMessage = 0;
                if (spikes && *spikes)
                {
                    bits.Skip(kSpikesLeadBits);
                    spikesValue   = bits.Read(kSpikesValueBits);
                    spikesMessage = bits.Read(kMessageBits);
                }
                if (!value || !message || !addedValue || !addedMessage || !spikes || !spikesValue || !spikesMessage)
                    return std::nullopt;
                action.results.push_back(
                    {*target, r, *value, *message, *addedValue, *addedMessage, *spikesValue, *spikesMessage});
            }
        }
        return action;
    }
}
