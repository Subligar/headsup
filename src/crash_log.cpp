#include "crash_log.h"

#include "crash_report.h"
#include "memory_query.h"

#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstring>

namespace headsup::crash
{
    namespace
    {
        constexpr size_t kMostModules    = 512;
        constexpr size_t kMostStackWords = 16384;
        constexpr size_t kMostSteps      = 64;
        constexpr LONG kMostReports      = 4;
        constexpr size_t kReportBytes    = 128 * 1024;
        constexpr const char* kStartFile = "start.txt";
        constexpr const char* kPreviousStartFile = "start-previous.txt";

        char g_Folder[MAX_PATH];
        const char* g_Version         = "";
        const char* volatile g_Step   = "loading";
        const char* g_Seen[kMostSteps] = {};
        size_t g_SeenCount            = 0;
        DWORD g_Loaded                = 0;
        uint32_t g_OwnBase            = 0;
        uint32_t g_OwnSize            = 0;
        void* g_Handler               = nullptr;
        LPTOP_LEVEL_EXCEPTION_FILTER g_Previous = nullptr;
        bool g_FilterSet              = false;
        volatile LONG g_Busy          = 0;
        LONG g_Reports                = 0;
        DWORD g_LastCode              = 0;
        uint32_t g_LastAddress        = 0;
        // Static, as the heap may be what broke.
        CrashModule g_Modules[kMostModules];
        char g_Report[kReportBytes];

        uint32_t Address(const void* p) { return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(p)); }

        void WriteFileText(const char* path, const char* text, DWORD access, DWORD disposition)
        {
            const HANDLE file = CreateFileA(path, access, FILE_SHARE_READ, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE) return;
            DWORD written = 0;
            WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
            CloseHandle(file);
        }

        void AddToStart(const char* text)
        {
            char path[MAX_PATH];
            std::snprintf(path, sizeof(path), "%s\\%s", g_Folder, kStartFile);
            WriteFileText(path, text, FILE_APPEND_DATA, OPEN_ALWAYS);
        }

