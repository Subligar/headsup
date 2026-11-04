#pragma once

#include <windows.h>

namespace headsup
{
    // The module's import address slot that holds `function` from `library`; nullptr when it has none.
    void** ImportSlot(HMODULE module, const char* library, void* function);
    // Points the slot at value; false when its page could not be made writable.
    bool WriteSlot(void** slot, void* value);
}
