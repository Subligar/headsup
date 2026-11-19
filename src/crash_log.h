#pragma once

#include <string>

namespace headsup::crash
{
    // From here to Stop, a fault in HeadsUp's code (caught or not) and any exception the game dies of each write a
    // report to folder\crash-<time>-<n>.txt; folder\start.txt gets each step the first time it is reached, for an end
    // that no handler sees (a fail-fast), and "unloaded" at Stop. The last load's is kept as start-previous.txt.
    void Start(const std::string& folder, const char* version);
    void Stop();
    // Where HeadsUp is, for the reports. Kept by pointer and told apart by it: a string literal.
    void Step(const char* step);
}