        // Its link time and the span of its executable sections, from its header when that can be read.
        void ReadHeader(CrashModule& module)
        {
            module.codeStart  = module.base;
            module.codeEnd    = module.base + module.size;
            const auto* image = reinterpret_cast<const BYTE*>(static_cast<uintptr_t>(module.base));
            const auto* dos   = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
            if (!Readable(dos, sizeof(*dos)) || dos->e_magic != IMAGE_DOS_SIGNATURE) return;
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(image + dos->e_lfanew);
            if (!Readable(nt, sizeof(*nt)) || nt->Signature != IMAGE_NT_SIGNATURE) return;
            module.stamp                         = nt->FileHeader.TimeDateStamp;
            const IMAGE_SECTION_HEADER* sections = IMAGE_FIRST_SECTION(nt);
            const WORD count                     = nt->FileHeader.NumberOfSections;
            if (!Readable(sections, count * sizeof(IMAGE_SECTION_HEADER))) return;
            uint32_t start = UINT32_MAX, end = 0;
            for (WORD i = 0; i < count; ++i)
                if ((sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0)
                {
                    const uint32_t from = module.base + sections[i].VirtualAddress;
                    if (from < start) start = from;
                    if (from + sections[i].Misc.VirtualSize > end) end = from + sections[i].Misc.VirtualSize;
                }
            if (start < end)
            {
                module.codeStart = start;
                module.codeEnd   = end;
            }
        }

        size_t ReadModules()
        {
            const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
            if (snapshot == INVALID_HANDLE_VALUE) return 0;
            size_t count = 0;
            MODULEENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            for (BOOL more = Module32First(snapshot, &entry); more && count < kMostModules; more = Module32Next(snapshot, &entry))
            {
                // The file's name alone: a path could hold the player's Windows user name.
                CrashModule& module = g_Modules[count++];
                std::snprintf(module.name, sizeof(module.name), "%s", entry.szModule);
                module.base  = Address(entry.modBaseAddr);
                module.size  = entry.modBaseSize;
                module.stamp = 0;
                ReadHeader(module);
            }
            CloseHandle(snapshot);
            return count;
        }

        void Report(EXCEPTION_POINTERS* pointers, const char* kind)
        {
            if (InterlockedExchange(&g_Busy, 1) != 0) return;
            const EXCEPTION_RECORD* record = pointers->ExceptionRecord;
            const CONTEXT* context         = pointers->ContextRecord;
            const uint32_t address         = Address(record->ExceptionAddress);
            // A fault reported where it happened comes again here when nothing catches it.
            if (g_Reports < kMostReports && !(record->ExceptionCode == g_LastCode && address == g_LastAddress))
            {
                ++g_Reports;
                g_LastCode    = record->ExceptionCode;
                g_LastAddress = address;
                SYSTEMTIME now{};
                GetLocalTime(&now);
                char when[32];
                std::snprintf(when, sizeof(when), "%04u-%02u-%02u %02u:%02u:%02u", now.wYear, now.wMonth, now.wDay,
                    now.wHour, now.wMinute, now.wSecond);
                CrashFacts facts{};
                facts.version   = g_Version;
                facts.when      = when;
                facts.kind      = kind;
                facts.step      = g_Step;
                facts.code      = record->ExceptionCode;
                facts.address   = address;
                facts.infoCount = record->NumberParameters < 2 ? record->NumberParameters : 2;
                for (uint32_t i = 0; i < facts.infoCount; ++i)
                    facts.info[i] = static_cast<uint32_t>(record->ExceptionInformation[i]);
                facts.eax    = context->Eax;
                facts.ebx    = context->Ebx;
                facts.ecx    = context->Ecx;
                facts.edx    = context->Edx;
                facts.esi    = context->Esi;
                facts.edi    = context->Edi;
                facts.ebp    = context->Ebp;
                facts.esp    = context->Esp;
                facts.eip    = context->Eip;
                facts.eflags = context->EFlags;
                // The faulting thread's stack is this thread's: the handlers run on it.
                const uint32_t top = Address(reinterpret_cast<const NT_TIB*>(NtCurrentTeb())->StackBase);
                if (context->Esp < top && context->Esp % sizeof(uint32_t) == 0)
                {
                    size_t words = (top - context->Esp) / sizeof(uint32_t);
                    if (words > kMostStackWords) words = kMostStackWords;
                    const auto* stack = reinterpret_cast<const uint32_t*>(static_cast<uintptr_t>(context->Esp));
                    if (Readable(stack, words * sizeof(uint32_t)))
                    {
                        facts.stack      = stack;
                        facts.stackWords = words;
                    }
                }
                facts.modules     = g_Modules;
                facts.moduleCount = ReadModules();
                WriteCrashReport(facts, g_Report, sizeof(g_Report));
                char name[MAX_PATH];
                std::snprintf(name, sizeof(name), "crash-%04u%02u%02u-%02u%02u%02u-%ld.txt", now.wYear, now.wMonth,
                    now.wDay, now.wHour, now.wMinute, now.wSecond, g_Reports);
                char path[MAX_PATH];
                std::snprintf(path, sizeof(path), "%s\\%s", g_Folder, name);
                WriteFileText(path, g_Report, GENERIC_WRITE, CREATE_ALWAYS);
                char line[MAX_PATH];
                std::snprintf(line, sizeof(line), "%8lu ms  crashed: %s\n", GetTickCount() - g_Loaded, name);
                AddToStart(line);
            }
            InterlockedExchange(&g_Busy, 0);
        }

        bool Fault(DWORD code)
        {
            switch (code)
            {
            case EXCEPTION_ACCESS_VIOLATION:
            case EXCEPTION_ILLEGAL_INSTRUCTION:
            case EXCEPTION_PRIV_INSTRUCTION:
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
            case EXCEPTION_STACK_OVERFLOW:
            case EXCEPTION_IN_PAGE_ERROR:
            case EXCEPTION_DATATYPE_MISALIGNMENT:
            case STATUS_HEAP_CORRUPTION:
            case STATUS_STACK_BUFFER_OVERRUN: return true;
            default: return false;
            }
        }

        // Every exception in the game passes here first, Lua's errors and the faults addons' memory reads catch
        // among them, at hundreds a second: anything more than these two checks before the report slows the game.
        LONG CALLBACK OnFirstChance(EXCEPTION_POINTERS* pointers)
        {
            const EXCEPTION_RECORD* record = pointers->ExceptionRecord;
            if (Address(record->ExceptionAddress) - g_OwnBase < g_OwnSize && Fault(record->ExceptionCode))
                Report(pointers, "a fault in HeadsUp's code");
            return EXCEPTION_CONTINUE_SEARCH;
        }

        LONG WINAPI OnUnhandled(EXCEPTION_POINTERS* pointers)
        {
            Report(pointers, "an exception nothing caught, which ends the game");
            return g_Previous != nullptr ? g_Previous(pointers) : EXCEPTION_CONTINUE_SEARCH;
        }
    }

