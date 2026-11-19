#include "memory_query.h"

#include <windows.h>
#include <winternl.h>

namespace headsup
{
    namespace
    {
        constexpr const char* kWineKey = "Software\\Wine";
        constexpr uint64_t kSpaceEnd   = uint64_t{1} << 32;
        constexpr DWORD kReadable      = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ |
            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
    }

    bool UnderWine()
    {
        HKEY key        = nullptr;
        const bool wine = RegOpenKeyExA(HKEY_CURRENT_USER, kWineKey, 0, KEY_READ, &key) == ERROR_SUCCESS;
        if (key != nullptr) RegCloseKey(key);
        return wine;
    }

    std::optional<uint64_t> VirtualSizeNow()
    {
        using QueryFn           = LONG(WINAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
        static const auto query = reinterpret_cast<QueryFn>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationProcess")));
        VM_COUNTERS counters{};
        if (query == nullptr || query(GetCurrentProcess(), ProcessVmCounters, &counters, sizeof counters, nullptr) != 0) return std::nullopt;
        return counters.VirtualSize;
    }

    std::optional<Region> RegionAt(uint64_t at)
    {
        MEMORY_BASIC_INFORMATION info{};
        if (at >= kSpaceEnd || VirtualQuery(reinterpret_cast<void*>(static_cast<uintptr_t>(at)), &info, sizeof info) == 0) return std::nullopt;
        return Region{reinterpret_cast<uintptr_t>(info.BaseAddress), info.RegionSize, info.State == MEM_FREE};
    }

    bool Readable(const void* from, size_t bytes)
    {
        const auto* at = static_cast<const uint8_t*>(from);
        while (bytes > 0)
        {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(at, &info, sizeof(info)) != sizeof(info)) return false;
            if (info.State != MEM_COMMIT || (info.Protect & kReadable) == 0 || (info.Protect & PAGE_GUARD) != 0)
                return false;
            const auto* end   = static_cast<const uint8_t*>(info.BaseAddress) + info.RegionSize;
            const size_t step = bytes < static_cast<size_t>(end - at) ? bytes : static_cast<size_t>(end - at);
            at += step;
            bytes -= step;
        }
        return true;
    }
}
