#include "crash_report.h"

#include <cstdarg>
#include <cstdio>

namespace headsup
{
    namespace
    {
        constexpr size_t kMostFrames        = 64;
        constexpr uint32_t kAccessViolation = 0xC0000005;

        struct Out
        {
            char* at;
            size_t room; // with the final zero
            size_t written = 0;

            void Add(const char* format, ...) __attribute__((format(printf, 2, 3)))
            {
                if (written + 1 >= room) return;
                va_list args;
                va_start(args, format);
                const int n = std::vsnprintf(at + written, room - written, format, args);
                va_end(args);
                if (n > 0) written += static_cast<size_t>(n) < room - written ? static_cast<size_t>(n) : room - written - 1;
            }
        };

        const char* FaultName(uint32_t code)
        {
            switch (code)
            {
            case kAccessViolation: return "access violation";
            case 0xC000001D: return "illegal instruction";
            case 0xC0000096: return "privileged instruction";
            case 0xC0000094: return "integer division by zero";
            case 0xC00000FD: return "stack overflow";
            case 0xC0000006: return "in-page error";
            case 0xC0000374: return "heap corruption";
            case 0xC0000409: return "stack buffer overrun";
            case 0x80000002: return "misaligned data";
            default: return "exception";
            }
        }

        const char* AccessName(uint32_t kind) { return kind == 0 ? "reading" : kind == 1 ? "writing" : "executing"; }

        const CrashModule* ModuleAt(const CrashFacts& facts, uint32_t address)
        {
            for (size_t i = 0; i < facts.moduleCount; ++i)
                if (address - facts.modules[i].base < facts.modules[i].size) return &facts.modules[i];
            return nullptr;
        }

        void AddPlace(Out& out, const CrashFacts& facts, uint32_t address)
        {
            if (const CrashModule* module = ModuleAt(facts, address))
                out.Add("%s+0x%x (%08x)\n", module->name, address - module->base, address);
            else
                out.Add("%08x (in no module)\n", address);
        }
    }

    size_t WriteCrashReport(const CrashFacts& facts, char* out, size_t size)
    {
        if (size == 0) return 0;
        out[0] = '\0';
        Out o{out, size};
        o.Add("HeadsUp %s: %s, %s\n", facts.version, facts.kind, facts.when);
        o.Add("last step: %s\n", facts.step);
        o.Add("%08x %s", facts.code, FaultName(facts.code));
        if (facts.code == kAccessViolation && facts.infoCount >= 2)
            o.Add(" %s %08x", AccessName(facts.info[0]), facts.info[1]);
        o.Add("\nat ");
        AddPlace(o, facts, facts.address);
        o.Add("eax %08x ebx %08x ecx %08x edx %08x esi %08x edi %08x\nebp %08x esp %08x eip %08x eflags %08x\n",
            facts.eax, facts.ebx, facts.ecx, facts.edx, facts.esi, facts.edi, facts.ebp, facts.esp, facts.eip, facts.eflags);
        o.Add("\nreturn addresses on the stack, newest first:\n");
        size_t frames = 0;
        for (size_t i = 0; i < facts.stackWords && frames < kMostFrames; ++i)
        {
            const uint32_t word       = facts.stack[i];
            const CrashModule* module = ModuleAt(facts, word);
            if (module == nullptr || word < module->codeStart || word >= module->codeEnd) continue;
            o.Add("  esp+%04x ", static_cast<unsigned>(i * sizeof(uint32_t)));
            AddPlace(o, facts, word);
            ++frames;
        }
        o.Add("\nmodules (base, size, link time, name):\n");
        for (size_t i = 0; i < facts.moduleCount; ++i)
            o.Add("  %08x %08x %08x %s\n", facts.modules[i].base, facts.modules[i].size, facts.modules[i].stamp,
                facts.modules[i].name);
        return o.written;
    }
}