    void Start(const std::string& folder, const char* version)
    {
        std::snprintf(g_Folder, sizeof(g_Folder), "%s", folder.c_str());
        g_Version = version;
        g_Loaded  = GetTickCount();
        HMODULE own = nullptr;
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(&Start), &own))
        {
            const auto* image = reinterpret_cast<const BYTE*>(own);
            const auto* nt    = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
                image + reinterpret_cast<const IMAGE_DOS_HEADER*>(image)->e_lfanew);
            g_OwnBase = Address(own);
            g_OwnSize = nt->OptionalHeader.SizeOfImage;
        }
        SYSTEMTIME now{};
        GetLocalTime(&now);
        char first[128];
        std::snprintf(first, sizeof(first), "HeadsUp %s loaded %04u-%02u-%02u %02u:%02u:%02u at %08x\n", version,
            now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, g_OwnBase);
        char path[MAX_PATH], previous[MAX_PATH];
        std::snprintf(path, sizeof(path), "%s\\%s", g_Folder, kStartFile);
        // A player starts the game again before sending the logs: the run that crashed is then the one before.
        std::snprintf(previous, sizeof(previous), "%s\\%s", g_Folder, kPreviousStartFile);
        MoveFileExA(path, previous, MOVEFILE_REPLACE_EXISTING);
        WriteFileText(path, first, GENERIC_WRITE, CREATE_ALWAYS);
        g_Handler   = AddVectoredExceptionHandler(1, OnFirstChance);
        g_Previous  = SetUnhandledExceptionFilter(OnUnhandled);
        g_FilterSet = true;
    }

    void Stop()
    {
        if (g_Handler != nullptr) RemoveVectoredExceptionHandler(g_Handler);
        g_Handler = nullptr;
        if (g_FilterSet)
        {
            // A filter set over HeadsUp's stays.
            if (LPTOP_LEVEL_EXCEPTION_FILTER now = SetUnhandledExceptionFilter(g_Previous); now != OnUnhandled)
                SetUnhandledExceptionFilter(now);
            g_FilterSet = false;
        }
        Step("unloaded");
    }

    void Step(const char* step)
    {
        g_Step = step;
        for (size_t i = 0; i < g_SeenCount; ++i)
            if (g_Seen[i] == step) return;
        if (g_SeenCount == kMostSteps || g_Folder[0] == '\0') return;
        g_Seen[g_SeenCount++] = step;
        char line[192];
        std::snprintf(line, sizeof(line), "%8lu ms  %s\n", GetTickCount() - g_Loaded, step);
        AddToStart(line);
    }
}
