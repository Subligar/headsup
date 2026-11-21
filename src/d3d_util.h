#pragma once

#include "Ashita.h"
#include "copy_state.h"
#include "tracker.h"

#include <cstdint>
#include <vector>

namespace headsup
{
    bool IsDeclarationHandle(DWORD vertexShader);
    bool BoundFvf(IDirect3DDevice8* device, DWORD& fvf);

    CopyState ReadCopyState(IDirect3DDevice8* device);
    void RestoreCopyState(IDirect3DDevice8* device, const CopyState& state);

    // FindOwner over the current stack.
    const ActorInfo* FindOwnerOnStack(const Tracker& tracker);
#ifdef HEADSUP_DEV
    // Every entity FindOwnerOnStack's scan meets, nearest first: bytes above the game's return address it starts at,
    // and the game's return addresses passed on the way (frames). For the developer's draw dump.
    struct StackOwner
    {
        uint16_t index;
        uint32_t bytes, frames;
    };
    std::vector<StackOwner> OwnersOnStack(const Tracker& tracker);
#endif
}
