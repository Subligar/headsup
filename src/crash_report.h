#pragma once

#include <cstddef>
#include <cstdint>

namespace headsup
{
    struct CrashModule
    {
        char name[64];
        uint32_t base;
        uint32_t size;
        uint32_t codeStart; // the span of its executable sections, which return addresses point into
        uint32_t codeEnd;
        uint32_t stamp; // the link time in its header, which tells one build of a file from another
    };

    struct CrashFacts
    {
        const char* version;
        const char* when;
        const char* kind; // what caught it
        const char* step; // the last step HeadsUp reached
        uint32_t code;
        uint32_t address;
        uint32_t info[2]; // an access violation's read, write or execute, and the address it touched
        uint32_t infoCount;
        uint32_t eax, ebx, ecx, edx, esi, edi, ebp, esp, eip, eflags;
        const uint32_t* stack; // from esp up
        size_t stackWords;
        const CrashModule* modules;
        size_t moduleCount;
    };

    // The report, written into out without the heap, which may be what broke. Cut short at size, always ended.
    size_t WriteCrashReport(const CrashFacts& facts, char* out, size_t size);
}
