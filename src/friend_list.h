#pragma once

#include "native_locate.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace headsup
{
    // The game's own friend list, which Phoenix's profile server fills: slots in FFXiMain's data, 200 for friends
    // and then the block list's. Each holds its own number, filled or not, which is how the list is found.
    constexpr size_t kFriendSlotBytes = 0x100;
    constexpr int kFriendSlots        = 200;

    // Where the list's first slot starts in the image's data, found by the numbers its slots hold of their own
    // places; nothing when the data has no such run, or more than one.
    std::optional<uint32_t> LocateFriendList(const ImageSection& data);

    // The characters the list's friends are playing or last played, by server id, in ascending order: what tells
    // a player in view for a friend.
    std::vector<uint32_t> FriendCharacters(const uint8_t* firstSlot);
    bool IsFriend(std::span<const uint32_t> characters, uint32_t serverId);

    struct FriendEntry
    {
        uint32_t account = 0;         // the slot's number halved (2003 is account 1001)
        std::string added, character; // the name it was added by, and the character played or last played
        bool playing = false;
        uint16_t zone = 0; // where they play, as of the game's last ask: its friend list's, every two minutes or so
        bool operator==(const FriendEntry&) const = default;
    };
    // Every filled friend slot, in slot order; none of the block list's.
    std::vector<FriendEntry> FriendEntries(const uint8_t* firstSlot);
    // The character when the slot has one, else the name it was added by.
    const std::string& FriendName(const FriendEntry& entry);
}
