#pragma once

#include <clocale>
#include <cstring>
#include <locale.h>
#include <string>

namespace headsup
{
    // gdifonttexture (tTimers, Balloon) calls setlocale(LC_ALL, "") for the whole game, and a comma-decimal locale
    // breaks every number HeadsUp and RmlUi read or write, so HeadsUp's code runs with C numbers on its own thread.
    class CNumbers
    {
    public:
        CNumbers()
        {
            const char* now = std::setlocale(LC_NUMERIC, nullptr);
            if (std::strcmp(now, "C") == 0) return;
            m_Was  = now;
            m_Mode = _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
            std::setlocale(LC_NUMERIC, "C");
        }
        ~CNumbers()
        {
            if (m_Was.empty()) return;
            if (m_Mode == _ENABLE_PER_THREAD_LOCALE) std::setlocale(LC_NUMERIC, m_Was.c_str());
            else _configthreadlocale(m_Mode);
        }
        CNumbers(const CNumbers&)            = delete;
        CNumbers& operator=(const CNumbers&) = delete;

    private:
        std::string m_Was;
        int m_Mode = 0;
    };
}
