#include "crash_report.h"
#include "test.h"

#include <string>

using namespace headsup;

namespace
{
    const CrashModule kModules[] = {
        {"FFXiMain.dll", 0x00400000, 0x00500000, 0x00401000, 0x00700000, 0x4a3f0000},
        {"HeadsUp.dll", 0x6e000000, 0x01600000, 0x6e001000, 0x6f200000, 0x6a1c2b00},
    };

    CrashFacts Facts(const uint32_t* stack, size_t words)
    {
        CrashFacts facts{};
        facts.version     = "0.5.7";
        facts.when        = "2026-11-19 10:30:12";
        facts.kind        = "a fault in HeadsUp";
        facts.step        = "Present: tracker";
        facts.code        = 0xC0000005;
        facts.address     = 0x6e1a2b3c;
        facts.info[0]     = 0;
        facts.info[1]     = 0x10;
        facts.infoCount   = 2;
        facts.eip         = 0x6e1a2b3c;
        facts.esp         = 0x0019f000;
        facts.stack       = stack;
        facts.stackWords  = words;
        facts.modules     = kModules;
        facts.moduleCount = 2;
        return facts;
    }

    std::string Report(const CrashFacts& facts, size_t size = 8192)
    {
        std::string out(size, '\0');
        out.resize(WriteCrashReport(facts, out.data(), size));
        return out;
    }

    bool Has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }
}

TEST(a_crash_names_its_fault_and_where_it_happened_in_which_module)
{
    const std::string report = Report(Facts(nullptr, 0));
    CHECK(Has(report, "HeadsUp 0.5.7"));
    CHECK(Has(report, "Present: tracker"));
    CHECK(Has(report, "c0000005 access violation reading 00000010"));
    CHECK(Has(report, "HeadsUp.dll+0x1a2b3c"));
}

TEST(only_stack_words_in_a_module_s_code_are_listed_as_return_addresses)
{
    // A pointer to a module's data and plain numbers are on the stack too.
    const uint32_t stack[] = {0x00000002, 0x6e0105a0, 0x00700010, 0x0045d72f, 0x6f300000, 0x00400800};
    const std::string report = Report(Facts(stack, 6));
    CHECK(Has(report, "HeadsUp.dll+0x105a0"));
    CHECK(Has(report, "FFXiMain.dll+0x5d72f"));
    CHECK(!Has(report, "FFXiMain.dll+0x300010"));
    CHECK(!Has(report, "FFXiMain.dll+0x800"));
    CHECK(report.find("HeadsUp.dll+0x105a0") < report.find("FFXiMain.dll+0x5d72f"));
}

TEST(every_module_is_listed_with_its_base_and_link_time)
{
    const std::string report = Report(Facts(nullptr, 0));
    CHECK(Has(report, "00400000 00500000 4a3f0000 FFXiMain.dll"));
    CHECK(Has(report, "6e000000 01600000 6a1c2b00 HeadsUp.dll"));
}

TEST(a_fault_outside_every_module_gives_its_bare_address)
{
    CrashFacts facts = Facts(nullptr, 0);
    facts.address    = 0x12345678;
    CHECK(Has(Report(facts), "at 12345678 (in no module)"));
}

TEST(a_report_too_long_for_its_buffer_is_cut_and_still_ended)
{
    char out[40];
    const size_t written = WriteCrashReport(Facts(nullptr, 0), out, sizeof(out));
    CHECK(written == sizeof(out) - 1);
    CHECK(out[sizeof(out) - 1] == '\0');
}
