#pragma once

#include <cstdint>
#include <vector>

namespace test
{
    // An action packet (0x028) as the server packs it (0x028_battle2.cpp): fields of bits, each from its lowest, from
    // byte 5 on, the bits of each byte from its lowest.
    struct ActionPacket
    {
        std::vector<uint8_t> bytes = std::vector<uint8_t>(128, 0);
        uint32_t bit               = 40;
        ActionPacket& Put(uint64_t value, uint32_t bits)
        {
            for (uint32_t i = 0; i < bits; ++i, ++bit)
                if (value >> i & 1) bytes[bit / 8] |= static_cast<uint8_t>(1 << bit % 8);
            return *this;
        }
        // A spell's end (category 4) unless another category is given.
        ActionPacket& Head(uint32_t actor, uint32_t targets, uint32_t category = 4)
        {
            return Put(actor, 32).Put(targets, 6).Put(0, 4).Put(category, 4).Put(43, 32).Put(0, 32);
        }
        ActionPacket& Target(uint32_t id, uint32_t results) { return Put(id, 32).Put(results, 4); }
        // Miss, kind, animation, info, distortion and knockback; the value and message; the modifier; with an additional
        // effect and spikes or none.
        ActionPacket& Result(uint32_t value, uint32_t message, bool extras = false)
        {
            Put(0, 3).Put(0, 2).Put(0, 12).Put(0, 5).Put(0, 2).Put(0, 3).Put(value, 17).Put(message, 10).Put(0, 31);
            if (!extras) return Put(0, 1).Put(0, 1);
            return Put(1, 1).Put(0, 6).Put(0, 4).Put(5, 17).Put(163, 10).Put(1, 1).Put(0, 6).Put(0, 4).Put(9, 14).Put(374, 10);
        }
        // With an additional effect of its own, as a skillchain arrives, and no spikes.
        ActionPacket& Added(uint32_t value, uint32_t message, uint32_t addedValue, uint32_t addedMessage)
        {
            Put(0, 3).Put(0, 2).Put(0, 12).Put(0, 5).Put(0, 2).Put(0, 3).Put(value, 17).Put(message, 10).Put(0, 31);
            return Put(1, 1).Put(0, 6).Put(0, 4).Put(addedValue, 17).Put(addedMessage, 10).Put(0, 1);
        }
        // With spikes of its own and no additional effect.
        ActionPacket& Spiked(uint32_t value, uint32_t message, uint32_t spikesValue, uint32_t spikesMessage)
        {
            Put(0, 3).Put(0, 2).Put(0, 12).Put(0, 5).Put(0, 2).Put(0, 3).Put(value, 17).Put(message, 10).Put(0, 31);
            return Put(0, 1).Put(1, 1).Put(0, 6).Put(0, 4).Put(spikesValue, 14).Put(spikesMessage, 10);
        }
        uint32_t Size() const { return (bit + 7) / 8; }
    };
}
